/**
 * @file app_channels.c
 * @brief Definition of the zbus channels (docs/16-arquitetura-firmware.md, Eventos)
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "app/app_channels.h"
#include "ui/ui_model.h"

LOG_MODULE_REGISTER(app_chan, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * Publishers wait at most this long for a channel another thread is
 * publishing on: the listeners only copy the message into a queue.
 */
#define APP_PUB_TIMEOUT     K_MSEC(50)

ZBUS_CHAN_DEFINE(chan_gnss_fix, struct app_gnss_fix, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_gnss_sky, struct app_gnss_sky, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_baro, struct app_baro, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_imu, struct app_imu, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_mag, struct app_mag, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_ambient, struct app_ambient, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_ext_sensor, struct app_ext_sensor, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_link_status, struct app_link_status, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_pair_list, struct app_pair_list, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_phone_nav, struct app_phone_nav, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_power_status, struct app_power_status, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_system_cmd, struct app_system_cmd, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_system_state, struct app_system_state, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_mode, struct app_mode_state, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_shutdown_ack, struct app_shutdown_ack, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_notif, struct app_notif, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_log_point, struct app_log_point, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_input, struct app_input, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_storage_info, struct app_storage_info, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));
ZBUS_CHAN_DEFINE(chan_model_state, ui_model_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
                 ZBUS_MSG_INIT(0));

int app_publish(const struct zbus_channel *chan, const void *msg)
{
    int err = zbus_chan_pub(chan, msg, APP_PUB_TIMEOUT);

    if (err != 0) {
        LOG_WRN("publish on %s failed: %d", zbus_chan_name(chan), err);
    }
    return err;
}

void app_notify(const char *title, const char *text, const char *value, bool good,
                uint16_t duration_ms)
{
    struct app_notif n = {.good = good, .duration_ms = duration_ms};

    if (title != NULL) {
        (void)strncpy(n.title, title, sizeof(n.title) - 1U);
    }
    if (text != NULL) {
        (void)strncpy(n.text, text, sizeof(n.text) - 1U);
    }
    if (value != NULL) {
        (void)strncpy(n.value, value, sizeof(n.value) - 1U);
    }
    (void)app_publish(&chan_notif, &n);
}

void app_command(enum app_cmd_id id, int32_t arg)
{
    struct app_system_cmd cmd = {.id = (uint8_t)id, .arg = arg};

    (void)app_publish(&chan_system_cmd, &cmd);
}
