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

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "app/app_channels.h"
#include "app/app_cmd.h"
#include "app/app_svc.h"
#include "rf/ant.h"
#include "rf/ant_sensors.h"
#include "rf/power_ant.h"
#include "rf/radar_ant.h"
#include "rf/ble_ancs_client.h"
#include "rf/ble_bsc_client.h"
#include "rf/ble_cps_client.h"
#include "rf/ble_fec_client.h"
#include "model/komoot_turn.h"
#include "rf/ble_hrs_client.h"
#include "rf/ble_komoot_client.h"
#include "rf/ble_lns_client.h"
#include "rf/ble_manager.h"
#include "rf/ble_radar_client.h"
#include "rf/ble_nus.h"
#include "rf/dfu.h"
#include "rf/file_xfer.h"
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

/**
 * One frame of the rear radar.
 *
 * Runs on the BT RX thread: it only copies into the channel message and
 * publishes, as the rule for radio callbacks says (docs/05, threads).
 */
static void radar_frame(const struct radar_frame *f)
{
    struct app_radar msg = {.uptime_ms = k_uptime_get_32(), .n = f->n, .linked = true};

    for (uint8_t i = 0U; (i < f->n) && (i < RADAR_TARGETS_MAX); i++) {
        msg.id[i] = f->t[i].id;
        msg.range_m[i] = f->t[i].range_m;
        msg.closing_kmh[i] = f->t[i].closing_kmh;
        msg.level[i] = f->t[i].level;
        msg.side[i] = f->t[i].side;
    }
    (void)app_publish(&chan_radar, &msg);
}

static void radar_link(bool linked)
{
    struct app_radar msg = {.uptime_ms = k_uptime_get_32(), .n = 0U, .linked = linked};

    publish_link(APP_EXT_RADAR, linked);
    (void)app_publish(&chan_radar, &msg);
}

/**
 * One notification from the rider's power meter.
 *
 * The speed and the cadence of the meter ride along: many meters count the
 * crank, and a hub meter counts the wheel, so a rider with one may not need
 * a separate cadence sensor at all. A zero means the meter does not count
 * that thing, and the model keeps whatever the other sensors gave.
 */
static void cps_data(const cps_info_t *info)
{
    /*
     * The meter reports signed watts and the event carries unsigned ones.
     * A negative reading means the rider is not driving the pedals — a
     * freewheel going downhill, or the meter's own drift around zero — and
     * counts as no power, which is what the zones and the normalised power
     * would do with it anyway.
     */
    uint16_t watts = (info->power_w > 0) ? (uint16_t)info->power_w : 0U;

    struct app_ext_sensor e = {.uptime_ms = k_uptime_get_32(), .kind = APP_EXT_POWER,
                               .power_w = watts,
                               .cadence_rpm = info->cadence_rpm,
                               .speed_kmh100 = info->speed_kmh100};

    (void)app_publish(&chan_ext_sensor, &e);
}

static void cps_conn(bool connected)
{
    publish_link(APP_EXT_POWER, connected);
}

/** A notification from the phone that got past the filter */
static void ancs_notification(const struct ancs_notification *n)
{
    /*
     * A call is marked good so the screen gives it the colour it gives a
     * personal record: it is the one worth taking a hand off the bars for.
     */
    app_notify(n->title, n->message, NULL, n->is_call, 0U);
}

static void ancs_conn(bool connected)
{
    LOG_INF("phone notifications %s", connected ? "on" : "off");
}

/**
 * One navigation update from the Komoot application on the phone.
 *
 * The client was started and connected and nothing ever read it: the
 * navigation never reached the model, so the turn on the screen only ever
 * came from a route file. The twenty-four directions of the application
 * become the nine arrows of this screen in `model/komoot_turn.c`.
 */
static void komoot_nav(const komoot_nav_t *nav)
{
    struct app_phone_nav msg = {0};

    if ((nav != NULL) && komoot_turn_is_navigation((uint8_t)nav->direction)) {
        msg.valid = true;
        msg.turn = komoot_turn_of((uint8_t)nav->direction);
        msg.dist_m = komoot_turn_distance(nav->distance);
        (void)strncpy(msg.street, nav->street_name, sizeof(msg.street) - 1U);
    }

    (void)app_publish(&chan_phone_nav, &msg);
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

/** Runs in the Bluetooth receive thread: reads and publishes, nothing else */
static void nus_rx(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; i++) {
        if (cmd_parser_feed(&nus_parser, (char)data[i]) != CMD_NONE) {
            app_cmd_handle(cmd_parser_data(&nus_parser));
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

    /*
     * The rear radar over ANT+. In this repository it answers -ENOTSUP,
     * because the channel parameters of the ANT+ Bike Radar profile are not
     * here and `CONFIG_GNSS_ANT_RADAR_DEV_TYPE` is zero: the profile is
     * under a licence that forbids redistributing it and this repository is
     * public (`rf/radar_ant.h`). The call is the hook the owner needs, and
     * the radar over BLE below works either way.
     */
    int radar_ant = radar_ant_start();

    if ((radar_ant != 0) && (radar_ant != -ENOTSUP)) {
        LOG_WRN("ANT radar start failed (%d)", radar_ant);
    }

    /* and the rider's power meter, on the same terms (`rf/power_ant.h`) */
    int power_ant = power_ant_start();

    if ((power_ant != 0) && (power_ant != -ENOTSUP)) {
        LOG_WRN("ANT power start failed (%d)", power_ant);
    }

    /* and the two the legacy rode with: the strap and the cadence sensor */
    int ant_sens = ant_sensors_start();

    if ((ant_sens != 0) && (ant_sens != -ENOTSUP)) {
        LOG_WRN("ANT sensors start failed (%d)", ant_sens);
    }
#endif
    if (ble_manager_init() != APP_OK) {
        LOG_ERR("BLE start failed");
        return;
    }
    ble_hrs_client_register_callback(hrs_data);
    ble_hrs_client_register_conn_callback(hrs_conn);
    (void)ble_radar_client_init(radar_frame, radar_link);
    ble_bsc_client_register_callback(bsc_data);
    ble_bsc_client_register_conn_callback(bsc_conn);
    if (ble_ancs_client_init() != APP_OK) {
        LOG_WRN("phone notifications not available");
    }
    ble_ancs_client_register_callback(ancs_notification);
    ble_ancs_client_register_conn_callback(ancs_conn);
    (void)ble_komoot_client_register_callback(komoot_nav);
    (void)ble_cps_client_init();
    (void)ble_lns_client_init();
    ble_cps_client_register_callback(cps_data);
    ble_cps_client_register_conn_callback(cps_conn);
    ble_fec_client_register_callback(fec_data);
    ble_fec_client_register_conn_callback(fec_conn);

    /* the commands of the legacy come in over the Nordic UART Service */
    cmd_parser_init(&nus_parser);
    (void)ble_nus_register_callback(nus_rx);

    /* update over the air: the SMP service rides on this same stack */
    if (rf_dfu_init() != APP_OK) {
        LOG_ERR("DFU start failed");
    }
    /* routes and segments the phone sends, over the same SMP link */
    (void)rf_file_xfer_init();

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
