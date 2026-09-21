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
 * The mode machine (CRS, PRC, FEC, Zwift, DBG) follows
 * boucle__change_mode() (legacy/source/model/Boucle.cpp:101-143): leaving a
 * mode invalidates it, and each mode starts on demand.
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
ZBUS_CHAN_ADD_OBS(chan_link_status, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_pair_list, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_phone_nav, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_power_status, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_system_cmd, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_system_state, model_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_storage_info, model_lis, 3);

/* ==========================================================================
 * Mode machine (legacy Boucle modes, plus the DBG screen on the CRS loop)
 * ========================================================================== */

static const struct smf_state mode_states[APP_MODE_ID_DBG + 1];

static bool rides_outdoors(uint8_t mode)
{
    return (mode == APP_MODE_ID_CRS) || (mode == APP_MODE_ID_PRC) || (mode == APP_MODE_ID_DBG);
}

static void publish_mode(void)
{
    struct app_mode_state m = {.mode = ctx.mode, .route = parcours_is_active(),
                               .recording = ctx.recording};

    (void)app_publish(&chan_mode, &m);
}

static void mode_crs_entry(void *o)
{
    ARG_UNUSED(o);
    ctx.mode = APP_MODE_ID_CRS;
    publish_mode();
}

static void mode_prc_entry(void *o)
{
    ARG_UNUSED(o);
    ctx.mode = APP_MODE_ID_PRC;
    if (parcours_is_loaded()) {
        (void)parcours_start();
    }
    publish_mode();
}

static void mode_prc_exit(void *o)
{
    ARG_UNUSED(o);
    parcours_stop();
}

static void mode_fec_entry(void *o)
{
    ARG_UNUSED(o);
    ctx.mode = APP_MODE_ID_FEC;
    /* the legacy BoucleFEC starts its own zones and score (BoucleFEC.cpp) */
    power_zone_reset(&ctx.zones);
    suffer_score_reset(&ctx.suffer);
    publish_mode();
}

static void mode_zwift_entry(void *o)
{
    ARG_UNUSED(o);
    ctx.mode = APP_MODE_ID_ZWIFT;
    publish_mode();
}

static void mode_dbg_entry(void *o)
{
    ARG_UNUSED(o);
    /* legacy _page0_mode_debug(): the DBG screen over the CRS loop */
    ctx.mode = APP_MODE_ID_DBG;
    publish_mode();
}

static const struct smf_state mode_states[APP_MODE_ID_DBG + 1] = {
    [APP_MODE_ID_CRS] = SMF_CREATE_STATE(mode_crs_entry, NULL, NULL, NULL, NULL),
    [APP_MODE_ID_PRC] = SMF_CREATE_STATE(mode_prc_entry, NULL, mode_prc_exit, NULL, NULL),
    [APP_MODE_ID_FEC] = SMF_CREATE_STATE(mode_fec_entry, NULL, NULL, NULL, NULL),
    [APP_MODE_ID_ZWIFT] = SMF_CREATE_STATE(mode_zwift_entry, NULL, NULL, NULL, NULL),
    [APP_MODE_ID_DBG] = SMF_CREATE_STATE(mode_dbg_entry, NULL, NULL, NULL, NULL),
};

static void set_mode(int32_t mode)
{
    if ((mode < 0) || (mode > (int32_t)APP_MODE_ID_DBG) || ((uint8_t)mode == ctx.mode)) {
        return;
    }
    LOG_INF("mode %u -> %d", ctx.mode, (int)mode);
    smf_set_state(SMF_CTX(&ctx), &mode_states[mode]);
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
        return;
    }

    LOG_INF("route %s loaded", path);
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
     * A position given by a PC wins over the receiver while it keeps
     * coming, which is the SIM source of the legacy
     * (`legacy/source/model/Locator.cpp`, eLocationSourceSIM first).
     */
    if (f->sim) {
        ctx.sim_uptime_ms = now;
    } else if ((ctx.sim_uptime_ms != 0U) && ((now - ctx.sim_uptime_ms) < POS_MAX_AGE_MS)) {
        return;
    } else {
        ctx.sim_uptime_ms = 0U;
    }

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
    if (!rides_outdoors(ctx.mode)) {
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
    }

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

static void on_imu(const struct app_imu *imu)
{
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
        (void)user_settings_save(settings);
        break;
    case APP_CMD_SET_WEIGHT:
        /* stored in hectograms (UserSettings) */
        user_settings_set_weight(settings, (uint16_t)(cmd->arg * 10));
        attitude_set_rider_weight((float)cmd->arg);
        (void)user_settings_save(settings);
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
    smf_set_initial(SMF_CTX(&ctx), &mode_states[APP_MODE_ID_CRS]);

    activity_init(&act, (uint32_t)CONFIG_GNSS_AUTOLAP_M,
                  IS_ENABLED(CONFIG_GNSS_AUTO_PAUSE));
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
