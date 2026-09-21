/**
 * @file model_ui.c
 * @brief The snapshot of the screens (ui_model_t) from the model state
 *
 * Runs in the model thread, the only reader of the model modules. The
 * values keep the units and sources of the legacy pages (VueCRS, VueFEC,
 * VueGPS, VueDebug); what the legacy computed at display time is computed
 * here, so the interface only draws.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>

#include "app_types.h"
#include "model/attitude.h"
#include "model/climb.h"
#include "model/map_project.h"
#include "model/radar.h"
#include "model/vecteur.h"
#include "model/route_profile.h"
#include "model/parcours.h"
#include "model/segment.h"
#include "model/user_settings.h"
#include "model_internal.h"

uint32_t model_time_of_day(const struct model_ctx *ctx)
{
    if (!ctx->have_fix_msg || !ctx->fix.time_valid) {
        return UI_TIME_UNKNOWN;
    }
    /* seconds of the day at the epoch, plus the time since it came */
    uint32_t s = ((uint32_t)ctx->fix.hour * 3600U) + ((uint32_t)ctx->fix.minute * 60U) +
                 (ctx->fix.millisecond / 1000U);

    s += (k_uptime_get_32() - ctx->fix.uptime_ms) / 1000U;
    return s % 86400U;
}

static ui_gnss_mode_t gnss_mode(uint8_t mode)
{
    switch (mode) {
    case APP_GNSS_MODE_ACQ:
        return UI_GNSS_MODE_ACQ;
    case APP_GNSS_MODE_LEAP:
        return UI_GNSS_MODE_LEAP;
    case APP_GNSS_MODE_FULL:
        return UI_GNSS_MODE_FULL;
    default:
        return UI_GNSS_MODE_BACKUP;
    }
}

static void fill_status(const struct model_ctx *ctx, ui_model_t *m, uint32_t now)
{
    ui_status_t *s = &m->status;
    bool recent = ctx->have_fix_msg && ctx->fix.fix && ((now - ctx->fix_uptime_ms) <= POS_MAX_AGE_MS);

    s->time_s = model_time_of_day(ctx);
    if (!ctx->have_fix_msg || (ctx->fix.mode == APP_GNSS_MODE_BACKUP)) {
        s->gnss = UI_GNSS_OFF;
    } else {
        s->gnss = recent ? UI_GNSS_FIX : UI_GNSS_SEARCH;
    }
    for (uint32_t k = 0U; k < APP_EXT_KINDS; k++) {
        if (ctx->link[k].link == APP_LINK_CONNECTED) {
            if (ctx->link[k].ant) {
                s->ant_link = true;
            } else {
                s->ble_link = true;
            }
        }
    }
    /* the legacy logs every location in CRS and PRC: recording while they come */
    s->recording = recent && ((ctx->mode == APP_MODE_ID_CRS) || (ctx->mode == APP_MODE_ID_PRC) ||
                              (ctx->mode == APP_MODE_ID_DBG));
    s->paused = !ctx->act.running;
    s->charge = (ui_charge_t)ctx->power.charge;
    s->batt_pct = ctx->power.gauge ? ctx->power.pct : 0U;
}

/** Lap and totals of the ride; nothing of this is in the legacy */
static void fill_activity(const struct model_ctx *ctx, ui_model_t *m)
{
    ui_activity_t *a = &m->act;

    a->timer_s = ctx->act.ride.timer_ms / 1000U;
    a->elapsed_s = ctx->act.ride.elapsed_ms / 1000U;
    a->lap_timer_s = ctx->act.lap_timer_ms / 1000U;
    a->lap_dist_m = ctx->act.lap_dist_m;
    a->avg_kmh = ctx->act.ride.avg_speed_kmh;
    a->max_kmh = ctx->act.ride.max_speed_kmh;
    a->descent_m = ctx->act.ride.descent_m;
    a->laps = ctx->act.laps;
    a->kcal = ctx->act.ride.calories_kcal;
}

/** Vehicles behind, for the strip down the side of the data pages */
static void fill_radar(const struct model_ctx *ctx, ui_model_t *m)
{
    ui_radar_t *r = &m->radar;

    (void)memset(r, 0, sizeof(*r));
    r->linked = ctx->rad.linked;
    r->n = radar_count(&ctx->rad);
    r->worst = radar_worst(&ctx->rad);
    for (uint8_t i = 0U; (i < r->n) && (i < 8U); i++) {
        r->range_m[i] = ctx->rad.t[i].range_m;
        r->level[i] = ctx->rad.t[i].level;
        r->live[i] = ctx->rad.t[i].live;
    }
}

/** The climb ahead, and the profile of that climb alone */
static void fill_climb(const struct model_ctx *ctx, ui_model_t *m)
{
    ui_climb_t *c = &m->climb;

    (void)memset(c, 0, sizeof(*c));
    c->total = ctx->climbs.n;
    if (ctx->climbs.n == 0U) {
        return;
    }

    c->on_climb = ctx->climb.on_climb;
    c->remain_m = ctx->climb.remain_m;
    c->remain_gain_m = ctx->climb.remain_gain_m;
    c->grade_pct = ctx->climb.grade_pct;
    c->ahead_grade_pct = ctx->climb.ahead_grade_pct;
    c->done_pct = ctx->climb.done_pct;
    c->to_next_m = ctx->climb.to_next_m;
    c->index = (uint8_t)(ctx->climb.index + 1U);

    if (ctx->climb.on_climb) {
        c->cat = ctx->climbs.c[ctx->climb.index].cat;
    }
    if (ctx->climb.have_next) {
        const struct climb *nx = &ctx->climbs.c[ctx->climb.next_index];

        c->next_cat = nx->cat;
        c->next_len_m = climb_length(nx);
        c->next_gain_m = climb_gain(nx);
    }

    /*
     * The profile of the climb being ridden, not of the whole route: on a
     * pass the route profile is a flat line with a bump, and what the rider
     * needs is the bump filling the screen.
     */
    if (!ctx->climb.on_climb || !parcours_is_loaded()) {
        return;
    }

    const struct climb *cur = &ctx->climbs.c[ctx->climb.index];
    uint16_t first = cur->start_idx;
    uint16_t last = cur->end_idx;
    uint16_t n = parcours_get_num_points();

    if ((last <= first) || (last >= n)) {
        return;
    }

    /*
     * start_idx and end_idx are points of the thinned copy the model
     * scanned, so they are scaled back to points of the route.
     */
    uint16_t step = (uint16_t)(((uint32_t)n + MODEL_CLIMB_SCAN_MAX - 1U) / MODEL_CLIMB_SCAN_MAX);

    if (step < 1U) {
        step = 1U;
    }
    first = (uint16_t)((uint32_t)first * step);
    last = (uint16_t)((uint32_t)last * step);
    if (last >= n) {
        last = (uint16_t)(n - 1U);
    }
    if (last <= first) {
        return;
    }

    c->prof_span_m = climb_length(cur);

    uint16_t span = (uint16_t)(last - first);
    uint16_t cols = (span < UI_PROFILE_PTS) ? span : (uint16_t)UI_PROFILE_PTS;

    c->prof_min_m = INT16_MAX;
    c->prof_max_m = INT16_MIN;
    for (uint16_t i = 0U; i < cols; i++) {
        uint16_t idx = (uint16_t)(first + (((uint32_t)span * i) / cols));
        const point_t *p = parcours_get_point(idx);

        if (p == NULL) {
            break;
        }

        int16_t a = (int16_t)lroundf(p->alt);

        c->prof_m[c->prof_n] = a;
        c->prof_n++;
        if (a < c->prof_min_m) {
            c->prof_min_m = a;
        }
        if (a > c->prof_max_m) {
            c->prof_max_m = a;
        }
    }
    if (c->prof_n == 0U) {
        c->prof_min_m = 0;
        c->prof_max_m = 0;
        return;
    }
    c->prof_here = (uint8_t)(((float)c->prof_n * c->done_pct) / 100.0f);
    if (c->prof_here >= c->prof_n) {
        c->prof_here = (uint8_t)(c->prof_n - 1U);
    }
}

static void fill_ride(const struct model_ctx *ctx, ui_model_t *m)
{
    attitude_t att;
    ui_ride_t *r = &m->ride;

    if (attitude_get(&att) != APP_OK) {
        return;
    }
    r->dist_m = att.dist;
    r->speed_kmh = att.loc.speed;
    /* legacy afficheScreen1: dist * 3.6 / active seconds */
    r->avg_kmh = (att.nbsec_act > 0U) ? ((att.dist * 3.6f) / (float)att.nbsec_act) : 0.0f;
    r->climb_m = att.climb;
    r->alt_m = att.loc.alt;
    r->va_ms = att.vit_asc;
    r->score = suffer_score_get(&ctx->suffer);
    r->pwr_w = att.pwr;
    r->next_seg_m = att.next;
    r->solar_mw = ctx->power.solar_mw;
    r->cad_rpm = ctx->ext[APP_EXT_BSC].cadence_rpm;
    r->hr_bpm = ctx->ext[APP_EXT_HR].hr_bpm;
    r->slope_pct = att.slope;
    r->pr = att.pr;
}

static void fill_attitude(const struct model_ctx *ctx, ui_model_t *m)
{
    ui_attitude_t *a = &m->att;

    a->pitch_pct = tanf(ctx->pitch_deg * 0.0174533f) * 100.0f;
    a->histo_n = ctx->pitch_histo_n;
    (void)memcpy(a->histo, ctx->pitch_histo, sizeof(a->histo));
    a->heading_deg = ctx->heading_valid ? (int16_t)lroundf(ctx->heading_deg) % 360
                                        : (int16_t)UI_ANGLE_UNKNOWN;
    a->rough[0] = ctx->rough[0];
    a->rough[1] = ctx->rough[1];
    a->rough[2] = ctx->rough[2];
    /* the barometer roughness of the legacy (baro.getRoughness()) is not ported yet */
    a->rough[3] = 0.0f;
}

static void fill_zones(const struct model_ctx *ctx, ui_model_t *m)
{
    ui_rr_t *rr = &m->rr;
    ui_fec_t *f = &m->fec;
    uint32_t total = power_zone_get_total_time(&ctx->zones);

    rr->nzones = (RR_ZONES_NB < UI_RR_ZONES) ? RR_ZONES_NB : UI_RR_ZONES;
    for (uint8_t z = 0U; z < rr->nzones; z++) {
        rr->val[z] = rr_zone_get_value(&ctx->rr, z);
    }
    rr->cur = rr_zone_get_current(&ctx->rr);

    f->time_s = ctx->ext[APP_EXT_FEC].elapsed_s;
    f->score = suffer_score_get(&ctx->suffer);
    f->pwr_w = ctx->ext[APP_EXT_FEC].power_w;
    f->rr_ms = ctx->ext[APP_EXT_HR].rr_ms;
    f->cad_rpm = ctx->ext[APP_EXT_FEC].cadence_rpm;
    f->hr_bpm = ctx->ext[APP_EXT_HR].hr_bpm;
    f->zone = power_zone_get_current(&ctx->zones);
    for (uint8_t z = 0U; (z < UI_PWR_ZONES) && (z < PW_ZONES_NB); z++) {
        uint32_t t = power_zone_get_time(&ctx->zones, z);

        f->zone_pct[z] = (total > 0U) ? (uint8_t)((t * 100U) / total) : 0U;
    }
    f->vector_valid = false;
}

static void fill_gnss(const struct model_ctx *ctx, ui_model_t *m, uint32_t now)
{
    ui_gnss_info_t *g = &m->gnss;

    g->mode = gnss_mode(ctx->fix.mode);
    g->fix3d = ctx->fix.fix;
    g->nsat = (ctx->sky.n < UI_SAT_MAX) ? ctx->sky.n : UI_SAT_MAX;
    g->used = 0U;
    for (uint8_t i = 0U; i < g->nsat; i++) {
        const struct app_gnss_sat *s = &ctx->sky.sat[i];

        g->sat[i].az_deg = (int16_t)s->az_deg;
        g->sat[i].el_deg = (int8_t)s->el_deg;
        g->sat[i].cn0 = s->cn0;
        g->sat[i].sys = s->sys;
        g->sat[i].used = s->used;
        if (s->used) {
            g->used++;
        }
    }
    if ((ctx->sky.n == 0U) && ctx->fix.fix) {
        g->used = ctx->fix.nsat;
    }
    g->fix_age_s = (ctx->fix_uptime_ms != 0U) ? ((now - ctx->fix_uptime_ms) / 1000U) : 100000U;
    /* the GNSS API gives the dilution of precision, not the accuracy */
    g->hacc_m = NAN;
}

static void fill_energy(const struct model_ctx *ctx, ui_model_t *m)
{
    ui_energy_t *e = &m->energy;

    e->mv = ctx->power.mv;
    e->ma = ctx->power.ma;
    e->pct = ctx->power.pct;
    e->source = (ui_charge_t)ctx->power.charge;
    e->solar_mw = ctx->power.solar_mw;
    e->solar_limit_mv = ctx->power.solar_limit_mv;
    e->temp_c = ctx->power.temp_c;
    e->autonomy_h = ctx->power.autonomy_h;
}

static void sensor_value(const struct model_ctx *ctx, uint8_t kind, char *buf, size_t size)
{
    const struct app_ext_sensor *e = &ctx->ext[kind];

    switch (kind) {
    case APP_EXT_HR:
        (void)snprintf(buf, size, "%u bpm", (unsigned int)e->hr_bpm);
        break;
    case APP_EXT_BSC:
        (void)snprintf(buf, size, "%u rpm", (unsigned int)e->cadence_rpm);
        break;
    case APP_EXT_POWER:
    case APP_EXT_FEC:
        (void)snprintf(buf, size, "%u W", (unsigned int)e->power_w);
        break;
    default:
        buf[0] = '\0';
        break;
    }
}

static void fill_sensors(const struct model_ctx *ctx, ui_model_t *m)
{
    ui_sensors_t *s = &m->sensors;

    s->n = 0U;
    for (uint8_t k = 0U; (k < APP_EXT_KINDS) && (s->n < UI_SENSOR_MAX); k++) {
        const struct app_link_status *l = &ctx->link[k];
        ui_sensor_t *u;

        if (l->link == APP_LINK_NONE) {
            continue;
        }
        u = &s->s[s->n++];
        u->kind = k;
        u->link = (ui_link_t)l->link;
        u->ant = l->ant;
        u->dev_id = l->dev_id;
        (void)strncpy(u->dev_name, l->name, sizeof(u->dev_name) - 1U);
        sensor_value(ctx, k, u->value, sizeof(u->value));
    }

    ui_pair_t *p = &m->pair;

    p->searching = ctx->pair.searching;
    p->kind = ctx->pair.kind;
    p->n = (ctx->pair.n < UI_PAIR_MAX) ? ctx->pair.n : UI_PAIR_MAX;
    for (uint8_t i = 0U; i < p->n; i++) {
        p->item[i].ant = ctx->pair.dev[i].ant;
        p->item[i].id = ctx->pair.dev[i].id;
        p->item[i].rssi = ctx->pair.dev[i].rssi;
        (void)strncpy(p->item[i].name, ctx->pair.dev[i].name, sizeof(p->item[i].name) - 1U);
    }
}

static void fill_nav(const struct model_ctx *ctx, ui_model_t *m)
{
    m->nav.valid = ctx->nav.valid;
    m->nav.dist_m = ctx->nav.dist_m;
    m->nav.turn = (ui_turn_t)ctx->nav.turn;
    (void)strncpy(m->nav.street, ctx->nav.street, sizeof(m->nav.street) - 1U);

    /*
     * With no phone talking, the turns come from the course itself, when
     * the file brought a cue sheet (`model/route_file.h`). The phone wins
     * because it knows where the rider actually is on the streets.
     */
    if (!m->nav.valid && parcours_is_active()) {
        parcours_cue_t cue;
        float dist = 0.0f;

        if (parcours_get_next_cue(&cue, &dist)) {
            m->nav.valid = true;
            m->nav.dist_m = (dist > 65535.0f) ? 65535U : (uint16_t)dist;
            m->nav.turn = (ui_turn_t)cue.turn;
            (void)strncpy(m->nav.street, cue.street, sizeof(m->nav.street) - 1U);
            m->nav.street[sizeof(m->nav.street) - 1U] = '\0';
        }
    }
}

static void fill_settings(const struct model_ctx *ctx, ui_model_t *m)
{
    const user_settings_t *us = user_settings_get_global();

    m->settings.ftp_w = user_settings_get_ftp(us);
    m->settings.weight_kg = (uint8_t)(user_settings_get_weight(us) / 10U);
    /* the MAX-F10S has no low power tracking mode: the menu shows what the
     * receiver of the board actually does (docs/16, GNSS) */
    m->settings.gnss_leap = (ctx->fix.mode == (uint8_t)APP_GNSS_MODE_LEAP);
    m->settings.light_auto = true;     /* the ui service puts its preference over it */
    m->settings.solar_limit_mv = ctx->power.solar_limit_mv;

    m->routes.n = (ctx->storage.nroutes < UI_ROUTE_LIST_MAX) ? ctx->storage.nroutes
                                                             : UI_ROUTE_LIST_MAX;
    for (uint8_t i = 0U; i < m->routes.n; i++) {
        (void)strncpy(m->routes.name[i], ctx->storage.route[i], UI_NAME_LEN - 1U);
    }
    m->debug.seg_loaded = (ctx->storage.segments > 255U) ? 255U : (uint8_t)ctx->storage.segments;
    (void)snprintf(m->debug.version, sizeof(m->debug.version), "%u.%u.%u",
                   (unsigned int)APP_VERSION_MAJOR, (unsigned int)APP_VERSION_MINOR,
                   (unsigned int)APP_VERSION_PATCH);
}

/**
 * The map windows of the screens are 240 by 107 pixels (`ui_row_y()` gives
 * two rows of the seven under the status bar): the projection needs the
 * shape to keep a metre the same length on both axes.
 */
#define UI_MAP_ASPECT_PM    2243U

/** Where the rider is, or NULL while there is no position */
static bool rider_position(const struct model_ctx *ctx, float *lat, float *lon)
{
    if (!ctx->have_fix_msg || !ctx->fix.fix) {
        return false;
    }
    *lat = (float)((double)ctx->fix.lat_e7 * 1e-7);
    *lon = (float)((double)ctx->fix.lon_e7 * 1e-7);

    return true;
}

static ui_pt_t to_ui(struct map_point p)
{
    ui_pt_t out = {.x = p.x, .y = p.y};

    return out;
}

/**
 * @brief The segments of the screen, with their mini-map
 *
 * The legacy draws the segments the rider is on or coming to
 * (`Vue::afficheSegment()`), each in its own window centred on the rider.
 */
static void fill_segments(const struct model_ctx *ctx, ui_model_t *m)
{
    float lat;
    float lon;

    m->nseg = 0U;
    if (!rider_position(ctx, &lat, &lon)) {
        return;
    }

    uint8_t index[UI_SEG_MAX];
    uint8_t n = segment_get_screen_list(index, UI_SEG_MAX, lat, lon);
    uint16_t span = map_span_m(ctx->zoom);

    for (uint8_t k = 0U; k < n; k++) {
        segment_t seg;

        if (segment_get(index[k], &seg) != APP_OK) {
            continue;
        }

        ui_segment_t *out = &m->seg[m->nseg];

        out->on = (seg.status == SEG_START) || (seg.status == SEG_ON);
        out->done = (seg.status == SEG_FIN);
        out->pct = (uint8_t)((seg.pct_dist * 100.0f) + 0.5f);
        out->advance_s = seg.advance;
        out->cur_s = seg.cur_time;
        (void)snprintf(out->name, sizeof(out->name), "%s", seg.name);

        uint16_t count = segment_point_count(index[k]);
        uint16_t stride = map_stride(count, UI_SEG_PTS_MAX);

        out->npts = 0U;
        for (uint16_t i = 0U; (i < count) && (out->npts < UI_SEG_PTS_MAX); i += stride) {
            point_t pt;

            if (segment_point_at(index[k], i, &pt)) {
                out->pts[out->npts] = to_ui(map_project(pt.lat, pt.lon, lat, lon, span,
                                                        UI_MAP_ASPECT_PM));
                out->npts++;
            }
        }

        out->rider.x = 500;
        out->rider.y = 500;
        out->course_deg = ctx->fix.fix ? (int16_t)(ctx->fix.course_mdeg / 1000) : UI_ANGLE_UNKNOWN;
        m->nseg++;
    }
}

/**
 * @brief The route on the PRC screen, centred on the rider
 */
/** Altitude of a point of the route, for the profile */
static float route_alt(uint16_t index, void *user)
{
    const point_t *pt = parcours_get_point(index);

    (void)user;

    return (pt != NULL) ? pt->alt : 0.0f;
}

/**
 * @brief The elevation profile of the route on the PRC screen
 */
static void fill_profile(const struct model_ctx *ctx, ui_model_t *m)
{
    struct route_profile prof;

    (void)ctx;
    if (!parcours_is_loaded()) {
        return;
    }
    if (!route_profile_build(&prof, parcours_get_num_points(), parcours_get_current_index(),
                             route_alt, NULL)) {
        return;
    }

    m->profile.n = (prof.n < UI_PROFILE_PTS) ? prof.n : UI_PROFILE_PTS;
    m->profile.here = prof.here;
    (void)memcpy(m->profile.alt_m, prof.alt_m, (size_t)m->profile.n * sizeof(prof.alt_m[0]));
    m->profile.min_m = prof.min_m;
    m->profile.max_m = prof.max_m;
    m->profile.climb_left_m = prof.climb_left_m;
    m->profile.remain_km = m->route.remain_km;
}

static void fill_route(const struct model_ctx *ctx, ui_model_t *m)
{
    uint16_t span = map_span_m(ctx->zoom);
    uint16_t bar_pm = 0U;

    m->route.n = 0U;
    m->route.scale_m = map_scale_bar(span, &bar_pm);
    m->route.scale_pm = bar_pm;

    float lat;
    float lon;

    if (!parcours_is_loaded() || !rider_position(ctx, &lat, &lon)) {
        return;
    }

    uint16_t count = parcours_get_num_points();
    uint16_t stride = map_stride(count, UI_ROUTE_PTS_MAX);
    uint16_t here = parcours_get_current_index();

    for (uint16_t i = 0U; (i < count) && (m->route.n < UI_ROUTE_PTS_MAX); i += stride) {
        const point_t *pt = parcours_get_point(i);

        if (pt == NULL) {
            continue;
        }
        m->route.pts[m->route.n] = to_ui(map_project(pt->lat, pt->lon, lat, lon, span,
                                                     UI_MAP_ASPECT_PM));
        if (i <= here) {
            m->route.done = m->route.n;
        }
        m->route.n++;
    }

    nav_info_t nav;

    if (parcours_get_nav_info(&nav) == APP_OK) {
        m->route.remain_km = nav.dist_remaining / 1000.0f;
    }
    m->route.rider.x = 500;
    m->route.rider.y = 500;
    m->route.course_deg = (int16_t)(ctx->fix.course_mdeg / 1000);
}

void model_ui_fill(const struct model_ctx *ctx, ui_model_t *m)
{
    uint32_t now = k_uptime_get_32();

    (void)memset(m, 0, sizeof(*m));
    fill_status(ctx, m, now);
    fill_activity(ctx, m);
    fill_climb(ctx, m);
    fill_radar(ctx, m);
    fill_ride(ctx, m);
    fill_attitude(ctx, m);
    fill_zones(ctx, m);
    fill_gnss(ctx, m, now);
    fill_energy(ctx, m);
    fill_sensors(ctx, m);
    fill_nav(ctx, m);
    fill_settings(ctx, m);
    fill_segments(ctx, m);
    fill_route(ctx, m);
    fill_profile(ctx, m);
}
