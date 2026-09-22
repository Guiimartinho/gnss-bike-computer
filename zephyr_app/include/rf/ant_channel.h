/**
 * @file ant_channel.h
 * @brief Opening an ANT slave channel, without any number of a profile
 *
 * The ANT stack of the port started and set the ANT+ network key, and then
 * did nothing: no channel was ever opened, so no ANT+ sensor could be
 * found. This is the part that opens one.
 *
 * What it deliberately does **not** carry is the parameters. A channel
 * needs a device type, a channel period and an RF frequency, and those
 * numbers are the ANT+ device profiles: they are under the ANT+ Shared
 * Source License, the Adopter Agreement forbids redistributing them, and
 * this repository is public
 * ([07](../../docs/07-radio-ant-ble.md#decisão-ant-e-ble)). They come from
 * Kconfig, where they are zero as shipped, and the owner sets them on their
 * own machine from the profile document.
 *
 * So this file is the mechanism and the Kconfig is the data. A channel with
 * a device type of zero is not opened and answers `-ENOTSUP`, which is how
 * everything ANT+ behaves in this repository: present, wired, and quiet.
 *
 * ## The search
 *
 * A slave channel with a wildcard device number finds whatever sensor of
 * that type is near, which is what pairing means here. Once a broadcast
 * arrives the device number is known, and `ant_ch_pair()` locks the
 * channel to it so that the rider's own sensor is followed and not the one
 * on the next bicycle.
 *
 * Nothing here has been run against a sensor.
 */

#ifndef RF_ANT_CHANNEL_H
#define RF_ANT_CHANNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Channels this firmware may hold open at once */
#define ANT_CHANNEL_MAX     4U

/** What one channel is for */
enum ant_channel_use {
    ANT_CH_HR = 0,
    ANT_CH_BSC,
    ANT_CH_POWER,
    ANT_CH_RADAR,
    ANT_CH_USES
};

/** A broadcast arrived on a channel */
typedef void (*ant_ch_rx_t)(enum ant_channel_use use, const uint8_t *data, size_t len);

/**
 * @brief Open a slave channel that searches for a sensor
 *
 * @param use which sensor it is for; it also picks the channel number
 * @param device_type of the ANT+ profile; **0 leaves the channel closed**
 * @param period in 1/32768 s, from the profile
 * @param rf_freq of the profile, as an offset from 2400 MHz
 * @param cb called with every broadcast, from the ANT event thread
 * @return 0, or -ENOTSUP when the parameters are zero, or a negative errno
 */
int ant_ch_open(enum ant_channel_use use, uint8_t device_type, uint16_t period,
                     uint8_t rf_freq, ant_ch_rx_t cb);

/**
 * @brief Lock a searching channel to the sensor that answered
 *
 * Until this is called the channel takes any sensor of its type. After it,
 * it follows one device number: the rider's own sensor, and not the one on
 * the bicycle that just went past.
 */
int ant_ch_pair(enum ant_channel_use use, uint16_t device_number);

/** Close one */
int ant_ch_close(enum ant_channel_use use);

/** Whether a channel is open and a sensor is answering on it */
bool ant_ch_is_linked(enum ant_channel_use use);

/** The device number the channel settled on, or 0 while it is searching */
uint16_t ant_ch_device(enum ant_channel_use use);

/**
 * @brief Hand one broadcast to the channel layer
 *
 * The ANT event loop of the owner calls this; it is the seam that keeps
 * everything above it free of the stack's own types.
 */
void ant_ch_on_broadcast(uint8_t channel, const uint8_t *data, size_t len);

/** Tell the channel layer a search ended with nothing */
void ant_ch_on_search_timeout(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* RF_ANT_CHANNEL_H */
