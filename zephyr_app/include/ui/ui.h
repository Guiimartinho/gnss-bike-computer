/**
 * @file ui.h
 * @brief User interface of the new board (docs/18-interface-telas.md)
 *
 * Portable C over LVGL 9: the same code runs in the firmware (ui thread) and
 * in the host renderer of tests/ui, which draws every screen to PNG. The
 * caller owns LVGL and its display; this module builds the screens, keeps the
 * navigation of the legacy (pages, menu, lists, value editor, notifications)
 * and reports what the user asks for through an action callback. Only the
 * thread that calls lv_timer_handler() may call these functions.
 */

#ifndef UI_API_H
#define UI_API_H

#include <stdint.h>
#include <stdbool.h>

#include "ui/ui_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Screen size in portrait, like the legacy LS027 (setRotation(3)) */
#define UI_WIDTH            240
#define UI_HEIGHT           400
/** Status bar height */
#define UI_BAR_H            20
/** Rows of the legacy grid (VUE_CRS_NB_LINES) */
#define UI_ROWS             7

/** Colours: the 8 of the JDI panel, or black and white for the Sharp */
typedef enum {
    UI_THEME_COLOR = 0,
    UI_THEME_MONO
} ui_theme_t;

typedef enum {
    UI_LANG_PT = 0,
    UI_LANG_EN
} ui_lang_t;

typedef enum {
    UI_KEY_LEFT = 0,
    UI_KEY_CENTER,
    UI_KEY_RIGHT
} ui_key_t;

typedef enum {
    UI_PRESS_SHORT = 0,
    UI_PRESS_LONG
} ui_press_t;

/** Device mode, as in the legacy Boucle */
typedef enum {
    UI_MODE_CRS = 0,
    UI_MODE_PRC,
    UI_MODE_FEC,
    UI_MODE_ZWIFT,
    UI_MODE_DBG
} ui_mode_t;

/** Screens */
/** Phase of an update over the air, as the radio publishes it */
typedef enum {
    UI_DFU_IDLE = 0,
    UI_DFU_RUNNING,
    UI_DFU_DONE,
    UI_DFU_FAILED,
} ui_dfu_t;

typedef enum {
    UI_SCREEN_BOOT = 0,
    UI_SCREEN_CRS1,
    UI_SCREEN_CRS2,
    UI_SCREEN_CRS3,
    UI_SCREEN_PRC,
    UI_SCREEN_FEC,
    UI_SCREEN_GPS,
    UI_SCREEN_DBG,
    UI_SCREEN_MENU,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_SENSORS,
    UI_SCREEN_PAIR,
    UI_SCREEN_VALUE,
    UI_SCREEN_LIGHT,
    UI_SCREEN_CONFIRM,
    UI_SCREEN_ENERGY,
    UI_SCREEN_USB,
    UI_SCREEN_SHUTDOWN,
    UI_SCREEN_ROUTES,
    UI_SCREEN_DFU,
    UI_SCREEN_COUNT
} ui_screen_t;

/** What the user asked for */
typedef enum {
    UI_ACT_SET_MODE = 0,    /**< arg: ui_mode_t */
    UI_ACT_SHUTDOWN,
    UI_ACT_PAIR_START,      /**< arg: ui_sensor_kind_t */
    UI_ACT_PAIR_SELECT,     /**< arg: index in the pairing list */
    UI_ACT_PAIR_CANCEL,
    UI_ACT_SET_FTP,         /**< arg: watts */
    UI_ACT_SET_WEIGHT,      /**< arg: kg */
    UI_ACT_CALIB_COMPASS,
    UI_ACT_GNSS_TOGGLE,     /**< LEAP or full power */
    UI_ACT_LIGHT_TOGGLE,    /**< automatic backlight on or off */
    UI_ACT_THEME_TOGGLE,    /**< colours or black and white */
    UI_ACT_FORMAT,
    UI_ACT_ZOOM,            /**< arg: +1 closer, -1 farther (PRC) */
    UI_ACT_KEY,             /**< any key: feeds the backlight state machine */
    UI_ACT_ROUTE_SELECT     /**< arg: index in the route list, before UI_ACT_SET_MODE PRC */
} ui_action_t;

typedef void (*ui_action_cb_t)(ui_action_t action, int32_t arg, void *user);

typedef struct {
    ui_theme_t theme;
    ui_lang_t lang;
    const char *version;    /**< firmware version shown on the boot and DBG screens */
    ui_action_cb_t on_action;
    void *user;
} ui_config_t;

/**
 * @brief Build the interface on the default LVGL display and show the boot screen
 *
 * @return 0, or -1 when LVGL has no display
 */
int ui_init(const ui_config_t *cfg, uint32_t now_ms);

/** Change the colour theme; the current screen is rebuilt */
void ui_set_theme(ui_theme_t theme);

/** Mode of the device: chooses the data pages */
void ui_set_mode(ui_mode_t mode);

/** New snapshot of the device: refreshes the screen on show */
void ui_update(const ui_model_t *m, uint32_t now_ms);

/** A key was pressed */
void ui_key(ui_key_t key, ui_press_t press, uint32_t now_ms);

/**
 * @brief Queue a notification (legacy: up to 10, a band over the first row)
 *
 * @param value      optional coloured value on the right, or NULL
 * @param value_good green when true, red when false
 */
void ui_notify(const char *title, const char *text, const char *value, bool value_good,
               uint32_t duration_ms, uint32_t now_ms);

/** Force a screen: system screens (boot, USB, shutdown) and the host renderer */
void ui_show(ui_screen_t screen);

/** Leave a system screen (USB) for the page of the mode */
void ui_show_pages(void);

/** Progress of the shutdown screen, 0..100 */
void ui_set_progress(uint8_t pct);

/**
 * @brief Update over the air: phase and percentage
 *
 * The screen of the update comes up on its own while one is running and
 * gives the pages back when it is over.
 */
void ui_set_dfu(ui_dfu_t phase, uint8_t pct);

/** Screen on show */
ui_screen_t ui_current(void);

/** Timers: notification expiry and the 5 s menu lock after the boot */
void ui_tick(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* UI_API_H */
