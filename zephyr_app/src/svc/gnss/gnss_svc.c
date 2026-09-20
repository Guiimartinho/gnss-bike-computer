/**
 * @file gnss_svc.c
 * @brief GNSS service: the receiver through the Zephyr GNSS API
 *
 * docs/16-arquitetura-firmware.md (Serviços, GNSS). The receiver is the
 * device of the alias gnss: the u-blox MAX-M10N by UBX on the new board
 * (driver in modules/gnss_drivers), an NMEA module through
 * gnss-nmea-generic elsewhere. The driver callbacks run in the modem work
 * queue and only convert and hand the epoch to this thread, which runs the
 * power machine (svc/gnss_power.h), talks to the receiver and publishes.
 *
 * Nothing that waits for an answer from the receiver may run in the modem
 * work queue: the answer arrives in a work item of that same queue.
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
#include "svc/gnss_power.h"

LOG_MODULE_REGISTER(gnss_svc, CONFIG_LOG_DEFAULT_LEVEL);

/* a zbus publish, the power machine and the UBX commands of the driver */
#define GNSS_STACK_SIZE     2048
#define GNSS_INBOX_LEN      8

#define GNSS_NODE           DT_ALIAS(gnss)

/* The board takes the single-band MAX-M10N or the dual-band MAX-F10S in the
 * same footprint; one driver serves both, and only LEAP tells them apart */
#if DT_NODE_HAS_STATUS(GNSS_NODE, okay) &&                                                         \
    (DT_NODE_HAS_COMPAT(GNSS_NODE, u_blox_max_m10) || DT_NODE_HAS_COMPAT(GNSS_NODE, u_blox_max_f10))
#define GNSS_HAS_M10        1
#include "drivers/gnss/ublox_m10.h"
#define GNSS_EPOCH_MS       DT_PROP(GNSS_NODE, fix_rate_ms)
/** The F10 has no CFG-PM group: it tracks at full power and there is no LEAP */
#define GNSS_HAS_LEAP       DT_NODE_HAS_COMPAT(GNSS_NODE, u_blox_max_m10)
#else
#define GNSS_HAS_M10        0
#define GNSS_HAS_LEAP       0
#define GNSS_EPOCH_MS       1000U
#endif

/** Time between two tries to configure a receiver that did not answer */
#define GNSS_RETRY_MS       5000U

struct gnss_msg {
    const struct zbus_channel *chan; /**< NULL: an epoch from the driver */
    union {
        struct app_mode_state mode;
        struct app_system_state sys;
        struct app_gnss_fix fix;
    } u;
};

K_MSGQ_DEFINE(gnss_inbox, sizeof(struct gnss_msg), GNSS_INBOX_LEN, 4);

/** enum app_gnss_mode, published by the thread and read by the callbacks */
static atomic_t gnss_mode = ATOMIC_INIT(APP_GNSS_MODE_BACKUP);

static struct gnss_power power;

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

    if (atomic_get(&gnss_mode) == APP_GNSS_MODE_BACKUP) {
        return; /* the receiver is going down: the epoch is from before */
    }

    bool fix = (data->info.fix_status == GNSS_FIX_STATUS_GNSS_FIX) ||
               (data->info.fix_status == GNSS_FIX_STATUS_DGNSS_FIX);
    struct gnss_msg msg = {
        .chan = NULL,
        .u.fix = {
            .uptime_ms = k_uptime_get_32(),
            .fix = fix,
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
        },
    };

    app_inbox_put(&gnss_inbox, &msg, "gnss");
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

#if GNSS_HAS_M10

static const struct device *const m10 = DEVICE_DT_GET(GNSS_NODE);
static bool m10_configured;

/** Apply what the power machine asked; false when the receiver did not answer */
static bool apply(enum gnss_power_action action)
{
    int err = 0;

    if (!device_is_ready(m10)) {
        return false;
    }

    switch (action) {
    case GNSS_POWER_ACTION_WAKE:
        err = ublox_m10_wake(m10);
        if (err == 0) {
            /* the standby cleared the RAM of the receiver: configure it again */
            err = ublox_m10_configure(m10);
        }
        m10_configured = (err == 0);
        break;
    case GNSS_POWER_ACTION_STANDBY:
        err = ublox_m10_standby(m10);
        m10_configured = false;
        break;
    case GNSS_POWER_ACTION_FULL:
        err = ublox_m10_set_power_mode(m10, UBLOX_M10_POWER_FULL);
        break;
    case GNSS_POWER_ACTION_LEAP:
        err = ublox_m10_set_power_mode(m10, UBLOX_M10_POWER_LEAP);
        break;
    case GNSS_POWER_ACTION_CONFIGURE:
        LOG_WRN("no epoch for %u s: configuring the receiver again",
                GNSS_POWER_SILENCE_MS / 1000U);
        err = ublox_m10_configure(m10);
        m10_configured = (err == 0);
        break;
    case GNSS_POWER_ACTION_RESET:
        /* the legacy warned here too (`legacy/source/sensors/GPSMGMT.cpp:157`) */
        LOG_ERR("receiver quiet for %u s: pulling its reset", GNSS_POWER_RESET_MS / 1000U);
        app_notify("GNSS", "Reiniciando o receptor", NULL, false, 0U);
        err = ublox_m10_hw_reset(m10);
        if (err == -ENOTSUP) {
            /* without the pin, UBX-CFG-RST is what there is */
            err = ublox_m10_restart(m10, UBLOX_M10_RESTART_WARM);
            k_msleep(200);
        }
        if (err == 0) {
            err = ublox_m10_configure(m10);
        }
        m10_configured = (err == 0);
        break;
    default:
        break;
    }

    if (err != 0) {
        LOG_WRN("receiver did not answer (action %d): %d", (int)action, err);
    }

    return err == 0;
}

#else

/** Without the UBX driver there is nothing to configure */
static const bool m10_configured = true;

static bool apply(enum gnss_power_action action)
{
    ARG_UNUSED(action);

    return true; /* a receiver without UBX only follows the published mode */
}

#endif /* GNSS_HAS_M10 */

static bool mode_uses_gnss(uint8_t mode)
{
    return (mode == APP_MODE_ID_CRS) || (mode == APP_MODE_ID_PRC) || (mode == APP_MODE_ID_DBG);
}

static void publish_off(void)
{
    struct app_gnss_fix f = {.uptime_ms = k_uptime_get_32(), .mode = APP_GNSS_MODE_BACKUP};

    (void)app_publish(&chan_gnss_fix, &f);
}

static void on_epoch(struct app_gnss_fix *f)
{
    enum gnss_power_action action = gnss_power_epoch(&power, f->fix, GNSS_EPOCH_MS);

    (void)apply(action);
    f->mode = gnss_power_mode(&power);
    atomic_set(&gnss_mode, f->mode);
    (void)app_publish(&chan_gnss_fix, f);
}

static void gnss_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct gnss_msg msg;
    bool shutdown_done = false;
    uint32_t retry_at = 0U;
    uint32_t last_tick_ms = 0U;

#if !DT_NODE_HAS_STATUS(GNSS_NODE, okay)
    LOG_WRN("no GNSS receiver (alias gnss)");
#endif
    gnss_power_init(&power, GNSS_HAS_LEAP);

    int wdt = app_wdt_add("gnss");

    for (;;) {
        if (app_inbox_get(&gnss_inbox, &msg, wdt, APP_SVC_TICK_MS) != 0) {
            uint32_t now = k_uptime_get_32();
            enum gnss_power_action action = gnss_power_tick(&power, now - last_tick_ms);

            last_tick_ms = now;
            if (action != GNSS_POWER_ACTION_NONE) {
                (void)apply(action);
            } else if (!m10_configured && (gnss_power_mode(&power) != APP_GNSS_MODE_BACKUP) &&
                       (now >= retry_at)) {
                /* a receiver that did not answer gets another try */
                retry_at = now + GNSS_RETRY_MS;
                (void)apply(GNSS_POWER_ACTION_CONFIGURE);
            } else {
                /* the receiver is answering */
            }
            continue;
        }
        last_tick_ms = k_uptime_get_32();

        if (msg.chan == NULL) {
            on_epoch(&msg.u.fix);
        } else if (msg.chan == &chan_mode) {
            enum gnss_power_action action =
                gnss_power_mode_change(&power, mode_uses_gnss(msg.u.mode.mode));

            atomic_set(&gnss_mode, gnss_power_mode(&power));
            if (action == GNSS_POWER_ACTION_STANDBY) {
                atomic_set(&gnss_mode, APP_GNSS_MODE_BACKUP);
                (void)apply(action);
                publish_off();
            } else if (action != GNSS_POWER_ACTION_NONE) {
                if (!apply(action)) {
                    retry_at = k_uptime_get_32() + GNSS_RETRY_MS;
                }
            } else {
                /* the mode did not change what the receiver does */
            }
        } else if ((msg.chan == &chan_system_state) && (msg.u.sys.state == APP_SYS_SHUTDOWN) &&
                   !shutdown_done) {
            /* the receiver goes to backup before its rail (UBX-RXM-PMREQ) */
            shutdown_done = true;
            (void)gnss_power_mode_change(&power, false);
            atomic_set(&gnss_mode, APP_GNSS_MODE_BACKUP);
            (void)apply(GNSS_POWER_ACTION_STANDBY);

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
