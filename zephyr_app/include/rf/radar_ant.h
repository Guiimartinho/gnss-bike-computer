/**
 * @file radar_ant.h
 * @brief The rear radar over ANT+, with the profile left outside this repo
 *
 * The ANT+ **Bike Radar** profile (device type 40) is the specified way to
 * talk to every radar on the market — Garmin Varia, Wahoo TRACKR, Bryton
 * Gardia, Magene L508 — and the one this port cannot carry: the profile
 * documents and the code derived from them are under the ANT+ Shared
 * Source License, the ANT+ Adopter Agreement forbids redistributing them,
 * and this repository is public
 * ([07](../../docs/07-radio-ant-ble.md#decisão-ant-e-ble)).
 *
 * So the repository holds the plumbing and the owner holds the profile:
 *
 * 1. The channel parameters come from Kconfig
 *    (`GNSS_ANT_RADAR_DEV_TYPE`, `_PERIOD`, `_RF`), which are zero here and
 *    which the owner sets on their machine from the profile document.
 * 2. The page decoding is `radar_ant_page()`, **declared** here and
 *    **defined** in `src/rf/ant/radar_pages.c`, a file `.gitignore` keeps
 *    out of the repository. Without that file the weak definition in
 *    `radar_ant.c` returns false and the radar simply never reports over
 *    ANT+; the BLE side goes on working.
 *
 * What the owner has to write is one function: take the eight bytes of a
 * broadcast and fill a `struct radar_frame`. Everything else — matching
 * vehicles between frames, ageing, sorting, the threat level, the screen —
 * is already here and covered by the host tests, because none of it is
 * profile material.
 */

#ifndef RF_RADAR_ANT_H
#define RF_RADAR_ANT_H

#include <stdbool.h>
#include <stdint.h>

#include "model/radar.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Bytes of an ANT broadcast */
#define RADAR_ANT_PAYLOAD   8U

/**
 * Turn one ANT broadcast into a frame.
 *
 * Defined in `src/rf/ant/radar_pages.c`, which is not in this repository
 * (see the note at the top of this file). The weak definition that ships
 * here returns false.
 *
 * @param page first byte of the broadcast, the page number
 * @param data the eight bytes
 * @param out frame to fill
 * @return true when the page carried targets and @p out was filled
 */
bool radar_ant_page(uint8_t page, const uint8_t *data, struct radar_frame *out);

/** Open the radar channel; does nothing while the Kconfig values are zero */
int radar_ant_start(void);

/** Close it */
void radar_ant_stop(void);

/** true while the radar channel is open and a radar is answering */
bool radar_ant_is_linked(void);

#ifdef __cplusplus
}
#endif

#endif /* RF_RADAR_ANT_H */
