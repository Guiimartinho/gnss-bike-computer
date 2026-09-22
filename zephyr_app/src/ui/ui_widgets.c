/**
 * @file ui_widgets.c
 * @brief Building blocks of the screens: status bar, cadran, bands, lists
 *
 * The cadran follows legacy/source/vue/Vue.cpp (Vue::cadran, cadranH): label
 * at the top left, unit at the top right, big value in the middle, and a
 * 1-pixel grid between cells. Plots draw in LV_EVENT_DRAW_MAIN with the
 * helpers at the end of this file; anti-aliasing is off on the display, so
 * every pixel is a pure panel colour.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui_internal.h"

/* ==========================================================================
 * Basic objects
 * ========================================================================== */

lv_obj_t *ui_box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *o = lv_obj_create(parent);

    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);

    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, (text != NULL) ? text : "");
    return l;
}

lv_obj_t *ui_plot(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                  lv_event_cb_t draw_cb, void *user)
{
    lv_obj_t *o = ui_box(parent, x, y, w, h);

    lv_obj_add_event_cb(o, draw_cb, LV_EVENT_DRAW_MAIN, user);
    return o;
}

/* ==========================================================================
 * Status bar
 * ========================================================================== */

typedef struct {
    lv_obj_t *time;
    lv_obj_t *gps;
    lv_obj_t *ant;
    lv_obj_t *radar;
    lv_obj_t *ble;
    lv_obj_t *pct;
    lv_obj_t *icons;
} ui_bar_t;

static ui_bar_t bar;

/** Recording dot, sun or plug, and the battery gauge */
static void bar_icons_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_status_t *st = &ui_ctx.m.status;
    lv_area_t a;

    lv_obj_get_coords(obj, &a);
    /* The icon box starts at x = 148 of the bar */
    int32_t ox = a.x1 - 148;
    int32_t oy = a.y1;

    if (st->recording) {
        ui_draw_disc(layer, ox + 153, oy + 10, 4, ui_col_dark(UI_C_BAD));
    }
    /*
     * The rear radar, on every page: a dot coloured by the worst vehicle
     * behind. The strip with each one at its distance only goes on the
     * pages whose right edge is a drawing and not a column of units
     * (ui_radar_strip_create).
     */
    if (ui_ctx.m.radar.linked && (ui_ctx.m.radar.n > 0U)) {
        ui_role_t role = UI_C_RADIO;

        if (ui_ctx.m.radar.worst == (uint8_t)RADAR_LEVEL_DANGER) {
            role = UI_C_BAD;
        } else if (ui_ctx.m.radar.worst == (uint8_t)RADAR_LEVEL_FAST) {
            role = UI_C_WARN;
        }
        ui_draw_disc(layer, ox + 210, oy + 10, 4, ui_col_dark(role));
    }
    if (st->charge == UI_CHARGE_SOLAR) {
        lv_color_t sun = ui_col_dark(UI_C_WARN);

        ui_draw_disc(layer, ox + 168, oy + 10, 3, sun);
        for (int32_t ang = 0; ang < 360; ang += 45) {
            float r = (float)ang * 0.0174533f;
            int32_t x1 = ox + 168 + (int32_t)lroundf(5.0f * cosf(r));
            int32_t y1 = oy + 10 + (int32_t)lroundf(5.0f * sinf(r));
            int32_t x2 = ox + 168 + (int32_t)lroundf(7.0f * cosf(r));
            int32_t y2 = oy + 10 + (int32_t)lroundf(7.0f * sinf(r));

            ui_draw_line(layer, x1, y1, x2, y2, sun, 1);
        }
    } else if ((st->charge == UI_CHARGE_USB) || (st->charge == UI_CHARGE_USB_FULL)) {
        lv_color_t plug = ui_col_dark(UI_C_WARN);

        ui_draw_fill(layer, ox + 161, oy + 7, 11, 6, plug);
        ui_draw_fill(layer, ox + 172, oy + 8, 3, 1, plug);
        ui_draw_fill(layer, ox + 172, oy + 11, 3, 1, plug);
    }

    /* Battery: outline, tip and a fill coloured by the level (docs/18) */
    lv_color_t white = ui_col_dark(UI_C_ON_DARK);
    uint8_t pct = (st->batt_pct > 100U) ? 100U : st->batt_pct;
    ui_role_t level = (pct > 50U) ? UI_C_GOOD : ((pct > 20U) ? UI_C_WARN : UI_C_BAD);
    int32_t fill_w = (20 * (int32_t)pct) / 100;

    ui_draw_frame(layer, ox + 180, oy + 5, 24, 10, white, 1);
    ui_draw_fill(layer, ox + 204, oy + 8, 2, 4, white);
    if (fill_w > 0) {
        ui_draw_fill(layer, ox + 182, oy + 7, fill_w, 6, ui_col_dark(level));
    }
}

/**
 * The strip of the rear radar, down the right edge of a data page.
 *
 * Garmin puts it there and a rider reads it without thinking: the bottom
 * is the bike, the top is as far as the radar reaches, and each vehicle is
 * a mark at its distance, coloured by how much of a threat it is. A mark
 * that stopped being reported is drawn hollow instead of vanishing, which
 * is what `model/radar.h` calls fading.
 */
static void radar_strip_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_radar_t *r = &ui_ctx.m.radar;
    lv_area_t a;

    if (!r->linked) {
        return;
    }

    lv_obj_get_coords(obj, &a);

    int32_t w = lv_area_get_width(&a);
    int32_t h = lv_area_get_height(&a);

    /* the lane: a thin line the whole height, with the bike at the bottom */
    ui_draw_fill(layer, a.x1 + (w / 2) - 1, a.y1, 2, h, ui_col(UI_C_NAV));
    ui_draw_disc(layer, a.x1 + (w / 2), a.y2 - 3, 3, ui_col(UI_C_FG));

    for (uint8_t i = 0U; (i < r->n) && (i < 8U); i++) {
        uint32_t range = r->range_m[i];

        if (range > RADAR_RANGE_MAX_M) {
            range = RADAR_RANGE_MAX_M;
        }

        /* far away is at the top, right behind is at the bottom */
        int32_t y = a.y2 - 8 - (int32_t)((range * (uint32_t)(h - 14)) / RADAR_RANGE_MAX_M);
        ui_role_t role = UI_C_NAV;

        if (r->level[i] == (uint8_t)RADAR_LEVEL_DANGER) {
            role = UI_C_BAD;
        } else if (r->level[i] == (uint8_t)RADAR_LEVEL_FAST) {
            role = UI_C_WARN;
        }

        if (r->live[i]) {
            ui_draw_disc(layer, a.x1 + (w / 2), y, 5, ui_col_fill(role));
        } else {
            /* fading: the outline only, so it does not blink out */
            ui_draw_circle(layer, a.x1 + (w / 2), y, 5, ui_col_fill(role), 2);
        }
    }
}

void ui_statusbar_create(lv_obj_t *scr)
{
    lv_obj_t *b = ui_box(scr, 0, 0, UI_WIDTH, UI_BAR_H);

    lv_obj_set_style_bg_color(b, ui_col(UI_C_DARK), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);

    bar.time = ui_label(b, UI_FONT_ITEM, ui_col_dark(UI_C_ON_DARK), "--:--");
    lv_obj_set_pos(bar.time, 3, 1);
    bar.gps = ui_label(b, UI_FONT_SMALL_B, ui_col_dark(UI_C_GOOD), "GPS");
    lv_obj_set_pos(bar.gps, 51, 3);
    bar.ant = ui_label(b, UI_FONT_SMALL_B, ui_col_dark(UI_C_RADIO), "ANT+");
    lv_obj_set_pos(bar.ant, 82, 3);
    bar.ble = ui_label(b, UI_FONT_SMALL_B, ui_col_dark(UI_C_RADIO), "BLE");
    lv_obj_set_pos(bar.ble, 122, 3);
    bar.icons = ui_plot(b, 148, 0, 60, UI_BAR_H, bar_icons_draw, NULL);
    bar.pct = ui_label(b, UI_FONT_SMALL_B, ui_col_dark(UI_C_ON_DARK), "");
    lv_obj_align(bar.pct, LV_ALIGN_TOP_RIGHT, -2, 3);

    ui_statusbar_update();
}

void ui_radar_strip_create(lv_obj_t *parent, int32_t y, int32_t h)
{
    /*
     * Only over a drawing: on the data pages the units sit at the right
     * edge and a strip would cover them, so those pages get the dot in the
     * status bar and nothing else. The caller gives the band the strip may
     * use, because only it knows where its drawing is.
     */
    bar.radar = ui_plot(parent, UI_WIDTH - UI_RADAR_W, y, UI_RADAR_W, h, radar_strip_draw, NULL);
    lv_obj_move_foreground(bar.radar);
}

void ui_statusbar_update(void)
{
    const ui_status_t *st = &ui_ctx.m.status;
    char buf[8];

    if (bar.time == NULL) {
        return;
    }
    if (bar.radar != NULL) {
        lv_obj_invalidate(bar.radar);
    }
    lv_label_set_text(bar.time, ui_fmt_hm(buf, sizeof(buf), st->time_s));

    if (st->gnss == UI_GNSS_OFF) {
        lv_obj_add_flag(bar.gps, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(bar.gps, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(bar.gps, ui_col_dark((st->gnss == UI_GNSS_FIX) ? UI_C_GOOD : UI_C_WARN), 0);
    }
    if (st->ant_link) {
        lv_obj_remove_flag(bar.ant, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(bar.ant, LV_OBJ_FLAG_HIDDEN);
    }
    if (st->ble_link) {
        lv_obj_remove_flag(bar.ble, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(bar.ble, LV_OBJ_FLAG_HIDDEN);
    }
    (void)snprintf(buf, sizeof(buf), "%u", (unsigned int)((st->batt_pct > 100U) ? 100U : st->batt_pct));
    lv_label_set_text(bar.pct, buf);
    lv_obj_invalidate(bar.icons);
}

/** Called when a screen is deleted: the bar objects go with it */
void ui_statusbar_forget(void)
{
    (void)memset(&bar, 0, sizeof(bar));
}

void ui_titlebar_create(lv_obj_t *scr, const char *title)
{
    lv_obj_t *t = ui_box(scr, 0, UI_BAR_H, UI_WIDTH, 28);
    lv_obj_t *l;

    lv_obj_set_style_bg_color(t, ui_col(UI_C_TITLE), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    l = ui_label(t, UI_FONT_TITLE, ui_col_dark(UI_C_ON_DARK), title);
    lv_obj_center(l);
}

/* ==========================================================================
 * Cadran and bands
 * ========================================================================== */

void ui_field_create(ui_field_t *f, lv_obj_t *parent, int32_t col, int32_t row, int32_t span,
                     const char *label, const char *unit, ui_role_t label_role)
{
    int32_t x = col * (UI_WIDTH / 2);
    int32_t w = (UI_WIDTH / 2) * span;
    int32_t y = ui_row_y(row);
    int32_t h = ui_rows_h(row, 1);
    lv_border_side_t sides = LV_BORDER_SIDE_BOTTOM;
    lv_obj_t *l;

    f->box = ui_box(parent, x, y, w, h);
    if ((col == 0) && (span == 1)) {
        sides |= LV_BORDER_SIDE_RIGHT;
    }
    lv_obj_set_style_border_color(f->box, ui_col(UI_C_FG), 0);
    lv_obj_set_style_border_width(f->box, 1, 0);
    lv_obj_set_style_border_side(f->box, sides, 0);

    if ((label != NULL) && (label[0] != '\0')) {
        l = ui_label(f->box, UI_FONT_LABEL, ui_col(label_role), label);
        lv_obj_set_pos(l, 4, 2);
    }
    if ((unit != NULL) && (unit[0] != '\0')) {
        l = ui_label(f->box, UI_FONT_LABEL, ui_col(UI_C_FG), unit);
        lv_obj_align(l, LV_ALIGN_TOP_RIGHT, (span == 1) && (col == 0) ? -5 : -4, 2);
    }
    f->value = ui_label(f->box, (span == 1) ? UI_FONT_VALUE : UI_FONT_VALUE_WIDE, ui_col(UI_C_FG), "");
    lv_obj_align(f->value, LV_ALIGN_BOTTOM_MID, 0, -1);
    f->max_len = (span == 1) ? 6U : 9U;
}

void ui_field_set(ui_field_t *f, const char *value, ui_role_t value_role)
{
    if ((f == NULL) || (f->value == NULL)) {
        return;
    }
    lv_label_set_text(f->value, ui_fmt_cadran(value, f->max_len));
    lv_obj_set_style_text_color(f->value, ui_col(value_role), 0);
}

lv_obj_t *ui_band(lv_obj_t *parent, int32_t row, int32_t nrows)
{
    lv_obj_t *b = ui_box(parent, 0, ui_row_y(row), UI_WIDTH, ui_rows_h(row, nrows));

    lv_obj_set_style_border_color(b, ui_col(UI_C_FG), 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_side(b, LV_BORDER_SIDE_BOTTOM, 0);
    return b;
}

/* ==========================================================================
 * Lists (legacy MenuPageItems)
 * ========================================================================== */

#define UI_LIST_MAX     12

typedef struct {
    lv_obj_t *row;
    lv_obj_t *text;
    lv_obj_t *right;
    lv_obj_t *sub;
    lv_obj_t *dot;
    ui_role_t role;
} ui_list_row_t;

static ui_list_row_t list_rows[UI_LIST_MAX];
static int32_t list_n;
static int32_t list_sel;

static void list_style(int32_t i, bool is_sel)
{
    ui_list_row_t *r = &list_rows[i];

    if (is_sel) {
        /* legacy MenuItem::render(): inverted round rect over the item */
        lv_obj_set_style_bg_color(r->row, ui_col(UI_C_DARK), 0);
        lv_obj_set_style_bg_opa(r->row, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_opa(r->row, LV_OPA_TRANSP, 0);
    }
    lv_obj_set_style_text_color(r->text, is_sel ? ui_col_dark(UI_C_ON_DARK) : ui_col(r->role), 0);
    if (r->right != NULL) {
        lv_obj_set_style_text_color(r->right, is_sel ? ui_col_dark(UI_C_ON_DARK) : ui_col(UI_C_FG), 0);
    }
    if (r->sub != NULL) {
        lv_obj_set_style_text_color(r->sub, is_sel ? ui_col_dark(UI_C_ON_DARK) : ui_col(UI_C_FG), 0);
    }
}

void ui_list_create(lv_obj_t *scr, const ui_list_item_t *items, int32_t n, int32_t sel,
                    int32_t top, int32_t step)
{
    list_n = (n > UI_LIST_MAX) ? UI_LIST_MAX : n;
    list_sel = sel;
    for (int32_t i = 0; i < list_n; i++) {
        ui_list_row_t *r = &list_rows[i];

        int32_t tx = 10;

        r->role = items[i].role;
        r->row = ui_box(scr, 6, top + (i * step), UI_WIDTH - 12, step - 4);
        lv_obj_set_style_radius(r->row, 6, 0);
        r->sub = NULL;
        r->dot = NULL;
        if (items[i].dot < UI_C_ROLES) {
            r->dot = ui_box(r->row, 6, ((step - 4) / 2) - 6, 12, 12);
            lv_obj_set_style_radius(r->dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(r->dot, ui_col_fill(items[i].dot), 0);
            lv_obj_set_style_bg_opa(r->dot, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(r->dot, ui_col(UI_C_FG), 0);
            lv_obj_set_style_border_width(r->dot, 1, 0);
            tx = 26;
        }
        r->text = ui_label(r->row, UI_FONT_ITEM, ui_col(r->role), items[i].text);
        if (items[i].sub != NULL) {
            lv_obj_set_pos(r->text, tx, 3);
            r->sub = ui_label(r->row, UI_FONT_LABEL, ui_col(UI_C_FG), items[i].sub);
            lv_obj_set_pos(r->sub, tx, 22);
        } else {
            lv_obj_align(r->text, LV_ALIGN_LEFT_MID, tx, 0);
        }
        r->right = NULL;
        if (items[i].right != NULL) {
            r->right = ui_label(r->row, UI_FONT_SMALL, ui_col(UI_C_FG), items[i].right);
            lv_obj_align(r->right, LV_ALIGN_RIGHT_MID, -8, 0);
        }
        list_style(i, i == sel);
    }
}

void ui_list_select(int32_t sel)
{
    if ((sel == list_sel) || (sel < 0) || (sel >= list_n)) {
        return;
    }
    if ((list_sel >= 0) && (list_sel < list_n)) {
        list_style(list_sel, false);
    }
    list_style(sel, true);
    list_sel = sel;
}

void ui_list_set_right(int32_t i, const char *text)
{
    if ((i >= 0) && (i < list_n) && (list_rows[i].right != NULL)) {
        lv_label_set_text(list_rows[i].right, text);
    }
}

void ui_list_forget(void)
{
    (void)memset(list_rows, 0, sizeof(list_rows));
    list_n = 0;
    list_sel = -1;
}

/* ==========================================================================
 * Drawing helpers
 * ========================================================================== */

void ui_draw_line(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                  lv_color_t color, int32_t width)
{
    lv_draw_line_dsc_t d;

    lv_draw_line_dsc_init(&d);
    d.color = color;
    d.width = width;
    d.opa = LV_OPA_COVER;
    d.p1.x = x1;
    d.p1.y = y1;
    d.p2.x = x2;
    d.p2.y = y2;
    d.round_start = (width > 2) ? 1U : 0U;
    d.round_end = (width > 2) ? 1U : 0U;
    lv_draw_line(layer, &d);
}

void ui_draw_dash(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                  lv_color_t color, int32_t width, int32_t dash, int32_t gap)
{
    lv_draw_line_dsc_t d;

    lv_draw_line_dsc_init(&d);
    d.color = color;
    d.width = width;
    d.opa = LV_OPA_COVER;
    d.p1.x = x1;
    d.p1.y = y1;
    d.p2.x = x2;
    d.p2.y = y2;
    d.dash_width = dash;
    d.dash_gap = gap;
    lv_draw_line(layer, &d);
}

void ui_draw_fill(lv_layer_t *layer, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t color)
{
    lv_draw_rect_dsc_t r;
    lv_area_t a;

    if ((w <= 0) || (h <= 0)) {
        return;
    }
    lv_draw_rect_dsc_init(&r);
    r.bg_color = color;
    r.bg_opa = LV_OPA_COVER;
    r.border_width = 0;
    r.radius = 0;
    a.x1 = x;
    a.y1 = y;
    a.x2 = x + w - 1;
    a.y2 = y + h - 1;
    lv_draw_rect(layer, &r, &a);
}

void ui_draw_frame(lv_layer_t *layer, int32_t x, int32_t y, int32_t w, int32_t h,
                   lv_color_t color, int32_t width)
{
    lv_draw_rect_dsc_t r;
    lv_area_t a;

    if ((w <= 0) || (h <= 0)) {
        return;
    }
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_TRANSP;
    r.border_color = color;
    r.border_width = width;
    r.border_opa = LV_OPA_COVER;
    r.radius = 0;
    a.x1 = x;
    a.y1 = y;
    a.x2 = x + w - 1;
    a.y2 = y + h - 1;
    lv_draw_rect(layer, &r, &a);
}

void ui_draw_circle(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t r, lv_color_t color,
                    int32_t width)
{
    lv_draw_rect_dsc_t d;
    lv_area_t a;

    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.border_color = color;
    d.border_width = width;
    d.border_opa = LV_OPA_COVER;
    d.radius = LV_RADIUS_CIRCLE;
    a.x1 = cx - r;
    a.y1 = cy - r;
    a.x2 = cx + r;
    a.y2 = cy + r;
    lv_draw_rect(layer, &d, &a);
}

void ui_draw_disc(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t r, lv_color_t color)
{
    lv_draw_rect_dsc_t d;
    lv_area_t a;

    lv_draw_rect_dsc_init(&d);
    d.bg_color = color;
    d.bg_opa = LV_OPA_COVER;
    d.border_width = 0;
    d.radius = LV_RADIUS_CIRCLE;
    a.x1 = cx - r;
    a.y1 = cy - r;
    a.x2 = cx + r;
    a.y2 = cy + r;
    lv_draw_rect(layer, &d, &a);
}

void ui_draw_triangle(lv_layer_t *layer, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                      int32_t x2, int32_t y2, lv_color_t color)
{
    lv_draw_triangle_dsc_t t;

    lv_draw_triangle_dsc_init(&t);
    t.color = color;
    t.opa = LV_OPA_COVER;
    t.p[0].x = x0;
    t.p[0].y = y0;
    t.p[1].x = x1;
    t.p[1].y = y1;
    t.p[2].x = x2;
    t.p[2].y = y2;
    lv_draw_triangle(layer, &t);
}

void ui_draw_text(lv_layer_t *layer, int32_t x, int32_t y, const char *text, const lv_font_t *font,
                  lv_color_t color, lv_text_align_t align)
{
    lv_draw_label_dsc_t d;
    lv_area_t a;

    if ((text == NULL) || (text[0] == '\0')) {
        return;
    }
    lv_draw_label_dsc_init(&d);
    d.text = text;
    d.text_local = 1U;      /* the caller's buffer may not live until the draw */
    d.font = font;
    d.color = color;
    d.opa = LV_OPA_COVER;
    d.align = align;
    a.y1 = y;
    a.y2 = y + font->line_height - 1;
    if (align == LV_TEXT_ALIGN_CENTER) {
        a.x1 = x - UI_WIDTH;
        a.x2 = x + UI_WIDTH - 1;
    } else if (align == LV_TEXT_ALIGN_RIGHT) {
        a.x1 = x - (2 * UI_WIDTH);
        a.x2 = x - 1;
    } else {
        a.x1 = x;
        a.x2 = x + (2 * UI_WIDTH) - 1;
    }
    lv_draw_label(layer, &d, &a);
}

void ui_rotate(int32_t cx, int32_t cy, int32_t x, int32_t y, float deg, int32_t *xo, int32_t *yo)
{
    /* legacy rotate_point(): positive clockwise on the screen (y down) */
    float r = deg * 0.0174532925f;
    float dx = (float)(x - cx);
    float dy = (float)(y - cy);

    *xo = cx + (int32_t)lroundf((dx * cosf(r)) - (dy * sinf(r)));
    *yo = cy + (int32_t)lroundf((dx * sinf(r)) + (dy * cosf(r)));
}

void ui_draw_rider(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t course_deg, lv_color_t color)
{
    if (course_deg < 0) {
        ui_draw_disc(layer, cx, cy, 4, color);
        return;
    }
    /* legacy VueCRS::afficheSegment(): (0,-15), (5,5), (-5,5) rotated by the course */
    int32_t x0, y0, x1, y1, x2, y2;

    ui_rotate(cx, cy, cx, cy - 15, (float)course_deg, &x0, &y0);
    ui_rotate(cx, cy, cx + 6, cy + 6, (float)course_deg, &x1, &y1);
    ui_rotate(cx, cy, cx - 6, cy + 6, (float)course_deg, &x2, &y2);
    ui_draw_triangle(layer, x0, y0, x1, y1, x2, y2, color);
}
