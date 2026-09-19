/**
 * @file gnss_svc.c
 * @brief GNSS service: the receiver through the Zephyr GNSS API
 *
 * docs/16-arquitetura-firmware.md (Serviços, GNSS). The receiver is the
 * device of the alias gnss: any driver of the Zephyr GNSS API (the u-blox
 * M10 by UBX on the new board, an NMEA module through gnss-nmea-generic).
 * Its callbacks run in the modem work queue and only convert and publish;
 * the thread follows the mode: the legacy wakes the GPS in CRS and PRC
 * (BoucleCRS.cpp:38) and puts it to sleep in FEC (BoucleFEC.cpp:45).
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gnss.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/zbus/zbus.h>

#include "app/app_channels.h"
#include "app/app_svc.h"

LOG_MODULE_REGISTER(gnss_svc, CONFIG_LOG_DEFAULT_LEVEL);

/* a zbus publish and a log call, about 0.7 KB; the UBX commands join it */
#define GNSS_STACK_SIZE     2048
#define GNSS_INBOX_LEN      8

#define GNSS_NODE           DT_ALIAS(gnss)

struct gnss_msg {
    const struct zbus_channel *chan;
    union {
        struct app_mode_state mode;
        struct app_system_state sys;
    } u;
};

K_MSGQ_DEFINE(gnss_inbox, sizeof(struct gnss_msg), GNSS_INBOX_LEN, 4);

/** enum app_gnss_mode, read by the callbacks in the modem work queue */
static atomic_t gnss_mode = ATOMIC_INIT(APP_GNSS_MODE_ACQ);

static void gnss_listener(const struct zbus_channel *chan)
{
    struct gnss_msg msg = {.chan = chan};

    if (zbus_chan_msg_size(chan) > sizeof(msg.u)) {
        return;
    }
    (void)memcpy(&msg.u, zbus_chan_const_msg(chan), zbus_chan_msg_size(chan));
    app_inbox_put(&gnss_inbox, &msg, "gnss");
}

ZBUS_LISTENER_DEFINE(gnss_lis, gnss_listener);
ZBUS_CHAN_ADD_OBS(chan_mode, gnss_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_system_state, gnss_lis, 3);

#if DT_NODE_HAS_STATUS(GNSS_NODE, okay)

static void gnss_data_cb(const struct device *dev, const struct gnss_data *data)
{
    ARG_UNUSED(dev);

    uint8_t mode = (uint8_t)atomic_get(&gnss_mode);

    if (mode == APP_GNSS_MODE_BACKUP) {
        return;
    }

    bool fix = (data->info.fix_status == GNSS_FIX_STATUS_GNSS_FIX) ||
               (data->info.fix_status == GNSS_FIX_STATUS_DGNSS_FIX);
    struct app_gnss_fix f = {
        .uptime_ms = k_uptime_get_32(),
        .fix = fix,
        .mode = mode,
        .nsat = (uint8_t)MIN(data->info.satellites_cnt, 255U),
        .lat_e7 = (int32_t)(data->nav_data.latitude / 100),
        .lon_e7 = (int32_t)(data->nav_data.longitude / 100),
        .alt_mm = data->nav_data.altitude,
        .speed_mms = data->nav_data.speed,
        .course_mdeg = data->nav_data.bearing,
        .hdop_milli = data->info.hdop,
        .time_valid = (data->utc.month != 0U),
        .hour = data->utc.hour,
        .minute = data->utc.minute,
        .millisecond = data->utc.millisecond,
        .day = data->utc.month_day,
        .month = data->utc.month,
        .year2 = data->utc.century_year,
    };

    /* the first fix leaves the acquisition; LEAP or full power is the UBX step */
    if (fix && (mode == APP_GNSS_MODE_ACQ)) {
        (void)atomic_cas(&gnss_mode, APP_GNSS_MODE_ACQ, APP_GNSS_MODE_FULL);
    }
    (void)app_publish(&chan_gnss_fix, &f);
}

GNSS_DATA_CALLBACK_DEFINE(DEVICE_DT_GET(GNSS_NODE), gnss_data_cb);

#if defined(CONFIG_GNSS_SATELLITES)

static struct app_gnss_sky sky;

static uint8_t sys_of(enum gnss_system system)
{
    switch (system) {
    case GNSS_SYSTEM_GALILEO:
        return APP_SYS_GALILEO;
    case GNSS_SYSTEM_BEIDOU:
        return APP_SYS_BEIDOU;
    case GNSS_SYSTEM_QZSS:
        return APP_SYS_QZSS;
    case GNSS_SYSTEM_GLONASS:
        return APP_SYS_GLONASS;
    case GNSS_SYSTEM_SBAS:
        return APP_SYS_SBAS;
    default:
        return APP_SYS_GPS;
    }
}

static void gnss_sat_cb(const struct device *dev, const struct gnss_satellite *sats, uint16_t n)
{
    ARG_UNUSED(dev);

    if (atomic_get(&gnss_mode) == APP_GNSS_MODE_BACKUP) {
        return;
    }
    sky.n = (uint8_t)MIN(n, APP_SAT_MAX);
    for (uint8_t i = 0U; i < sky.n; i++) {
        sky.sat[i] = (struct app_gnss_sat){
            .az_deg = sats[i].azimuth,
            .el_deg = sats[i].elevation,
            .cn0 = sats[i].snr,
            .sys = sys_of(sats[i].system),
            .used = sats[i].is_tracked,
        };
    }
    (void)app_publish(&chan_gnss_sky, &sky);
}

GNSS_SATELLITES_CALLBACK_DEFINE(DEVICE_DT_GET(GNSS_NODE), gnss_sat_cb);

#endif /* CONFIG_GNSS_SATELLITES */

#endif /* GNSS_NODE okay */

static bool mode_uses_gnss(uint8_t mode)
{
    return (mode == APP_MODE_ID_CRS) || (mode == APP_MODE_ID_PRC) || (mode == APP_MODE_ID_DBG);
}

static void publish_off(void)
{
    struct app_gnss_fix f = {.uptime_ms = k_uptime_get_32(), .mode = APP_GNSS_MODE_BACKUP};

    (void)app_publish(&chan_gnss_fix, &f);
}

static void gnss_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct gnss_msg msg;
    bool done = false;

#if !DT_NODE_HAS_STATUS(GNSS_NODE, okay)
    LOG_WRN("no GNSS receiver (alias gnss)");
#endif
    int wdt = app_wdt_add("gnss");

    for (;;) {
        if (app_inbox_get(&gnss_inbox, &msg, wdt, APP_SVC_TICK_MS) != 0) {
            continue;
        }
        if (msg.chan == &chan_mode) {
            if (mode_uses_gnss(msg.u.mode.mode)) {
                (void)atomic_cas(&gnss_mode, APP_GNSS_MODE_BACKUP, APP_GNSS_MODE_ACQ);
            } else {
                atomic_set(&gnss_mode, APP_GNSS_MODE_BACKUP);
                publish_off();
            }
        } else if ((msg.chan == &chan_system_state) && (msg.u.sys.state == APP_SYS_SHUTDOWN) &&
                   !done) {
            /* the receiver goes to backup before its rail (UBX-RXM-PMREQ, GNSS step) */
            done = true;
            atomic_set(&gnss_mode, APP_GNSS_MODE_BACKUP);

            struct app_shutdown_ack ack = {.svc = APP_SVC_GNSS};

            (void)app_publish(&chan_shutdown_ack, &ack);
        } else {
            /* nothing else reaches this inbox */
        }
    }
}

K_THREAD_DEFINE(gnss_tid, GNSS_STACK_SIZE, gnss_thread, NULL, NULL, NULL, APP_PRIO_GNSS, 0,
                SYS_FOREVER_MS);

void gnss_svc_start(void)
{
    k_thread_name_set(gnss_tid, "gnss");
    k_thread_start(gnss_tid);
}
