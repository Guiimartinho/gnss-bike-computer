/**
 * @file ui_internal.h
 * @brief Shared declarations of the user interface modules
 */

#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

#include "lvgl.h"
#include "model/climb.h"
#include "ui/ui.h"
#include "ui/ui_fmt.h"
#include "fonts/ui_fonts.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))
#endif

/* ==========================================================================
 * Colours
 * ========================================================================== */

/**
 * Colour roles. The colour theme maps them to the 8 panel colours; the mono
 * theme maps every role on white to black and every role on black to white,
 * so no information depends on colour alone (docs/18, Paleta).
 */
typedef enum {
    UI_C_BG = 0,        /**< page background: white */
    UI_C_FG,            /**< text and lines: black */
    UI_C_DARK,          /**< status bar, notification, selection: black */
    UI_C_ON_DARK,       /**< text on dark: white */
    UI_C_GOOD,          /**< green: ahead, fix, connected, solar, done route */
    UI_C_BAD,           /**< red: behind, alert, recording, destructive */
    UI_C_WARN,          /**< yellow, only on dark: searching, sun, notification title */
    UI_C_NAV,           /**< blue: route to ride, navigation, titles */
    UI_C_RADIO,         /**< cyan on dark, blue on white: ANT+ and BLE */
    UI_C_SEG,           /**< magenta: segments */
    UI_C_TITLE,         /**< title bar: blue, black in mono */
    UI_C_Z1,            /**< power zones 1..7 */
    UI_C_Z2,
    UI_C_Z3,
    UI_C_Z4,
    UI_C_Z5,
    UI_C_Z6,
    UI_C_Z7,
    UI_C_ROLES
} ui_role_t;

/** Colour of a role drawn on the white page */
lv_color_t ui_col(ui_role_t role);

/** Colour of a role drawn on a dark band (status bar, notification, selection) */
lv_color_t ui_col_dark(ui_role_t role);

/** Filled shapes (bars, dots): in mono every fill is black on white */
lv_color_t ui_col_fill(ui_role_t role);

/* ==========================================================================
 * Grid and fonts
 * ========================================================================== */

/** Top of grid row r (0..UI_ROWS) under the status bar */
int32_t ui_row_y(int32_t row);

/** Height of nrows rows starting at row */
int32_t ui_rows_h(int32_t row, int32_t nrows);

#define UI_FONT_LABEL       (&ui_font_r10)
#define UI_FONT_SMALL       (&ui_font_r12)
#define UI_FONT_SMALL_B     (&ui_font_b12)
#define UI_FONT_ITEM        (&ui_font_b14)
#define UI_FONT_TITLE       (&ui_font_b16)
#define UI_FONT_MEDIUM      (&ui_font_b18)
#define UI_FONT_LARGE       (&ui_font_b22)
#define UI_FONT_VALUE       (&ui_font_b28)
#define UI_FONT_VALUE_WIDE  (&ui_font_b32)
#define UI_FONT_BIG         (&ui_font_b44)
#define UI_FONT_HUGE        (&ui_font_b64)

/* ==========================================================================
 * Texts
 * ========================================================================== */

typedef enum {
    T_DIST = 0, T_PWR, T_SPEED, T_CLIMB, T_CAD, T_HR, T_SLOPE, T_VA, T_NEXT_SEG,
    T_AVG, T_SCORE, T_SOLAR, T_BATT, T_PR, T_NEXT_TURN, T_RR, T_INCLINATION,
    T_SLOPE_HISTO, T_HEADING, T_ROUGHNESS, T_TIME, T_PZONE, T_VECTOR,
    T_CONNECTING, T_SEARCHING_SATS, T_SATS_IN_USE, T_MODE, T_LAST_FIX,
    T_GNSS_BACKUP, T_GNSS_ACQ, T_GNSS_LEAP, T_GNSS_FULL,
    T_SEGMENTS_LOADED, T_BACK, T_MODE_FEC, T_MODE_CRS, T_MODE_PRC, T_MODE_ZWIFT,
    T_MODE_DBG, T_SETTINGS, T_SHUTDOWN, T_MENU, T_SENSORS, T_FTP, T_WEIGHT,
    T_CAL_COMPASS, T_SCREEN_LIGHT, T_GNSS, T_ENERGY, T_FORMAT, T_PAIR,
    T_CANCEL, T_CENTER_PAIRS, T_SAVE, T_LIGHT_AUTO, T_LIGHT_OFF, T_THEME_COLOR,
    T_THEME_MONO, T_FORMAT_Q, T_FORMAT_YES, T_VOLTAGE, T_CURRENT, T_SOURCE,
    T_SOLAR_LIMIT, T_TEMPERATURE, T_AUTONOMY, T_SRC_NONE, T_SRC_SOLAR,
    T_SRC_USB, T_SRC_USB_FULL, T_USB_MODE, T_USB_FILES, T_USB_UNPLUG,
    T_SAVING, T_ACTIVITY, T_SHUTTING_DOWN, T_UPDATING, T_UPDATE_KEEP, T_UPDATE_DONE,
    T_UPDATE_FAIL, T_PROFILE, T_CLIMB_LEFT, T_NO_PROFILE, T_S_HR, T_S_BSC, T_S_POWER, T_S_FEC,
    T_S_RADAR, T_S_LIGHT, T_L_NONE, T_L_CONNECTED, T_L_LOST, T_L_SEARCH,
    T_NO_SENSOR, T_FIX, T_SATELLITES, T_POS_AGE, T_ACCURACY, T_BATTERY,
    T_CHARGE, T_VERSION, T_NO_ROUTE, T_ABOUT_H, T_REMAIN, T_LIGHT, T_SCREEN,
    T_AUTO, T_OFF, T_COLOURS, T_BW, T_SEARCHING, T_KG, T_W, T_SEGMENTS,
    T_ALT, T_ELAPSED, T_ROUTES, T_ERROR, T_NO_ROUTES,
    T_LAP, T_LAPS, T_MOVING, T_PAUSED, T_DESCENT, T_KCAL,
    T_CLIMB_N, T_TO_TOP, T_GRADE, T_NEXT_M, T_NO_CLIMB, T_HC,
    T_COUNT
} ui_text_t;

/** Text in the current language */
const char *ui_txt(ui_text_t id);

/** Name of a sensor kind */
const char *ui_sensor_name(uint8_t kind);

/* ==========================================================================
 * Interface state shared by the screens
 * ========================================================================== */

typedef struct {
    ui_theme_t theme;
    ui_lang_t lang;
    ui_mode_t mode;
    ui_model_t m;           /**< last snapshot */
    bool have_model;
    uint32_t now_ms;
    uint32_t boot_ms;
    uint8_t progress;       /**< shutdown progress */
    uint8_t dfu_pct;        /**< update over the air, 0 to 100 */
    uint8_t dfu_phase;      /**< enum dfu_phase, as the radio publishes it */
    int32_t sel;            /**< selected item of the list on show */
    int32_t value;          /**< value being edited */
    ui_text_t value_title;  /**< FTP or weight */
    uint8_t zoom;           /**< PRC zoom level shown in the corner (1..5) */
    char version[16];       /**< firmware version (boot and DBG) */
} ui_ctx_t;

extern ui_ctx_t ui_ctx;

/** Report an action to the application */
void ui_action(ui_action_t action, int32_t arg);

/* ==========================================================================
 * Screens
 * ========================================================================== */

typedef struct {
    /** Build the objects on the new screen */
    void (*create)(lv_obj_t *scr);
    /** New snapshot: refresh texts and invalidate plots */
    void (*update)(lv_obj_t *scr);
    /** Key on this screen; returns false to let the core handle it */
    bool (*key)(ui_key_t key, ui_press_t press);
} ui_screen_ops_t;

extern const ui_screen_ops_t ui_scr_boot;
extern const ui_screen_ops_t ui_scr_crs1;
extern const ui_screen_ops_t ui_scr_crs2;
extern const ui_screen_ops_t ui_scr_crs3;
extern const ui_screen_ops_t ui_scr_prc;
extern const ui_screen_ops_t ui_scr_fec;
extern const ui_screen_ops_t ui_scr_gps;
extern const ui_screen_ops_t ui_scr_dbg;
extern const ui_screen_ops_t ui_scr_menu;
extern const ui_screen_ops_t ui_scr_settings;
extern const ui_screen_ops_t ui_scr_sensors;
extern const ui_screen_ops_t ui_scr_pair;
extern const ui_screen_ops_t ui_scr_value;
extern const ui_screen_ops_t ui_scr_light;
extern const ui_screen_ops_t ui_scr_confirm;
extern const ui_screen_ops_t ui_scr_energy;
extern const ui_screen_ops_t ui_scr_usb;
extern const ui_screen_ops_t ui_scr_shutdown;
extern const ui_screen_ops_t ui_scr_routes;
extern const ui_screen_ops_t ui_scr_dfu;
extern const ui_screen_ops_t ui_scr_profile;
extern const ui_screen_ops_t ui_scr_lap;
extern const ui_screen_ops_t ui_scr_climb;

/** Go to another screen (rebuilds it) */
void ui_go(ui_screen_t screen);

/** Rebuild the screen on show (layout depends on the data) */
void ui_rebuild(void);

/** The data page of the current mode (CRS page 1, PRC, FEC, DBG) */
ui_screen_t ui_mode_page(void);

/* ==========================================================================
 * Widgets
 * ========================================================================== */

/** Plain box without scrolling, padding or radius */
lv_obj_t *ui_box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h);

/** Label with a font and a colour */
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);

/** Status bar on top of a data screen */
void ui_statusbar_create(lv_obj_t *scr);
void ui_statusbar_update(void);
/** Forget the objects of the deleted screen */
void ui_statusbar_forget(void);

/** Title bar under the status bar (menus) */
void ui_titlebar_create(lv_obj_t *scr, const char *title);

/** Legacy cadran: label on the left, unit on the right, value in the middle */
typedef struct {
    lv_obj_t *box;
    lv_obj_t *value;
    size_t max_len;         /**< 6 for a cadran, 9 for a full-width cadranH */
} ui_field_t;

void ui_field_create(ui_field_t *f, lv_obj_t *parent, int32_t col, int32_t row, int32_t span,
                     const char *label, const char *unit, ui_role_t label_role);
void ui_field_set(ui_field_t *f, const char *value, ui_role_t value_role);

/** Data fields of the pages (ui_fields.c) */
typedef enum {
    UI_F_DIST = 0,
    UI_F_PWR,
    UI_F_SPEED,
    UI_F_CLIMB,
    UI_F_CAD,
    UI_F_HR,
    UI_F_SLOPE,
    UI_F_VA,
    UI_F_NEXT_SEG,
    UI_F_AVG,
    UI_F_SCORE,
    UI_F_SOLAR,
    UI_F_BATT,
    UI_F_PR,
    UI_F_NEXT_TURN,
    UI_F_FEC_TIME,
    UI_F_FEC_CAD,
    UI_F_FEC_HR,
    UI_F_FEC_SCORE,
    UI_F_FEC_PWR,
    UI_F_TIME,
    UI_F_ALT,
    UI_F_COUNT
} ui_fid_t;

/** Where a field goes: column 0 or 1, grid row, span 1 or 2 */
typedef struct {
    ui_fid_t id;
    int32_t col;
    int32_t row;
    int32_t span;
} ui_fplace_t;

void ui_fields_create(lv_obj_t *parent, const ui_fplace_t *places, uint32_t n);
void ui_fields_update(void);
void ui_fields_forget(void);

/** Band of nrows full-width rows with a border, for maps and plots */
lv_obj_t *ui_band(lv_obj_t *parent, int32_t row, int32_t nrows);

/** Object drawn by a callback on LV_EVENT_DRAW_MAIN */
lv_obj_t *ui_plot(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                  lv_event_cb_t draw_cb, void *user);

/** Generic list (menus, settings, pairing): items with optional right text */
typedef struct {
    const char *text;
    const char *right;      /**< value on the right, or NULL */
    ui_role_t role;         /**< colour of the text when not selected */
    const char *sub;        /**< second line in small type, or NULL */
    ui_role_t dot;          /**< status dot colour, or UI_C_ROLES for none */
} ui_list_item_t;

/** Rows of a list; only the two rows that change are redrawn on a new selection */
void ui_list_create(lv_obj_t *scr, const ui_list_item_t *items, int32_t n, int32_t sel,
                    int32_t top, int32_t step);
void ui_list_select(int32_t sel);
void ui_list_set_right(int32_t i, const char *text);
void ui_list_forget(void);

/* ==========================================================================
 * Drawing helpers for plot callbacks (absolute coordinates)
 * ========================================================================== */

void ui_draw_line(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                  lv_color_t color, int32_t width);
void ui_draw_dash(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                  lv_color_t color, int32_t width, int32_t dash, int32_t gap);
void ui_draw_fill(lv_layer_t *layer, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t color);
void ui_draw_frame(lv_layer_t *layer, int32_t x, int32_t y, int32_t w, int32_t h,
                   lv_color_t color, int32_t width);
void ui_draw_circle(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t r, lv_color_t color,
                    int32_t width);
void ui_draw_disc(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t r, lv_color_t color);
void ui_draw_triangle(lv_layer_t *layer, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                      int32_t x2, int32_t y2, lv_color_t color);
/** Text with its top at y; align LV_TEXT_ALIGN_LEFT, CENTER (x is the centre) or RIGHT (x is the end) */
void ui_draw_text(lv_layer_t *layer, int32_t x, int32_t y, const char *text, const lv_font_t *font,
                  lv_color_t color, lv_text_align_t align);
/** Rider marker: arrow along the course (legacy triangle), or a dot without course */
void ui_draw_rider(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t course_deg, lv_color_t color);
/** Rotate (x, y) around (cx, cy) by deg clockwise (legacy rotate_point) */
void ui_rotate(int32_t cx, int32_t cy, int32_t x, int32_t y, float deg, int32_t *xo, int32_t *yo);

/** Map point in per mille of area a, with a margin of pad pixels */
static inline int32_t ui_map_x(const lv_area_t *a, int32_t pad, int32_t xpm)
{
    int32_t w = lv_area_get_width(a) - (2 * pad);
    return a->x1 + pad + ((xpm * w) / UI_PM);
}

static inline int32_t ui_map_y(const lv_area_t *a, int32_t pad, int32_t ypm)
{
    int32_t h = lv_area_get_height(a) - (2 * pad);
    return a->y2 - pad - ((ypm * h) / UI_PM);
}

#ifdef __cplusplus
}
#endif

#endif /* UI_INTERNAL_H */
