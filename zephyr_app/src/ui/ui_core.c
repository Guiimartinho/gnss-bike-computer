/**
 * @file ui_core.c
 * @brief Screen switching, key navigation and notifications
 *
 * Navigation of the legacy (legacy/source/vue/Vue.cpp, VueCRS.cpp,
 * Menuable.cpp): in CRS left and right change the page, in PRC they change
 * the zoom, the centre opens the menu except in the first 5 s after the boot
 * (Menuable::propagateEvent), and CRS and PRC show the GNSS screen while the
 * last position is older than 6 s (LOCATOR_MAX_DATA_AGE_MS,
 * legacy/source/parameters.h:39). Notifications queue up to 10 and show
 * one at a time over the first row (Notif.h). New, as docs/18 proposes: the
 * centre held long asks to shut down from the data pages, and any key closes
 * the notification on show.
 */

#include <string.h>

#include "ui_internal.h"

/** Legacy LOCATOR_MAX_DATA_AGE_MS */
#define UI_FIX_MAX_AGE_S        6U
/** Legacy Menuable::propagateEvent(): no menu before 5 s */
#define UI_MENU_LOCK_MS         5000U
/** Legacy NotifiableDevice::addNotif() keeps up to 10 */
#define UI_NOTIF_MAX            10U
/** Legacy default persistence: 5 refreshes of about 1 s */
#define UI_NOTIF_DEFAULT_MS     5000U

ui_ctx_t ui_ctx;

static ui_config_t cfg;
static ui_screen_t cur = UI_SCREEN_BOOT;
static ui_screen_t last_crs_page = UI_SCREEN_CRS1;
static lv_obj_t *cur_scr;
static bool started;

static const ui_screen_ops_t *const ops[UI_SCREEN_COUNT] = {
    [UI_SCREEN_BOOT] = &ui_scr_boot,
    [UI_SCREEN_CRS1] = &ui_scr_crs1,
    [UI_SCREEN_CRS2] = &ui_scr_crs2,
    [UI_SCREEN_CRS3] = &ui_scr_crs3,
    [UI_SCREEN_PRC] = &ui_scr_prc,
    [UI_SCREEN_FEC] = &ui_scr_fec,
    [UI_SCREEN_GPS] = &ui_scr_gps,
    [UI_SCREEN_DBG] = &ui_scr_dbg,
    [UI_SCREEN_MENU] = &ui_scr_menu,
    [UI_SCREEN_SETTINGS] = &ui_scr_settings,
    [UI_SCREEN_SENSORS] = &ui_scr_sensors,
    [UI_SCREEN_PAIR] = &ui_scr_pair,
    [UI_SCREEN_VALUE] = &ui_scr_value,
    [UI_SCREEN_LIGHT] = &ui_scr_light,
    [UI_SCREEN_CONFIRM] = &ui_scr_confirm,
    [UI_SCREEN_ENERGY] = &ui_scr_energy,
    [UI_SCREEN_USB] = &ui_scr_usb,
    [UI_SCREEN_SHUTDOWN] = &ui_scr_shutdown,
    [UI_SCREEN_DFU] = &ui_scr_dfu,
    [UI_SCREEN_PROFILE] = &ui_scr_profile,
    [UI_SCREEN_LAP] = &ui_scr_lap,
    [UI_SCREEN_ROUTES] = &ui_scr_routes,
};

/* ==========================================================================
 * Notifications
 * ========================================================================== */

typedef struct {
    char title[16];
    char text[40];
    char value[16];
    bool good;
    uint32_t duration_ms;
} ui_notif_t;

static ui_notif_t nq[UI_NOTIF_MAX];
static uint32_t nq_head;
static uint32_t nq_count;
static lv_obj_t *notif_obj;
static uint32_t notif_since;

static void copy_text(char *dst, size_t size, const char *src)
{
    if (size == 0U) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    (void)strncpy(dst, src, size - 1U);
    dst[size - 1U] = '\0';
}

static void notif_show(void)
{
    if (notif_obj != NULL) {
        lv_obj_delete(notif_obj);
        notif_obj = NULL;
    }
    if (nq_count == 0U) {
        return;
    }
    const ui_notif_t *n = &nq[nq_head];
    lv_obj_t *l;

    notif_obj = ui_box(lv_layer_top(), 0, UI_BAR_H, UI_WIDTH, ui_rows_h(0, 1));
    lv_obj_set_style_bg_color(notif_obj, ui_col(UI_C_DARK), 0);
    lv_obj_set_style_bg_opa(notif_obj, LV_OPA_COVER, 0);
    l = ui_label(notif_obj, UI_FONT_SMALL_B, ui_col_dark(UI_C_WARN), n->title);
    lv_obj_set_pos(l, 8, 3);
    l = ui_label(notif_obj, UI_FONT_TITLE, ui_col_dark(UI_C_ON_DARK), n->text);
    lv_obj_set_pos(l, 8, 23);
    if (n->value[0] != '\0') {
        l = ui_label(notif_obj, UI_FONT_TITLE, ui_col_dark(n->good ? UI_C_GOOD : UI_C_BAD), n->value);
        lv_obj_align(l, LV_ALIGN_TOP_RIGHT, -8, 23);
    }
    notif_since = ui_ctx.now_ms;
}

static void notif_pop(void)
{
    if (nq_count == 0U) {
        return;
    }
    nq_head = (nq_head + 1U) % UI_NOTIF_MAX;
    nq_count--;
    notif_show();
}

void ui_notify(const char *title, const char *text, const char *value, bool value_good,
               uint32_t duration_ms, uint32_t now_ms)
{
    if (nq_count >= UI_NOTIF_MAX) {
        return;     /* legacy: dropped when the list is full */
    }
    ui_notif_t *n = &nq[(nq_head + nq_count) % UI_NOTIF_MAX];

    copy_text(n->title, sizeof(n->title), title);
    copy_text(n->text, sizeof(n->text), text);
    copy_text(n->value, sizeof(n->value), value);
    n->good = value_good;
    n->duration_ms = (duration_ms > 0U) ? duration_ms : UI_NOTIF_DEFAULT_MS;
    nq_count++;
    ui_ctx.now_ms = now_ms;
    if (nq_count == 1U) {
        notif_show();
    }
}

/* ==========================================================================
 * Screens
 * ========================================================================== */

void ui_action(ui_action_t action, int32_t arg)
{
    if (cfg.on_action != NULL) {
        cfg.on_action(action, arg, cfg.user);
    }
}

static bool is_crs_page(ui_screen_t s)
{
    return (s == UI_SCREEN_CRS1) || (s == UI_SCREEN_CRS2) || (s == UI_SCREEN_CRS3);
}

static bool is_data_page(ui_screen_t s)
{
    return is_crs_page(s) || (s == UI_SCREEN_PRC) || (s == UI_SCREEN_PROFILE) ||
           (s == UI_SCREEN_LAP) ||
           (s == UI_SCREEN_FEC) || (s == UI_SCREEN_DBG);
}

ui_screen_t ui_mode_page(void)
{
    switch (ui_ctx.mode) {
    case UI_MODE_PRC:
        return UI_SCREEN_PRC;
    case UI_MODE_FEC:
    case UI_MODE_ZWIFT:
        return UI_SCREEN_FEC;
    case UI_MODE_DBG:
        return UI_SCREEN_DBG;
    case UI_MODE_CRS:
    default:
        return last_crs_page;
    }
}

void ui_go(ui_screen_t screen)
{
    lv_obj_t *scr;

    if ((uint32_t)screen >= (uint32_t)UI_SCREEN_COUNT) {
        return;
    }
    if (screen != cur) {
        ui_ctx.sel = 0;
    }
    cur = screen;
    if (is_crs_page(screen)) {
        last_crs_page = screen;
    }

    scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, ui_col(UI_C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* the objects of the screen on show go with it */
    ui_statusbar_forget();
    ui_fields_forget();
    ui_list_forget();
    ops[screen]->create(scr);

    lv_screen_load_anim(scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
    cur_scr = scr;
}

void ui_rebuild(void)
{
    ui_go(cur);
}

void ui_show(ui_screen_t screen)
{
    ui_go(screen);
}

void ui_show_pages(void)
{
    if (started) {
        ui_go(ui_mode_page());
    }
}

ui_screen_t ui_current(void)
{
    return cur;
}

/* ==========================================================================
 * Public interface
 * ========================================================================== */

int ui_init(const ui_config_t *config, uint32_t now_ms)
{
    if ((config == NULL) || (lv_display_get_default() == NULL)) {
        return -1;
    }
    (void)memset(&ui_ctx, 0, sizeof(ui_ctx));
    cfg = *config;
    ui_ctx.theme = config->theme;
    ui_ctx.lang = config->lang;
    ui_ctx.mode = UI_MODE_CRS;
    ui_ctx.now_ms = now_ms;
    ui_ctx.boot_ms = now_ms;
    ui_ctx.m.status.time_s = UI_TIME_UNKNOWN;
    copy_text(ui_ctx.version, sizeof(ui_ctx.version), config->version);
    nq_head = 0U;
    nq_count = 0U;
    notif_obj = NULL;
    last_crs_page = UI_SCREEN_CRS1;
    started = true;
    cur = UI_SCREEN_BOOT;
    ui_go(UI_SCREEN_BOOT);
    return 0;
}

void ui_set_theme(ui_theme_t theme)
{
    if (!started) {
        return;
    }
    ui_ctx.theme = theme;
    ui_rebuild();
    notif_show();
}

void ui_set_mode(ui_mode_t mode)
{
    ui_ctx.mode = mode;
    if (!started) {
        return;
    }
    /* leave the menus alone; from a page, the boot, the menu or the route list go to the new page */
    if (is_data_page(cur) || (cur == UI_SCREEN_GPS) || (cur == UI_SCREEN_BOOT) ||
        (cur == UI_SCREEN_MENU) || (cur == UI_SCREEN_ROUTES)) {
        ui_go(ui_mode_page());
    }
}

void ui_update(const ui_model_t *m, uint32_t now_ms)
{
    if (!started || (m == NULL)) {
        return;
    }
    ui_ctx.m = *m;
    ui_ctx.have_model = true;
    ui_ctx.now_ms = now_ms;

    /* CRS and PRC show the GNSS screen while the position is too old */
    if (((ui_ctx.mode == UI_MODE_CRS) || (ui_ctx.mode == UI_MODE_PRC)) &&
        (is_data_page(cur) || (cur == UI_SCREEN_GPS))) {
        bool stale = (m->status.gnss != UI_GNSS_FIX) || (m->gnss.fix_age_s > UI_FIX_MAX_AGE_S);

        if (stale && (cur != UI_SCREEN_GPS)) {
            ui_go(UI_SCREEN_GPS);
            return;
        }
        if (!stale && (cur == UI_SCREEN_GPS)) {
            ui_go(ui_mode_page());
            return;
        }
    }
    ops[cur]->update(cur_scr);
}

void ui_set_progress(uint8_t pct)
{
    ui_ctx.progress = pct;
    if (started && (cur == UI_SCREEN_SHUTDOWN)) {
        ops[cur]->update(cur_scr);
    }
}

void ui_set_dfu(ui_dfu_t phase, uint8_t pct)
{
    bool was_busy = (ui_ctx.dfu_phase == UI_DFU_RUNNING) || (ui_ctx.dfu_phase == UI_DFU_DONE);
    bool busy = (phase == UI_DFU_RUNNING) || (phase == UI_DFU_DONE);

    ui_ctx.dfu_phase = (uint8_t)phase;
    ui_ctx.dfu_pct = pct;

    if (!started) {
        return;
    }
    if (busy && (cur != UI_SCREEN_DFU)) {
        /* nothing else matters while the image comes in */
        ui_go(UI_SCREEN_DFU);
        return;
    }
    if (cur == UI_SCREEN_DFU) {
        if (!busy && was_busy && (phase != UI_DFU_FAILED)) {
            ui_show_pages();
            return;
        }
        ops[cur]->update(cur_scr);
    }
}

void ui_tick(uint32_t now_ms)
{
    ui_ctx.now_ms = now_ms;
    if ((nq_count > 0U) && ((now_ms - notif_since) >= nq[nq_head].duration_ms)) {
        notif_pop();
    }
}

void ui_key(ui_key_t key, ui_press_t press, uint32_t now_ms)
{
    if (!started) {
        return;
    }
    ui_ctx.now_ms = now_ms;
    ui_action(UI_ACT_KEY, (int32_t)key);

    /* any key closes the notification on show */
    if (nq_count > 0U) {
        notif_pop();
        return;
    }
    if ((ops[cur]->key != NULL) && ops[cur]->key(key, press)) {
        return;
    }
    if (!is_data_page(cur) && (cur != UI_SCREEN_GPS)) {
        return;
    }
    if (key == UI_KEY_CENTER) {
        if (press == UI_PRESS_LONG) {
            ui_action(UI_ACT_SHUTDOWN, 0);
        } else if ((now_ms - ui_ctx.boot_ms) >= UI_MENU_LOCK_MS) {
            ui_ctx.sel = 0;
            ui_go(UI_SCREEN_MENU);
        }
        return;
    }
    /*
     * A long press on the left marks a lap from any data page. The legacy
     * has no lap, so the key was free; a notification confirms it.
     */
    if ((key == UI_KEY_LEFT) && (press == UI_PRESS_LONG)) {
        ui_action(UI_ACT_LAP, 0);
        return;
    }
    if (press != UI_PRESS_SHORT) {
        return;
    }
    if (is_crs_page(cur) || (cur == UI_SCREEN_LAP)) {
        /*
         * legacy VueCRS::propagateEventsCRS(): pages 1, 2 and 3 in a ring.
         * The lap page of the port joins the ring after page 3.
         */
        static const ui_screen_t ring[4] = {
            UI_SCREEN_CRS1, UI_SCREEN_CRS2, UI_SCREEN_CRS3, UI_SCREEN_LAP,
        };
        uint32_t i = (cur == UI_SCREEN_LAP) ? 3U : ((uint32_t)cur - (uint32_t)UI_SCREEN_CRS1);

        ui_go(ring[(key == UI_KEY_RIGHT) ? ((i + 1U) % 4U) : ((i + 3U) % 4U)]);
    }
}
