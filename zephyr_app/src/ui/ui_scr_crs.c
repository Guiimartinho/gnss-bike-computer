/**
 * @file ui_scr_crs.c
 * @brief CRS pages: data and segments, navigation and RR, attitude
 *
 * Layouts of legacy/source/vue/VueCRS.cpp: afficheScreen1() with 0, 1 or 2
 * segments (and each segment on or off), afficheScreen2() and
 * afficheSensors(). The mini-map and the "partner" gauge follow
 * afficheSegment() and partner(); the model gives the track already
 * projected into its window.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui_internal.h"

/* ==========================================================================
 * Segment mini-map (VueCRS::afficheSegment)
 * ========================================================================== */

/** Which segments a map band draws: bit 0 = seg[0], bit 1 = seg[1] */
#define MAP_SEG0    ((void *)(uintptr_t)1U)
#define MAP_SEG1    ((void *)(uintptr_t)2U)
#define MAP_BOTH    ((void *)(uintptr_t)3U)

/** Legacy status == SEG_OFF: neither running nor finished */
static bool seg_off(const ui_segment_t *s)
{
    return !s->on && !s->done;
}

static void draw_one_segment(lv_layer_t *layer, const lv_area_t *a, const ui_segment_t *s)
{
    lv_color_t track = ui_col(UI_C_SEG);
    lv_color_t fg = ui_col(UI_C_FG);
    int32_t last_x = 0;
    int32_t last_y = 0;

    if (s->npts < 2U) {
        return;
    }
    for (uint32_t i = 1U; i < s->npts; i++) {
        ui_draw_line(layer, ui_map_x(a, 0, s->pts[i - 1U].x), ui_map_y(a, 0, s->pts[i - 1U].y),
                     ui_map_x(a, 0, s->pts[i].x), ui_map_y(a, 0, s->pts[i].y), track, 3);
    }
    last_x = ui_map_x(a, 0, s->pts[s->npts - 1U].x);
    last_y = ui_map_y(a, 0, s->pts[s->npts - 1U].y);

    if (s->done) {
        /* finished: frame at the end */
        ui_draw_frame(layer, last_x - 5, last_y - 5, 10, 10, fg, 1);
    } else if (s->on) {
        /* running: chequered flag at the end */
        ui_draw_frame(layer, last_x - 5, last_y - 5, 10, 10, fg, 1);
        ui_draw_fill(layer, last_x - 5, last_y - 5, 5, 5, fg);
        ui_draw_fill(layer, last_x, last_y, 5, 5, fg);
    } else {
        /* approaching: circle at the start */
        ui_draw_circle(layer, ui_map_x(a, 0, s->pts[0].x), ui_map_y(a, 0, s->pts[0].y), 5, fg, 1);
    }

    /* the rider stops at the end when the segment is done */
    int32_t rx = s->done ? last_x : ui_map_x(a, 0, s->rider.x);
    int32_t ry = s->done ? last_y : ui_map_y(a, 0, s->rider.y);

    ui_draw_rider(layer, rx, ry, s->course_deg, fg);

    if (seg_off(s)) {
        return;
    }

    /* gap to the record next to the rider, completion in the corner */
    char buf[16];
    int32_t tx = (rx > (UI_WIDTH - 70)) ? (UI_WIDTH - 70) : rx;
    int32_t ty = (ry > (a->y2 - 30)) ? (ry - 22) : (ry + 10);

    (void)ui_fmt_signed(buf, sizeof(buf), s->advance_s, 1U, "s");
    ui_draw_text(layer, a->x1 + tx - 10, ty, buf, UI_FONT_MEDIUM,
                 ui_col((s->advance_s >= 0.0f) ? UI_C_GOOD : UI_C_BAD), LV_TEXT_ALIGN_LEFT);
    (void)snprintf(buf, sizeof(buf), "%u%%", (unsigned int)(s->done ? 100U : s->pct));
    ui_draw_text(layer, a->x1 + 6, a->y1 + 4, buf, UI_FONT_ITEM, fg, LV_TEXT_ALIGN_LEFT);
}

static void seg_map_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    uintptr_t which = (uintptr_t)lv_event_get_user_data(e);
    lv_area_t a;

    lv_obj_get_coords(obj, &a);
    for (uint32_t i = 0U; (i < UI_SEG_MAX) && (i < ui_ctx.m.nseg); i++) {
        if ((which & (1U << i)) != 0U) {
            draw_one_segment(layer, &a, &ui_ctx.m.seg[i]);
        }
    }
}

/* ==========================================================================
 * Partner gauge (VueCRS::partner)
 * ========================================================================== */

static void partner_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    uintptr_t idx = (uintptr_t)lv_event_get_user_data(e);
    const ui_segment_t *s = &ui_ctx.m.seg[(idx < UI_SEG_MAX) ? idx : 0U];
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;
    char buf[12];

    lv_obj_get_coords(obj, &a);

    /* indice = advance / time on the segment, with at least 5 s */
    float indice = (s->cur_s < 5.0f) ? (s->advance_s / 5.0f) : (s->advance_s / s->cur_s);
    const int32_t ol = 30;
    int32_t hl = a.y1 + 25;
    int32_t centre;
    float t = (indice + 0.25f) / 0.5f;

    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    centre = a.x1 + ol + (int32_t)lroundf(t * (float)(UI_WIDTH - (2 * ol)));

    /* bar, +-10 % ticks and the centre mark */
    int32_t dix = ((UI_WIDTH - (2 * ol)) * 10) / 50;

    ui_draw_fill(layer, a.x1 + ol, hl, UI_WIDTH - (2 * ol), 3, fg);
    ui_draw_fill(layer, a.x1 + (UI_WIDTH / 2) - dix, hl + 2, 1, 7, fg);
    ui_draw_fill(layer, a.x1 + (UI_WIDTH / 2) + dix, hl + 2, 1, 7, fg);
    ui_draw_fill(layer, a.x1 + (UI_WIDTH / 2), hl - 12, 1, 12, fg);

    lv_color_t mark = ui_col((indice >= 0.0f) ? UI_C_GOOD : UI_C_BAD);

    ui_draw_triangle(layer, centre - 7, hl + 7, centre, hl - 7, centre + 7, hl + 7, mark);
    (void)snprintf(buf, sizeof(buf), "%s%d %%", (indice >= 0.0f) ? "+" : "", (int)(indice * 100.0f));
    ui_draw_text(layer, centre, hl + 10, buf, UI_FONT_ITEM, mark, LV_TEXT_ALIGN_CENTER);
}

/* ==========================================================================
 * Page 1 (afficheScreen1)
 * ========================================================================== */

typedef enum {
    CRS1_FULL = 0,          /**< no segment */
    CRS1_SS_OFF,            /**< one segment, approaching */
    CRS1_SS_ON,             /**< one segment, running or done */
    CRS1_DS_BOTH_OFF,
    CRS1_DS_FIRST_OFF,
    CRS1_DS_SECOND_OFF,
    CRS1_DS_NONE_OFF,
} crs1_layout_t;

static crs1_layout_t crs1_layout;
static lv_obj_t *crs1_plots[4];
static uint32_t crs1_nplots;

static crs1_layout_t crs1_pick(void)
{
    const ui_model_t *m = &ui_ctx.m;

    if (m->nseg == 0U) {
        return CRS1_FULL;
    }
    if (m->nseg == 1U) {
        return seg_off(&m->seg[0]) ? CRS1_SS_OFF : CRS1_SS_ON;
    }
    bool off0 = seg_off(&m->seg[0]);
    bool off1 = seg_off(&m->seg[1]);

    if (off0 && off1) {
        return CRS1_DS_BOTH_OFF;
    }
    if (off0) {
        return CRS1_DS_FIRST_OFF;
    }
    if (off1) {
        return CRS1_DS_SECOND_OFF;
    }
    return CRS1_DS_NONE_OFF;
}

static void add_map(lv_obj_t *scr, int32_t row, void *which)
{
    lv_obj_t *band = ui_band(scr, row, 2);

    crs1_plots[crs1_nplots++] = ui_plot(band, 0, 0, UI_WIDTH, ui_rows_h(row, 2) - 1, seg_map_draw, which);
}

static void add_partner(lv_obj_t *scr, int32_t row, uintptr_t idx)
{
    lv_obj_t *band = ui_band(scr, row, 1);

    crs1_plots[crs1_nplots++] = ui_plot(band, 0, 0, UI_WIDTH, ui_rows_h(row, 1) - 1, partner_draw,
                                        (void *)idx);
}

static void crs1_create(lv_obj_t *scr)
{
    static const ui_fplace_t full[] = {
        {UI_F_DIST, 0, 0, 1}, {UI_F_PWR, 1, 0, 1},
        {UI_F_SPEED, 0, 1, 1}, {UI_F_CLIMB, 1, 1, 1},
        {UI_F_CAD, 0, 2, 1}, {UI_F_HR, 1, 2, 1},
        {UI_F_SLOPE, 0, 3, 1}, {UI_F_VA, 1, 3, 1},
        {UI_F_NEXT_SEG, 0, 4, 1}, {UI_F_ALT, 1, 4, 1},
        {UI_F_AVG, 0, 5, 1}, {UI_F_SCORE, 1, 5, 1},
        {UI_F_SOLAR, 0, 6, 1}, {UI_F_BATT, 1, 6, 1},
    };
    static const ui_fplace_t ss_top[] = {
        {UI_F_DIST, 0, 0, 1}, {UI_F_PWR, 1, 0, 1},
        {UI_F_SPEED, 0, 1, 1}, {UI_F_CLIMB, 1, 1, 1},
        {UI_F_CAD, 0, 2, 1}, {UI_F_HR, 1, 2, 1},
        {UI_F_SLOPE, 0, 3, 1}, {UI_F_VA, 1, 3, 1},
    };
    static const ui_fplace_t next_row6[] = {{UI_F_NEXT_SEG, 0, 6, 2}};
    static const ui_fplace_t ds_both_off[] = {
        {UI_F_SLOPE, 0, 0, 1}, {UI_F_HR, 1, 0, 1},
        {UI_F_SPEED, 0, 1, 1}, {UI_F_PWR, 1, 1, 1},
        {UI_F_CAD, 0, 2, 1}, {UI_F_CLIMB, 1, 2, 1},
        {UI_F_DIST, 0, 3, 1}, {UI_F_SLOPE, 1, 3, 1},
    };
    static const ui_fplace_t ds_two_rows[] = {
        {UI_F_SLOPE, 0, 0, 1}, {UI_F_HR, 1, 0, 1},
        {UI_F_SPEED, 0, 1, 1}, {UI_F_PWR, 1, 1, 1},
    };
    static const ui_fplace_t ds_one_row[] = {
        {UI_F_SLOPE, 0, 0, 1}, {UI_F_HR, 1, 0, 1},
    };

    crs1_nplots = 0U;
    crs1_layout = crs1_pick();
    ui_statusbar_create(scr);

    switch (crs1_layout) {
    case CRS1_FULL:
        ui_fields_create(scr, full, ARRAY_SIZE(full));
        break;
    case CRS1_SS_OFF:
        ui_fields_create(scr, ss_top, ARRAY_SIZE(ss_top));
        add_map(scr, 4, MAP_SEG0);
        ui_fields_create(scr, next_row6, ARRAY_SIZE(next_row6));
        break;
    case CRS1_SS_ON:
        ui_fields_create(scr, ss_top, ARRAY_SIZE(ss_top));
        add_map(scr, 4, MAP_SEG0);
        add_partner(scr, 6, 0U);
        break;
    case CRS1_DS_BOTH_OFF:
        ui_fields_create(scr, ds_both_off, ARRAY_SIZE(ds_both_off));
        add_map(scr, 4, MAP_BOTH);
        ui_fields_create(scr, next_row6, ARRAY_SIZE(next_row6));
        break;
    case CRS1_DS_FIRST_OFF:
        ui_fields_create(scr, ds_two_rows, ARRAY_SIZE(ds_two_rows));
        add_map(scr, 2, MAP_SEG0);
        add_map(scr, 4, MAP_SEG1);
        add_partner(scr, 6, 1U);
        break;
    case CRS1_DS_SECOND_OFF:
        ui_fields_create(scr, ds_two_rows, ARRAY_SIZE(ds_two_rows));
        add_map(scr, 2, MAP_SEG0);
        add_partner(scr, 4, 0U);
        add_map(scr, 5, MAP_SEG1);
        break;
    case CRS1_DS_NONE_OFF:
    default:
        ui_fields_create(scr, ds_one_row, ARRAY_SIZE(ds_one_row));
        add_map(scr, 1, MAP_SEG0);
        add_partner(scr, 3, 0U);
        add_map(scr, 4, MAP_SEG1);
        add_partner(scr, 6, 1U);
        break;
    }
}

static void crs1_update(lv_obj_t *scr)
{
    (void)scr;
    if (crs1_pick() != crs1_layout) {
        ui_rebuild();
        return;
    }
    ui_statusbar_update();
    ui_fields_update();
    for (uint32_t i = 0U; i < crs1_nplots; i++) {
        lv_obj_invalidate(crs1_plots[i]);
    }
}

const ui_screen_ops_t ui_scr_crs1 = {crs1_create, crs1_update, NULL};

/* ==========================================================================
 * Page 2 (afficheScreen2): navigation and RR zones
 * ========================================================================== */

static lv_obj_t *crs2_rr;
static lv_obj_t *crs2_turn;

/** Legacy Vue::cadranRR(): one bar per zone, '>' on the current one */
static void rr_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_rr_t *rr = &ui_ctx.m.rr;
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;
    float val_max = 0.0f;
    char buf[8];

    lv_obj_get_coords(obj, &a);
    ui_draw_text(layer, a.x1 + 4, a.y1 + 2, ui_txt(T_RR), UI_FONT_LABEL, fg, LV_TEXT_ALIGN_LEFT);

    for (uint32_t i = 0U; (i < rr->nzones) && (i < UI_RR_ZONES); i++) {
        if (rr->val[i] > val_max) {
            val_max = rr->val[i];
        }
    }
    /* force the span into a window, as the legacy */
    float span = val_max;

    if (span > 50.0f) {
        span = 50.0f;
    }
    if (span < 5.0f) {
        span = 5.0f;
    }
    for (uint32_t i = 0U; (i < rr->nzones) && (i < UI_RR_ZONES); i++) {
        float v = rr->val[i];
        int32_t full = (UI_WIDTH / 2) - 35;
        int32_t w = 2 + (int32_t)((v / span) * (float)(full - 2));

        if (w > full) {
            w = full;
        }
        ui_draw_fill(layer, a.x1 + 20, a.y1 + 16 + ((int32_t)i * 6), w, 4, ui_col_fill(UI_C_FG));
        if (i == rr->cur) {
            ui_draw_text(layer, a.x1 + 8, a.y1 + 11 + ((int32_t)i * 6), ">", UI_FONT_SMALL_B, fg,
                         LV_TEXT_ALIGN_LEFT);
        }
    }
    (void)snprintf(buf, sizeof(buf), "%u", (unsigned int)val_max);
    ui_draw_text(layer, a.x2 - 4, a.y1 + 2, buf, UI_FONT_LABEL, fg, LV_TEXT_ALIGN_RIGHT);
}

/** Direction of each turn in degrees (0 ahead, negative to the left) */
static int32_t turn_angle(ui_turn_t t)
{
    switch (t) {
    case UI_TURN_SLIGHT_LEFT:
        return -45;
    case UI_TURN_LEFT:
        return -90;
    case UI_TURN_SHARP_LEFT:
        return -135;
    case UI_TURN_SLIGHT_RIGHT:
        return 45;
    case UI_TURN_RIGHT:
        return 90;
    case UI_TURN_SHARP_RIGHT:
        return 135;
    default:
        return 0;
    }
}

/** Turn arrow in place of the 110 x 110 Komoot icon (legacy y = 289) */
static void turn_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_nav_t *nav = &ui_ctx.m.nav;
    lv_color_t c = ui_col(UI_C_NAV);
    lv_area_t a;

    lv_obj_get_coords(obj, &a);
    int32_t cx = (a.x1 + a.x2) / 2;
    int32_t top = a.y1 + 22;
    int32_t bottom = a.y2 - 8;
    int32_t mid = (top + bottom) / 2 + 6;

    if (!nav->valid || (nav->turn == UI_TURN_NONE)) {
        return;
    }
    ui_draw_text(layer, cx, a.y1 + 3, nav->street, UI_FONT_ITEM, ui_col(UI_C_FG), LV_TEXT_ALIGN_CENTER);

    if (nav->turn == UI_TURN_ARRIVE) {
        ui_draw_circle(layer, cx, mid, 22, c, 5);
        ui_draw_disc(layer, cx, mid, 9, c);
        return;
    }
    if (nav->turn == UI_TURN_UTURN) {
        ui_draw_line(layer, cx + 18, bottom, cx + 18, mid - 10, c, 8);
        ui_draw_line(layer, cx + 18, mid - 10, cx + 8, mid - 22, c, 8);
        ui_draw_line(layer, cx + 8, mid - 22, cx - 8, mid - 22, c, 8);
        ui_draw_line(layer, cx - 8, mid - 22, cx - 18, mid - 10, c, 8);
        ui_draw_line(layer, cx - 18, mid - 10, cx - 18, mid + 6, c, 8);
        ui_draw_triangle(layer, cx - 32, mid + 4, cx - 4, mid + 4, cx - 18, mid + 26, c);
        return;
    }

    /* shaft from the bottom to the middle, then towards the turn */
    int32_t ang = turn_angle(nav->turn);
    int32_t len = 34;
    int32_t ex;
    int32_t ey;

    ui_draw_line(layer, cx, bottom, cx, mid, c, 8);
    ui_rotate(cx, mid, cx, mid - len, (float)ang, &ex, &ey);
    ui_draw_line(layer, cx, mid, ex, ey, c, 8);

    /* arrow head at the end of the second leg */
    int32_t hx0, hy0, hx1, hy1, hx2, hy2;

    ui_rotate(cx, mid, cx, mid - len - 16, (float)ang, &hx0, &hy0);
    ui_rotate(cx, mid, cx - 14, mid - len + 4, (float)ang, &hx1, &hy1);
    ui_rotate(cx, mid, cx + 14, mid - len + 4, (float)ang, &hx2, &hy2);
    ui_draw_triangle(layer, hx0, hy0, hx1, hy1, hx2, hy2, c);
}

static void crs2_create(lv_obj_t *scr)
{
    static const ui_fplace_t fields[] = {
        {UI_F_DIST, 0, 0, 1}, {UI_F_PWR, 1, 0, 1},
        {UI_F_SPEED, 0, 1, 1}, {UI_F_CLIMB, 1, 1, 1},
        {UI_F_CAD, 0, 2, 1}, {UI_F_HR, 1, 2, 1},
        {UI_F_PR, 0, 3, 1}, {UI_F_VA, 1, 3, 1},
        {UI_F_NEXT_TURN, 0, 4, 1},
    };
    lv_obj_t *cell;

    ui_statusbar_create(scr);
    ui_fields_create(scr, fields, ARRAY_SIZE(fields));
    cell = ui_box(scr, UI_WIDTH / 2, ui_row_y(4), UI_WIDTH / 2, ui_rows_h(4, 1));
    lv_obj_set_style_border_color(cell, ui_col(UI_C_FG), 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_side(cell, LV_BORDER_SIDE_BOTTOM, 0);
    crs2_rr = ui_plot(cell, 0, 0, UI_WIDTH / 2, ui_rows_h(4, 1) - 1, rr_draw, NULL);
    crs2_turn = ui_plot(scr, 0, ui_row_y(5), UI_WIDTH, ui_rows_h(5, 2), turn_draw, NULL);
}

static void crs2_update(lv_obj_t *scr)
{
    (void)scr;
    ui_statusbar_update();
    ui_fields_update();
    lv_obj_invalidate(crs2_rr);
    lv_obj_invalidate(crs2_turn);
}

const ui_screen_ops_t ui_scr_crs2 = {crs2_create, crs2_update, NULL};

/* ==========================================================================
 * Page 3 (afficheSensors): inclination, history, heading, roughness
 * ========================================================================== */

static lv_obj_t *crs3_plot;

static void crs3_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_attitude_t *att = &ui_ctx.m.att;
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;
    char buf[16];

    lv_obj_get_coords(obj, &a);
    int32_t y0 = a.y1;

    /* rows 0-1: inclination value and the legacy pitch bar (140 px, +-24 %) */
    ui_draw_text(layer, a.x1 + 6, y0 + ui_row_y(0) - UI_BAR_H + 3, ui_txt(T_INCLINATION),
                 UI_FONT_LABEL, fg, LV_TEXT_ALIGN_LEFT);
    (void)ui_fmt_float(buf, sizeof(buf), att->pitch_pct, 1U);
    (void)strncat(buf, " %", sizeof(buf) - strlen(buf) - 1U);
    ui_draw_text(layer, a.x1 + (UI_WIDTH / 2), y0 + 22, buf, UI_FONT_VALUE_WIDE, fg,
                 LV_TEXT_ALIGN_CENTER);
    {
        int32_t by = y0 + ui_rows_h(0, 2) - 26;
        int32_t half = 70;
        float v = att->pitch_pct / 24.0f;

        if (v > 1.0f) {
            v = 1.0f;
        }
        if (v < -1.0f) {
            v = -1.0f;
        }
        int32_t len = (int32_t)lroundf(v * (float)half);
        int32_t cx = a.x1 + (UI_WIDTH / 2);

        if (len > 0) {
            ui_draw_fill(layer, cx, by + 2, len, 8, ui_col_fill(UI_C_BAD));
        } else if (len < 0) {
            ui_draw_fill(layer, cx + len, by + 2, -len, 8, ui_col_fill(UI_C_GOOD));
        }
        ui_draw_frame(layer, cx - half, by, 2 * half, 12, fg, 1);
        ui_draw_fill(layer, cx, by - 4, 1, 20, fg);
    }
    ui_draw_fill(layer, a.x1, y0 + ui_rows_h(0, 2) - 1, UI_WIDTH, 1, fg);

    /* rows 2-3: history of the inclination around a dashed zero line */
    {
        int32_t top = y0 + ui_rows_h(0, 2);
        int32_t h = ui_rows_h(2, 2);
        int32_t base = top + (h / 2) + 8;

        ui_draw_text(layer, a.x1 + 6, top + 3, ui_txt(T_SLOPE_HISTO), UI_FONT_LABEL, fg,
                     LV_TEXT_ALIGN_LEFT);
        ui_draw_dash(layer, a.x1 + 8, base, a.x1 + UI_WIDTH - 8, base, fg, 1, 4, 3);
        for (uint32_t i = 0U; (i < att->histo_n) && (i < UI_HISTO_MAX); i++) {
            int32_t v = (int32_t)att->histo[i] * 2;
            int32_t x = a.x1 + 12 + ((int32_t)i * 6);

            if (v > 36) {
                v = 36;
            }
            if (v < -36) {
                v = -36;
            }
            if (v > 0) {
                ui_draw_fill(layer, x, base - v, 4, v, ui_col_fill(UI_C_BAD));
            } else if (v < 0) {
                ui_draw_fill(layer, x, base + 1, 4, -v, ui_col_fill(UI_C_GOOD));
            }
        }
        ui_draw_fill(layer, a.x1, top + h - 1, UI_WIDTH, 1, fg);
    }

    /* rows 4-5: compass with the magnetic heading (legacy circle and needle) */
    {
        int32_t top = y0 + ui_rows_h(0, 4);
        int32_t h = ui_rows_h(4, 2);
        int32_t cx = a.x1 + 70;
        int32_t cy = top + (h / 2) + 2;
        int32_t r = 44;

        ui_draw_circle(layer, cx, cy, r, fg, 2);
        ui_draw_text(layer, cx, cy - r + 3, "N", UI_FONT_ITEM, ui_col(UI_C_BAD), LV_TEXT_ALIGN_CENTER);
        ui_draw_text(layer, a.x1 + 175, cy - 26, ui_txt(T_HEADING), UI_FONT_LABEL, fg,
                     LV_TEXT_ALIGN_CENTER);
        if (att->heading_deg >= 0) {
            int32_t tx, ty, lx, ly, rx, ry;

            ui_rotate(cx, cy, cx, cy - (r - 10), (float)att->heading_deg, &tx, &ty);
            ui_rotate(cx, cy, cx - 8, cy, (float)att->heading_deg, &lx, &ly);
            ui_rotate(cx, cy, cx + 8, cy, (float)att->heading_deg, &rx, &ry);
            ui_draw_triangle(layer, tx, ty, lx, ly, rx, ry, fg);
            (void)snprintf(buf, sizeof(buf), "%03d°", (int)att->heading_deg % 360);
        } else {
            (void)snprintf(buf, sizeof(buf), "---");
        }
        ui_draw_text(layer, a.x1 + 175, cy - 8, buf, UI_FONT_LARGE, fg, LV_TEXT_ALIGN_CENTER);
        ui_draw_fill(layer, a.x1, top + h - 1, UI_WIDTH, 1, fg);
    }

    /* row 6: roughness of the accelerometer axes and of the barometer, one
     * truncated decimal each (legacy afficheSensors) */
    {
        static const char *const names[UI_ROUGH_N] = {"X", "Y", "Z", "Baro"};
        int32_t top = y0 + ui_rows_h(0, 6);
        int32_t w = UI_WIDTH / (int32_t)UI_ROUGH_N;

        ui_draw_text(layer, a.x1 + 6, top + 3, ui_txt(T_ROUGHNESS), UI_FONT_LABEL, fg,
                     LV_TEXT_ALIGN_LEFT);
        for (int32_t i = 0; i < (int32_t)UI_ROUGH_N; i++) {
            int32_t cx = a.x1 + (w * i) + (w / 2);

            (void)ui_fmt_float(buf, sizeof(buf), att->rough[i], 1U);
            ui_draw_text(layer, cx, top + 17, names[i], UI_FONT_LABEL, fg, LV_TEXT_ALIGN_CENTER);
            ui_draw_text(layer, cx, top + 29, buf, UI_FONT_ITEM, fg, LV_TEXT_ALIGN_CENTER);
        }
    }
}

static void crs3_create(lv_obj_t *scr)
{
    ui_statusbar_create(scr);
    crs3_plot = ui_plot(scr, 0, UI_BAR_H, UI_WIDTH, UI_HEIGHT - UI_BAR_H, crs3_draw, NULL);
}

static void crs3_update(lv_obj_t *scr)
{
    (void)scr;
    ui_statusbar_update();
    lv_obj_invalidate(crs3_plot);
}

const ui_screen_ops_t ui_scr_crs3 = {crs3_create, crs3_update, NULL};
