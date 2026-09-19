/**
 * @file app_channels.h
 * @brief zbus channels of the application (docs/16-arquitetura-firmware.md, Eventos)
 *
 * The channels carry copies: whoever publishes does not wait for whoever
 * reads. Each service adds its own listener to the channels it needs with
 * ZBUS_CHAN_ADD_OBS() in its own file, so the channel list stays free of
 * who listens.
 */

#ifndef APP_CHANNELS_H
#define APP_CHANNELS_H

#include <zephyr/zbus/zbus.h>

#include "app/app_events.h"

ZBUS_CHAN_DECLARE(chan_gnss_fix,        /* struct app_gnss_fix: GNSS -> model */
                  chan_gnss_sky,        /* struct app_gnss_sky: GNSS -> model */
                  chan_baro,            /* struct app_baro: sensors -> model */
                  chan_imu,             /* struct app_imu: sensors -> model */
                  chan_mag,             /* struct app_mag: sensors -> model */
                  chan_ambient,         /* struct app_ambient: sensors -> interface */
                  chan_ext_sensor,      /* struct app_ext_sensor: radio -> model */
                  chan_link_status,     /* struct app_link_status: radio -> model */
                  chan_pair_list,       /* struct app_pair_list: radio -> model */
                  chan_phone_nav,       /* struct app_phone_nav: radio -> model */
                  chan_power_status,    /* struct app_power_status: energy -> model, interface */
                  chan_system_cmd,      /* struct app_system_cmd: interface, services -> all */
                  chan_system_state,    /* struct app_system_state: energy -> all */
                  chan_mode,            /* struct app_mode_state: model -> all */
                  chan_shutdown_ack,    /* struct app_shutdown_ack: services -> energy */
                  chan_notif,           /* struct app_notif: anyone -> interface */
                  chan_log_point,       /* struct app_log_point: model -> storage */
                  chan_input,           /* struct app_input: keys -> interface */
                  chan_storage_info,    /* struct app_storage_info: storage -> model */
                  chan_model_state);    /* ui_model_t: model -> interface */

/**
 * @brief Publish a message, logging a failure
 *
 * Every channel is published without waiting: a service never blocks on
 * another one. A failure means the channel is being read at that instant,
 * which the next epoch covers.
 */
int app_publish(const struct zbus_channel *chan, const void *msg);

/**
 * @brief Publish a notification (legacy Vue::addNotif)
 *
 * @param title Title, or NULL
 * @param text Message
 * @param value Value on the right, or NULL
 * @param good Colour of the value: ahead (true) or behind
 * @param duration_ms 0 for the interface default
 */
void app_notify(const char *title, const char *text, const char *value, bool good,
                uint16_t duration_ms);

/**
 * @brief Publish a system command (the actions of the interface)
 */
void app_command(enum app_cmd_id id, int32_t arg);

#endif /* APP_CHANNELS_H */
