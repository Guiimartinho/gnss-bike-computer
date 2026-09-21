/**
 * @file radar_wire.h
 * @brief Turning what a radar sends into a frame the model takes
 *
 * Kept apart from `model/radar.c` because a wire format is the one thing
 * that can be wrong without the logic being wrong, and apart from the
 * radios because it is plain C and the host tests can read it byte by
 * byte.
 *
 * > [!IMPORTANT]
 * > **The BLE layout below has not been checked against a radar.** Garmin
 * > publishes no specification for the service the Varia exposes; what is
 * > here follows the layout the open projects that talk to one report, and
 * > the one number most likely to be wrong — the unit of the closing speed
 * > — is `RADAR_VARIA_SPEED_KMH`, in one place, so a bench session fixes it
 * > with one edit. The ANT+ Bike Radar profile is the specified one, and
 * > the port cannot carry it: see `radar_ant_page()`.
 */

#ifndef MODEL_RADAR_WIRE_H
#define MODEL_RADAR_WIRE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "model/radar.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Bytes each vehicle takes in a Varia notification: id, range, speed */
#define RADAR_VARIA_PER_TARGET  3U

/** First byte of a notification is a counter, not a vehicle */
#define RADAR_VARIA_HEADER      1U

/**
 * Closing speed of a Varia, in km/h for each count of the third byte.
 *
 * **Not verified on a radar.** One is the value that makes the numbers of
 * the open projects come out as km/h; if a bench session shows metres per
 * second, this becomes 3.6f and nothing else changes.
 */
#define RADAR_VARIA_SPEED_KMH   1.0f

/**
 * Read one notification of the radar service of a Varia.
 *
 * @param buf payload of the notification
 * @param len its length
 * @param out frame to fill; emptied first
 * @return true when the payload had the shape of a radar notification
 */
bool radar_wire_varia(const uint8_t *buf, size_t len, struct radar_frame *out);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_RADAR_WIRE_H */
