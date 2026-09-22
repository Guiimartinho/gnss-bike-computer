/**
 * @file ui_scr_menus.c
 * @brief Menu, settings, sensors, pairing, value editor and confirmations
 *
 * The tree of legacy/source/vue/Menuable.cpp:255-294 in Portuguese: every
 * page starts with Voltar (the legacy "Back" item), left and right move the
 * selection around, the centre runs the item (MenuPageItems::propagateEvent),
 * and the value editor counts by one (MenuPageSetting). New: the centre held
 * long goes back to the data page (docs/18, Botões).
 */

#include <stdio.h>
#include <string.h>

#include "ui_internal.h"

/** Move the selection of a list with n items; returns true when handled */
static bool list_nav(ui_key_t key, int32_t n)
{
    if (n <= 0) {
        return false;
    }
    if (key == UI_KEY_LEFT) {
        ui_ctx.sel = (ui_ctx.sel + n - 1) % n;
        ui_list_select(ui_ctx.sel);
        return true;
    }
    if (key == UI_KEY_RIGHT) {
        ui_ctx.sel = (ui_ctx.sel + 1) % n;
        ui_list_select(ui_ctx.sel);
        return true;
    }
    return false;
}

static ui_list_item_t item(const char *text, const char *right, ui_role_t role)
{
    ui_list_item_t it = {text, right, role, NULL, UI_C_ROLES};

    return it;
}

/* ==========================================================================
 * Main menu
 * ========================================================================== */

#define MENU_N  9

static void menu_create(lv_obj_t *scr)
{
    ui_list_item_t items[MENU_N] = {
        item(ui_txt(T_BACK), NULL, UI_C_FG),
        item(ui_txt(T_MODE_FEC), NULL, UI_C_FG),
        item(ui_txt(T_MODE_CRS), NULL, UI_C_FG),
        item(ui_txt(T_MODE_PRC), NULL, UI_C_FG),
        item(ui_txt(T_MODE_ZWIFT), NULL, UI_C_FG),
        item(ui_txt(T_MODE_DBG), NULL, UI_C_FG),
        item(ui_txt(T_WORKOUT), NULL, UI_C_FG),
        item(ui_txt(T_SETTINGS), NULL, UI_C_FG),
        item(ui_txt(T_SHUTDOWN), NULL, UI_C_BAD),
    };

    ui_statusbar_create(scr);
    ui_titlebar_create(scr, ui_txt(T_MENU));
    ui_list_create(scr, items, MENU_N, ui_ctx.sel, UI_BAR_H + 34, 43);
}

static void set_mode(ui_mode_t mode)
{
    ui_action(UI_ACT_SET_MODE, (int32_t)mode);
    ui_set_mode(mode);
}

static bool menu_key(ui_key_t key, ui_press_t press)
{
    if ((key == UI_KEY_CENTER) && (press == UI_PRESS_LONG)) {
        ui_go(ui_mode_page());
        return true;
    }
    if (press != UI_PRESS_SHORT) {
        return true;
    }
    if (list_nav(key, MENU_N)) {
        return true;
    }
    switch (ui_ctx.sel) {
    case 0:
        ui_go(ui_mode_page());
        break;
    case 1:
        set_mode(UI_MODE_FEC);
        break;
    case 2:
        set_mode(UI_MODE_CRS);
        break;
    case 3:
        /* legacy _page0_mode_prc_list(): the routes on the card, or an error */
        if (ui_ctx.m.routes.n == 0U) {
            ui_notify(ui_txt(T_ERROR), ui_txt(T_NO_ROUTES), NULL, false, 5000U, ui_ctx.now_ms);
            ui_go(ui_mode_page());
        } else {
            ui_go(UI_SCREEN_ROUTES);
        }
        break;
    case 4:
        set_mode(UI_MODE_ZWIFT);
        break;
    case 5:
        set_mode(UI_MODE_DBG);
        break;
    case 6:
        /*
         * A session already loaded goes straight to its page; otherwise the
         * rider picks one, and is told when there is nothing to pick.
         */
        if (ui_ctx.m.wk.loaded) {
            ui_go(UI_SCREEN_WORKOUT);
        } else if (ui_ctx.m.workouts.n == 0U) {
            ui_notify(ui_txt(T_ERROR), ui_txt(T_NO_WORKOUT), NULL, false, 5000U,
                      ui_ctx.now_ms);
            ui_go(ui_mode_page());
        } else {
            ui_go(UI_SCREEN_WORKOUTS);
        }
        break;
    case 7:
        ui_go(UI_SCREEN_SETTINGS);
        break;
    default:
        ui_action(UI_ACT_SHUTDOWN, 0);
        break;
    }
    return true;
}

static void list_update(lv_obj_t *scr)
{
    (void)scr;
    ui_statusbar_update();
}

const ui_screen_ops_t ui_scr_menu = {menu_create, list_update, menu_key};

/* ==========================================================================
 * Routes (legacy prc_sel): Voltar and the routes on the card
 * ========================================================================== */

static uint8_t routes_shown;

static int32_t routes_count(void)
{
    uint8_t n = ui_ctx.m.routes.n;

    return 1 + (int32_t)((n > UI_ROUTE_LIST_MAX) ? UI_ROUTE_LIST_MAX : n);
}

static void routes_create(lv_obj_t *scr)
{
    ui_list_item_t items[1 + UI_ROUTE_LIST_MAX];
    int32_t n = routes_count();

    items[0] = item(ui_txt(T_BACK), NULL, UI_C_FG);
    for (int32_t i = 1; i < n; i++) {
        items[i] = item(ui_ctx.m.routes.name[i - 1], NULL, UI_C_FG);
    }
    routes_shown = ui_ctx.m.routes.n;
    if (ui_ctx.sel >= n) {
        ui_ctx.sel = n - 1;
    }
    ui_statusbar_create(scr);
    ui_titlebar_create(scr, ui_txt(T_ROUTES));
    ui_list_create(scr, items, n, ui_ctx.sel, UI_BAR_H + 32, 31);
}

static void routes_update(lv_obj_t *scr)
{
    (void)scr;
    if (ui_ctx.m.routes.n != routes_shown) {
        ui_rebuild();
        return;
    }
    ui_statusbar_update();
}

static bool routes_key(ui_key_t key, ui_press_t press)
{
    if ((key == UI_KEY_CENTER) && (press == UI_PRESS_LONG)) {
        ui_go(ui_mode_page());
        return true;
    }
    if (press != UI_PRESS_SHORT) {
        return true;
    }
    if (list_nav(key, routes_count())) {
        return true;
    }
    if (ui_ctx.sel == 0) {
        ui_go(UI_SCREEN_MENU);
    } else {
        /* legacy _page0_mode_prc_go(): the route first, then the mode */
        ui_action(UI_ACT_ROUTE_SELECT, ui_ctx.sel - 1);
        set_mode(UI_MODE_PRC);
    }
    return true;
}

const ui_screen_ops_t ui_scr_routes = {routes_create, routes_update, routes_key};

/* ==========================================================================
 * Workouts: Back, the sessions on the storage, and the one loaded
 *
 * The same shape as the routes above, because it is the same job: a list
 * the rider walks with the keys. Picking one loads it and opens its page,
 * where the countdown lives (`ui_scr_workout`).
 * ========================================================================== */

static uint8_t workouts_shown;

static int32_t workouts_count(void)
{
    uint8_t n = ui_ctx.m.workouts.n;

    /* Back, the sessions, and Unload when one is loaded */
    return 1 + (int32_t)((n > UI_ROUTE_LIST_MAX) ? UI_ROUTE_LIST_MAX : n) +
           (ui_ctx.m.wk.loaded ? 1 : 0);
}

static void workouts_create(lv_obj_t *scr)
{
    ui_list_item_t items[2 + UI_ROUTE_LIST_MAX];
    int32_t n = workouts_count();
    uint8_t files = ui_ctx.m.workouts.n;

    if (files > UI_ROUTE_LIST_MAX) {
        files = UI_ROUTE_LIST_MAX;
    }

    items[0] = item(ui_txt(T_BACK), NULL, UI_C_FG);
    for (uint8_t i = 0U; i < files; i++) {
        items[1U + i] = item(ui_ctx.m.workouts.name[i], NULL, UI_C_FG);
    }
    if (ui_ctx.m.wk.loaded) {
        items[1U + files] = item(ui_txt(T_UNLOAD), NULL, UI_C_BAD);
    }
    workouts_shown = ui_ctx.m.workouts.n;
    if (ui_ctx.sel >= n) {
        ui_ctx.sel = n - 1;
    }
    ui_statusbar_create(scr);
    ui_titlebar_create(scr, ui_txt(T_WORKOUT));
    ui_list_create(scr, items, n, ui_ctx.sel, UI_BAR_H + 32, 31);
}

static void workouts_update(lv_obj_t *scr)
{
    (void)scr;
    if (ui_ctx.m.workouts.n != workouts_shown) {
        ui_rebuild();
        return;
    }
    ui_statusbar_update();
}

static bool workouts_key(ui_key_t key, ui_press_t press)
{
    if ((key == UI_KEY_CENTER) && (press == UI_PRESS_LONG)) {
        ui_go(ui_mode_page());
        return true;
    }
    if (press != UI_PRESS_SHORT) {
        return true;
    }
    if (list_nav(key, workouts_count())) {
        return true;
    }

    uint8_t files = ui_ctx.m.workouts.n;

    if (files > UI_ROUTE_LIST_MAX) {
        files = UI_ROUTE_LIST_MAX;
    }

    if (ui_ctx.sel == 0) {
        ui_go(UI_SCREEN_MENU);
    } else if ((uint8_t)ui_ctx.sel <= files) {
        ui_action(UI_ACT_WORKOUT_SELECT, ui_ctx.sel - 1);
        ui_go(UI_SCREEN_WORKOUT);
    } else {
        /* the last line, which only exists while one is loaded */
        ui_action(UI_ACT_WORKOUT_SELECT, -1);
        ui_go(UI_SCREEN_MENU);
    }
    return true;
}

const ui_screen_ops_t ui_scr_workouts = {workouts_create, workouts_update, workouts_key};

/* ==========================================================================
 * Alerts (`model/alerts.h`): the lines the rider turns on and tunes
 *
 * One line per alert, with its value beside it or a dash when it is off.
 * Left and right step the value of the selected line and send it; the
 * centre key turns the line off, which is the same as setting it to zero.
 * The step of each is the one that makes sense in its unit, so a rider
 * setting a heart rate does not walk through a thousand beats.
 *
 * The twelve come in **two screens**, and not because twelve is untidy:
 * `ui_list_create()` draws twelve rows and no more, and it does not
 * scroll, so a thirteenth line — Back plus twelve alerts — would leave the
 * last reminder unreachable. The split is the one a rider would make
 * anyway: the eight that watch a number, and the four that go off every so
 * much.
 * ========================================================================== */

/** Order, name, unit, step and ceiling of each alert, as the rider sees it */
static const struct {
    ui_text_t name;
    const char *unit;
    uint16_t step;
    uint16_t max;
} alert_row[UI_ALERTS] = {
    {T_A_HR_HI, "bpm", 5U, 220U},
    {T_A_HR_LO, "bpm", 5U, 220U},
    {T_A_PWR_HI, "W", 10U, 1500U},
    {T_A_PWR_LO, "W", 10U, 1500U},
    {T_A_SPD_HI, "km/h", 10U, 990U},    /* tenths of a km/h */
    {T_A_SPD_LO, "km/h", 10U, 990U},
    {T_A_CAD_HI, "rpm", 5U, 200U},
    {T_A_CAD_LO, "rpm", 5U, 200U},
    {T_A_DIST, "km", 10U, 2000U},       /* hundreds of metres */
    {T_A_TIME, "min", 5U, 600U},
    {T_A_DRINK, "min", 5U, 600U},
    {T_A_EAT, "min", 5U, 600U},
};

static char alert_val[UI_ALERTS][12];

/** Which of the two groups is on the screen: [first, first + count) */
static uint8_t alerts_first;
static uint8_t alerts_count;

/** Called before going to the screen, to say which group it shows */
void ui_alerts_group(uint8_t first, uint8_t count)
{
    alerts_first = first;
    alerts_count = count;
    ui_ctx.sel = 0;
}

/** What one line shows on the right: the value in its unit, or a dash */
static const char *alert_text(uint8_t i)
{
    uint16_t v = ui_ctx.m.alerts.value[i];

    if (v == 0U) {
        (void)snprintf(alert_val[i], sizeof(alert_val[0]), "--");
    } else if ((i == 4U) || (i == 5U)) {
        /* speed is kept in tenths */
        (void)snprintf(alert_val[i], sizeof(alert_val[0]), "%u.%u %s",
                       (unsigned int)(v / 10U), (unsigned int)(v % 10U), alert_row[i].unit);
    } else if (i == 8U) {
        /* distance is kept in hundreds of metres */
        (void)snprintf(alert_val[i], sizeof(alert_val[0]), "%u.%u %s",
                       (unsigned int)(v / 10U), (unsigned int)(v % 10U), alert_row[i].unit);
    } else {
        (void)snprintf(alert_val[i], sizeof(alert_val[0]), "%u %s", (unsigned int)v,
                       alert_row[i].unit);
    }

    return alert_val[i];
}

static void alerts_create(lv_obj_t *scr)
{
    ui_list_item_t items[1 + UI_ALERTS];

    if (alerts_count == 0U) {
        alerts_count = UI_ALERTS_THRESHOLDS;
    }

    items[0] = item(ui_txt(T_BACK), NULL, UI_C_FG);
    for (uint8_t i = 0U; i < alerts_count; i++) {
        uint8_t a = (uint8_t)(alerts_first + i);

        /* the dash on the right is what says an alert is off; there is no
         * dimmed role in the palette, and the panel has eight colours */
        items[1U + i] = item(ui_txt(alert_row[a].name), alert_text(a), UI_C_FG);
    }
    if (ui_ctx.sel > (int32_t)alerts_count) {
        ui_ctx.sel = (int32_t)alerts_count;
    }
    ui_statusbar_create(scr);
    ui_titlebar_create(scr, ui_txt((alerts_first == 0U) ? T_ALERTS : T_REMINDERS));
    ui_list_create(scr, items, 1 + (int32_t)alerts_count, ui_ctx.sel, UI_BAR_H + 32, 31);
}

static void alerts_update(lv_obj_t *scr)
{
    (void)scr;
    ui_statusbar_update();
}

/** Send one line as the model wants it: identifier and value in one word */
static void alert_send(uint8_t i, uint16_t value)
{
    ui_action(UI_ACT_SET_ALERT, (int32_t)(((uint32_t)i << 16) | value));
}

static bool alerts_key(ui_key_t key, ui_press_t press)
{
    if ((key == UI_KEY_CENTER) && (press == UI_PRESS_LONG)) {
        ui_go(ui_mode_page());
        return true;
    }
    if (press != UI_PRESS_SHORT) {
        return true;
    }

    if (ui_ctx.sel == 0) {
        if (key == UI_KEY_CENTER) {
            ui_go(UI_SCREEN_SETTINGS);
            return true;
        }
        (void)list_nav(key, 1 + (int32_t)alerts_count);
        return true;
    }

    uint8_t i = (uint8_t)(alerts_first + (uint8_t)(ui_ctx.sel - 1));
    uint16_t v = ui_ctx.m.alerts.value[i];

    switch (key) {
    case UI_KEY_CENTER:
        /* the centre turns the line off, or back on at a sensible value */
        alert_send(i, (v > 0U) ? 0U : alert_row[i].step);
        break;
    case UI_KEY_RIGHT:
        if (v < alert_row[i].max) {
            v = (uint16_t)(v + alert_row[i].step);
            if (v > alert_row[i].max) {
                v = alert_row[i].max;
            }
            alert_send(i, v);
        }
        break;
    case UI_KEY_LEFT:
    default:
        alert_send(i, (v > alert_row[i].step) ? (uint16_t)(v - alert_row[i].step) : 0U);
        break;
    }

    return true;
}

const ui_screen_ops_t ui_scr_alerts = {alerts_create, alerts_update, alerts_key};

/* ==========================================================================
 * Settings (legacy page_set, with the new items)
 * ========================================================================== */

#define SET_N   12

static char set_ftp[12];
static char set_weight[12];

static void settings_values(void)
{
    (void)snprintf(set_ftp, sizeof(set_ftp), "%u W", (unsigned int)ui_ctx.m.settings.ftp_w);
    (void)snprintf(set_weight, sizeof(set_weight), "%u kg", (unsigned int)ui_ctx.m.settings.weight_kg);
}

static const char *gnss_mode_name(void)
{
    return ui_ctx.m.settings.gnss_leap ? ui_txt(T_GNSS_LEAP) : ui_txt(T_GNSS_FULL);
}

static void settings_create(lv_obj_t *scr)
{
    settings_values();
    ui_list_item_t items[SET_N] = {
        item(ui_txt(T_BACK), NULL, UI_C_FG),
        item(ui_txt(T_SENSORS), NULL, UI_C_FG),
        item(ui_txt(T_FTP), set_ftp, UI_C_FG),
        item(ui_txt(T_WEIGHT), set_weight, UI_C_FG),
        item(ui_txt(T_CAL_COMPASS), NULL, UI_C_FG),
        item(ui_txt(T_SCREEN_LIGHT), NULL, UI_C_FG),
        item(ui_txt(T_GNSS), gnss_mode_name(), UI_C_FG),
        item(ui_ctx.m.inc.armed ? ui_txt(T_ALARM_OFF) : ui_txt(T_ALARM_ARM), NULL,
             ui_ctx.m.inc.armed ? UI_C_BAD : UI_C_FG),
        item(ui_txt(T_ALERTS), NULL, UI_C_FG),
        item(ui_txt(T_REMINDERS), NULL, UI_C_FG),
        item(ui_txt(T_ENERGY), NULL, UI_C_FG),
        item(ui_txt(T_FORMAT), NULL, UI_C_BAD),
    };

    ui_statusbar_create(scr);
    ui_titlebar_create(scr, ui_txt(T_SETTINGS));
    ui_list_create(scr, items, SET_N, ui_ctx.sel, UI_BAR_H + 32, 38);
}

static void settings_update(lv_obj_t *scr)
{
    (void)scr;
    ui_statusbar_update();
    settings_values();
    ui_list_set_right(2, set_ftp);
    ui_list_set_right(3, set_weight);
    ui_list_set_right(6, gnss_mode_name());
}

static void edit_value(ui_text_t title, int32_t value)
{
    ui_ctx.value_title = title;
    ui_ctx.value = value;
    ui_go(UI_SCREEN_VALUE);
}

static bool settings_key(ui_key_t key, ui_press_t press)
{
    if ((key == UI_KEY_CENTER) && (press == UI_PRESS_LONG)) {
        ui_go(ui_mode_page());
        return true;
    }
    if (press != UI_PRESS_SHORT) {
        return true;
    }
    if (list_nav(key, SET_N)) {
        return true;
    }
    switch (ui_ctx.sel) {
    case 0:
        ui_go(UI_SCREEN_MENU);
        break;
    case 1:
        ui_go(UI_SCREEN_SENSORS);
        break;
    case 2:
        edit_value(T_FTP, ui_ctx.m.settings.ftp_w);
        break;
    case 3:
        edit_value(T_WEIGHT, ui_ctx.m.settings.weight_kg);
        break;
    case 4:
        ui_action(UI_ACT_CALIB_COMPASS, 0);
        break;
    case 5:
        ui_go(UI_SCREEN_LIGHT);
        break;
    case 6:
        ui_action(UI_ACT_GNSS_TOGGLE, 0);
        break;
    case 7:
        /* the rider arms the alarm when walking away from the bike */
        ui_action(UI_ACT_ALARM_TOGGLE, 0);
        ui_go(ui_mode_page());
        break;
    case 8:
        ui_alerts_group(0U, UI_ALERTS_THRESHOLDS);
        ui_go(UI_SCREEN_ALERTS);
        break;
    case 9:
        ui_alerts_group(UI_ALERTS_THRESHOLDS, UI_ALERTS - UI_ALERTS_THRESHOLDS);
        ui_go(UI_SCREEN_ALERTS);
        break;
    case 10:
        ui_go(UI_SCREEN_ENERGY);
        break;
    default:
        ui_go(UI_SCREEN_CONFIRM);
        break;
    }
    return true;
}

const ui_screen_ops_t ui_scr_settings = {settings_create, settings_update, settings_key};

/* ==========================================================================
 * Sensors (new): one line per kind, with its state; the centre pairs
 * ========================================================================== */

#define SENS_N  (1 + (int32_t)UI_SENSOR_KINDS)

static char sens_sub[UI_SENSOR_KINDS][UI_NAME_LEN + 12];
static char sens_right[UI_SENSOR_KINDS][16];

static const ui_sensor_t *sensor_of(uint8_t kind)
{
    for (uint32_t i = 0U; (i < ui_ctx.m.sensors.n) && (i < UI_SENSOR_MAX); i++) {
        if (ui_ctx.m.sensors.s[i].kind == kind) {
            return &ui_ctx.m.sensors.s[i];
        }
    }
    return NULL;
}

static void sensors_create(lv_obj_t *scr)
{
    ui_list_item_t items[SENS_N];

    items[0] = item(ui_txt(T_BACK), NULL, UI_C_FG);
    for (uint8_t k = 0U; k < (uint8_t)UI_SENSOR_KINDS; k++) {
        const ui_sensor_t *s = sensor_of(k);
        ui_list_item_t *it = &items[1 + k];

        *it = item(ui_sensor_name(k), sens_right[k], UI_C_FG);
        it->sub = sens_sub[k];
        if ((s == NULL) || (s->link == UI_LINK_NONE)) {
            (void)snprintf(sens_sub[k], sizeof(sens_sub[k]), "%s", ui_txt(T_L_NONE));
            (void)snprintf(sens_right[k], sizeof(sens_right[k]), "-");
            it->dot = UI_C_BG;
        } else {
            if (s->dev_name[0] != '\0') {
                (void)snprintf(sens_sub[k], sizeof(sens_sub[k]), "%s %s", s->ant ? "ANT+" : "BLE", s->dev_name);
            } else {
                (void)snprintf(sens_sub[k], sizeof(sens_sub[k]), "%s %lu", s->ant ? "ANT+" : "BLE",
                               (unsigned long)s->dev_id);
            }
            if (s->link == UI_LINK_CONNECTED) {
                (void)snprintf(sens_right[k], sizeof(sens_right[k]), "%s", s->value);
                it->dot = UI_C_GOOD;
            } else if (s->link == UI_LINK_LOST) {
                (void)snprintf(sens_right[k], sizeof(sens_right[k]), "%s", ui_txt(T_L_LOST));
                it->dot = UI_C_BAD;
            } else {
                (void)snprintf(sens_right[k], sizeof(sens_right[k]), "%s", ui_txt(T_L_SEARCH));
                it->dot = UI_C_WARN;
            }
        }
    }
    ui_statusbar_create(scr);
    ui_titlebar_create(scr, ui_txt(T_SENSORS));
    ui_list_create(scr, items, SENS_N, ui_ctx.sel, UI_BAR_H + 32, 49);
}

static bool sensors_key(ui_key_t key, ui_press_t press)
{
    if ((key == UI_KEY_CENTER) && (press == UI_PRESS_LONG)) {
        ui_go(ui_mode_page());
        return true;
    }
    if (press != UI_PRESS_SHORT) {
        return true;
    }
    if (list_nav(key, SENS_N)) {
        return true;
    }
    if (ui_ctx.sel == 0) {
        ui_go(UI_SCREEN_SETTINGS);
    } else {
        /* legacy "Pair HRM", "Pair BSC", "Pair FEC", now for every kind */
        ui_action(UI_ACT_PAIR_START, ui_ctx.sel - 1);
        ui_ctx.m.pair.kind = (uint8_t)(ui_ctx.sel - 1);
        ui_ctx.m.pair.searching = true;
        ui_ctx.m.pair.n = 0U;
        ui_go(UI_SCREEN_PAIR);
    }
    return true;
}

static void sensors_update(lv_obj_t *scr)
{
    (void)scr;
    /* states change rarely: rebuild the list keeping the selection */
    ui_rebuild();
}

const ui_screen_ops_t ui_scr_sensors = {sensors_create, sensors_update, sensors_key};

/* ==========================================================================
 * Pairing (legacy MenuPagePairing): Cancel and the devices found
 * ========================================================================== */

static char pair_title[32];
static char pair_name[UI_PAIR_MAX][UI_NAME_LEN + 8];
static char pair_rssi[UI_PAIR_MAX][12];
static uint8_t pair_n_shown;

static void pair_create(lv_obj_t *scr)
{
    const ui_pair_t *p = &ui_ctx.m.pair;
    ui_list_item_t items[1 + UI_PAIR_MAX];
    int32_t n = 1;
    lv_obj_t *l;

    items[0] = item(ui_txt(T_CANCEL), NULL, UI_C_FG);
    for (uint32_t i = 0U; (i < p->n) && (i < UI_PAIR_MAX); i++) {
        const ui_pair_item_t *d = &p->item[i];

        if (d->name[0] != '\0') {
            (void)snprintf(pair_name[i], sizeof(pair_name[i]), "%s %s", d->ant ? "ANT+" : "BLE", d->name);
        } else {
            (void)snprintf(pair_name[i], sizeof(pair_name[i]), "%s %lu", d->ant ? "ANT+" : "BLE",
                           (unsigned long)d->id);
        }
        (void)snprintf(pair_rssi[i], sizeof(pair_rssi[i]), "%d dBm", (int)d->rssi);
        items[n++] = item(pair_name[i], pair_rssi[i], UI_C_FG);
    }
    pair_n_shown = p->n;
    if (ui_ctx.sel >= n) {
        ui_ctx.sel = n - 1;
    }
    (void)snprintf(pair_title, sizeof(pair_title), "%s %s", ui_txt(T_PAIR), ui_sensor_name(p->kind));
    ui_statusbar_create(scr);
    ui_titlebar_create(scr, pair_title);
    if (p->searching) {
        l = ui_label(scr, UI_FONT_ITEM, ui_col(UI_C_NAV), ui_txt(T_SEARCHING));
        lv_obj_align(l, LV_ALIGN_TOP_MID, 0, UI_BAR_H + 34);
    }
    ui_list_create(scr, items, n, ui_ctx.sel, UI_BAR_H + 56, 38);
    l = ui_label(scr, UI_FONT_SMALL, ui_col(UI_C_FG), ui_txt(T_CENTER_PAIRS));
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -4);
}

static void pair_update(lv_obj_t *scr)
{
    (void)scr;
    if (ui_ctx.m.pair.n != pair_n_shown) {
        ui_rebuild();
        return;
    }
    ui_statusbar_update();
}

static bool pair_key(ui_key_t key, ui_press_t press)
{
    int32_t n = 1 + (int32_t)pair_n_shown;

    if ((key == UI_KEY_CENTER) && (press == UI_PRESS_LONG)) {
        ui_action(UI_ACT_PAIR_CANCEL, 0);
        ui_go(ui_mode_page());
        return true;
    }
    if (press != UI_PRESS_SHORT) {
        return true;
    }
    if (list_nav(key, n)) {
        return true;
    }
    if (ui_ctx.sel == 0) {
        ui_action(UI_ACT_PAIR_CANCEL, 0);
    } else {
        ui_action(UI_ACT_PAIR_SELECT, ui_ctx.sel - 1);
    }
    ui_go(UI_SCREEN_SENSORS);
    return true;
}

const ui_screen_ops_t ui_scr_pair = {pair_create, pair_update, pair_key};

/* ==========================================================================
 * Value editor (legacy MenuPageSetting)
 * ========================================================================== */

/**
 * The legacy counts without limits. Here the value stays between 1 and the
 * maximum: the snapshot keeps the weight in a byte, and an FTP of zero would
 * put every power in the same zone.
 */
#define UI_WEIGHT_MAX_KG    255
#define UI_FTP_MAX_W        2000

static lv_obj_t *value_label;

static void value_create(lv_obj_t *scr)
{
    char buf[12];
    lv_obj_t *l;
    lv_obj_t *b;

    ui_statusbar_create(scr);
    ui_titlebar_create(scr, ui_txt(ui_ctx.value_title));
    (void)snprintf(buf, sizeof(buf), "%ld", (long)ui_ctx.value);
    value_label = ui_label(scr, UI_FONT_HUGE, ui_col(UI_C_FG), buf);
    lv_obj_align(value_label, LV_ALIGN_TOP_MID, 0, 130);
    l = ui_label(scr, UI_FONT_MEDIUM, ui_col(UI_C_FG),
                 ui_txt((ui_ctx.value_title == T_WEIGHT) ? T_KG : T_W));
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 214);

    /* -1 and +1 boxes over the left and right keys, "gravar" over the centre */
    b = ui_box(scr, 14, 316, 60, 44);
    lv_obj_set_style_border_color(b, ui_col(UI_C_FG), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    l = ui_label(b, UI_FONT_MEDIUM, ui_col(UI_C_FG), "-1");
    lv_obj_center(l);
    b = ui_box(scr, UI_WIDTH - 74, 316, 60, 44);
    lv_obj_set_style_border_color(b, ui_col(UI_C_FG), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    l = ui_label(b, UI_FONT_MEDIUM, ui_col(UI_C_FG), "+1");
    lv_obj_center(l);
    l = ui_label(scr, UI_FONT_SMALL_B, ui_col(UI_C_NAV), ui_txt(T_SAVE));
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 331);
}

static bool value_key(ui_key_t key, ui_press_t press)
{
    char buf[12];

    if (press != UI_PRESS_SHORT) {
        return true;
    }
    int32_t max = (ui_ctx.value_title == T_WEIGHT) ? UI_WEIGHT_MAX_KG : UI_FTP_MAX_W;

    if (key == UI_KEY_LEFT) {
        if (ui_ctx.value > 1) {
            ui_ctx.value--;
        }
    } else if (key == UI_KEY_RIGHT) {
        if (ui_ctx.value < max) {
            ui_ctx.value++;
        }
    } else {
        ui_action((ui_ctx.value_title == T_WEIGHT) ? UI_ACT_SET_WEIGHT : UI_ACT_SET_FTP, ui_ctx.value);
        if (ui_ctx.value_title == T_WEIGHT) {
            ui_ctx.m.settings.weight_kg = (uint8_t)ui_ctx.value;
        } else {
            ui_ctx.m.settings.ftp_w = (uint16_t)ui_ctx.value;
        }
        ui_go(UI_SCREEN_SETTINGS);
        return true;
    }
    (void)snprintf(buf, sizeof(buf), "%ld", (long)ui_ctx.value);
    lv_label_set_text(value_label, buf);
    return true;
}

const ui_screen_ops_t ui_scr_value = {value_create, list_update, value_key};

/* ==========================================================================
 * Screen and light (new)
 * ========================================================================== */

#define LIGHT_N 3

static void light_create(lv_obj_t *scr)
{
    ui_list_item_t items[LIGHT_N] = {
        item(ui_txt(T_BACK), NULL, UI_C_FG),
        item(ui_txt(T_LIGHT), ui_ctx.m.settings.light_auto ? ui_txt(T_AUTO) : ui_txt(T_OFF), UI_C_FG),
        item(ui_txt(T_SCREEN), (ui_ctx.theme == UI_THEME_COLOR) ? ui_txt(T_COLOURS) : ui_txt(T_BW), UI_C_FG),
    };

    ui_statusbar_create(scr);
    ui_titlebar_create(scr, ui_txt(T_SCREEN_LIGHT));
    ui_list_create(scr, items, LIGHT_N, ui_ctx.sel, UI_BAR_H + 40, 44);
}

static void light_update(lv_obj_t *scr)
{
    (void)scr;
    ui_statusbar_update();
    ui_list_set_right(1, ui_ctx.m.settings.light_auto ? ui_txt(T_AUTO) : ui_txt(T_OFF));
}

static bool light_key(ui_key_t key, ui_press_t press)
{
    if ((key == UI_KEY_CENTER) && (press == UI_PRESS_LONG)) {
        ui_go(ui_mode_page());
        return true;
    }
    if (press != UI_PRESS_SHORT) {
        return true;
    }
    if (list_nav(key, LIGHT_N)) {
        return true;
    }
    if (ui_ctx.sel == 0) {
        ui_go(UI_SCREEN_SETTINGS);
    } else if (ui_ctx.sel == 1) {
        ui_action(UI_ACT_LIGHT_TOGGLE, 0);
    } else {
        ui_action(UI_ACT_THEME_TOGGLE, 0);
    }
    return true;
}

const ui_screen_ops_t ui_scr_light = {light_create, light_update, light_key};

/* ==========================================================================
 * Format confirmation (legacy "! Format !" ran at once)
 * ========================================================================== */

#define CONFIRM_N   2

static void confirm_create(lv_obj_t *scr)
{
    ui_list_item_t items[CONFIRM_N] = {
        item(ui_txt(T_CANCEL), NULL, UI_C_FG),
        item(ui_txt(T_FORMAT_YES), NULL, UI_C_BAD),
    };
    lv_obj_t *l;

    ui_statusbar_create(scr);
    ui_titlebar_create(scr, ui_txt(T_FORMAT));
    l = ui_label(scr, UI_FONT_TITLE, ui_col(UI_C_FG), ui_txt(T_FORMAT_Q));
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, UI_BAR_H + 56);
    ui_list_create(scr, items, CONFIRM_N, ui_ctx.sel, UI_BAR_H + 100, 46);
}

static bool confirm_key(ui_key_t key, ui_press_t press)
{
    if (press != UI_PRESS_SHORT) {
        return true;
    }
    if (list_nav(key, CONFIRM_N)) {
        return true;
    }
    if (ui_ctx.sel == 1) {
        ui_action(UI_ACT_FORMAT, 0);
    }
    ui_go(UI_SCREEN_SETTINGS);
    return true;
}

const ui_screen_ops_t ui_scr_confirm = {confirm_create, list_update, confirm_key};
