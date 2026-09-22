/**
 * @file ant_sensors.c
 * @brief The heart rate strap and the cadence sensor over ANT+
 *
 * Why the page decoding is not here, and what the owner has to add on
 * their own machine, in rf/ant_sensors.h.
 */

#include <errno.h>
#include <stddef.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/app_channels.h"
#include "model/csc_calc.h"
#include "rf/ant_channel.h"
#include "rf/ant_sensors.h"

LOG_MODULE_REGISTER(ant_sensors, CONFIG_LOG_DEFAULT_LEVEL);

/** The wheel the rider set; a sensor counts turns and cannot know it */
static uint16_t wheel_mm = CSC_WHEEL_MM;

/*
 * The weak definitions. Without `src/rf/ant/sensor_pages.c` neither sensor
 * ever reports over ANT+, and the Bluetooth clients go on working. A strong
 * definition in that file takes over at link time.
 */
__weak bool ant_hrm_page(uint8_t page, const uint8_t *data, struct ant_hrm_reading *out)
{
    ARG_UNUSED(page);
    ARG_UNUSED(data);

    if (out != NULL) {
        out->have_bpm = false;
        out->have_rr = false;
    }

    return false;
}

__weak bool ant_bsc_page(uint8_t page, const uint8_t *data, uint16_t wheel,
                         struct ant_bsc_reading *out)
{
    ARG_UNUSED(page);
    ARG_UNUSED(data);
    ARG_UNUSED(wheel);

    if (out != NULL) {
        out->have_speed = false;
        out->have_cadence = false;
    }

    return false;
}

void ant_sensors_set_wheel(uint16_t circumference_mm)
{
    if (circumference_mm != 0U) {
        wheel_mm = circumference_mm;
    }
}

/** One broadcast of the strap: decode it and publish what it carried */
static void hrm_rx(enum ant_channel_use use, const uint8_t *data, size_t len)
{
    struct ant_hrm_reading r;

    ARG_UNUSED(use);
    if ((data == NULL) || (len < ANT_SENSOR_PAYLOAD)) {
        return;
    }
    if (!ant_hrm_page(data[0], data, &r) || !r.have_bpm) {
        return;
    }

    struct app_ext_sensor e = {
        .uptime_ms = k_uptime_get_32(),
        .kind = APP_EXT_HR,
        .hr_bpm = r.bpm,
        .rr_ms = r.have_rr ? r.rr_ms : 0U,
    };

    (void)app_publish(&chan_ext_sensor, &e);
}

/** One broadcast of the cadence sensor */
static void bsc_rx(enum ant_channel_use use, const uint8_t *data, size_t len)
{
    struct ant_bsc_reading r;

    ARG_UNUSED(use);
    if ((data == NULL) || (len < ANT_SENSOR_PAYLOAD)) {
        return;
    }
    if (!ant_bsc_page(data[0], data, wheel_mm, &r)) {
        return;
    }
    if (!r.have_speed && !r.have_cadence) {
        return;
    }

    struct app_ext_sensor e = {
        .uptime_ms = k_uptime_get_32(),
        .kind = APP_EXT_BSC,
        .cadence_rpm = r.have_cadence ? r.cadence_rpm : 0U,
        .speed_kmh100 = r.have_speed ? r.speed_kmh100 : 0U,
    };

    (void)app_publish(&chan_ext_sensor, &e);
}

/*
 * The period and the frequency only exist as Kconfig symbols once a device
 * type is set, which is how the radar and the power meter are written too:
 * the parameters of a profile that is not on this machine have no value to
 * carry, and a symbol with a made-up default would be a number of the
 * profile sitting in a public repository.
 */
#if (CONFIG_GNSS_ANT_HRM_DEV_TYPE > 0)
#define HRM_PERIOD  CONFIG_GNSS_ANT_HRM_PERIOD
#define HRM_RF      CONFIG_GNSS_ANT_HRM_RF
#else
#define HRM_PERIOD  0
#define HRM_RF      0
#endif

#if (CONFIG_GNSS_ANT_BSC_DEV_TYPE > 0)
#define BSC_PERIOD  CONFIG_GNSS_ANT_BSC_PERIOD
#define BSC_RF      CONFIG_GNSS_ANT_BSC_RF
#else
#define BSC_PERIOD  0
#define BSC_RF      0
#endif

int ant_sensors_start(void)
{
    int hrm = ant_ch_open(ANT_CH_HR, (uint8_t)CONFIG_GNSS_ANT_HRM_DEV_TYPE,
                          (uint16_t)HRM_PERIOD, (uint8_t)HRM_RF, hrm_rx);
    int bsc = ant_ch_open(ANT_CH_BSC, (uint8_t)CONFIG_GNSS_ANT_BSC_DEV_TYPE,
                          (uint16_t)BSC_PERIOD, (uint8_t)BSC_RF, bsc_rx);

    /*
     * Either one being unconfigured is the normal state of this
     * repository, and is not a failure: the caller only wants to know
     * about a channel that was configured and would not open.
     */
    if ((hrm != 0) && (hrm != -ENOTSUP)) {
        return hrm;
    }
    if ((bsc != 0) && (bsc != -ENOTSUP)) {
        return bsc;
    }

    return ((hrm == -ENOTSUP) && (bsc == -ENOTSUP)) ? -ENOTSUP : 0;
}

void ant_sensors_stop(void)
{
    (void)ant_ch_close(ANT_CH_HR);
    (void)ant_ch_close(ANT_CH_BSC);
}
