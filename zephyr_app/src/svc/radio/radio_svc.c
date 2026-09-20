/**
 * @file radio_svc.c
 * @brief Radio service: ANT and BLE start, sensors into events
 *
 * docs/16-arquitetura-firmware.md (Serviços, Rádio). The thread brings up
 * ANT (with ANT=1) before BLE, as the sdk-ant sample and the legacy
 * (main.cpp:480-493), and turns the data of the sensor clients into
 * chan_ext_sensor and chan_link_status. The client callbacks run in the BT
 * receive thread: they only copy and publish.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "app/app_channels.h"
#include "app/app_svc.h"
#include "rf/ant.h"
#include "rf/ble_bsc_client.h"
#include "rf/ble_fec_client.h"
#include "rf/ble_hrs_client.h"
#include "rf/ble_manager.h"
#include "rf/ble_nus.h"
#include "rf/dfu.h"
#include "model/cmd_parser.h"

LOG_MODULE_REGISTER(radio_svc, CONFIG_LOG_DEFAULT_LEVEL);

/* bt_enable() and the settings load of the bonds run here: about 1.5 KB */
#define RADIO_STACK_SIZE    3072
#define RADIO_INBOX_LEN     8

struct radio_msg {
    const struct zbus_channel *chan;
    union {
        struct app_system_state sys;
        struct app_system_cmd cmd;
        struct app_power_status power;
        struct app_mode_state mode;
    } u;
};

K_MSGQ_DEFINE(radio_inbox, sizeof(struct radio_msg), RADIO_INBOX_LEN, 4);

static void radio_listener(const struct zbus_channel *chan)
{
    struct radio_msg msg = {.chan = chan};

    if (chan == &chan_system_cmd) {
        const struct app_system_cmd *cmd = zbus_chan_const_msg(chan);

        if ((cmd->id != APP_CMD_PAIR_START) && (cmd->id != APP_CMD_PAIR_SELECT) &&
            (cmd->id != APP_CMD_PAIR_CANCEL)) {
            return;
        }
    }
    if (zbus_chan_msg_size(chan) > sizeof(msg.u)) {
        return;
    }
    (void)memcpy(&msg.u, zbus_chan_const_msg(chan), zbus_chan_msg_size(chan));
    app_inbox_put(&radio_inbox, &msg, "radio");
}

ZBUS_LISTENER_DEFINE(radio_lis, radio_listener);
ZBUS_CHAN_ADD_OBS(chan_system_state, radio_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_system_cmd, radio_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_power_status, radio_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_mode, radio_lis, 3);

/* ---- client callbacks, in the BT receive thread -------------------------- */

static void publish_link(uint8_t kind, bool connected)
{
    struct app_link_status l = {
        .kind = kind,
        .link = connected ? APP_LINK_CONNECTED : APP_LINK_LOST,
        .ant = false,
    };

    (void)app_publish(&chan_link_status, &l);
}

static void hrs_data(uint8_t bpm, uint16_t rr_interval)
{
    struct app_ext_sensor e = {.uptime_ms = k_uptime_get_32(), .kind = APP_EXT_HR,
                               .hr_bpm = bpm, .rr_ms = rr_interval};

    (void)app_publish(&chan_ext_sensor, &e);
}

static void hrs_conn(bool connected)
{
    publish_link(APP_EXT_HR, connected);
}

static void bsc_data(uint16_t speed, uint8_t cadence)
{
    struct app_ext_sensor e = {.uptime_ms = k_uptime_get_32(), .kind = APP_EXT_BSC,
                               .cadence_rpm = cadence, .speed_kmh100 = speed};

    (void)app_publish(&chan_ext_sensor, &e);
}

static void bsc_conn(bool connected)
{
    publish_link(APP_EXT_BSC, connected);
}

static void fec_data(const fec_data_t *data)
{
    fec_info_t info;
    struct app_ext_sensor e = {.uptime_ms = k_uptime_get_32(), .kind = APP_EXT_FEC};

    ARG_UNUSED(data);
    if (ble_fec_client_get_info(&info) != APP_OK) {
        return;
    }
    e.power_w = info.power;
    e.cadence_rpm = info.cadence;
    e.elapsed_s = info.el_time;
    e.grade_pct = info.grade;
    (void)app_publish(&chan_ext_sensor, &e);
}

static void fec_conn(bool connected)
{
    publish_link(APP_EXT_FEC, connected);
}

/* ---- commands of the legacy over the Nordic UART Service --------------------- */

/** One reader for the radio; the USB serial will get its own */
static struct cmd_parser nus_parser;

static void handle_command(const struct cmd_data *d)
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

/** Runs in the Bluetooth receive thread: reads and publishes, nothing else */
static void nus_rx(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; i++) {
        if (cmd_parser_feed(&nus_parser, (char)data[i]) != CMD_NONE) {
            handle_command(cmd_parser_data(&nus_parser));
        }
    }
}

/* ---- thread ----------------------------------------------------------------- */

static void radio_start(void)
{
#if defined(CONFIG_ANT)
    /* ANT before bt_enable(), as the sdk-ant sample with BLE and ANT */
    if (rf_ant_init() != APP_OK) {
        LOG_ERR("ANT start failed");
    }
#endif
    if (ble_manager_init() != APP_OK) {
        LOG_ERR("BLE start failed");
        return;
    }
    ble_hrs_client_register_callback(hrs_data);
    ble_hrs_client_register_conn_callback(hrs_conn);
    ble_bsc_client_register_callback(bsc_data);
    ble_bsc_client_register_conn_callback(bsc_conn);
    ble_fec_client_register_callback(fec_data);
    ble_fec_client_register_conn_callback(fec_conn);

    /* the commands of the legacy come in over the Nordic UART Service */
    cmd_parser_init(&nus_parser);
    (void)ble_nus_register_callback(nus_rx);

    /* update over the air: the SMP service rides on this same stack */
    if (rf_dfu_init() != APP_OK) {
        LOG_ERR("DFU start failed");
    }

    /*
     * Nothing was looking for anything until here: the manager only
     * registered its callbacks. The device announces itself so the phone
     * app can reach it (SMP for the update, NUS for the commands) and looks
     * for the sensors of the rider, as the legacy does when a mode starts.
     */
    if (ble_manager_start_advertising() != APP_OK) {
        LOG_WRN("advertising did not start");
    }
    if (ble_manager_start_scan() != APP_OK) {
        LOG_WRN("scan did not start");
    }
}

static void radio_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct radio_msg msg;
    int16_t last_batt = -1;
    bool done = false;
    bool ride_active = false;
    bool usb_present = false;
    uint8_t batt_pct = 100U;

    radio_start();

    int wdt = app_wdt_add("radio");

    for (;;) {
        if (app_inbox_get(&radio_inbox, &msg, wdt, APP_SVC_TICK_MS) != 0) {
            continue;
        }
        if (msg.chan == &chan_system_state) {
            if ((msg.u.sys.state == APP_SYS_SHUTDOWN) && !done) {
                done = true;
                (void)ble_manager_stop_scan();
                (void)ble_manager_disconnect();

                struct app_shutdown_ack ack = {.svc = APP_SVC_RADIO};

                (void)app_publish(&chan_shutdown_ack, &ack);
            }
        } else if (msg.chan == &chan_power_status) {
            /* the Battery Service follows the gauge, only on change */
            if (msg.u.power.gauge && ((int16_t)msg.u.power.pct != last_batt)) {
                last_batt = (int16_t)msg.u.power.pct;
                ble_manager_update_battery(msg.u.power.pct);
            }
            usb_present = msg.u.power.vbus;
            batt_pct = msg.u.power.gauge ? msg.u.power.pct : 100U;
            rf_dfu_set_conditions(ride_active, usb_present, batt_pct);
        } else if (msg.chan == &chan_mode) {
            /* an update is refused in the middle of a ride (rf/dfu.c) */
            ride_active = msg.u.mode.recording;
            rf_dfu_set_conditions(ride_active, usb_present, batt_pct);
        } else if (msg.chan == &chan_system_cmd) {
            /* pairing: the radio step (docs/17, Pareamento) */
            LOG_INF("pairing command %u", msg.u.cmd.id);
        } else {
            /* nothing else reaches this inbox */
        }
    }
}

K_THREAD_DEFINE(radio_tid, RADIO_STACK_SIZE, radio_thread, NULL, NULL, NULL, APP_PRIO_RADIO, 0,
                SYS_FOREVER_MS);

void radio_svc_start(void)
{
    k_thread_name_set(radio_tid, "radio");
    k_thread_start(radio_tid);
}
