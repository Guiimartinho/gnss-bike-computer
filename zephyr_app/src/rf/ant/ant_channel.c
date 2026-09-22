/**
 * @file ant_channel.c
 * @brief Opening an ANT slave channel, without any number of a profile
 *
 * Why the parameters are not here, and what a channel is for, in
 * rf/ant_channel.h.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "rf/ant_channel.h"

LOG_MODULE_REGISTER(ant_channel, CONFIG_LOG_DEFAULT_LEVEL);

#if defined(CONFIG_ANT)
#include <ant_interface.h>
#include <ant_parameters.h>
#endif

/** ANT+ network number, ANTPLUS_NETWORK_NUMBER of legacy/rf/ant.h:10 */
#define ANT_NETWORK     0U

/** Seconds a channel looks for a sensor before giving up */
#define ANT_SEARCH_S    30U

/** What one channel is doing */
struct ant_ch {
    ant_ch_rx_t cb;
    uint16_t device;        /**< the sensor it settled on, 0 while searching */
    bool open;
    bool linked;
};

static struct ant_ch channels[ANT_CH_USES];

/*
 * One channel number per use, so a broadcast routes straight back without
 * a search. The stack numbers channels from 0 and this firmware holds at
 * most ANT_CHANNEL_MAX of them at a time.
 */
static uint8_t channel_of(enum ant_channel_use use)
{
    return (uint8_t)use;
}

static enum ant_channel_use use_of(uint8_t channel)
{
    return (enum ant_channel_use)channel;
}

int ant_ch_open(enum ant_channel_use use, uint8_t device_type, uint16_t period,
                     uint8_t rf_freq, ant_ch_rx_t cb)
{
    if (use >= ANT_CH_USES) {
        return -EINVAL;
    }

    /*
     * A device type of zero means the profile is not on this machine, which
     * is how the repository ships. The channel stays closed and everything
     * above carries on over Bluetooth.
     */
    if ((device_type == 0U) || (period == 0U)) {
        LOG_DBG("ANT channel %d not configured", (int)use);

        return -ENOTSUP;
    }

    if (use >= ANT_CHANNEL_MAX) {
        return -ENOSPC;
    }

#if defined(CONFIG_ANT)
    uint8_t ch = channel_of(use);
    ant_err_t err;

    /* a slave that listens: CHANNEL_TYPE_SLAVE of ant_parameters.h */
    err = ant_channel_assign(ch, CHANNEL_TYPE_SLAVE, ANT_NETWORK, 0U);
    if (err != 0) {
        LOG_ERR("ANT assign %u: %d", ch, (int)err);

        return -EIO;
    }

    /*
     * A wildcard device number and a wildcard transmission type: the
     * channel takes whatever sensor of this type is near, which is what
     * pairing means here. `ant_ch_pair()` locks it afterwards.
     */
    err = ant_channel_id_set(ch, 0U, device_type, 0U);
    if (err != 0) {
        LOG_ERR("ANT id %u: %d", ch, (int)err);
        (void)ant_channel_unassign(ch);

        return -EIO;
    }

    err = ant_channel_period_set(ch, period);
    if (err != 0) {
        LOG_ERR("ANT period %u: %d", ch, (int)err);
        (void)ant_channel_unassign(ch);

        return -EIO;
    }

    err = ant_channel_radio_freq_set(ch, rf_freq);
    if (err != 0) {
        LOG_ERR("ANT frequency %u: %d", ch, (int)err);
        (void)ant_channel_unassign(ch);

        return -EIO;
    }

    /*
     * The search is bounded. A channel left searching for ever keeps the
     * radio busy and the battery draining for a sensor that is not there;
     * the timeout is in units of 2.5 s, as the stack counts them.
     */
    (void)ant_channel_search_timeout_set(ch, (uint8_t)((ANT_SEARCH_S * 10U) / 25U));

    err = ant_channel_open_with_offset(ch, 0U);
    if (err != 0) {
        LOG_ERR("ANT open %u: %d", ch, (int)err);
        (void)ant_channel_unassign(ch);

        return -EIO;
    }
#endif

    channels[use].cb = cb;
    channels[use].device = 0U;
    channels[use].open = true;
    channels[use].linked = false;

    LOG_INF("ANT channel %d searching: type %u, period %u, rf %u", (int)use,
            (unsigned int)device_type, (unsigned int)period, (unsigned int)rf_freq);

    return 0;
}

int ant_ch_pair(enum ant_channel_use use, uint16_t device_number)
{
    if ((use >= ANT_CH_USES) || !channels[use].open) {
        return -EINVAL;
    }

    if (device_number == 0U) {
        return -EINVAL;
    }

    channels[use].device = device_number;
    LOG_INF("ANT channel %d is now following sensor %u", (int)use,
            (unsigned int)device_number);

    return 0;
}

int ant_ch_close(enum ant_channel_use use)
{
    if (use >= ANT_CH_USES) {
        return -EINVAL;
    }
    if (!channels[use].open) {
        return 0;
    }

#if defined(CONFIG_ANT)
    uint8_t ch = channel_of(use);

    (void)ant_channel_close(ch);
    (void)ant_channel_unassign(ch);
#endif

    (void)memset(&channels[use], 0, sizeof(channels[use]));

    return 0;
}

bool ant_ch_is_linked(enum ant_channel_use use)
{
    return (use < ANT_CH_USES) && channels[use].linked;
}

uint16_t ant_ch_device(enum ant_channel_use use)
{
    return (use < ANT_CH_USES) ? channels[use].device : 0U;
}

void ant_ch_on_broadcast(uint8_t channel, const uint8_t *data, size_t len)
{
    if (channel >= ANT_CH_USES) {
        return;
    }

    enum ant_channel_use use = use_of(channel);
    struct ant_ch *c = &channels[use];

    if (!c->open || (data == NULL)) {
        return;
    }

    c->linked = true;

    if (c->cb != NULL) {
        c->cb(use, data, len);
    }
}

void ant_ch_on_search_timeout(uint8_t channel)
{
    if (channel >= ANT_CH_USES) {
        return;
    }

    struct ant_ch *c = &channels[use_of(channel)];

    c->linked = false;
    LOG_INF("ANT channel %u found nothing", (unsigned int)channel);
}
