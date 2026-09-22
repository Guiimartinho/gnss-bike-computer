/**
 * @file ui_svc.c
 * @brief Interface service: the thread of the screen
 *
 * docs/16-arquitetura-firmware.md (Threads, Interface, Luz do display) and
 * docs/18-interface-telas.md. The only thread that calls LVGL and the ui_*
 * functions of zephyr_app/src/ui:
 *  - a new model snapshot (chan_model_state) goes to ui_update(); the first
 *    one leaves the boot screen for the page of the mode (chan_mode);
 *  - notifications go to ui_notify(), keys (chan_input) to ui_key() and to
 *    the light, the ambient light (chan_ambient) to the light;
 *  - the system state shows the USB and shutdown screens;
 *  - the actions of the screens become commands (app_command()), except the
 *    light and the colour theme, which this service owns and keeps under the
 *    settings key ui/prefs;
 *  - lv_timer_handler() draws; the display driver sends only the panel lines
 *    that changed, and the thread sleeps while nothing changes.
 * On shutdown it shows the progress of the other services, then clears the
 * panel as the datasheets ask before the supply goes, and acknowledges.
 */

#include <stdio.h>
#include <string.h>

#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>
#include <zephyr/zbus/zbus.h>

#include "app/app_channels.h"
#include "app/app_svc.h"
#include "app_types.h"
#include "svc/backlight.h"
#include "ui/ui.h"

#if defined(CONFIG_MEMLCD)
#include <drivers/display/memlcd.h>
#endif

LOG_MODULE_REGISTER(ui_svc, CONFIG_LOG_DEFAULT_LEVEL);

/* LVGL drawing: about 4.7 KB measured with CONFIG_STACK_USAGE (624 B per
 * level of the object tree, 4 levels, and the arc of the SW renderer), plus
 * 1.4 KB of margin (docs/05, Pilhas) */
#define UI_STACK_SIZE       6144
#define UI_INBOX_LEN        16
/* The other services acknowledge the shutdown within this; the system gives up at 5 s */
#define UI_SHUTDOWN_WAIT_MS 4000U

#define UI_DISPLAY_NODE     DT_CHOSEN(zephyr_display)
#define UI_JDI              (DT_NODE_HAS_COMPAT(UI_DISPLAY_NODE, jdi_lpm027m128b) || \
                             DT_NODE_HAS_COMPAT(UI_DISPLAY_NODE, jdi_lpm027m128c))

/* COM inversions per second (memlcd_set_com_hz): 1 with the light off, the
 * low-power setting of the datasheets; with the light on the JDI asks the
 * COM near 60 Hz (LPM027M128B 9.1.2: EXTCOMIN near 120 Hz). The Sharp asks
 * nothing for its front light: bench. */
#define UI_COM_HZ_DARK      1U
#if UI_JDI
#define UI_COM_HZ_LIGHT     120U
#else
#define UI_COM_HZ_LIGHT     UI_COM_HZ_DARK
#endif

/* The light of the display, when the board has one (alias backlight) */
#if DT_NODE_EXISTS(DT_ALIAS(backlight)) && defined(CONFIG_PWM)
#define UI_HAS_LIGHT 1
static const struct pwm_dt_spec light_pwm = PWM_DT_SPEC_GET(DT_ALIAS(backlight));
/* brightness, a starting point for the bench */
#define UI_LIGHT_PCT        50U
#else
#define UI_HAS_LIGHT 0
#endif

/* The supply of the light, when the board switches it (3V3BL of the nPM1300 LDO2) */
#if DT_NODE_EXISTS(DT_ALIAS(backlight_supply)) && defined(CONFIG_REGULATOR)
#define UI_HAS_LIGHT_SUPPLY 1
static const struct device *const light_supply = DEVICE_DT_GET(DT_ALIAS(backlight_supply));
#else
#define UI_HAS_LIGHT_SUPPLY 0
#endif

BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(UI_DISPLAY_NODE), "the interface needs a zephyr,display");
/* The actions of the screens are the commands of the application, in the same order */
BUILD_ASSERT((int)UI_ACT_SET_MODE == (int)APP_CMD_SET_MODE);
BUILD_ASSERT((int)UI_ACT_FORMAT == (int)APP_CMD_FORMAT);
BUILD_ASSERT((int)UI_ACT_KEY == (int)APP_CMD_KEY);
BUILD_ASSERT((int)UI_ACT_ROUTE_SELECT == (int)APP_CMD_ROUTE_SELECT);

struct ui_msg {
    const struct zbus_channel *chan;
    /* the snapshot is read from its channel; the rest comes by copy */
    union {
        struct app_system_state sys;
        struct app_notif notif;
        struct app_input input;
        struct app_ambient ambient;
        struct app_mode_state mode;
        struct app_shutdown_ack ack;
        struct app_dfu dfu;
    } u;
};

K_MSGQ_DEFINE(ui_inbox, sizeof(struct ui_msg), UI_INBOX_LEN, 4);

static void ui_listener(const struct zbus_channel *chan)
{
    struct ui_msg msg = {.chan = chan};
    size_t size = zbus_chan_msg_size(chan);

    if (size <= sizeof(msg.u)) {
        (void)memcpy(&msg.u, zbus_chan_const_msg(chan), size);
    }
    app_inbox_put(&ui_inbox, &msg, "ui");
}

ZBUS_LISTENER_DEFINE(ui_lis, ui_listener);
ZBUS_CHAN_ADD_OBS(chan_model_state, ui_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_notif, ui_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_input, ui_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_ambient, ui_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_system_state, ui_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_mode, ui_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_shutdown_ack, ui_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_dfu, ui_lis, 3);

/* ==========================================================================
 * Preferences of the interface (settings key ui/prefs)
 * ========================================================================== */

#define UI_PREFS_VERSION    1U

struct ui_prefs {
    uint8_t version;
    uint8_t theme;          /**< ui_theme_t */
    uint8_t light;          /**< 1: automatic light, 0: off */
    uint8_t lang;           /**< ui_lang_t */
};

static struct ui_prefs prefs;

static int prefs_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg,
                     void *param)
{
    struct ui_prefs *out = param;
    struct ui_prefs stored;
    const char *next;

    if ((key == NULL) || !settings_name_steq(key, "prefs", &next) || (next != NULL) ||
        (len != sizeof(stored))) {
        return 0;
    }
    if ((read_cb(cb_arg, &stored, sizeof(stored)) == (ssize_t)sizeof(stored)) &&
        (stored.version == UI_PREFS_VERSION) && (stored.theme <= (uint8_t)UI_THEME_MONO) &&
        (stored.lang <= (uint8_t)UI_LANG_EN)) {
        *out = stored;
    }
    return 0;
}

static void prefs_load(void)
{
    prefs.version = UI_PREFS_VERSION;
    /* the JDI shows the 8 colours, the Sharp black and white (docs/18) */
    prefs.theme = UI_JDI ? (uint8_t)UI_THEME_COLOR : (uint8_t)UI_THEME_MONO;
    prefs.light = 1U;
    prefs.lang = (uint8_t)UI_LANG_PT;
    (void)settings_load_subtree_direct("ui", prefs_set, &prefs);
}

static void prefs_save(void)
{
    int err = settings_save_one("ui/prefs", &prefs, sizeof(prefs));

    if (err != 0) {
        LOG_WRN("preferences not saved: %d", err);
    }
}

/* ==========================================================================
 * State of the thread
 * ========================================================================== */

static const struct device *const display = DEVICE_DT_GET(UI_DISPLAY_NODE);

/** The last snapshot of the model: static, off the stack of the thread */
static ui_model_t model;
static bool have_model;
static uint8_t mode = APP_MODE_ID_CRS;
static bool in_msc;

static backlight_t light;
static bool light_shown;

/* Light and theme asked by a screen, applied after the key is handled */
static bool want_light_toggle;
static bool want_theme_toggle;

static bool shutting_down;
static bool shutdown_done;
static uint32_t shutdown_ms;
static uint32_t shutdown_acks;

static char version[12];

static void on_action(ui_action_t action, int32_t arg, void *user)
{
    ARG_UNUSED(user);

    switch (action) {
    case UI_ACT_KEY:
        backlight_key(&light, k_uptime_get_32());
        break;
    case UI_ACT_LIGHT_TOGGLE:
        want_light_toggle = true;
        break;
    case UI_ACT_THEME_TOGGLE:
        want_theme_toggle = true;
        break;
    case UI_ACT_LAP:
        /* the two enumerations run together up to UI_ACT_ROUTE_SELECT, and
         * the commands the interface never sends come after it */
        app_command(APP_CMD_LAP, 0);
        break;
    case UI_ACT_ALARM_TOGGLE:
        app_command(APP_CMD_ALARM_TOGGLE, 0);
        break;
    default:
        app_command((enum app_cmd_id)action, arg);
        break;
    }
}

/** The light and the COM follow the state machine */
static void light_apply(void)
{
    bool on = backlight_is_on(&light);

    if (on == light_shown) {
        return;
    }
    light_shown = on;
#if UI_HAS_LIGHT_SUPPLY
    /* supply up before the PWM, down after it */
    if (on && device_is_ready(light_supply)) {
        (void)regulator_enable(light_supply);
    }
#endif
#if UI_HAS_LIGHT
    (void)pwm_set_pulse_dt(&light_pwm, on ? ((light_pwm.period * UI_LIGHT_PCT) / 100U) : 0U);
#endif
#if UI_HAS_LIGHT_SUPPLY
    if (!on && device_is_ready(light_supply)) {
        (void)regulator_disable(light_supply);
    }
#endif
#if defined(CONFIG_MEMLCD)
    (void)memlcd_set_com_hz(display, on ? UI_COM_HZ_LIGHT : UI_COM_HZ_DARK);
#endif
    LOG_DBG("light %s", on ? "on" : "off");
}

/** Light and theme toggled by a screen: after ui_key(), not inside it */
static void apply_local_actions(uint32_t now)
{
    if (want_theme_toggle) {
        want_theme_toggle = false;
        prefs.theme = (prefs.theme == (uint8_t)UI_THEME_COLOR) ? (uint8_t)UI_THEME_MONO
                                                               : (uint8_t)UI_THEME_COLOR;
        ui_set_theme((ui_theme_t)prefs.theme);
        prefs_save();
    }
    if (want_light_toggle) {
        want_light_toggle = false;
        prefs.light = (prefs.light != 0U) ? 0U : 1U;
        backlight_enable(&light, prefs.light != 0U);
        model.settings.light_auto = (prefs.light != 0U);
        if (have_model) {
            ui_update(&model, now);
        }
        prefs_save();
    }
}

static void on_snapshot(uint32_t now)
{
    if (zbus_chan_read(&chan_model_state, &model, K_MSEC(20)) != 0) {
        return;
    }
    /* the light belongs to this service */
    model.settings.light_auto = (prefs.light != 0U);
    if (!have_model) {
        /* the first snapshot leaves the boot screen for the page of the mode */
        have_model = true;
        ui_set_mode((ui_mode_t)mode);
    }
    if (!shutting_down) {
        ui_update(&model, now);
    }
}

static void on_system_state(uint8_t state, uint32_t now)
{
    if ((state == APP_SYS_SHUTDOWN) && !shutting_down) {
        shutting_down = true;
        shutdown_ms = now;
        shutdown_acks = 0U;
        ui_show(UI_SCREEN_SHUTDOWN);
        ui_set_progress(0U);
    } else if (state == APP_SYS_MSC) {
        in_msc = true;
        ui_show(UI_SCREEN_USB);
    } else if ((state == APP_SYS_ON) && in_msc) {
        in_msc = false;
        ui_show_pages();
    } else {
        /* boot and off: nothing to show */
    }
}

static void on_shutdown_ack(uint8_t svc)
{
    uint32_t others = (uint32_t)APP_SVC_COUNT - 1U;

#if !defined(CONFIG_USB_DEVICE_STACK_NEXT)
    others--;   /* no USB service on this target */
#endif

    if (!shutting_down || (svc >= (uint8_t)APP_SVC_COUNT) || (svc == (uint8_t)APP_SVC_UI)) {
        return;
    }
    shutdown_acks |= BIT(svc);
    ui_set_progress((uint8_t)((POPCOUNT(shutdown_acks) * 100U) / others));
}

/** The other services are done (or late): clear the panel and acknowledge */
static void shutdown_finish_if_ready(uint32_t now)
{
    uint32_t all = BIT_MASK(APP_SVC_COUNT) & ~BIT(APP_SVC_UI);

#if !defined(CONFIG_USB_DEVICE_STACK_NEXT)
    /* the USB service only exists where the stack is built in */
    all &= ~BIT(APP_SVC_USB);
#endif
    struct app_shutdown_ack ack = {.svc = APP_SVC_UI};

    if (!shutting_down || shutdown_done) {
        return;
    }
    if (((shutdown_acks & all) != all) && ((now - shutdown_ms) < UI_SHUTDOWN_WAIT_MS)) {
        return;
    }
    shutdown_done = true;
    backlight_enable(&light, false);
    light_apply();
#if defined(CONFIG_MEMLCD)
    (void)memlcd_power_off(display);
#else
    (void)display_clear(display);
    (void)display_blanking_on(display);
#endif
    (void)app_publish(&chan_shutdown_ack, &ack);
}

static void handle(const struct ui_msg *msg, uint32_t now)
{
    if (msg->chan == &chan_model_state) {
        on_snapshot(now);
    } else if (msg->chan == &chan_notif) {
        const struct app_notif *n = &msg->u.notif;

        ui_notify(n->title, n->text, (n->value[0] != '\0') ? n->value : NULL, n->good,
                  n->duration_ms, now);
    } else if (msg->chan == &chan_input) {
        if (!shutting_down) {
            ui_key((ui_key_t)msg->u.input.key,
                   msg->u.input.long_press ? UI_PRESS_LONG : UI_PRESS_SHORT, now);
            apply_local_actions(now);
        }
    } else if (msg->chan == &chan_ambient) {
        backlight_lux(&light, msg->u.ambient.lux);
    } else if (msg->chan == &chan_mode) {
        mode = msg->u.mode.mode;
        if (have_model && !shutting_down) {
            ui_set_mode((ui_mode_t)mode);
        }
    } else if (msg->chan == &chan_system_state) {
        on_system_state(msg->u.sys.state, now);
    } else if (msg->chan == &chan_shutdown_ack) {
        on_shutdown_ack(msg->u.ack.svc);
    } else if (msg->chan == &chan_dfu) {
        /* the update takes the screen while the image comes in (rf/dfu.c) */
        if (!shutting_down) {
            ui_set_dfu((ui_dfu_t)msg->u.dfu.phase, msg->u.dfu.percent);
        }
    } else {
        /* nothing else reaches this inbox */
    }
}

/** Without a working display: keep the watchdog fed and answer the shutdown */
static void run_headless(int wdt)
{
    struct ui_msg msg;

    for (;;) {
        if (app_inbox_get(&ui_inbox, &msg, wdt, APP_SVC_TICK_MS) != 0) {
            continue;
        }
        if ((msg.chan == &chan_system_state) && (msg.u.sys.state == APP_SYS_SHUTDOWN) &&
            !shutdown_done) {
            struct app_shutdown_ack ack = {.svc = APP_SVC_UI};

            shutdown_done = true;
            (void)app_publish(&chan_shutdown_ack, &ack);
        }
    }
}

static void ui_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct app_mode_state ms;
    struct ui_msg msg;
    int wdt = app_wdt_add("ui");
    uint32_t now = k_uptime_get_32();

    if (!device_is_ready(display) || (lv_display_get_default() == NULL)) {
        LOG_ERR("display not ready: the interface runs without screen");
        run_headless(wdt);
    }

    prefs_load();
    backlight_init(&light, prefs.light != 0U);
    (void)snprintf(version, sizeof(version), "%u.%u.%u", (unsigned int)APP_VERSION_MAJOR,
                   (unsigned int)APP_VERSION_MINOR, (unsigned int)APP_VERSION_PATCH);
    if (zbus_chan_read(&chan_mode, &ms, K_NO_WAIT) == 0) {
        mode = ms.mode;
    }

    const ui_config_t cfg = {
        .theme = (ui_theme_t)prefs.theme,
        .lang = (ui_lang_t)prefs.lang,
        .version = version,
        .on_action = on_action,
        .user = NULL,
    };

    if (ui_init(&cfg, now) != 0) {
        LOG_ERR("interface not started");
        run_headless(wdt);
    }
    /* the boot screen reaches the panel before the picture is turned on */
    (void)lv_timer_handler();
    (void)display_blanking_off(display);

    for (;;) {
        uint32_t wait = lv_timer_handler();

        if (wait > APP_SVC_TICK_MS) {
            wait = APP_SVC_TICK_MS;
        }
        if (app_inbox_get(&ui_inbox, &msg, wdt, wait) == 0) {
            handle(&msg, k_uptime_get_32());
        }
        now = k_uptime_get_32();
        ui_tick(now);
        backlight_tick(&light, now);
        light_apply();
        shutdown_finish_if_ready(now);
    }
}

K_THREAD_DEFINE(ui_tid, UI_STACK_SIZE, ui_thread, NULL, NULL, NULL, APP_PRIO_UI, 0,
                SYS_FOREVER_MS);

void ui_svc_start(void)
{
    k_thread_name_set(ui_tid, "ui");
    k_thread_start(ui_tid);
}
