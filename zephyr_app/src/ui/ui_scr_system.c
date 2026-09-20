/**
 * @file ui_scr_system.c
 * @brief Boot, energy, USB mode and shutdown screens
 *
 * The boot screen keeps the bicycle of the legacy splash
 * (legacy/drivers/lcd/ls027_splash.h) as a drawing; energy, USB mode and
 * shutdown are new (docs/18-interface-telas.md, Sistema).
 */

#include <stdio.h>
#include <string.h>

#include "ui_internal.h"

/* ==========================================================================
 * Boot
 * ========================================================================== */

static void bike_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;

    lv_obj_get_coords(obj, &a);
    int32_t ox = a.x1;
    int32_t oy = a.y1;

    /* wheels, frame, seat post, saddle and handlebar */
    ui_draw_circle(layer, ox + 70, oy + 190, 42, fg, 5);
    ui_draw_circle(layer, ox + 170, oy + 190, 42, fg, 5);
    ui_draw_line(layer, ox + 70, oy + 190, ox + 108, oy + 128, fg, 5);
    ui_draw_line(layer, ox + 108, oy + 128, ox + 150, oy + 128, fg, 5);
    ui_draw_line(layer, ox + 150, oy + 128, ox + 170, oy + 190, fg, 5);
    ui_draw_line(layer, ox + 108, oy + 128, ox + 122, oy + 190, fg, 5);
    ui_draw_line(layer, ox + 122, oy + 190, ox + 70, oy + 190, fg, 5);
    ui_draw_line(layer, ox + 122, oy + 190, ox + 150, oy + 128, fg, 5);
    ui_draw_line(layer, ox + 104, oy + 116, ox + 108, oy + 128, fg, 5);
    ui_draw_line(layer, ox + 96, oy + 112, ox + 118, oy + 112, fg, 5);
    ui_draw_line(layer, ox + 150, oy + 128, ox + 146, oy + 108, fg, 5);
    ui_draw_line(layer, ox + 146, oy + 108, ox + 162, oy + 104, fg, 5);
}

static void boot_create(lv_obj_t *scr)
{
    lv_obj_t *l;

    (void)ui_plot(scr, 0, 0, UI_WIDTH, UI_HEIGHT, bike_draw, NULL);
    l = ui_label(scr, UI_FONT_LARGE, ui_col(UI_C_FG), "GNSS Bike");
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 268);
    l = ui_label(scr, UI_FONT_LARGE, ui_col(UI_C_FG), "Computer");
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 296);
    l = ui_label(scr, UI_FONT_SMALL, ui_col(UI_C_NAV), ui_ctx.version);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 348);
}

static void no_update(lv_obj_t *scr)
{
    (void)scr;
}

static bool boot_key(ui_key_t key, ui_press_t press)
{
    (void)key;
    (void)press;
    return true;
}

const ui_screen_ops_t ui_scr_boot = {boot_create, no_update, boot_key};

/* ==========================================================================
 * Energy (new)
 * ========================================================================== */

static lv_obj_t *energy_plot;

static void energy_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    const ui_energy_t *en = &ui_ctx.m.energy;
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;
    char v[32];
    char n[12];

    lv_obj_get_coords(obj, &a);
    int32_t cx = (a.x1 + a.x2) / 2;
    uint8_t pct = (en->pct > 100U) ? 100U : en->pct;
    ui_role_t level = (pct > 50U) ? UI_C_GOOD : ((pct > 20U) ? UI_C_Z4 : UI_C_BAD);

    (void)snprintf(v, sizeof(v), "%u %%", (unsigned int)pct);
    ui_draw_text(layer, cx, a.y1 + 12, v, UI_FONT_BIG, fg, LV_TEXT_ALIGN_CENTER);

    /* battery gauge */
    int32_t gy = a.y1 + 72;

    ui_draw_frame(layer, a.x1 + 30, gy, 170, 26, fg, 2);
    ui_draw_fill(layer, a.x1 + 200, gy + 7, 6, 12, fg);
    ui_draw_fill(layer, a.x1 + 33, gy + 3, (164 * (int32_t)pct) / 100, 20,
                 (ui_ctx.theme == UI_THEME_MONO) ? fg : ui_col_fill(level));

    int32_t y = a.y1 + 124;
    const int32_t step = 30;

    (void)ui_fmt_float(n, sizeof(n), (float)en->mv / 1000.0f, 2U);
    (void)snprintf(v, sizeof(v), "%s V", n);
    ui_draw_text(layer, a.x1 + 10, y, ui_txt(T_VOLTAGE), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 10, y, v, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
    y += step;

    (void)snprintf(v, sizeof(v), "%d mA", (int)en->ma);
    ui_draw_text(layer, a.x1 + 10, y, ui_txt(T_CURRENT), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 10, y, v, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
    y += step;

    switch (en->source) {
    case UI_CHARGE_SOLAR:
        (void)snprintf(v, sizeof(v), "%s %u mW", ui_txt(T_SRC_SOLAR), (unsigned int)en->solar_mw);
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
    ui_draw_text(layer, a.x1 + 10, y, ui_txt(T_SOURCE), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 10, y, v, UI_FONT_SMALL_B,
                 ui_col((en->source == UI_CHARGE_NONE) ? UI_C_FG : UI_C_GOOD), LV_TEXT_ALIGN_RIGHT);
    y += step;

    (void)ui_fmt_float(n, sizeof(n), (float)en->solar_limit_mv / 1000.0f, 2U);
    (void)snprintf(v, sizeof(v), "%s V", n);
    ui_draw_text(layer, a.x1 + 10, y, ui_txt(T_SOLAR_LIMIT), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 10, y, v, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
    y += step;

    (void)snprintf(v, sizeof(v), "%d °C", (int)en->temp_c);
    ui_draw_text(layer, a.x1 + 10, y, ui_txt(T_TEMPERATURE), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 10, y, v, UI_FONT_SMALL_B, fg, LV_TEXT_ALIGN_RIGHT);
    y += step;

    (void)snprintf(v, sizeof(v), "%s %u h", ui_txt(T_ABOUT_H), (unsigned int)en->autonomy_h);
    ui_draw_text(layer, a.x1 + 10, y, ui_txt(T_AUTONOMY), UI_FONT_SMALL, fg, LV_TEXT_ALIGN_LEFT);
    ui_draw_text(layer, a.x2 - 10, y, v, UI_FONT_SMALL_B, ui_col(UI_C_NAV), LV_TEXT_ALIGN_RIGHT);
}

static void energy_create(lv_obj_t *scr)
{
    ui_statusbar_create(scr);
    ui_titlebar_create(scr, ui_txt(T_ENERGY));
    energy_plot = ui_plot(scr, 0, UI_BAR_H + 28, UI_WIDTH, UI_HEIGHT - UI_BAR_H - 28, energy_draw, NULL);
}

static void energy_update(lv_obj_t *scr)
{
    (void)scr;
    ui_statusbar_update();
    lv_obj_invalidate(energy_plot);
}

static bool back_to_settings(ui_key_t key, ui_press_t press)
{
    (void)key;
    (void)press;
    ui_go(UI_SCREEN_SETTINGS);
    return true;
}

const ui_screen_ops_t ui_scr_energy = {energy_create, energy_update, back_to_settings};

/* ==========================================================================
 * USB mode (MSC)
 * ========================================================================== */

static void usb_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;

    lv_obj_get_coords(obj, &a);
    /* computer and plug */
    ui_draw_frame(layer, a.x1 + 80, a.y1 + 90, 80, 60, fg, 4);
    ui_draw_fill(layer, a.x1 + 96, a.y1 + 150, 48, 8, fg);
    ui_draw_fill(layer, a.x1 + 112, a.y1 + 158, 16, 22, fg);
}

static void usb_create(lv_obj_t *scr)
{
    lv_obj_t *l;

    ui_statusbar_create(scr);
    (void)ui_plot(scr, 0, UI_BAR_H, UI_WIDTH, 200, usb_draw, NULL);
    l = ui_label(scr, UI_FONT_LARGE, ui_col(UI_C_FG), ui_txt(T_USB_MODE));
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 232);
    l = ui_label(scr, UI_FONT_SMALL, ui_col(UI_C_FG), ui_txt(T_USB_FILES));
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 266);
    l = ui_label(scr, UI_FONT_TITLE, ui_col(UI_C_BAD), ui_txt(T_USB_UNPLUG));
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 318);
}

static void usb_update(lv_obj_t *scr)
{
    (void)scr;
    ui_statusbar_update();
}

const ui_screen_ops_t ui_scr_usb = {usb_create, usb_update, boot_key};

/* ==========================================================================
 * Shutdown (new: the legacy switches off without a word)
 * ========================================================================== */

static lv_obj_t *shutdown_bar;

static void progress_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_area_t a;
    uint8_t pct = (ui_ctx.progress > 100U) ? 100U : ui_ctx.progress;

    lv_obj_get_coords(obj, &a);
    ui_draw_frame(layer, a.x1, a.y1, lv_area_get_width(&a), lv_area_get_height(&a), ui_col(UI_C_FG), 2);
    ui_draw_fill(layer, a.x1 + 3, a.y1 + 3, ((lv_area_get_width(&a) - 6) * (int32_t)pct) / 100,
                 lv_area_get_height(&a) - 6, ui_col(UI_C_NAV));
}

static void shutdown_create(lv_obj_t *scr)
{
    lv_obj_t *l;

    l = ui_label(scr, UI_FONT_LARGE, ui_col(UI_C_FG), ui_txt(T_SAVING));
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 160);
    l = ui_label(scr, UI_FONT_LARGE, ui_col(UI_C_FG), ui_txt(T_ACTIVITY));
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 188);
    shutdown_bar = ui_plot(scr, 40, 238, 160, 16, progress_draw, NULL);
    l = ui_label(scr, UI_FONT_TITLE, ui_col(UI_C_FG), ui_txt(T_SHUTTING_DOWN));
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 290);
}

static void shutdown_update(lv_obj_t *scr)
{
    (void)scr;
    lv_obj_invalidate(shutdown_bar);
}

const ui_screen_ops_t ui_scr_shutdown = {shutdown_create, shutdown_update, boot_key};

/* ==========================================================================
 * Update over the air (new: the legacy had no update)
 * ========================================================================== */

static void dfu_update(lv_obj_t *scr);

static lv_obj_t *dfu_bar;
static lv_obj_t *dfu_pct_label;
static lv_obj_t *dfu_state_label;

static void dfu_bar_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_area_t a;
    uint8_t pct = (ui_ctx.dfu_pct > 100U) ? 100U : ui_ctx.dfu_pct;

    lv_obj_get_coords(obj, &a);
    ui_draw_frame(layer, a.x1, a.y1, lv_area_get_width(&a), lv_area_get_height(&a),
                  ui_col(UI_C_FG), 2);
    ui_draw_fill(layer, a.x1 + 3, a.y1 + 3, ((lv_area_get_width(&a) - 6) * (int32_t)pct) / 100,
                 lv_area_get_height(&a) - 6, ui_col(UI_C_NAV));
}

static void dfu_icon_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_color_t fg = ui_col(UI_C_FG);
    lv_area_t a;

    lv_obj_get_coords(obj, &a);
    /* an arrow coming down into the device: the image on its way in */
    ui_draw_fill(layer, a.x1 + 108, a.y1 + 16, 24, 40, fg);
    ui_draw_triangle(layer, a.x1 + 88, a.y1 + 56, a.x1 + 152, a.y1 + 56, a.x1 + 120,
                     a.y1 + 88, fg);
    ui_draw_frame(layer, a.x1 + 70, a.y1 + 100, 100, 56, fg, 4);
}

static void dfu_create(lv_obj_t *scr)
{
    lv_obj_t *l;

    ui_statusbar_create(scr);
    (void)ui_plot(scr, 0, UI_BAR_H, UI_WIDTH, 176, dfu_icon_draw, NULL);
    l = ui_label(scr, UI_FONT_LARGE, ui_col(UI_C_FG), ui_txt(T_UPDATING));
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 214);
    dfu_bar = ui_plot(scr, 40, 252, 160, 16, dfu_bar_draw, NULL);
    dfu_pct_label = ui_label(scr, UI_FONT_LARGE, ui_col(UI_C_FG), "0%");
    /* the screen may come up in the middle of a transfer */
    lv_obj_align(dfu_pct_label, LV_ALIGN_TOP_MID, 0, 278);
    dfu_state_label = ui_label(scr, UI_FONT_TITLE, ui_col(UI_C_BAD), ui_txt(T_UPDATE_KEEP));
    lv_obj_align(dfu_state_label, LV_ALIGN_TOP_MID, 0, 320);
    dfu_update(scr);
}

static void dfu_update(lv_obj_t *scr)
{
    char pct[8];

    (void)scr;
    ui_statusbar_update();
    (void)snprintf(pct, sizeof(pct), "%u%%", (unsigned int)ui_ctx.dfu_pct);
    lv_label_set_text(dfu_pct_label, pct);

    switch (ui_ctx.dfu_phase) {
    case UI_DFU_DONE:
        lv_label_set_text(dfu_state_label, ui_txt(T_UPDATE_DONE));
        lv_obj_set_style_text_color(dfu_state_label, ui_col(UI_C_GOOD), 0);
        break;
    case UI_DFU_FAILED:
        lv_label_set_text(dfu_state_label, ui_txt(T_UPDATE_FAIL));
        lv_obj_set_style_text_color(dfu_state_label, ui_col(UI_C_BAD), 0);
        break;
    default:
        lv_label_set_text(dfu_state_label, ui_txt(T_UPDATE_KEEP));
        lv_obj_set_style_text_color(dfu_state_label, ui_col(UI_C_BAD), 0);
        break;
    }
    lv_obj_align(dfu_state_label, LV_ALIGN_TOP_MID, 0, 320);
    lv_obj_invalidate(dfu_bar);
}

const ui_screen_ops_t ui_scr_dfu = {dfu_create, dfu_update, boot_key};
