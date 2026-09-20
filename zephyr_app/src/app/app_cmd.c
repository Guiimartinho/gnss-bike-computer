/**
 * @file app_cmd.c
 * @brief What the device does with a command sentence of the legacy
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/app_channels.h"
#include "app/app_cmd.h"
#include "rf/ble_nus.h"

LOG_MODULE_REGISTER(app_cmd, CONFIG_LOG_DEFAULT_LEVEL);

void app_cmd_handle(const struct cmd_data *d)
{
    switch (d->kind) {
    case CMD_LOC: {
        /* a position given by a PC, the SIM source of the legacy */
        struct app_gnss_fix fix = {
            .uptime_ms = k_uptime_get_32(),
            .fix = true,
            .sim = true,
            .mode = APP_GNSS_MODE_FULL,
            .nsat = 0U,
            .lat_e7 = d->lat_e7,
            .lon_e7 = d->lon_e7,
            .alt_mm = d->ele_cm * 10,
            .speed_mms = (uint32_t)((d->speed_cms > 0) ? (d->speed_cms * 10) : 0),
            .time_valid = false,
        };

        (void)app_publish(&chan_gnss_fix, &fix);
        break;
    }

    case CMD_HRM: {
        struct app_ext_sensor e = {
            .uptime_ms = k_uptime_get_32(),
            .kind = APP_EXT_HR,
            .hr_bpm = (uint8_t)d->bpm,
            .rr_ms = d->rr_ms,
        };

        (void)app_publish(&chan_ext_sensor, &e);
        break;
    }

    case CMD_CAD: {
        struct app_ext_sensor e = {
            .uptime_ms = k_uptime_get_32(),
            .kind = APP_EXT_BSC,
            .cadence_rpm = (uint8_t)d->rpm,
            .speed_kmh100 = d->cad_speed,
        };

        (void)app_publish(&chan_ext_sensor, &e);
        break;
    }

    case CMD_BTN: {
        struct app_input in = {
            .key = (uint8_t)((d->code <= (uint8_t)APP_KEY_RIGHT) ? d->code : APP_KEY_CENTER),
            .long_press = false,
        };

        (void)app_publish(&chan_input, &in);
        break;
    }

    case CMD_ANCS:
        app_notify(d->title, d->text, NULL, true, 0U);
        break;

    case CMD_DBG:
        LOG_INF("debug message from the PC: %u %s", (unsigned int)d->code, d->text);
        app_notify("DBG", d->text, NULL, true, 0U);
        break;

    case CMD_DWN: {
        if (!cmd_dwn_allowed(d->code)) {
            /* formatting and the tests only come from the menu, where the
             * rider confirms them on the screen */
            LOG_WRN("order %u refused over the radio", (unsigned int)d->code);
            app_notify("PC", "Comando recusado", NULL, false, 0U);
            break;
        }

        struct app_system_cmd cmd = {
            .id = (d->code == (uint8_t)CMD_DWN_MSC) ? (uint8_t)APP_CMD_MSC
                                                    : (uint8_t)APP_CMD_CALIB_COMPASS,
            .arg = 0,
        };

        (void)app_publish(&chan_system_cmd, &cmd);
        break;
    }

    case CMD_QRY:
        /* listing and sending files is the piece that is still missing */
        LOG_WRN("query %u is not answered yet", (unsigned int)d->qry_type);
        (void)ble_nus_send_str("$QRY,0\r\n");
        break;

    default:
        break;
    }
}
