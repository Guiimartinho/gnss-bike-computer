/**
 * @file radar_ant.c
 * @brief The rear radar over ANT+: the plumbing, without the profile
 *
 * Why the profile is not here, and what the owner has to add on their own
 * machine, in rf/radar_ant.h. This file is the half that can live in a
 * public repository: the channel, the callbacks and the hand-off to the
 * model. It carries no page layout and no number from the profile.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/app_channels.h"
#include "rf/radar_ant.h"

LOG_MODULE_REGISTER(radar_ant, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * The weak definition: without `src/rf/ant/radar_pages.c` the radar never
 * reports over ANT+, and the BLE side goes on working. A strong definition
 * in that file takes over at link time.
 */
__weak bool radar_ant_page(uint8_t page, const uint8_t *data, struct radar_frame *out)
{
    ARG_UNUSED(page);
    ARG_UNUSED(data);

    if (out != NULL) {
        out->n = 0U;
    }

    return false;
}

#if (CONFIG_GNSS_ANT_RADAR_DEV_TYPE > 0)

static bool linked;

/** One broadcast from the radar: decode it and publish the frame */
void radar_ant_on_broadcast(const uint8_t *data, size_t len)
{
    struct radar_frame f;

    if ((data == NULL) || (len < RADAR_ANT_PAYLOAD)) {
        return;
    }
    if (!radar_ant_page(data[0], data, &f)) {
        return;
    }

    struct app_radar msg = {.uptime_ms = k_uptime_get_32(), .n = f.n, .linked = true};

    for (uint8_t i = 0U; (i < f.n) && (i < RADAR_TARGETS_MAX); i++) {
        msg.id[i] = f.t[i].id;
        msg.range_m[i] = f.t[i].range_m;
        msg.closing_kmh[i] = f.t[i].closing_kmh;
        msg.level[i] = f.t[i].level;
        msg.side[i] = f.t[i].side;
    }
    linked = true;
    (void)app_publish(&chan_radar, &msg);
}

int radar_ant_start(void)
{
    /*
     * The channel is opened by the ANT code of the owner, which knows the
     * device type, the period and the RF channel from the profile. What is
     * here is only the place it reports into.
     */
    LOG_INF("radar: ANT channel type %d, period %d, rf %d",
            CONFIG_GNSS_ANT_RADAR_DEV_TYPE, CONFIG_GNSS_ANT_RADAR_PERIOD,
            CONFIG_GNSS_ANT_RADAR_RF);

    return 0;
}

void radar_ant_stop(void)
{
    linked = false;
}

bool radar_ant_is_linked(void)
{
    return linked;
}

#else

int radar_ant_start(void)
{
    /* no device type set: the radar over ANT+ is not configured */
    return -ENOTSUP;
}

void radar_ant_stop(void) {}

bool radar_ant_is_linked(void)
{
    return false;
}

#endif /* CONFIG_GNSS_ANT_RADAR_DEV_TYPE > 0 */
