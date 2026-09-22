/**
 * @file power_ant.c
 * @brief The power meter over ANT+: the plumbing, without the profile
 *
 * Why the profile is not here, and what the owner has to add on their own
 * machine, in rf/power_ant.h. This file is the half that can live in a
 * public repository: the channel, the callbacks and the hand-off to the
 * model. It carries no page layout and no number from the profile.
 */

#include <errno.h>
#include <stddef.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/app_channels.h"
#include "rf/power_ant.h"

LOG_MODULE_REGISTER(power_ant, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * The weak definition: without `src/rf/ant/power_pages.c` the meter never
 * reports over ANT+, and the Bluetooth client goes on working. A strong
 * definition in that file takes over at link time.
 */
__weak bool power_ant_page(uint8_t page, const uint8_t *data, struct power_ant_reading *out)
{
    ARG_UNUSED(page);
    ARG_UNUSED(data);

    if (out != NULL) {
        out->have_power = false;
    }

    return false;
}

#if (CONFIG_GNSS_ANT_POWER_DEV_TYPE > 0)

static bool linked;

void power_ant_on_broadcast(const uint8_t *data, size_t len)
{
    struct power_ant_reading r;

    if ((data == NULL) || (len < POWER_ANT_PAYLOAD)) {
        return;
    }
    if (!power_ant_page(data[0], data, &r) || !r.have_power) {
        return;
    }

    struct app_ext_sensor e = {
        .uptime_ms = k_uptime_get_32(),
        .kind = APP_EXT_POWER,
        .power_w = r.power_w,
        .cadence_rpm = r.have_cadence ? r.cadence_rpm : 0U,
    };

    linked = true;
    (void)app_publish(&chan_ext_sensor, &e);
}

int power_ant_start(void)
{
    /*
     * The channel is opened by the ANT code of the owner, which knows the
     * device type, the period and the RF channel from the profile. What is
     * here is only the place it reports into.
     */
    LOG_INF("power: ANT channel type %d, period %d, rf %d",
            CONFIG_GNSS_ANT_POWER_DEV_TYPE, CONFIG_GNSS_ANT_POWER_PERIOD,
            CONFIG_GNSS_ANT_POWER_RF);

    return 0;
}

void power_ant_stop(void)
{
    linked = false;
}

bool power_ant_is_linked(void)
{
    return linked;
}

#else

void power_ant_on_broadcast(const uint8_t *data, size_t len)
{
    ARG_UNUSED(data);
    ARG_UNUSED(len);
}

int power_ant_start(void)
{
    /* no device type set: the power meter over ANT+ is not configured */
    return -ENOTSUP;
}

void power_ant_stop(void) {}

bool power_ant_is_linked(void)
{
    return false;
}

#endif /* CONFIG_GNSS_ANT_POWER_DEV_TYPE > 0 */
