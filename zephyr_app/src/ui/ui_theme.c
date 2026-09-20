/**
 * @file ui_theme.c
 * @brief Colours of the two panels and the legacy grid
 *
 * The JDI LPM027M128C shows 8 colours (1 bit per channel); LVGL draws in
 * RGB565 with anti-aliasing off, so every pixel is one of the pure colours
 * below and the display driver keeps the top bit of each channel. The Sharp
 * LS027B7DH01A is black and white: the mono theme uses only those two.
 */

#include "ui_internal.h"

/* Pure panel colours (RGB888; LVGL converts them to RGB565 exactly) */
#define RGB_BLACK       0x000000U
#define RGB_WHITE       0xFFFFFFU
#define RGB_RED         0xFF0000U
#define RGB_GREEN       0x00FF00U
#define RGB_BLUE        0x0000FFU
#define RGB_YELLOW      0xFFFF00U
#define RGB_CYAN        0x00FFFFU
#define RGB_MAGENTA     0xFF00FFU

/** Colour theme, roles drawn on the white page */
static const uint32_t col_on_light[UI_C_ROLES] = {
    [UI_C_BG] = RGB_WHITE,
    [UI_C_FG] = RGB_BLACK,
    [UI_C_DARK] = RGB_BLACK,
    [UI_C_ON_DARK] = RGB_WHITE,
    [UI_C_GOOD] = RGB_GREEN,
    [UI_C_BAD] = RGB_RED,
    [UI_C_WARN] = RGB_BLACK,        /* yellow almost vanishes on white */
    [UI_C_NAV] = RGB_BLUE,
    [UI_C_RADIO] = RGB_BLUE,        /* cyan is too light on white */
    [UI_C_SEG] = RGB_MAGENTA,
    [UI_C_TITLE] = RGB_BLUE,
    [UI_C_Z1] = RGB_BLUE,
    [UI_C_Z2] = RGB_CYAN,
    [UI_C_Z3] = RGB_GREEN,
    [UI_C_Z4] = RGB_YELLOW,
    [UI_C_Z5] = RGB_MAGENTA,
    [UI_C_Z6] = RGB_RED,
    [UI_C_Z7] = RGB_BLACK,
};

/** Colour theme, roles drawn on a dark band */
static const uint32_t col_on_dark[UI_C_ROLES] = {
    [UI_C_BG] = RGB_BLACK,
    [UI_C_FG] = RGB_WHITE,
    [UI_C_DARK] = RGB_BLACK,
    [UI_C_ON_DARK] = RGB_WHITE,
    [UI_C_GOOD] = RGB_GREEN,
    [UI_C_BAD] = RGB_RED,
    [UI_C_WARN] = RGB_YELLOW,
    [UI_C_NAV] = RGB_CYAN,
    [UI_C_RADIO] = RGB_CYAN,
    [UI_C_SEG] = RGB_MAGENTA,
    [UI_C_TITLE] = RGB_WHITE,
    [UI_C_Z1] = RGB_BLUE,
    [UI_C_Z2] = RGB_CYAN,
    [UI_C_Z3] = RGB_GREEN,
    [UI_C_Z4] = RGB_YELLOW,
    [UI_C_Z5] = RGB_MAGENTA,
    [UI_C_Z6] = RGB_RED,
    [UI_C_Z7] = RGB_WHITE,
};

lv_color_t ui_col(ui_role_t role)
{
    if ((uint32_t)role >= (uint32_t)UI_C_ROLES) {
        role = UI_C_FG;
    }
    if (ui_ctx.theme == UI_THEME_MONO) {
        switch (role) {
        case UI_C_BG:
        case UI_C_ON_DARK:
            return lv_color_hex(RGB_WHITE);
        default:
            return lv_color_hex(RGB_BLACK);
        }
    }
    return lv_color_hex(col_on_light[role]);
}

lv_color_t ui_col_dark(ui_role_t role)
{
    if ((uint32_t)role >= (uint32_t)UI_C_ROLES) {
        role = UI_C_FG;
    }
    if (ui_ctx.theme == UI_THEME_MONO) {
        return (role == UI_C_DARK) ? lv_color_hex(RGB_BLACK) : lv_color_hex(RGB_WHITE);
    }
    return lv_color_hex(col_on_dark[role]);
}

lv_color_t ui_col_fill(ui_role_t role)
{
    /* yellow and cyan text vanish on white, but a filled dot or bar with a
     * black outline stays visible: only text keeps them on dark bands */
    if (ui_ctx.theme == UI_THEME_COLOR) {
        if (role == UI_C_WARN) {
            return lv_color_hex(RGB_YELLOW);
        }
        if (role == UI_C_RADIO) {
            return lv_color_hex(RGB_CYAN);
        }
    }
    return ui_col(role);
}

int32_t ui_row_y(int32_t row)
{
    if (row < 0) {
        row = 0;
    }
    if (row > UI_ROWS) {
        row = UI_ROWS;
    }
    /* 7 rows share the 380 pixels under the status bar: 54 or 55 each */
    return UI_BAR_H + ((row * (UI_HEIGHT - UI_BAR_H)) / UI_ROWS);
}

int32_t ui_rows_h(int32_t row, int32_t nrows)
{
    return ui_row_y(row + nrows) - ui_row_y(row);
}
