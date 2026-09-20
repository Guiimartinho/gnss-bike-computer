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
    fec_vector = ui_plot(scr, 0, ui_row_y(4), UI_WIDTH, ui_rows_h(4, 3), vector_draw, NULL);
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
