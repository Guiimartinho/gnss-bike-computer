/**
 * @file app_cmd.c
 * @brief What the device does with a command sentence of the legacy
 */

#include <errno.h>

#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/app_channels.h"
#include "app/app_cmd.h"
#include "model/file_policy.h"
#include "model/qry.h"
#include "rf/ble_nus.h"

LOG_MODULE_REGISTER(app_cmd, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Answer a `$QRY` (model/qry.h)
 *
 * Listing walks the root of the storage; erasing goes through the policy of
 * `model/file_policy.h`, which is what keeps an activity from being taken
 * away by a sentence. Sending a file is refused and points at the SMP link,
 * which already does it with offsets, a checksum and a resume.
 */
static void handle_qry(const struct cmd_data *d)
{
    char line[QRY_REPLY_LEN];

    switch ((enum cmd_qry_type)d->qry_type) {
    case CMD_QRY_LIST: {
#if defined(CONFIG_FILE_SYSTEM)
        struct fs_dir_t dir;
        struct fs_dirent entry;
        uint32_t count = 0U;

        fs_dir_t_init(&dir);
        if (fs_opendir(&dir, FILE_POLICY_ROOT) != 0) {
            (void)qry_format_error(line, sizeof(line), (uint8_t)CMD_QRY_LIST, QRY_ERR_IO);
            (void)ble_nus_send_str(line);
            break;
        }
        while ((fs_readdir(&dir, &entry) == 0) && (entry.name[0] != 0)) {
            if (entry.type != FS_DIR_ENTRY_FILE) {
                continue;
            }
            if (qry_format_entry(line, sizeof(line), entry.name, (uint32_t)entry.size) > 0U) {
                (void)ble_nus_send_str(line);
                count++;
            }
        }
        (void)fs_closedir(&dir);
        (void)qry_format_end(line, sizeof(line), count);
        (void)ble_nus_send_str(line);
#else
        (void)qry_format_error(line, sizeof(line), (uint8_t)CMD_QRY_LIST, QRY_ERR_IO);
        (void)ble_nus_send_str(line);
#endif
        break;
    }

    case CMD_QRY_SEND:
        /* on purpose: mcumgr over SMP already does this properly */
        (void)qry_format_error(line, sizeof(line), (uint8_t)CMD_QRY_SEND, QRY_ERR_USE_SMP);
        (void)ble_nus_send_str(line);
        break;

    case CMD_QRY_ERASE: {
        char path[80];
        enum qry_error err = qry_check_erase(d->name, path, sizeof(path));

#if defined(CONFIG_FILE_SYSTEM)
        if (err == QRY_ERR_NONE) {
            int rc = fs_unlink(path);

            if (rc == -ENOENT) {
                err = QRY_ERR_NOTFOUND;
            } else if (rc != 0) {
                err = QRY_ERR_IO;
            } else {
                LOG_INF("%s erased over the wire", path);
            }
        }
#else
        if (err == QRY_ERR_NONE) {
            err = QRY_ERR_IO;
        }
#endif
        if (err == QRY_ERR_NONE) {
            (void)qry_format_ok(line, sizeof(line), (uint8_t)CMD_QRY_ERASE);
        } else {
            (void)qry_format_error(line, sizeof(line), (uint8_t)CMD_QRY_ERASE, err);
        }
        (void)ble_nus_send_str(line);
        break;
    }

    default:
        (void)qry_format_error(line, sizeof(line), d->qry_type, QRY_ERR_UNKNOWN);
        (void)ble_nus_send_str(line);
        break;
    }
}

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
        handle_qry(d);
        break;

    default:
        break;
    }
}
