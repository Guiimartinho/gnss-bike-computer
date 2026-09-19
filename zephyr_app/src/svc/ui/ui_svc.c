/**
 * @file ui_svc.c
 * @brief Interface service: the thread of the screen
 *
 * docs/16-arquitetura-firmware.md (Threads, Interface). The thread takes the
 * snapshots of the model, the notifications and the keys, and answers the
 * shutdown once the screen is clear. The LVGL screens of zephyr_app/src/ui
 * and the display driver join it in the interface step; until then it keeps
 * the last snapshot and logs the keys.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "app/app_channels.h"
#include "app/app_svc.h"
#include "ui/ui_model.h"

LOG_MODULE_REGISTER(ui_svc, CONFIG_LOG_DEFAULT_LEVEL);

/* zbus read and logs, about 0.6 KB; LVGL joins it in the interface step */
#define UI_STACK_SIZE       2048
#define UI_INBOX_LEN        16

struct ui_msg {
    const struct zbus_channel *chan;
    /* the snapshot is read from its channel; the rest comes by copy */
    union {
        struct app_system_state sys;
        struct app_notif notif;
        struct app_input input;
        struct app_ambient ambient;
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

/** The last snapshot of the model */
static ui_model_t model;

/** Last ambient light, for the backlight machine of the interface step */
static float ambient_lux;

static void ui_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct ui_msg msg;
    bool done = false;

    int wdt = app_wdt_add("ui");

    for (;;) {
        if (app_inbox_get(&ui_inbox, &msg, wdt, APP_SVC_TICK_MS) != 0) {
            continue;
        }
        if (msg.chan == &chan_model_state) {
            (void)zbus_chan_read(&chan_model_state, &model, K_MSEC(20));
        } else if (msg.chan == &chan_notif) {
            LOG_INF("notification: %s %s %s", msg.u.notif.title, msg.u.notif.text,
                    msg.u.notif.value);
        } else if (msg.chan == &chan_input) {
            LOG_INF("key %u%s", msg.u.input.key, msg.u.input.long_press ? " long" : "");
        } else if ((msg.chan == &chan_system_state) && (msg.u.sys.state == APP_SYS_SHUTDOWN) &&
                   !done) {
            done = true;

            struct app_shutdown_ack ack = {.svc = APP_SVC_UI};

            (void)app_publish(&chan_shutdown_ack, &ack);
        } else if (msg.chan == &chan_ambient) {
            ambient_lux = msg.u.ambient.lux;
        } else {
            /* nothing else reaches this inbox */
        }
    }
}

K_THREAD_DEFINE(ui_tid, UI_STACK_SIZE, ui_thread, NULL, NULL, NULL, APP_PRIO_UI, 0,
                SYS_FOREVER_MS);

void ui_svc_start(void)
{
    k_thread_name_set(ui_tid, "ui");
    k_thread_start(ui_tid);
}
