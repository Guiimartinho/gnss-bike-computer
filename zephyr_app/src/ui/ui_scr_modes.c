/**
 * @file ui_scr_modes.c
 * @brief PRC, FEC, GNSS search and DBG screens
 *
 * PRC follows legacy/source/vue/VuePRC.cpp (four rows of data, the route map
 * on two rows with the zoom in the corner, the battery row), FEC follows
 * VueFEC.cpp (time, cadence and heart rate, score and power zones, power and
 * RR, the power vector), the GNSS screen replaces VueGPS::displayGPS() text
 * with a sky plot and keeps its bottom rows, and DBG follows
 * VueDebug::displayDebug() with the data of the new hardware.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui_internal.h"

static const ui_fplace_t batt_row[] = {
    {UI_F_SOLAR, 0, 6, 1}, {UI_F_BATT, 1, 6, 1},
};

/* ==========================================================================
 * PRC
 * ========================================================================== */

static lv_obj_t *prc_map;

static void prc_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_route_t *r = &ui_ctx.m.route;
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;
    char buf[16];

    lv_obj_get_coords(obj, &a);
    if (r->n < 2U) {
        ui_draw_text(layer, (a.x1 + a.x2) / 2, ((a.y1 + a.y2) / 2) - 10, ui_txt(T_NO_ROUTE),
                     UI_FONT_TITLE, fg, LV_TEXT_ALIGN_CENTER);
        return;
    }
    /* remaining part in blue, ridden part in green (legacy: all black) */
    for (uint32_t i = 1U; (i < r->n) && (i < UI_ROUTE_PTS_MAX); i++) {
        bool done = (i <= r->done);
        /* without colour, the ridden part is the thin line */
        int32_t width = ((ui_ctx.theme == UI_THEME_MONO) && done) ? 2 : 4;

        ui_draw_line(layer, ui_map_x(&a, 0, r->pts[i - 1U].x), ui_map_y(&a, 0, r->pts[i - 1U].y),
                     ui_map_x(&a, 0, r->pts[i].x), ui_map_y(&a, 0, r->pts[i].y),
                     ui_col(done ? UI_C_GOOD : UI_C_NAV), width);
    }
    /* the first segment over the route: VuePRC stops at NB_SEG_ON_DISPLAY - 1 */
    for (uint32_t s = 0U; (s < ui_ctx.m.nseg) && (s < (UI_SEG_MAX - 1U)); s++) {
        const ui_segment_t *sg = &ui_ctx.m.seg[s];

        for (uint32_t i = 1U; i < sg->npts; i++) {
            ui_draw_line(layer, ui_map_x(&a, 0, sg->pts[i - 1U].x), ui_map_y(&a, 0, sg->pts[i - 1U].y),
                         ui_map_x(&a, 0, sg->pts[i].x), ui_map_y(&a, 0, sg->pts[i].y),
                         ui_col(UI_C_SEG), 2);
        }
    }
    ui_draw_rider(layer, ui_map_x(&a, 0, r->rider.x), ui_map_y(&a, 0, r->rider.y), r->course_deg, fg);

    /* zoom in the top right corner (legacy: getLastZoom() and "m") */
    (void)snprintf(buf, sizeof(buf), "%u m", (unsigned int)r->scale_m);
    ui_draw_text(layer, a.x2 - 6, a.y1 + 4, buf, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
}

static void prc_create(lv_obj_t *scr)
{
    static const ui_fplace_t fields[] = {
        {UI_F_DIST, 0, 0, 1}, {UI_F_PWR, 1, 0, 1},
        {UI_F_SPEED, 0, 1, 1}, {UI_F_CLIMB, 1, 1, 1},
        {UI_F_CAD, 0, 2, 1}, {UI_F_HR, 1, 2, 1},
        {UI_F_SLOPE, 0, 3, 1}, {UI_F_VA, 1, 3, 1},
    };
    lv_obj_t *band;

    ui_statusbar_create(scr);
    ui_fields_create(scr, fields, ARRAY_SIZE(fields));
    band = ui_band(scr, 4, 2);
    prc_map = ui_plot(band, 0, 0, UI_WIDTH, ui_rows_h(4, 2) - 1, prc_draw, NULL);
    ui_fields_create(scr, batt_row, ARRAY_SIZE(batt_row));
    /* the map is the only part of this page the strip may cover */
    ui_radar_strip_create(band, 0, ui_rows_h(4, 2) - 1);
}

static void prc_update(lv_obj_t *scr)
{
    (void)scr;
    ui_statusbar_update();
    ui_fields_update();
    lv_obj_invalidate(prc_map);
}

static bool prc_key(ui_key_t key, ui_press_t press)
{
    /* PRC: left and right change the zoom (docs/18, Botões) */
    if (press == UI_PRESS_LONG) {
        /* and a long press on the right opens the elevation profile */
        if (key == UI_KEY_RIGHT) {
            ui_go(UI_SCREEN_PROFILE);
            return true;
        }
        return false;
    }
    if (press != UI_PRESS_SHORT) {
        return false;
    }
    if (key == UI_KEY_LEFT) {
        ui_action(UI_ACT_ZOOM, -1);
        return true;
    }
    if (key == UI_KEY_RIGHT) {
        ui_action(UI_ACT_ZOOM, 1);
        return true;
    }
    return false;
}

const ui_screen_ops_t ui_scr_prc = {prc_create, prc_update, prc_key};

/* ==========================================================================
 * FEC
 * ========================================================================== */

static lv_obj_t *fec_zones;
static lv_obj_t *fec_rr;
static lv_obj_t *fec_metrics;
static lv_obj_t *fec_vector;
static lv_obj_t *fec_wait;
static bool fec_has_data;

/** Legacy VueFEC::cadranZones(): time in each power zone, '>' on the current one */
static void zones_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_fec_t *f = &ui_ctx.m.fec;
    lv_color_t fg = ui_col(UI_C_FG);
    uint32_t tot = 0U;
    lv_area_t a;
    char buf[8];

    lv_obj_get_coords(obj, &a);
    ui_draw_text(layer, a.x1 + 4, a.y1 + 2, ui_txt(T_PZONE), UI_FONT_LABEL, fg, LV_TEXT_ALIGN_LEFT);
    for (uint32_t i = 0U; i < UI_PWR_ZONES; i++) {
        if (f->zone_pct[i] > tot) {
            tot = f->zone_pct[i];
        }
    }
    for (uint32_t i = 0U; i < UI_PWR_ZONES; i++) {
        int32_t full = (UI_WIDTH / 2) - 35;
        int32_t w = (tot > 0U) ? (2 + (int32_t)(((uint32_t)(full - 2) * f->zone_pct[i]) / tot)) : 2;

        ui_draw_fill(layer, a.x1 + 20, a.y1 + 16 + ((int32_t)i * 5), w, 3,
                     ui_col_fill((ui_role_t)(UI_C_Z1 + (int32_t)i)));
        if ((i + 1U) == f->zone) {
            ui_draw_text(layer, a.x1 + 8, a.y1 + 9 + ((int32_t)i * 5), ">", UI_FONT_LABEL, fg,
                         LV_TEXT_ALIGN_LEFT);
        }
    }
    (void)snprintf(buf, sizeof(buf), "Z%u", (unsigned int)f->zone);
    ui_draw_text(layer, a.x2 - 4, a.y1 + 2, buf, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
}

/** RR zones as in page 2, with the RR interval on the right */
static void fec_rr_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_rr_t *rr = &ui_ctx.m.rr;
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;
    float span = 5.0f;
    char buf[12];

    lv_obj_get_coords(obj, &a);
    ui_draw_text(layer, a.x1 + 4, a.y1 + 2, ui_txt(T_RR), UI_FONT_LABEL, fg, LV_TEXT_ALIGN_LEFT);
    for (uint32_t i = 0U; (i < rr->nzones) && (i < UI_RR_ZONES); i++) {
        if (rr->val[i] > span) {
            span = rr->val[i];
        }
    }
    if (span > 50.0f) {
        span = 50.0f;
    }
    for (uint32_t i = 0U; (i < rr->nzones) && (i < UI_RR_ZONES); i++) {
        int32_t full = (UI_WIDTH / 2) - 35;
        int32_t w = 2 + (int32_t)((rr->val[i] / span) * (float)(full - 2));

        if (w > full) {
            w = full;
        }
        ui_draw_fill(layer, a.x1 + 20, a.y1 + 16 + ((int32_t)i * 6), w, 4, ui_col_fill(UI_C_FG));
        if (i == rr->cur) {
            ui_draw_text(layer, a.x1 + 8, a.y1 + 11 + ((int32_t)i * 6), ">", UI_FONT_SMALL_B, fg,
                         LV_TEXT_ALIGN_LEFT);
        }
    }
    (void)snprintf(buf, sizeof(buf), "%u ms", (unsigned int)ui_ctx.m.fec.rr_ms);
    ui_draw_text(layer, a.x2 - 4, a.y1 + 2, buf, UI_FONT_LABEL, fg, LV_TEXT_ALIGN_RIGHT);
}

/**
 * What the ride was worth: normalised power, intensity and stress.
 *
 * Three numbers on one line, because they are read together and mean
 * nothing apart (`model/power_metrics.h`). They stay dashes until the
 * rolling average of thirty seconds exists, and the intensity and the
 * stress stay dashes until the rider has set a threshold in the settings.
 */
static void metrics_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_fec_t *f = &ui_ctx.m.fec;
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;
    char buf[16];

    lv_obj_get_coords(obj, &a);

    int32_t third = (a.x2 - a.x1) / 3;

    ui_draw_text(layer, a.x1 + 4, a.y1 + 1, "NP", UI_FONT_LABEL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x1 + third + 4, a.y1 + 1, "IF", UI_FONT_LABEL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x1 + (2 * third) + 4, a.y1 + 1, "TSS", UI_FONT_LABEL, fg,
                 LV_TEXT_ALIGN_LEFT);

    if (f->np_w > 0U) {
        (void)snprintf(buf, sizeof(buf), "%u", (unsigned int)f->np_w);
    } else {
        (void)snprintf(buf, sizeof(buf), "--");
    }
    ui_draw_text(layer, a.x1 + third - 6, a.y1 + 9, buf, UI_FONT_SMALL_B, fg,
                 LV_TEXT_ALIGN_RIGHT);

    if (f->if100 > 0U) {
        (void)snprintf(buf, sizeof(buf), "%u.%02u", (unsigned int)(f->if100 / 100U),
                       (unsigned int)(f->if100 % 100U));
    } else {
        (void)snprintf(buf, sizeof(buf), "--");
    }
    ui_draw_text(layer, a.x1 + (2 * third) - 6, a.y1 + 9, buf, UI_FONT_SMALL_B, fg,
                 LV_TEXT_ALIGN_RIGHT);

    if (f->if100 > 0U) {
        (void)snprintf(buf, sizeof(buf), "%u", (unsigned int)f->tss);
    } else {
        (void)snprintf(buf, sizeof(buf), "--");
    }
    ui_draw_text(layer, a.x2 - 4, a.y1 + 9, buf, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
}

/** Legacy VueFEC::cadranPowerVector(): torque over one crank turn, closed curve */
static void vector_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_fec_t *f = &ui_ctx.m.fec;
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;

    lv_obj_get_coords(obj, &a);
    int32_t cx = (a.x1 + a.x2) / 2;
    int32_t cy = ((a.y1 + a.y2) / 2) + 8;
    int32_t r = (lv_area_get_height(&a) / 2) - 16;

    ui_draw_text(layer, a.x1 + 6, a.y1 + 3, ui_txt(T_VECTOR), UI_FONT_LABEL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_circle(layer, cx, cy, r, fg, 1);
    ui_draw_dash(layer, cx - r, cy, cx + r, cy, fg, 1, 3, 3);
    ui_draw_dash(layer, cx, cy - r, cx, cy + r, fg, 1, 3, 3);
    if (!f->vector_valid) {
        return;
    }
    int32_t px = 0;
    int32_t py = 0;
    int32_t fx = 0;
    int32_t fy = 0;

    for (uint32_t i = 0U; i < UI_VECTOR_PTS; i++) {
        float ang = (360.0f * (float)i) / (float)UI_VECTOR_PTS;
        int32_t len = ((int32_t)f->vector[i] * r) / 100;
        int32_t x;
        int32_t y;

        ui_rotate(cx, cy, cx, cy - len, ang, &x, &y);
        if (i == 0U) {
            fx = x;
            fy = y;
        } else {
            ui_draw_line(layer, px, py, x, y, ui_col(UI_C_SEG), 3);
        }
        px = x;
        py = y;
    }
    ui_draw_line(layer, px, py, fx, fy, ui_col(UI_C_SEG), 3);
}

static void fec_create(lv_obj_t *scr)
{
    static const ui_fplace_t fields[] = {
        {UI_F_FEC_TIME, 0, 0, 2},
        {UI_F_FEC_CAD, 0, 1, 1}, {UI_F_FEC_HR, 1, 1, 1},
        {UI_F_FEC_SCORE, 0, 2, 1},
        {UI_F_FEC_PWR, 0, 3, 1},
    };
    lv_obj_t *cell;

    fec_has_data = (ui_ctx.m.fec.time_s > 0U);
    ui_statusbar_create(scr);
    if (!fec_has_data) {
        /* legacy: "Connecting" until the trainer sends its first page */
        fec_wait = ui_label(scr, UI_FONT_LARGE, ui_col(UI_C_FG), ui_txt(T_CONNECTING));
        lv_obj_set_pos(fec_wait, 10, 60);
        return;
    }
    ui_fields_create(scr, fields, ARRAY_SIZE(fields));
    cell = ui_box(scr, UI_WIDTH / 2, ui_row_y(2), UI_WIDTH / 2, ui_rows_h(2, 1));
    lv_obj_set_style_border_color(cell, ui_col(UI_C_FG), 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_side(cell, LV_BORDER_SIDE_BOTTOM, 0);
    fec_zones = ui_plot(cell, 0, 0, UI_WIDTH / 2, ui_rows_h(2, 1) - 1, zones_draw, NULL);
    cell = ui_box(scr, UI_WIDTH / 2, ui_row_y(3), UI_WIDTH / 2, ui_rows_h(3, 1));
    lv_obj_set_style_border_color(cell, ui_col(UI_C_FG), 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_side(cell, LV_BORDER_SIDE_BOTTOM, 0);
    fec_rr = ui_plot(cell, 0, 0, UI_WIDTH / 2, ui_rows_h(3, 1) - 1, fec_rr_draw, NULL);
    fec_metrics = ui_plot(scr, 0, ui_row_y(4), UI_WIDTH, ui_rows_h(4, 1), metrics_draw, NULL);
    fec_vector = ui_plot(scr, 0, ui_row_y(5), UI_WIDTH, ui_rows_h(5, 2), vector_draw, NULL);
}

static void fec_update(lv_obj_t *scr)
{
    (void)scr;
    if ((ui_ctx.m.fec.time_s > 0U) != fec_has_data) {
        ui_rebuild();
        return;
    }
    ui_statusbar_update();
    if (fec_has_data) {
        ui_fields_update();
        lv_obj_invalidate(fec_zones);
        lv_obj_invalidate(fec_rr);
        lv_obj_invalidate(fec_metrics);
        lv_obj_invalidate(fec_vector);
    }
}

const ui_screen_ops_t ui_scr_fec = {fec_create, fec_update, NULL};

/* ==========================================================================
 * GNSS searching (VueGPS::displayGPS)
 * ========================================================================== */

static lv_obj_t *gps_plot;

static const char *gnss_mode_text(ui_gnss_mode_t mode)
{
    switch (mode) {
    case UI_GNSS_MODE_ACQ:
        return ui_txt(T_GNSS_ACQ);
    case UI_GNSS_MODE_LEAP:
        return ui_txt(T_GNSS_LEAP);
    case UI_GNSS_MODE_FULL:
        return ui_txt(T_GNSS_FULL);
    default:
        return ui_txt(T_GNSS_BACKUP);
    }
}

/** Satellite colour by C/N0: strong green, medium yellow, weak red */
static lv_color_t cn0_color(uint8_t cn0)
{
    if (cn0 >= 35U) {
        return ui_col_fill(UI_C_Z3);
    }
    if (cn0 >= 28U) {
        return ui_col_fill(UI_C_Z4);
    }
    return ui_col_fill(UI_C_Z6);
}

static void gps_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_gnss_info_t *g = &ui_ctx.m.gnss;
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;
    char buf[24];

    lv_obj_get_coords(obj, &a);
    ui_draw_text(layer, (a.x1 + a.x2) / 2, a.y1 + 6, ui_txt(T_SEARCHING_SATS), UI_FONT_TITLE, fg,
                 LV_TEXT_ALIGN_CENTER);

    /* sky: horizon and 45 degree circles, north up */
    int32_t cx = a.x1 + 64;
    int32_t cy = a.y1 + 124;
    int32_t r = 56;

    ui_draw_circle(layer, cx, cy, r, fg, 1);
    ui_draw_circle(layer, cx, cy, r / 2, fg, 1);
    ui_draw_dash(layer, cx - r, cy, cx + r, cy, fg, 1, 3, 3);
    ui_draw_dash(layer, cx, cy - r, cx, cy + r, fg, 1, 3, 3);
    ui_draw_text(layer, cx, cy - r - 15, "N", UI_FONT_SMALL_B, ui_col(UI_C_BAD), LV_TEXT_ALIGN_CENTER);
    for (uint32_t i = 0U; (i < g->nsat) && (i < UI_SAT_MAX); i++) {
        const ui_sat_t *s = &g->sat[i];
        float el = (float)((s->el_deg < 0) ? 0 : s->el_deg);
        float d = ((90.0f - el) / 90.0f) * (float)r;
        float az = (float)s->az_deg * 0.0174533f;
        int32_t sx = cx + (int32_t)lroundf(d * sinf(az));
        int32_t sy = cy - (int32_t)lroundf(d * cosf(az));

        if (ui_ctx.theme == UI_THEME_MONO) {
            if (s->used) {
                ui_draw_disc(layer, sx, sy, 5, fg);
            } else {
                ui_draw_circle(layer, sx, sy, 5, fg, 1);
            }
        } else {
            ui_draw_disc(layer, sx, sy, 5, cn0_color(s->cn0));
            ui_draw_circle(layer, sx, sy, 5, fg, 1);
        }
    }

    /* satellites in use, mode and age of the last position */
    int32_t tx = a.x1 + 180;

    (void)snprintf(buf, sizeof(buf), "%u/%u", (unsigned int)g->used, (unsigned int)g->nsat);
    ui_draw_text(layer, tx, a.y1 + 58, buf, UI_FONT_VALUE, fg, LV_TEXT_ALIGN_CENTER);
    ui_draw_text(layer, tx, a.y1 + 92, ui_txt(T_SATS_IN_USE), UI_FONT_LABEL, fg, LV_TEXT_ALIGN_CENTER);
    ui_draw_text(layer, tx, a.y1 + 120, ui_txt(T_MODE), UI_FONT_LABEL, fg, LV_TEXT_ALIGN_CENTER);
    ui_draw_text(layer, tx, a.y1 + 132, gnss_mode_text(g->mode), UI_FONT_SMALL_B, ui_col(UI_C_NAV),
                 LV_TEXT_ALIGN_CENTER);
    ui_draw_text(layer, tx, a.y1 + 160, ui_txt(T_LAST_FIX), UI_FONT_LABEL, fg, LV_TEXT_ALIGN_CENTER);
    if (g->fix_age_s < 100000U) {
        (void)snprintf(buf, sizeof(buf), "%lu s", (unsigned long)g->fix_age_s);
    } else {
        (void)snprintf(buf, sizeof(buf), "---");
    }
    ui_draw_text(layer, tx, a.y1 + 172, buf, UI_FONT_ITEM, fg, LV_TEXT_ALIGN_CENTER);
    (void)snprintf(buf, sizeof(buf), "%u %s", (unsigned int)ui_ctx.m.debug.seg_loaded,
                   ui_txt(T_SEGMENTS_LOADED));
    ui_draw_text(layer, (a.x1 + a.x2) / 2, a.y2 - 18, buf, UI_FONT_SMALL, fg, LV_TEXT_ALIGN_CENTER);
}

static void gps_create(lv_obj_t *scr)
{
    static const ui_fplace_t fields[] = {
        {UI_F_TIME, 0, 5, 2},
    };
    lv_obj_t *band;

    ui_statusbar_create(scr);
    band = ui_band(scr, 0, 5);
    gps_plot = ui_plot(band, 0, 0, UI_WIDTH, ui_rows_h(0, 5) - 1, gps_draw, NULL);
    ui_fields_create(scr, fields, ARRAY_SIZE(fields));
    ui_fields_create(scr, batt_row, ARRAY_SIZE(batt_row));
}

static void gps_update(lv_obj_t *scr)
{
    (void)scr;
    ui_statusbar_update();
    ui_fields_update();
    lv_obj_invalidate(gps_plot);
}

const ui_screen_ops_t ui_scr_gps = {gps_create, gps_update, NULL};

/* ==========================================================================
 * DBG (VueDebug::displayDebug)
 * ========================================================================== */

static lv_obj_t *dbg_plot;

static void dbg_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_model_t *m = &ui_ctx.m;
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;
    char v[40];
    char n1[12];
    uint32_t per_sys[6] = {0};

    lv_obj_get_coords(obj, &a);
    int32_t y = a.y1 + 6;
    const int32_t step = 22;

    for (uint32_t i = 0U; (i < m->gnss.nsat) && (i < UI_SAT_MAX); i++) {
        if (m->gnss.sat[i].sys < 6U) {
            per_sys[m->gnss.sat[i].sys]++;
        }
    }

    /* GNSS mode and fix */
    (void)snprintf(v, sizeof(v), "%s, %s", gnss_mode_text(m->gnss.mode),
                   m->gnss.fix3d ? "fix 3D" : ((m->status.gnss == UI_GNSS_FIX) ? "fix 2D" : "---"));
    ui_draw_text(layer, a.x1 + 8, y, ui_txt(T_GNSS), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 8, y, v, UI_FONT_SMALL_B,
                 ui_col((m->status.gnss == UI_GNSS_FIX) ? UI_C_GOOD : UI_C_BAD), LV_TEXT_ALIGN_RIGHT);
    y += step;

    (void)snprintf(v, sizeof(v), "%u (GPS %lu, GAL %lu, BDS %lu)", (unsigned int)m->gnss.nsat,
                   (unsigned long)per_sys[0], (unsigned long)per_sys[1], (unsigned long)per_sys[2]);
    ui_draw_text(layer, a.x1 + 8, y, ui_txt(T_SATELLITES), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 8, y, v, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
    y += step;

    (void)snprintf(v, sizeof(v), "%lu s", (unsigned long)m->gnss.fix_age_s);
    ui_draw_text(layer, a.x1 + 8, y, ui_txt(T_POS_AGE), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 8, y, v, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
    y += step;

    (void)ui_fmt_float(n1, sizeof(n1), m->gnss.hacc_m, 1U);
    (void)snprintf(v, sizeof(v), "%s m", n1);
    ui_draw_text(layer, a.x1 + 8, y, ui_txt(T_ACCURACY), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 8, y, v, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
    y += step;

    (void)ui_fmt_float(n1, sizeof(n1), (float)m->energy.mv / 1000.0f, 2U);
    (void)snprintf(v, sizeof(v), "%s V  %d mA  %u %%", n1, (int)m->energy.ma, (unsigned int)m->energy.pct);
    ui_draw_text(layer, a.x1 + 8, y, ui_txt(T_BATTERY), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 8, y, v, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
    y += step;

    switch (m->energy.source) {
    case UI_CHARGE_SOLAR:
        (void)snprintf(v, sizeof(v), "%s, %u mW", ui_txt(T_SRC_SOLAR), (unsigned int)m->energy.solar_mw);
        break;
    case UI_CHARGE_USB:
        (void)snprintf(v, sizeof(v), "%s", ui_txt(T_SRC_USB));
        break;
    case UI_CHARGE_USB_FULL:
        (void)snprintf(v, sizeof(v), "%s", ui_txt(T_SRC_USB_FULL));
        break;
    default:
        (void)snprintf(v, sizeof(v), "%s", ui_txt(T_SRC_NONE));
        break;
    }
    ui_draw_text(layer, a.x1 + 8, y, ui_txt(T_CHARGE), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 8, y, v, UI_FONT_SMALL_B,
                 ui_col((m->energy.source == UI_CHARGE_NONE) ? UI_C_FG : UI_C_GOOD), LV_TEXT_ALIGN_RIGHT);
    y += step;

    (void)snprintf(v, sizeof(v), "%d °C", (int)m->energy.temp_c);
    ui_draw_text(layer, a.x1 + 8, y, ui_txt(T_TEMPERATURE), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 8, y, v, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
    y += step;

    (void)snprintf(v, sizeof(v), "%u", (unsigned int)m->debug.seg_loaded);
    ui_draw_text(layer, a.x1 + 8, y, ui_txt(T_SEGMENTS), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 8, y, v, UI_FONT_SMALL_B, ui_col(UI_C_SEG), LV_TEXT_ALIGN_RIGHT);
    y += step;

    ui_draw_text(layer, a.x1 + 8, y, ui_txt(T_VERSION), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 8, y, ui_ctx.version, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
    y += step;

    /* C/N0 of each satellite under the list, strongest first as the receiver reports */
    int32_t top = y + 2;
    int32_t bottom = a.y2 - 4;

    ui_draw_text(layer, a.x1 + 6, top, "C/N0 (dB-Hz)", UI_FONT_LABEL, fg, LV_TEXT_ALIGN_LEFT);
    for (uint32_t i = 0U; (i < m->gnss.nsat) && (i < 18U); i++) {
        int32_t cn0 = m->gnss.sat[i].cn0;
        int32_t bh = ((cn0 > 15) ? (cn0 - 15) : 0);

        if (bh > (bottom - top - 14)) {
            bh = bottom - top - 14;
        }
        ui_draw_fill(layer, a.x1 + 6 + ((int32_t)i * 13), bottom - bh, 9, bh,
                     (ui_ctx.theme == UI_THEME_MONO) ? fg : cn0_color((uint8_t)cn0));
    }
}

static void dbg_create(lv_obj_t *scr)
{
    static const ui_fplace_t fields[] = {
        {UI_F_TIME, 0, 5, 2},
    };
    lv_obj_t *band;

    ui_statusbar_create(scr);
    band = ui_band(scr, 0, 5);
    dbg_plot = ui_plot(band, 0, 0, UI_WIDTH, ui_rows_h(0, 5) - 1, dbg_draw, NULL);
    ui_fields_create(scr, fields, ARRAY_SIZE(fields));
    ui_fields_create(scr, batt_row, ARRAY_SIZE(batt_row));
}

static void dbg_update(lv_obj_t *scr)
{
    (void)scr;
    ui_statusbar_update();
    ui_fields_update();
    lv_obj_invalidate(dbg_plot);
}

const ui_screen_ops_t ui_scr_dbg = {dbg_create, dbg_update, NULL};

/* ==========================================================================
 * Elevation profile of the route (new: the legacy had no profile)
 * ========================================================================== */

static lv_obj_t *prof_plot;

static void profile_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_profile_t *p = &ui_ctx.m.profile;
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;

    lv_obj_get_coords(obj, &a);
    if (p->n < 2U) {
        ui_draw_text(layer, (a.x1 + a.x2) / 2, ((a.y1 + a.y2) / 2) - 10, ui_txt(T_NO_PROFILE),
                     UI_FONT_TITLE, fg, LV_TEXT_ALIGN_CENTER);
        return;
    }

    int32_t w = lv_area_get_width(&a);
    int32_t h = lv_area_get_height(&a);
    int32_t range = (int32_t)p->max_m - (int32_t)p->min_m;

    if (range < 10) {
        range = 10; /* a flat route still draws a line in the middle */
    }

    /* the ground of the profile, column by column */
    for (uint32_t i = 0U; i < p->n; i++) {
        int32_t x = a.x1 + (int32_t)((i * (uint32_t)w) / p->n);
        int32_t next = a.x1 + (int32_t)(((i + 1U) * (uint32_t)w) / p->n);
        int32_t top = a.y2 - (((int32_t)p->alt_m[i] - (int32_t)p->min_m) * (h - 6)) / range;
        bool done = (i <= p->here);

        int32_t cw = (next > x) ? (next - x) : 1;

        if (done && (ui_ctx.theme == UI_THEME_MONO)) {
            /* without colour the ridden part is only its outline */
            ui_draw_fill(layer, x, top, cw, 3, ui_col(UI_C_FG));
        } else {
            ui_draw_fill(layer, x, top, cw, a.y2 - top, ui_col(done ? UI_C_GOOD : UI_C_NAV));
        }
    }

    /* where the rider is */
    int32_t rx = a.x1 + (int32_t)((p->here * (uint32_t)w) / p->n);

    ui_draw_line(layer, rx, a.y1, rx, a.y2, fg, 2);
    ui_draw_disc(layer, rx, a.y2 - (((int32_t)p->alt_m[p->here] - (int32_t)p->min_m) * (h - 6)) /
                                      range, 4, fg);
}

static void profile_update(lv_obj_t *scr);

static ui_field_t prof_climb;
static ui_field_t prof_remain;
static ui_field_t prof_min;
static ui_field_t prof_max;

static void profile_create(lv_obj_t *scr)
{
    lv_obj_t *band;
    lv_obj_t *l;

    ui_statusbar_create(scr);

    band = ui_band(scr, 0, 1);
    l = ui_label(band, UI_FONT_TITLE, ui_col(UI_C_FG), ui_txt(T_PROFILE));
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 4, 0);

    ui_radar_strip_create(scr, ui_row_y(1), ui_rows_h(1, 3) - 1);
    prof_plot = ui_plot(scr, 0, ui_row_y(1), UI_WIDTH, ui_rows_h(1, 3) - 1, profile_draw, NULL);

    ui_field_create(&prof_climb, scr, 0, 4, 1, ui_txt(T_CLIMB_LEFT), "m", UI_C_FG);
    ui_field_create(&prof_remain, scr, 1, 4, 1, ui_txt(T_REMAIN), "km", UI_C_FG);
    ui_field_create(&prof_min, scr, 0, 5, 1, "MIN", "m", UI_C_FG);
    ui_field_create(&prof_max, scr, 1, 5, 1, "MAX", "m", UI_C_FG);

    /* the screen may come up with a route already loaded */
    profile_update(scr);
}

static void profile_update(lv_obj_t *scr)
{
    const ui_profile_t *p = &ui_ctx.m.profile;
    char v[12];

    (void)scr;
    ui_statusbar_update();

    ui_field_set(&prof_climb, ui_fmt_int(v, sizeof(v), (int32_t)p->climb_left_m), UI_C_FG);
    ui_field_set(&prof_remain, ui_fmt_float(v, sizeof(v), p->remain_km, 1U), UI_C_FG);
    ui_field_set(&prof_min, ui_fmt_int(v, sizeof(v), (int32_t)p->min_m), UI_C_FG);
    ui_field_set(&prof_max, ui_fmt_int(v, sizeof(v), (int32_t)p->max_m), UI_C_FG);

    lv_obj_invalidate(prof_plot);
}

static bool profile_key(ui_key_t key, ui_press_t press)
{
    /* a long press on the right goes back to the map (docs/telas, Botões) */
    if ((key == UI_KEY_RIGHT) && (press == UI_PRESS_LONG)) {
        ui_go(UI_SCREEN_PRC);
        return true;
    }
    return false;
}

const ui_screen_ops_t ui_scr_profile = {profile_create, profile_update, profile_key};

/* ==========================================================================
 * Lap page: the ride and the lap being ridden
 *
 * Nothing in the legacy answers "how far into this lap am I": it has no
 * timer and no lap at all. This page is of the port, from the totals of
 * `model/activity.h`, and it joins the CRS ring after page 3.
 * ========================================================================== */

static ui_field_t lap_num;
static ui_field_t lap_dist;
static ui_field_t lap_time;
static ui_field_t ride_time;
static ui_field_t ride_avg;
static ui_field_t ride_max;
static ui_field_t ride_desc;
static ui_field_t ride_kcal;
static lv_obj_t *lap_state;

static void lap_update(lv_obj_t *scr);

static void lap_create(lv_obj_t *scr)
{
    ui_statusbar_create(scr);

    lv_obj_t *l = ui_label(scr, UI_FONT_SMALL, ui_col(UI_C_FG), ui_txt(T_LAPS));

    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 4, ui_row_y(0) + 2);

    lap_state = ui_label(scr, UI_FONT_SMALL, ui_col(UI_C_BAD), "");
    lv_obj_align(lap_state, LV_ALIGN_TOP_RIGHT, -4, ui_row_y(0) + 2);

    ui_field_create(&lap_num, scr, 0, 1, 1, ui_txt(T_LAP), "", UI_C_FG);
    ui_field_create(&lap_dist, scr, 1, 1, 1, ui_txt(T_DIST), "km", UI_C_FG);
    ui_field_create(&lap_time, scr, 0, 2, 2, ui_txt(T_LAP), "", UI_C_FG);
    ui_field_create(&ride_time, scr, 0, 3, 2, ui_txt(T_MOVING), "", UI_C_FG);
    ui_field_create(&ride_avg, scr, 0, 4, 1, ui_txt(T_AVG), "km/h", UI_C_FG);
    ui_field_create(&ride_max, scr, 1, 4, 1, "MAX", "km/h", UI_C_FG);
    ui_field_create(&ride_desc, scr, 0, 5, 1, ui_txt(T_DESCENT), "m", UI_C_FG);
    ui_field_create(&ride_kcal, scr, 1, 5, 1, ui_txt(T_KCAL), "kcal", UI_C_FG);

    lap_update(scr);
}

static void lap_update(lv_obj_t *scr)
{
    const ui_activity_t *a = &ui_ctx.m.act;
    char v[12];

    (void)scr;
    ui_statusbar_update();

    /* the rider needs to see at a glance that the timer stopped */
    lv_label_set_text(lap_state, ui_ctx.m.status.paused ? ui_txt(T_PAUSED) : "");

    ui_field_set(&lap_num, ui_fmt_int(v, sizeof(v), (int32_t)a->laps + 1), UI_C_FG);
    ui_field_set(&lap_dist, ui_fmt_float(v, sizeof(v), a->lap_dist_m / 1000.0f, 2U), UI_C_FG);
    ui_field_set(&lap_time, ui_fmt_hms(v, sizeof(v), a->lap_timer_s, ':'), UI_C_FG);
    ui_field_set(&ride_time, ui_fmt_hms(v, sizeof(v), a->timer_s, ':'), UI_C_FG);
    ui_field_set(&ride_avg, ui_fmt_float(v, sizeof(v), a->avg_kmh, 1U), UI_C_FG);
    ui_field_set(&ride_max, ui_fmt_float(v, sizeof(v), a->max_kmh, 1U), UI_C_FG);
    ui_field_set(&ride_desc, ui_fmt_int(v, sizeof(v), (int32_t)a->descent_m), UI_C_FG);
    ui_field_set(&ride_kcal, ui_fmt_int(v, sizeof(v), (int32_t)a->kcal), UI_C_FG);
}

const ui_screen_ops_t ui_scr_lap = {lap_create, lap_update, NULL};

/* ==========================================================================
 * The structured session (`model/workout.h`)
 *
 * What a rider needs while a session runs, and nothing else: which step
 * they are on and of how many, what it is called, how much of it is left,
 * what they were asked to hold, and whether they are holding it. The value
 * is drawn in the colour of the answer, so it can be read at a glance from
 * the bars without counting digits.
 * ========================================================================== */

static lv_obj_t *wk_name;
static lv_obj_t *wk_step;
static lv_obj_t *wk_label;
static ui_field_t wk_left;
static ui_field_t wk_target;
static ui_field_t wk_value;
static lv_obj_t *wk_bar;
static uint8_t wk_kind;     /**< target of the step the fields were built for */
static bool wk_had_file;    /**< whether a session existed when it was built */

static void wk_update(lv_obj_t *scr);

/** Unit of each kind of target, for the corner of the field */
static const char *wk_unit(uint8_t kind)
{
    switch (kind) {
    case 1U:
        return "W";
    case 2U:
        return "bpm";
    case 3U:
        return "rpm";
    default:
        return "";
    }
}

/** The band of the target, with a mark where the rider is */
static void wk_bar_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_workout_t *w = &ui_ctx.m.wk;
    lv_area_t a;

    lv_obj_get_coords(obj, &a);

    if ((w->target == 0U) || (w->hi == 0U)) {
        return;
    }

    int32_t full = a.x2 - a.x1 - 8;
    int32_t y = a.y1 + 4;

    /*
     * The band runs from half the bottom of the range to one and a half
     * times the top, so that being far outside still has somewhere to be
     * drawn instead of pinning to an edge and looking like being close.
     */
    uint32_t span_lo = (uint32_t)w->lo / 2U;
    uint32_t span_hi = ((uint32_t)w->hi * 3U) / 2U;
    uint32_t span = (span_hi > span_lo) ? (span_hi - span_lo) : 1U;

    int32_t x_lo = (int32_t)(((uint32_t)(w->lo - span_lo) * (uint32_t)full) / span);
    int32_t x_hi = (int32_t)(((uint32_t)(w->hi - span_lo) * (uint32_t)full) / span);

    /* the range itself */
    ui_draw_fill(layer, a.x1 + 4 + x_lo, y, (x_hi - x_lo) + 1, 8, ui_col_fill(UI_C_GOOD));

    /* and where the rider is */
    uint32_t v = w->value;

    if (v < span_lo) {
        v = span_lo;
    }
    if (v > span_hi) {
        v = span_hi;
    }

    int32_t x = (int32_t)(((v - span_lo) * (uint32_t)full) / span);

    ui_draw_fill(layer, a.x1 + 2 + x, y - 3, 3, 14, ui_col_fill(UI_C_FG));
}

static void wk_create(lv_obj_t *scr)
{
    const ui_workout_t *w = &ui_ctx.m.wk;

    ui_statusbar_create(scr);

    wk_had_file = w->loaded;
    if (!w->loaded) {
        wk_name = ui_label(scr, UI_FONT_LARGE, ui_col(UI_C_FG), ui_txt(T_WORKOUT));
        lv_obj_set_pos(wk_name, 10, 60);
        wk_label = ui_label(scr, UI_FONT_SMALL, ui_col(UI_C_FG), ui_txt(T_NO_WORKOUT));
        lv_obj_set_pos(wk_label, 10, 90);
        wk_step = NULL;
        wk_bar = NULL;
        return;
    }

    wk_name = ui_label(scr, UI_FONT_SMALL, ui_col(UI_C_FG), w->name);
    lv_obj_align(wk_name, LV_ALIGN_TOP_LEFT, 4, ui_row_y(0) + 2);

    wk_step = ui_label(scr, UI_FONT_SMALL_B, ui_col(UI_C_FG), "");
    lv_obj_align(wk_step, LV_ALIGN_TOP_RIGHT, -4, ui_row_y(0) + 2);

    wk_label = ui_label(scr, UI_FONT_MEDIUM, ui_col(UI_C_FG), "");
    lv_obj_align(wk_label, LV_ALIGN_TOP_LEFT, 4, ui_row_y(1) + 2);

    /*
     * The unit belongs in the corner of the field and the value is a bare
     * number: the big font of a value carries digits only, so a letter in
     * it comes out as a box, and anything past six characters is cut to
     * `---` by `ui_fmt_cadran()`. The range of the step is the green band
     * of the bar below, which is where a range reads best anyway.
     */
    wk_kind = w->target;
    ui_field_create(&wk_left, scr, 0, 2, 2, ui_txt(T_REMAINING), "", UI_C_FG);
    ui_field_create(&wk_target, scr, 0, 4, 1, ui_txt(T_TARGET), wk_unit(wk_kind), UI_C_FG);
    ui_field_create(&wk_value, scr, 1, 4, 1, ui_txt(T_NOW), wk_unit(wk_kind), UI_C_FG);

    wk_bar = ui_plot(scr, 0, ui_row_y(5), UI_WIDTH, ui_rows_h(5, 1), wk_bar_draw, NULL);

    wk_update(scr);
}

static void wk_update(lv_obj_t *scr)
{
    const ui_workout_t *w = &ui_ctx.m.wk;
    char v[20];

    (void)scr;
    ui_statusbar_update();

    /* a session arriving or going away changes the whole page */
    if (w->loaded != wk_had_file) {
        ui_rebuild();
        return;
    }
    if (!w->loaded || (wk_step == NULL)) {
        return;
    }

    (void)snprintf(v, sizeof(v), "%u/%u", (unsigned int)w->step, (unsigned int)w->steps);
    lv_label_set_text(wk_step, v);
    lv_label_set_text(wk_label, (w->label[0] != (char)0) ? w->label : ui_txt(T_STEP));

    if (!w->running) {
        ui_field_set(&wk_left, ui_txt(T_STOPPED), UI_C_FG);
    } else if (w->by_key) {
        ui_field_set(&wk_left, ui_txt(T_PRESS_KEY), UI_C_FG);
    } else if (w->by_distance) {
        (void)snprintf(v, sizeof(v), "%u m", (unsigned int)w->remaining);
        ui_field_set(&wk_left, v, UI_C_FG);
    } else {
        ui_field_set(&wk_left, ui_fmt_hms(v, sizeof(v), w->remaining, ':'), UI_C_FG);
    }

    /* the unit is part of the field, so a step of another kind rebuilds */
    if (w->target != wk_kind) {
        ui_rebuild();
        return;
    }

    if (w->target == 0U) {
        ui_field_set(&wk_target, "-", UI_C_FG);
        ui_field_set(&wk_value, "-", UI_C_FG);
    } else {
        /* the middle of the range is what the trainer is asked to hold */
        ui_field_set(&wk_target,
                     ui_fmt_int(v, sizeof(v), (int32_t)(((uint32_t)w->lo + w->hi) / 2U)),
                     UI_C_FG);

        /*
         * The colour is the answer: green inside the range, red outside.
         * A rider on the bars reads a colour long before a number.
         */
        ui_role_t col = (w->zone == 0) ? UI_C_GOOD : UI_C_BAD;

        ui_field_set(&wk_value, ui_fmt_int(v, sizeof(v), (int32_t)w->value), col);
    }

    if (wk_bar != NULL) {
        lv_obj_invalidate(wk_bar);
    }
}

const ui_screen_ops_t ui_scr_workout = {wk_create, wk_update, NULL};

/* ==========================================================================
 * Climb page: the climb being ridden, not the whole route
 *
 * On a pass the profile of the route is a flat line with a bump in it, and
 * what the rider wants is the bump filling the screen: how much is left, how
 * steep the rest of it is, and how steep the next two hundred metres are.
 * The legacy has nothing like it (`model/climb.h`).
 * ========================================================================== */

static lv_obj_t *climb_plot;
static lv_obj_t *climb_title;
static ui_field_t climb_left;
static ui_field_t climb_up;
static ui_field_t climb_avg;
static ui_field_t climb_now;

static void climb_update_scr(lv_obj_t *scr);

/** The name of a category, as cycling writes it */
static const char *climb_cat_name(uint8_t cat)
{
    static const char *const names[] = {"", "4", "3", "2", "1"};

    if (cat == (uint8_t)CLIMB_CAT_HC) {
        return ui_txt(T_HC);
    }

    return (cat < (sizeof(names) / sizeof(names[0]))) ? names[cat] : "";
}

/** Colour of a stretch by how steep it is, as the maps of a race do */
static ui_role_t climb_grade_role(int32_t pct)
{
    if (pct >= 10) {
        return UI_C_BAD;
    }
    if (pct >= 6) {
        return UI_C_WARN;
    }

    return UI_C_NAV;
}

static void climb_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_climb_t *c = &ui_ctx.m.climb;
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;

    lv_obj_get_coords(obj, &a);
    if (c->prof_n < 2U) {
        const char *what = (c->total == 0U) ? ui_txt(T_NO_CLIMB) : ui_txt(T_NEXT_M);

        ui_draw_text(layer, (a.x1 + a.x2) / 2, ((a.y1 + a.y2) / 2) - 10, what, UI_FONT_TITLE, fg,
                     LV_TEXT_ALIGN_CENTER);
        return;
    }

    int32_t w = lv_area_get_width(&a);
    int32_t h = lv_area_get_height(&a);
    int32_t range = (int32_t)c->prof_max_m - (int32_t)c->prof_min_m;

    if (range < 10) {
        range = 10;
    }

    for (uint32_t i = 0U; i < c->prof_n; i++) {
        int32_t x = a.x1 + (int32_t)((i * (uint32_t)w) / c->prof_n);
        int32_t next = a.x1 + (int32_t)(((i + 1U) * (uint32_t)w) / c->prof_n);
        int32_t top = a.y2 - (((int32_t)c->prof_m[i] - (int32_t)c->prof_min_m) * (h - 6)) / range;
        int32_t cw = (next > x) ? (next - x) : 1;
        bool done = (i <= c->prof_here);

        /*
         * The slope of this column decides its colour, in metres over
         * metres: how wide a column is on the screen has nothing to do
         * with how much ground it covers.
         */
        uint32_t prev = (i > 0U) ? (i - 1U) : 0U;
        int32_t rise = (int32_t)c->prof_m[i] - (int32_t)c->prof_m[prev];
        float run_m = (c->prof_span_m > 0.0f) ? (c->prof_span_m / (float)c->prof_n) : 0.0f;
        int32_t pct = (run_m >= 1.0f) ? (int32_t)(((float)rise / run_m) * 100.0f) : 0;

        if (done && (ui_ctx.theme == UI_THEME_MONO)) {
            ui_draw_fill(layer, x, top, cw, 3, fg);
        } else {
            /* a filled bar keeps the yellow the light theme hides in text */
            ui_draw_fill(layer, x, top, cw, a.y2 - top,
                         ui_col_fill(done ? UI_C_GOOD : climb_grade_role(pct)));
        }
    }

    int32_t rx = a.x1 + (int32_t)((c->prof_here * (uint32_t)w) / c->prof_n);

    ui_draw_line(layer, rx, a.y1, rx, a.y2, fg, 2);
    ui_draw_disc(layer, rx,
                 a.y2 - (((int32_t)c->prof_m[c->prof_here] - (int32_t)c->prof_min_m) * (h - 6)) /
                            range,
                 4, fg);
}

static void climb_create(lv_obj_t *scr)
{
    ui_statusbar_create(scr);

    climb_title = ui_label(scr, UI_FONT_SMALL, ui_col(UI_C_FG), ui_txt(T_CLIMB_N));
    lv_obj_align(climb_title, LV_ALIGN_TOP_LEFT, 4, ui_row_y(0) + 2);

    ui_radar_strip_create(scr, ui_row_y(1), ui_rows_h(1, 3) - 1);
    climb_plot = ui_plot(scr, 0, ui_row_y(1), UI_WIDTH, ui_rows_h(1, 3) - 1, climb_draw, NULL);

    ui_field_create(&climb_left, scr, 0, 4, 1, ui_txt(T_TO_TOP), "km", UI_C_FG);
    ui_field_create(&climb_up, scr, 1, 4, 1, ui_txt(T_CLIMB_LEFT), "m", UI_C_FG);
    ui_field_create(&climb_avg, scr, 0, 5, 1, ui_txt(T_AVG), "%", UI_C_FG);
    ui_field_create(&climb_now, scr, 1, 5, 1, ui_txt(T_GRADE), "%", UI_C_FG);

    climb_update_scr(scr);
}

static void climb_update_scr(lv_obj_t *scr)
{
    const ui_climb_t *c = &ui_ctx.m.climb;
    char v[16];
    char t[24];

    (void)scr;
    ui_statusbar_update();

    if (c->on_climb) {
        const char *cat = climb_cat_name(c->cat);

        if (cat[0] != '\0') {
            /* the category as cycling writes it: C1 to C4, or HC */
            (void)snprintf(t, sizeof(t), "%s %u/%u  %s%s", ui_txt(T_CLIMB_N),
                           (unsigned int)c->index, (unsigned int)c->total,
                           (c->cat == (uint8_t)CLIMB_CAT_HC) ? "" : "C", cat);
        } else {
            (void)snprintf(t, sizeof(t), "%s %u/%u", ui_txt(T_CLIMB_N), (unsigned int)c->index,
                           (unsigned int)c->total);
        }
        lv_label_set_text(climb_title, t);

        ui_field_set(&climb_left, ui_fmt_float(v, sizeof(v), c->remain_m / 1000.0f, 2U), UI_C_FG);
        ui_field_set(&climb_up, ui_fmt_int(v, sizeof(v), (int32_t)c->remain_gain_m), UI_C_FG);
        ui_field_set(&climb_avg, ui_fmt_float(v, sizeof(v), c->grade_pct, 1U),
                     climb_grade_role((int32_t)c->grade_pct));
        ui_field_set(&climb_now, ui_fmt_float(v, sizeof(v), c->ahead_grade_pct, 1U),
                     climb_grade_role((int32_t)c->ahead_grade_pct));
    } else if (c->total > 0U) {
        /* between climbs: what the next one is and how far off */
        (void)snprintf(t, sizeof(t), "%s %s", ui_txt(T_NEXT_M), climb_cat_name(c->next_cat));
        lv_label_set_text(climb_title, t);

        ui_field_set(&climb_left, ui_fmt_float(v, sizeof(v), c->to_next_m / 1000.0f, 2U), UI_C_FG);
        ui_field_set(&climb_up, ui_fmt_int(v, sizeof(v), (int32_t)c->next_gain_m), UI_C_FG);
        ui_field_set(&climb_avg,
                     ui_fmt_float(v, sizeof(v),
                                  (c->next_len_m > 1.0f)
                                      ? ((c->next_gain_m / c->next_len_m) * 100.0f)
                                      : 0.0f,
                                  1U),
                     UI_C_FG);
        ui_field_set(&climb_now, ui_fmt_float(v, sizeof(v), c->ahead_grade_pct, 1U), UI_C_FG);
    } else {
        lv_label_set_text(climb_title, ui_txt(T_CLIMB_N));
        ui_field_set(&climb_left, "--", UI_C_FG);
        ui_field_set(&climb_up, "--", UI_C_FG);
        ui_field_set(&climb_avg, "--", UI_C_FG);
        ui_field_set(&climb_now, "--", UI_C_FG);
    }

    lv_obj_invalidate(climb_plot);
}

static bool climb_key(ui_key_t key, ui_press_t press)
{
    /* a long press on the right goes back to the map, as the profile does */
    if ((key == UI_KEY_RIGHT) && (press == UI_PRESS_LONG)) {
        ui_go(UI_SCREEN_PRC);
        return true;
    }

    return false;
}

const ui_screen_ops_t ui_scr_climb = {climb_create, climb_update_scr, climb_key};
