/**
 * @file sensors_svc.c
 * @brief Sensors service: barometer, accelerometer, magnetometer and light
 *
 * docs/16-arquitetura-firmware.md (Serviços, Sensores). The devices come
 * from the devicetree aliases baro0, imu0, mag0 and light0 through the
 * Zephyr sensor API, so the same code reads the BMP585, BMI270, MMC5633NJL
 * and OPT3001 of the new board and the BME280 and FXOS8700 of the V3. A
 * missing alias leaves that sensor out.
 *
 * Rates of the legacy: the accelerometer at 50 Hz averaged over one second
 * (fxos.cpp, see svc/tilt.h), the barometer at 10 Hz (Attitude.cpp); the
 * magnetometer and the light once a second, with the attitude.
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "app/app_channels.h"
#include "app/app_svc.h"
#include "svc/tilt.h"

LOG_MODULE_REGISTER(sensors_svc, CONFIG_LOG_DEFAULT_LEVEL);

/* a sensor fetch over I2C plus a zbus publish and a log call: about 0.9 KB */
#define SENSORS_STACK_SIZE  2048
#define SENSORS_INBOX_LEN   8

/** Accelerometer period: 50 Hz (legacy HYB_DATA_RATE_50HZ, fxos.cpp:600) */
#define SAMPLE_PERIOD_MS    20U
/** Barometer every 5 samples: 10 Hz */
#define BARO_DIVIDER        5U

static const struct device *const baro = DEVICE_DT_GET_OR_NULL(DT_ALIAS(baro0));
static const struct device *const imu = DEVICE_DT_GET_OR_NULL(DT_ALIAS(imu0));
static const struct device *const mag = DEVICE_DT_GET_OR_NULL(DT_ALIAS(mag0));
static const struct device *const light = DEVICE_DT_GET_OR_NULL(DT_ALIAS(light0));

struct sensors_msg {
    const struct zbus_channel *chan;
    union {
        struct app_system_state sys;
        struct app_system_cmd cmd;
    } u;
};

K_MSGQ_DEFINE(sensors_inbox, sizeof(struct sensors_msg), SENSORS_INBOX_LEN, 4);

static void sensors_listener(const struct zbus_channel *chan)
{
    struct sensors_msg msg = {.chan = chan};

    if (chan == &chan_system_cmd) {
        const struct app_system_cmd *cmd = zbus_chan_const_msg(chan);

        if (cmd->id != APP_CMD_CALIB_COMPASS) {
            return;
        }
    }
    if (zbus_chan_msg_size(chan) > sizeof(msg.u)) {
        return;
    }
    (void)memcpy(&msg.u, zbus_chan_const_msg(chan), zbus_chan_msg_size(chan));
    app_inbox_put(&sensors_inbox, &msg, "sensors");
}

ZBUS_LISTENER_DEFINE(sensors_lis, sensors_listener);
ZBUS_CHAN_ADD_OBS(chan_system_state, sensors_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_system_cmd, sensors_lis, 3);

static struct tilt_window window;

static bool ready(const struct device *dev, const char *what)
{
    if (dev == NULL) {
        return false;
    }
    if (!device_is_ready(dev)) {
        LOG_WRN("%s %s not ready", what, dev->name);
        return false;
    }
    return true;
}

static void read_baro(void)
{
    struct sensor_value p;
    struct sensor_value t;

    if ((sensor_sample_fetch(baro) != 0) ||
        (sensor_channel_get(baro, SENSOR_CHAN_PRESS, &p) != 0) ||
        (sensor_channel_get(baro, SENSOR_CHAN_AMBIENT_TEMP, &t) != 0)) {
        return;
    }

    /* the sensor API gives kPa */
    struct app_baro b = {
        .uptime_ms = k_uptime_get_32(),
        .pressure_pa = sensor_value_to_float(&p) * 1000.0f,
        .temp_c = sensor_value_to_float(&t),
    };

    (void)app_publish(&chan_baro, &b);
}

static void read_accel(void)
{
    struct sensor_value a[3];

    if ((sensor_sample_fetch_chan(imu, SENSOR_CHAN_ACCEL_XYZ) != 0) ||
        (sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, a) != 0)) {
        return;
    }
    (void)tilt_window_add(&window, sensor_value_to_float(&a[0]), sensor_value_to_float(&a[1]),
                          sensor_value_to_float(&a[2]));
}

/** Once a second: attitude, heading and light */
static void publish_second(bool have_imu, bool have_mag, bool have_light)
{
    struct tilt_out out = {0};
    bool attitude = have_imu && tilt_compute(&window, &out);
    uint32_t now = k_uptime_get_32();

    if (attitude) {
        struct app_imu m = {
            .uptime_ms = now,
            .pitch_deg = out.pitch_deg,
            .roll_deg = out.roll_deg,
        };

        (void)memcpy(m.rough, out.rough, sizeof(m.rough));
        (void)app_publish(&chan_imu, &m);
    }

    if (have_mag) {
        struct sensor_value f[3];
        struct app_mag m = {.uptime_ms = now};

        if ((sensor_sample_fetch_chan(mag, SENSOR_CHAN_MAGN_XYZ) == 0) &&
            (sensor_channel_get(mag, SENSOR_CHAN_MAGN_XYZ, f) == 0)) {
            /* without the attitude, the heading of a level device */
            m.heading_deg = tilt_heading_deg(out.pitch_deg, out.roll_deg,
                                             sensor_value_to_float(&f[0]),
                                             sensor_value_to_float(&f[1]),
                                             sensor_value_to_float(&f[2]));
            m.valid = true;
        }
        (void)app_publish(&chan_mag, &m);
    }

    if (have_light) {
        struct sensor_value lux;

        if ((sensor_sample_fetch(light) == 0) &&
            (sensor_channel_get(light, SENSOR_CHAN_LIGHT, &lux) == 0)) {
            struct app_ambient m = {.lux = sensor_value_to_float(&lux)};

            (void)app_publish(&chan_ambient, &m);
        }
    }
}

static void sensors_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    bool have_baro = ready(baro, "barometer");
    bool have_imu = ready(imu, "accelerometer");
    bool have_mag = ready(mag, "magnetometer");
    bool have_light = ready(light, "light sensor");
    bool stopped = false;
    uint32_t count = 0U;
    uint32_t next_ms = k_uptime_get_32();
    struct sensors_msg msg;

    tilt_window_reset(&window);
    LOG_INF("barometer %d, accelerometer %d, magnetometer %d, light %d", have_baro, have_imu,
            have_mag, have_light);

    int wdt = app_wdt_add("sensors");

    for (;;) {
        uint32_t now = k_uptime_get_32();
        uint32_t wait = ((int32_t)(next_ms - now) > 0) ? (next_ms - now) : 0U;

        if (stopped) {
            wait = APP_SVC_TICK_MS;
        }
        if (app_inbox_get(&sensors_inbox, &msg, wdt, wait) == 0) {
            if ((msg.chan == &chan_system_state) && (msg.u.sys.state == APP_SYS_SHUTDOWN) &&
                !stopped) {
                /* the sensors keep their power-on state: nothing to save */
                stopped = true;

                struct app_shutdown_ack ack = {.svc = APP_SVC_SENSORS};

                (void)app_publish(&chan_shutdown_ack, &ack);
            } else if (msg.chan == &chan_system_cmd) {
                /* calibration of the magnetometer: the sensors step */
                LOG_INF("compass calibration requested (%u)", msg.u.cmd.id);
            } else {
                /* nothing else reaches this inbox */
            }
            continue;
        }
        if (stopped || ((int32_t)(k_uptime_get_32() - next_ms) < 0)) {
            continue;
        }
        next_ms += SAMPLE_PERIOD_MS;
        count++;

        if (have_imu) {
            read_accel();
        }
        if (have_baro && ((count % BARO_DIVIDER) == 0U)) {
            read_baro();
        }
        if ((count % TILT_WINDOW) == 0U) {
            publish_second(have_imu, have_mag, have_light);
        }
        /* behind by more than a period (a long bus transfer): start again from now */
        if ((int32_t)(k_uptime_get_32() - next_ms) > (int32_t)SAMPLE_PERIOD_MS) {
            next_ms = k_uptime_get_32() + SAMPLE_PERIOD_MS;
        }
    }
}

K_THREAD_DEFINE(sensors_tid, SENSORS_STACK_SIZE, sensors_thread, NULL, NULL, NULL,
                APP_PRIO_SENSORS, 0, SYS_FOREVER_MS);

void sensors_svc_start(void)
{
    k_thread_name_set(sensors_tid, "sensors");
    k_thread_start(sensors_tid);
}
