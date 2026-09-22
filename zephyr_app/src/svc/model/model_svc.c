/**
 * @file model_svc.c
 * @brief The model thread: the legacy loop driven by events
 *
 * docs/16-arquitetura-firmware.md (Threads, Máquinas de estado): the only
 * writer of the device state. It takes the positions, the sensors and the
 * commands from its inbox, runs the model code ported from the legacy
 * (attitude and its Kalman filter, segments, zones, route) and publishes a
 * copy of what the screens show on chan_model_state, once per epoch and at
 * least once a second.
 *
 * The mode machine (CRS, PRC, FEC, Zwift, DBG) lives apart, in
 * `model/mode_fsm.c`, so that its rules run in the host tests; this file
 * only lends it the four operations it needs.
 */

#include <math.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>
#include <zephyr/zbus/zbus.h>

#include "app/app_channels.h"
#include "app/app_svc.h"
#include "model/activity.h"
#include "model/attitude.h"
#include "model/climb.h"
#include "model/incident.h"
#include "model/loc_arbiter.h"
#include "model/power_metrics.h"
#include "model/radar.h"
#include "model/vecteur.h"
#include "model/fit_encode.h"
#include "model/crash_recovery.h"
#include "model/parcours.h"
#include "model/segment.h"
#include "model/user_settings.h"
#include "model_internal.h"

LOG_MODULE_REGISTER(model_svc, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * The altitude Kalman chain needs about 2200 bytes (measured on the former
 * main_loop, docs/05); the rest is the zbus publish and the event handlers.
 */
#define MODEL_STACK_SIZE        4096
#define MODEL_INBOX_LEN         32
/** The screens get a new snapshot at least this often (clock, GNSS age) */
#define MODEL_STATE_PERIOD_MS   1000U
/** Default FTP until the settings give one (legacy UserSettings) */
#define MODEL_DEFAULT_FTP_W     200U
/** An external value older than this is lost (docs/16, Sensor externo) */
#define MODEL_EXT_MAX_AGE_MS    5000U

struct model_msg {
    const struct zbus_channel *chan;
    /* small messages travel by copy; the big ones are read from the channel */
    union {
        struct app_gnss_fix fix;
        struct app_baro baro;
        struct app_imu imu;
        struct app_mag mag;
        struct app_ext_sensor ext;
        struct app_link_status link;
        struct app_power_status power;
        struct app_system_cmd cmd;
        struct app_system_state sys;
        struct app_phone_nav nav;
        struct app_radar radar;
    } u;
};

K_MSGQ_DEFINE(model_inbox, sizeof(struct model_msg), MODEL_INBOX_LEN, 4);

static struct model_ctx ctx;

/*
 * Totals, auto-pause and laps of the ride (`model/activity.h`). The model
 * owns it, as it owns everything else the ride is made of; the storage and
 * the interface read it from `chan_activity`.
 */
static struct activity act;
static uint32_t act_last_ms;

static void update_activity(const attitude_t *att, const loc_data_t *loc);
static void publish_activity(enum activity_event ev, bool finished);

/* The thinned copy of the route the climb scan walks (model_internal.h) */
static struct {
    float dist_m[MODEL_CLIMB_SCAN_MAX];
    float alt_m[MODEL_CLIMB_SCAN_MAX];
    uint16_t n;
} climb_scan;

/** The snapshot is big (UI maps): it lives here, not on the stack */
static ui_model_t snapshot;

static void model_listener(const struct zbus_channel *chan)
{
    struct model_msg msg = {.chan = chan};
    size_t size = zbus_chan_msg_size(chan);

    if (size <= sizeof(msg.u)) {
        (void)memcpy(&msg.u, zbus_chan_const_msg(chan), size);
    }
    app_inbox_put(&model_inbox, &msg, "model");
}

ZBUS_LISTENER_DEFINE(model_lis, model_listener);
ZBUS_CHAN_ADD_OBS(chan_gnss_fix, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_gnss_sky, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_baro, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_imu, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_mag, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_ext_sensor, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_radar, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_link_status, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_pair_list, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_phone_nav, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_power_status, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_system_cmd, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_system_state, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_storage_info, model_lis, 3);

/* ==========================================================================
 * Mode machine: the rules in model/mode_fsm.c, the effects here
 * ========================================================================== */

static void publish_mode(void)
{
    struct app_mode_state m = {.mode = ctx.mode, .route = parcours_is_active(),
                               .recording = ctx.recording};

    (void)app_publish(&chan_mode, &m);
}

/* ---- what the machine of model/mode_fsm.c asks of this service ---- */

static void mode_op_publish(enum app_mode mode, void *user)
{
    ARG_UNUSED(user);
    ctx.mode = (uint8_t)mode;
    publish_mode();
}

static void mode_op_route_start(void *user)
{
    ARG_UNUSED(user);
    (void)parcours_start();
}

static void mode_op_route_stop(void *user)
{
    ARG_UNUSED(user);
    parcours_stop();
}

static void mode_op_refused(enum app_mode wanted, enum mode_refusal why, void *user)
{
    ARG_UNUSED(user);
    ARG_UNUSED(wanted);

    /* the rider is told why the key did nothing, instead of nothing happening */
    switch (why) {
    case MODE_REFUSED_NO_ROUTE:
        app_notify("MODO", "Carregue um percurso", NULL, false, 0U);
        break;
    case MODE_REFUSED_RECORDING:
        app_notify("MODO", "Termine o pedal antes", NULL, false, 0U);
        break;
    default:
        break;      /* the same mode, or one that does not exist: stay quiet */
    }
}

static const struct mode_fsm_ops mode_ops = {
    .publish = mode_op_publish,
    .route_start = mode_op_route_start,
    .route_stop = mode_op_route_stop,
    .refused = mode_op_refused,
};

static void set_mode(int32_t mode)
{
    (void)mode_fsm_select(&ctx.fsm, mode);
}

/* ==========================================================================
 * Handlers
 * ========================================================================== */

/**
 * @brief Open the route the interface chose, as the legacy does in PRC
 *
 * The list comes from the storage service, with the name of the file as it
 * is on the card (`legacy/source/sd/sd_functions.cpp:451`, `load_parcours`).
 */
/** The channel of this thread, for the loaders that take their time */
static int model_wdt_channel = -1;

static void model_feed_wdt(void)
{
    if (model_wdt_channel >= 0) {
        app_wdt_feed(model_wdt_channel);
    }
}

static void climb_scan_point(uint16_t index, float *dist_m, float *alt_m, void *user)
{
    ARG_UNUSED(user);
    *dist_m = climb_scan.dist_m[index];
    *alt_m = climb_scan.alt_m[index];
}

/** Walk the route once and write down where its climbs are */
static void find_climbs(void)
{
    uint16_t n = parcours_get_num_points();

    climb_scan.n = 0U;
    (void)memset(&ctx.climbs, 0, sizeof(ctx.climbs));
    if (n < 2U) {
        return;
    }

    uint16_t step = (uint16_t)(((uint32_t)n + MODEL_CLIMB_SCAN_MAX - 1U) / MODEL_CLIMB_SCAN_MAX);

    if (step < 1U) {
        step = 1U;
    }

    float total = 0.0f;
    const point_t *prev = parcours_get_point(0U);

    if (prev == NULL) {
        return;
    }

    float plat = prev->lat;
    float plon = prev->lon;

    climb_scan.dist_m[0] = 0.0f;
    climb_scan.alt_m[0] = prev->alt;
    climb_scan.n = 1U;

    for (uint16_t i = 1U; (i < n) && (climb_scan.n < MODEL_CLIMB_SCAN_MAX); i++) {
        const point_t *p = parcours_get_point(i);

        if (p == NULL) {
            break;
        }
        /* the distance follows every point, so thinning does not cut corners */
        total += distance_between(plat, plon, p->lat, p->lon);
        plat = p->lat;
        plon = p->lon;

        if (((i % step) == 0U) || (i == (n - 1U))) {
            climb_scan.dist_m[climb_scan.n] = total;
            climb_scan.alt_m[climb_scan.n] = p->alt;
            climb_scan.n++;
        }
    }

    uint8_t found = climb_find(&ctx.climbs, climb_scan.n, climb_scan_point, NULL);

    LOG_INF("route: %u climbs over %u m", (unsigned int)found, (unsigned int)total);
}

static void load_selected_route(void)
{
    char path[48];

    if ((ctx.route_sel < 0) || ((uint8_t)ctx.route_sel >= ctx.storage.nroutes)) {
        return;
    }

    (void)snprintf(path, sizeof(path), "/SD:/%s", ctx.storage.route[ctx.route_sel]);

    if (parcours_load(path) != APP_OK) {
        LOG_WRN("route %s did not load", path);
        app_notify("PRC", "Percurso nao abriu", NULL, false, 0U);
        /*
         * A failed load may have taken the route that was there with it, so
         * the machine hears the truth and not an assumption: with no route
         * left it drops a rider who was in PRC back to the free ride.
         */
        mode_fsm_set_route_loaded(&ctx.fsm, parcours_is_loaded());
        return;
    }

    LOG_INF("route %s loaded", path);
    find_climbs();
    mode_fsm_set_route_loaded(&ctx.fsm, true);
    if (ctx.mode == APP_MODE_ID_PRC) {
        (void)parcours_start();
    }
    publish_mode();
}

static void publish_state(void)
{
    model_ui_fill(&ctx, &snapshot);
    (void)app_publish(&chan_model_state, &snapshot);

    /*
     * Whoever is not the interface only needs to know whether a ride is
     * being recorded, and only when that changes: the radio refuses an
     * update over the air in the middle of one (`model/dfu_state.c`).
     */
    if (snapshot.status.recording != ctx.recording) {
        ctx.recording = snapshot.status.recording;
        /* rule 2 of model/mode_fsm.h needs to know a ride is under way */
        mode_fsm_set_recording(&ctx.fsm, ctx.recording);
        publish_mode();
    }
}

/** loc_data_t of the model from a GNSS epoch */
static void fix_to_loc(const struct app_gnss_fix *f, loc_data_t *loc)
{
    loc->lat = (float)((double)f->lat_e7 * 1e-7);
    loc->lon = (float)((double)f->lon_e7 * 1e-7);
    loc->alt = (float)f->alt_mm / 1000.0f;
    loc->speed = (float)f->speed_mms * 0.0036f;
    loc->course = (float)f->course_mdeg / 1000.0f;
    loc->timestamp = f->uptime_ms;
}

static void on_fix(const struct app_gnss_fix *f)
{
    uint32_t now = k_uptime_get_32();

    /*
     * Which source the model listens to is the rule of the legacy, in
     * `model/loc_arbiter.c` with its own tests: a simulated ride wins and
     * holds the floor for two seconds between its frames, so the receiver
     * cannot slip a position in and make the bike jump.
     */
    loc_arbiter_feed(&ctx.arb, f->sim ? LOC_ARB_SIM : LOC_ARB_GPS, now);

    enum loc_arb_src src = loc_arbiter_pick(&ctx.arb, now, ctx.have_fix_msg && ctx.fix.fix);

    if (src == LOC_ARB_NONE) {
        return;
    }
    ctx.sim_uptime_ms = (src == LOC_ARB_SIM) ? now : 0U;

    ctx.fix = *f;
    ctx.have_fix_msg = true;

    if (f->time_valid) {
        date_data_t date = {
            .date = ((uint32_t)f->day * 10000U) + ((uint32_t)f->month * 100U) + f->year2,
            .secj = ((uint32_t)f->hour * 3600U) + ((uint32_t)f->minute * 60U) +
                    (f->millisecond / 1000U),
            .timestamp = f->uptime_ms,
        };

        attitude_update_datetime(&date);
    }
    if (!f->fix) {
        return;
    }
    ctx.fix_uptime_ms = f->uptime_ms;
    if (!mode_fsm_is_outdoor(&ctx.fsm)) {
        return;
    }

    loc_data_t loc;

    fix_to_loc(f, &loc);
    (void)attitude_update_gps(&loc);
    if (ctx.storage.segments > 0U) {
        /*
         * The allocator first, as the legacy does in its own loop
         * (`legacy/source/model/BoucleCRS.cpp:121`): it opens the file of a
         * segment the rider is coming to and drops the ones left behind.
         */
        (void)segment_run_allocator(loc.lat, loc.lon);
        (void)segment_update(&loc);
    }
    if ((ctx.mode == APP_MODE_ID_PRC) && parcours_is_active()) {
        parcours_update(loc.lat, loc.lon, loc.alt);

        /* where the rider stands on the climb ahead (`model/climb.h`) */
        nav_info_t nav;
        bool was_on = ctx.climb.on_climb;
        uint8_t was_idx = ctx.climb.index;

        if (parcours_get_nav_info(&nav) == APP_OK) {
            climb_update(&ctx.climb, &ctx.climbs, nav.dist_completed, loc.alt);
            ctx.climb.ahead_grade_pct = climb_grade_ahead(climb_scan.n, climb_scan_point, NULL,
                                                          nav.dist_completed);
            if (ctx.climb.on_climb && (!was_on || (was_idx != ctx.climb.index))) {
                /* the foot of a climb: tell the rider what is coming */
                const struct climb *c = &ctx.climbs.c[ctx.climb.index];
                char what[24];
                char how[16];

                (void)snprintf(what, sizeof(what), "%.1f km a %.0f%%",
                               (double)(climb_length(c) / 1000.0f), (double)climb_grade(c));
                (void)snprintf(how, sizeof(how), "%d m", (int)climb_gain(c));
                app_notify("Subida", what, how, false, 0U);
            }
        }
    } else if (ctx.climb.on_climb) {
        (void)memset(&ctx.climb, 0, sizeof(ctx.climb));
    }

    /* the vehicles behind age whether a frame came or not */
    radar_tick(&ctx.rad, k_uptime_get_32());

    if (attitude_take_fdir_notice()) {
        /* the legacy shows "FDIR / Attitude restored" (`Attitude.cpp:410`) */
        app_notify("FDIR", "Atitude restaurada", NULL, true, 0U);
    }

    /* one log point per epoch; the storage keeps one per 15 m (sd_logger) */
    attitude_t att;

    if (attitude_get(&att) == APP_OK) {
        attitude_ext_t ext;
        struct app_log_point p = {
            .loc = loc,
            .date = att.date,
            .power_w = att.pwr,
            .hr_bpm = ctx.ext[APP_EXT_HR].hr_bpm,
            .cadence_rpm = ctx.ext[APP_EXT_BSC].cadence_rpm,
            .filt_alt = attitude_get_elevation(),
            .vit_asc = att.vit_asc,
            .slope_pct = att.slope,
            .dist_m = att.dist,
            .climb_m = att.climb,
            .fit_time = fit_time_from_date(att.date.date, att.date.secj),
        };

        (void)memcpy(p.rough, ctx.rough, sizeof(p.rough));

        if (attitude_get_ext(&ext) == APP_OK) {
            p.baro_alt = ext.baro_altitude;
            p.alpha_bar = ext.alpha_bar;
            p.alpha_zero = ext.alpha_zero;
            p.baro_corr = ext.baro_correction;
            p.b_rough = ext.baro_roughness;
        } else {
            p.baro_alt = loc.alt;
        }
        (void)app_publish(&chan_log_point, &p);

        /*
         * After the point, not before: the storage writes the record of
         * this epoch first, so a lap that closes here lands after the
         * records that belong to it.
         */
        update_activity(&att, &loc);
    }
}

static void on_ext(const struct app_ext_sensor *e)
{
    if (e->kind >= APP_EXT_KINDS) {
        return;
    }
    ctx.ext[e->kind] = *e;
    ctx.ext_uptime_ms[e->kind] = e->uptime_ms;

    switch (e->kind) {
    case APP_EXT_HR:
        attitude_update_hrm(e->hr_bpm, true);
        if (e->rr_ms > 0U) {
            hrm_info_t hrm = {.bpm = e->hr_bpm, .rr_interval = e->rr_ms,
                              .timestamp = e->uptime_ms, .connected = true};

            rr_zone_add_data(&ctx.rr, &hrm);
        }
        break;
    case APP_EXT_FEC:
        /*
         * legacy BoucleFEC::run_internal() (BoucleFEC.cpp:75): the power zones
         * take every trainer update, whatever the power; PowerZone leaves out
         * what falls outside 50 to 1950 W but moves its clock
         */
        if (ctx.mode == APP_MODE_ID_FEC) {
            power_zone_add_data(&ctx.zones, e->power_w, e->uptime_ms);
            publish_state();
        }
        break;
    default:
        break;
    }
}

/**
 * The alarm and the crash detection, once a second with the IMU message.
 *
 * Not a safety device: what it is and what it is not, in
 * `model/incident.h`. The rider is warned and can always say no.
 */
static void on_incident(const struct app_imu *imu)
{
    attitude_t att;
    struct incident_sample s = {
        .peak_g = imu->peak_g,
        .still_g = imu->still_g,
        .speed_kmh = (attitude_get(&att) == APP_OK) ? att.loc.speed : 0.0f,
    };

    switch (incident_update(&ctx.inc, &s, MODEL_STATE_PERIOD_MS)) {
    case INCIDENT_EVENT_ALARM:
        app_notify("ALARME", "A bicicleta se moveu", NULL, true, 0U);
        break;
    case INCIDENT_EVENT_COUNTING:
        app_notify("QUEDA?", "Toque para cancelar", NULL, true, 0U);
        break;
    case INCIDENT_EVENT_CRASH:
        /* the phone is told by the radio service, which reads the model */
        app_notify("QUEDA", "Sem resposta", NULL, true, 0U);
        break;
    default:
        break;
    }
}

static void on_imu(const struct app_imu *imu)
{
    on_incident(imu);
    ctx.pitch_deg = imu->pitch_deg;
    (void)memcpy(ctx.rough, imu->rough, sizeof(ctx.rough));
    (void)attitude_update_imu(ctx.heading_valid ? ctx.heading_deg : 0.0f, imu->pitch_deg,
                              imu->roll_deg);

    /* slope history of CRS page 3, one sample a second (legacy fxos pitch buffer) */
    float pct = tanf(imu->pitch_deg * 0.0174533f) * 100.0f;
    int8_t v = (int8_t)((pct > 100.0f) ? 100 : ((pct < -100.0f) ? -100 : (int)pct));

    if (ctx.pitch_histo_n < UI_HISTO_MAX) {
        ctx.pitch_histo[ctx.pitch_histo_n++] = v;
    } else {
        (void)memmove(&ctx.pitch_histo[0], &ctx.pitch_histo[1], UI_HISTO_MAX - 1U);
        ctx.pitch_histo[UI_HISTO_MAX - 1U] = v;
    }
}

/** One frame of the rear radar (`model/radar.h`) */
static void on_radar(const struct app_radar *in)
{
    uint32_t now = k_uptime_get_32();

    radar_set_link(&ctx.rad, in->linked, now);
    if (!in->linked) {
        return;
    }

    struct radar_frame f = {.n = (in->n < RADAR_TARGETS_MAX) ? in->n : RADAR_TARGETS_MAX};

    for (uint8_t i = 0U; i < f.n; i++) {
        f.t[i].id = in->id[i];
        f.t[i].range_m = in->range_m[i];
        f.t[i].closing_kmh = in->closing_kmh[i];
        f.t[i].level = in->level[i];
        f.t[i].side = in->side[i];
    }
    radar_feed(&ctx.rad, &f, now);
}

/** Fill the compact form the channel carries from a set of totals */
static void fill_totals(struct app_totals *out, const struct activity_totals *t)
{
    out->start_time = t->start_time;
    out->end_time = t->end_time;
    out->elapsed_ms = t->elapsed_ms;
    out->timer_ms = t->timer_ms;
    out->dist_m = t->dist_m;
    out->ascent_m = t->ascent_m;
    out->descent_m = t->descent_m;
    out->avg_speed_kmh = activity_avg_speed(t);
    out->max_speed_kmh = t->max_speed_kmh;
    out->avg_power_w = activity_avg_power(t);
    out->max_power_w = t->max_power_w;
    out->calories_kcal = activity_calories(t);
    out->avg_hr_bpm = activity_avg_hr(t);
    out->max_hr_bpm = t->max_hr_bpm;
    out->avg_cadence_rpm = activity_avg_cadence(t);
}

/** Tell the storage and the interface where the ride stands */
static void publish_activity(enum activity_event ev, bool finished)
{
    struct app_activity msg = {
        .lap_dist_m = act.lap.dist_m,
        .lap_timer_ms = act.lap.timer_ms,
        .laps = act.laps,
        .event = (uint8_t)ev,
        .running = act.running,
        .finished = finished,
    };

    fill_totals(&msg.ride, activity_ride(&act));
    fill_totals(&msg.lap, activity_lap(&act));
    /* the snapshot of the screens reads it from here (model_ui.c) */
    ctx.act = msg;
    (void)app_publish(&chan_activity, &msg);
}

/** One epoch of the ride: totals, auto-pause and the automatic lap */
static void update_activity(const attitude_t *att, const loc_data_t *loc)
{
    uint32_t now = k_uptime_get_32();
    uint32_t dt = (act_last_ms != 0U) ? (now - act_last_ms) : 0U;

    act_last_ms = now;

    struct activity_sample s = {
        .time = fit_time_from_date(att->date.date, att->date.secj),
        .speed_kmh = loc->speed,
        .dist_m = att->dist,
        .climb_m = att->climb,
        .alt_m = attitude_get_elevation(),
        .power_w = att->pwr,
        .hr_bpm = ctx.ext[APP_EXT_HR].hr_bpm,
        .cadence_rpm = ctx.ext[APP_EXT_BSC].cadence_rpm,
    };

    enum activity_event ev = activity_update(&act, &s, dt);

    /*
     * One sample a second of **moving** time into the power metrics: the
     * timer of the activity already leaves the pauses out, so a stop at the
     * traffic lights cannot feed zeros and drag the rolling average down
     * (`model/power_metrics.h`). A late epoch catches up second by second,
     * up to a window's worth; past that the gap is long enough that
     * pretending the power held would be a lie.
     */
    uint32_t moving_s = activity_ride(&act)->timer_ms / 1000U;

    if (moving_s > ctx.pm_last_s) {
        uint32_t missing = moving_s - ctx.pm_last_s;

        if (missing > PM_WINDOW_S) {
            missing = PM_WINDOW_S;
        }
        for (uint32_t i = 0U; i < missing; i++) {
            power_metrics_add(&ctx.pm, s.power_w);
        }
        ctx.pm_last_s = moving_s;
    }

    if (ev == ACTIVITY_EVENT_LAP) {
        app_notify("Volta", NULL, NULL, false, 0U);
    }
    publish_activity(ev, false);
}

static void on_command(const struct app_system_cmd *cmd)
{
    user_settings_t *settings = user_settings_get_global();

    switch (cmd->id) {
    case APP_CMD_SET_MODE:
        set_mode(cmd->arg);
        break;
    case APP_CMD_ROUTE_SELECT:
        ctx.route_sel = (int8_t)cmd->arg;
        load_selected_route();
        break;
    case APP_CMD_SET_FTP:
        user_settings_set_ftp(settings, (uint16_t)cmd->arg);
        power_zone_set_ftp(&ctx.zones, (uint16_t)cmd->arg);
        power_metrics_set_ftp(&ctx.pm, (uint16_t)cmd->arg);
        (void)user_settings_save(settings);
        break;
    case APP_CMD_SET_WEIGHT:
        /* stored in hectograms (UserSettings) */
        user_settings_set_weight(settings, (uint16_t)(cmd->arg * 10));
        attitude_set_rider_weight((float)cmd->arg);
        (void)user_settings_save(settings);
        break;
    case APP_CMD_ALARM_TOGGLE:
        incident_arm(&ctx.inc, !incident_is_armed(&ctx.inc));
        app_notify("Alarme", incident_is_armed(&ctx.inc) ? "Armado" : "Desarmado", NULL, false, 0U);
        break;
    case APP_CMD_KEY:
        /*
         * Any key answers the device: it silences the alarm and cancels a
         * crash countdown, because a rider who can press a key is there.
         */
        if (incident_state(&ctx.inc) != INCIDENT_OFF) {
            incident_cancel(&ctx.inc);
        }
        break;
    case APP_CMD_LAP:
        if (activity_lap_now(&act)) {
            publish_activity(ACTIVITY_EVENT_LAP, false);
            app_notify("Volta", NULL, NULL, false, 0U);
        }
        break;
    case APP_CMD_ZOOM:
        if ((cmd->arg > 0) && (ctx.zoom < 5U)) {
            ctx.zoom++;
        } else if ((cmd->arg < 0) && (ctx.zoom > 1U)) {
            ctx.zoom--;
        }
        break;
    default:
        return;
    }
    publish_state();
}

static void on_system_state(const struct app_system_state *s)
{
    if ((s->state != APP_SYS_SHUTDOWN) || ctx.shutting_down) {
        return;
    }
    ctx.shutting_down = true;

    /*
     * The lap that was being ridden closes, and the storage hears that the
     * ride ended: that is what lets it write the session of the FIT file
     * and seek back to fix the header.
     */
    activity_finish(&act);
    publish_activity(ACTIVITY_EVENT_NONE, true);

    /*
     * legacy power_scheduler__shutdown(): forget the saved activity, so the
     * next boot does not restore a ride that ended on purpose. The storage
     * service writes the last log batch on its own.
     */
    crash_recovery_clear_saved_state();

    struct app_shutdown_ack ack = {.svc = APP_SVC_MODEL};

    (void)app_publish(&chan_shutdown_ack, &ack);
}

static void handle(struct model_msg *msg)
{
    const struct zbus_channel *chan = msg->chan;

    if (chan == &chan_system_state) {
        on_system_state(&msg->u.sys);
        return;
    }
    if (ctx.shutting_down) {
        return;
    }
    if (chan == &chan_gnss_fix) {
        on_fix(&msg->u.fix);
        publish_state();
    } else if (chan == &chan_baro) {
        (void)attitude_update_baro(msg->u.baro.pressure_pa, msg->u.baro.temp_c);
    } else if (chan == &chan_imu) {
        on_imu(&msg->u.imu);
    } else if (chan == &chan_mag) {
        ctx.heading_valid = msg->u.mag.valid;
        ctx.heading_deg = msg->u.mag.heading_deg;
    } else if (chan == &chan_ext_sensor) {
        on_ext(&msg->u.ext);
    } else if (chan == &chan_radar) {
        on_radar(&msg->u.radar);
    } else if (chan == &chan_link_status) {
        if (msg->u.link.kind < APP_EXT_KINDS) {
            ctx.link[msg->u.link.kind] = msg->u.link;
            attitude_update_hrm(ctx.ext[APP_EXT_HR].hr_bpm,
                                ctx.link[APP_EXT_HR].link == APP_LINK_CONNECTED);
        }
    } else if (chan == &chan_power_status) {
        ctx.power = msg->u.power;
        if (ctx.power.gauge) {
            attitude_update_battery(ctx.power.pct, (float)ctx.power.mv);
        }
    } else if (chan == &chan_phone_nav) {
        ctx.nav = msg->u.nav;
    } else if (chan == &chan_system_cmd) {
        on_command(&msg->u.cmd);
    } else if (chan == &chan_gnss_sky) {
        (void)zbus_chan_read(chan, &ctx.sky, K_MSEC(10));
    } else if (chan == &chan_pair_list) {
        (void)zbus_chan_read(chan, &ctx.pair, K_MSEC(10));
        publish_state();
    } else if (chan == &chan_storage_info) {
        (void)zbus_chan_read(chan, &ctx.storage, K_MSEC(10));
    } else {
        /* a channel added to the listener without a handler */
    }
}

/** Heart rate of a connected strap with fresh data, else 0 */
static uint8_t current_hr(void)
{
    if ((ctx.link[APP_EXT_HR].link != APP_LINK_CONNECTED) ||
        ((k_uptime_get_32() - ctx.ext_uptime_ms[APP_EXT_HR]) > MODEL_EXT_MAX_AGE_MS)) {
        return 0U;
    }
    return ctx.ext[APP_EXT_HR].hr_bpm;
}

static void model_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct model_msg msg;
    uint32_t last_state_ms = 0U;

    /* CRS, as the legacy after the boot (main.cpp: boucle__change_mode) */
    mode_fsm_init(&ctx.fsm, &mode_ops);

    loc_arbiter_init(&ctx.arb);
    activity_init(&act, (uint32_t)CONFIG_GNSS_AUTOLAP_M,
                  IS_ENABLED(CONFIG_GNSS_AUTO_PAUSE));
    incident_init(&ctx.inc, IS_ENABLED(CONFIG_GNSS_CRASH_DETECT));
    /* so the status bar does not open showing the ride as paused */
    publish_activity(ACTIVITY_EVENT_NONE, false);

    int wdt = app_wdt_add("model");

    /* a course of megabytes takes longer than the watchdog allows */
    model_wdt_channel = wdt;
    parcours_set_progress(model_feed_wdt);

    for (;;) {
        uint32_t now = k_uptime_get_32();
        uint32_t since = now - last_state_ms;
        uint32_t wait = (since >= MODEL_STATE_PERIOD_MS) ? 0U : (MODEL_STATE_PERIOD_MS - since);

        if (app_inbox_get(&model_inbox, &msg, wdt, wait) == 0) {
            handle(&msg);
        }
        if (!ctx.shutting_down &&
            ((k_uptime_get_32() - last_state_ms) >= MODEL_STATE_PERIOD_MS)) {
            /*
             * The legacy scores the heart rate on every loop, in every mode,
             * with whatever it holds (Model.cpp:377); here once a second, with
             * 0 when the strap is lost, which moves the clock of the score
             */
            suffer_score_add_hrm(&ctx.suffer, current_hr(), k_uptime_get_32());
            publish_state();
            last_state_ms = k_uptime_get_32();
        }
    }
}

K_THREAD_DEFINE(model_tid, MODEL_STACK_SIZE, model_thread, NULL, NULL, NULL, APP_PRIO_MODEL, 0,
                SYS_FOREVER_MS);

void model_svc_init(void)
{
    const user_settings_t *settings = user_settings_get_global();
    uint16_t ftp = user_settings_get_ftp(settings);

    ctx.route_sel = -1;
    ctx.zoom = 3U;
    power_zone_init(&ctx.zones, (ftp > 0U) ? ftp : MODEL_DEFAULT_FTP_W);
    /*
     * The threshold goes in as the rider set it, zero included: without one
     * there is no intensity factor and no training stress to show, and a
     * made-up default would give them a number that means nothing.
     */
    power_metrics_init(&ctx.pm, ftp);
    ctx.pm_last_s = 0U;
    suffer_score_init(&ctx.suffer);
    rr_zone_init(&ctx.rr);
    /* attitude restores the activity a crash interrupted (FDIR) */
    if (attitude_init() != APP_OK) {
        LOG_ERR("attitude init failed");
    }
    /* before the storage thread loads the segments into these modules */
    (void)segment_init();
    (void)parcours_init();
}

void model_svc_start(void)
{
    k_thread_name_set(model_tid, "model");
    k_thread_start(model_tid);
}
