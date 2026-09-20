/**
 * @file dfu_state.h
 * @brief State and rules of a firmware update over the air
 *
 * The transport is mcumgr over Bluetooth (SMP), which the phone app drives;
 * this module holds only what the rest of the firmware needs to know: how
 * far the upload went, whether it finished, and whether the device may take
 * an update at all right now.
 *
 * Pure C: no Zephyr, no hardware. `src/svc/radio/dfu.c` feeds it from the
 * mcumgr callbacks.
 */

#ifndef MODEL_DFU_STATE_H
#define MODEL_DFU_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Below this charge, and off the charger, an update is refused */
#define DFU_MIN_BATTERY_PCT     30U

/** Phase of an update */
enum dfu_phase {
    DFU_PHASE_IDLE = 0,     /**< nothing going on */
    DFU_PHASE_RUNNING,      /**< the image is coming in */
    DFU_PHASE_DONE,         /**< image written and marked: waiting for the reset */
    DFU_PHASE_FAILED,       /**< the client gave up or the image was refused */
};

/** What the device is doing, for the rules below */
struct dfu_conditions {
    bool ride_active;       /**< an activity is running: a reset would lose it */
    bool usb_present;       /**< on the charger */
    uint8_t battery_pct;    /**< state of charge, 0 to 100 */
};

/** How far the update went */
struct dfu_state {
    enum dfu_phase phase;
    uint32_t total;         /**< size of the image, 0 while the client has not said */
    uint32_t written;       /**< bytes taken so far */
};

/**
 * @brief Start over, with nothing going on
 */
void dfu_state_init(struct dfu_state *state);

/**
 * @brief Whether an update may start now
 *
 * The rules of this port, not of the legacy, which had no update over the
 * air: the rider first, then the battery. A reset in the middle of an
 * activity loses the ride, and an update that stops halfway on an empty
 * battery leaves the second slot unusable (the device still boots the image
 * it is running).
 */
bool dfu_state_allow(const struct dfu_conditions *cond);

/** @brief The client began to send an image */
void dfu_state_started(struct dfu_state *state);

/**
 * @brief A piece of the image was written
 *
 * @param offset Bytes written so far, as the client counts them
 * @param total Size of the image, or 0 when the client did not say
 */
void dfu_state_progress(struct dfu_state *state, uint32_t offset, uint32_t total);

/** @brief The image is complete and marked to boot at the next reset */
void dfu_state_pending(struct dfu_state *state);

/** @brief The transfer ended: @p ok tells whether the image is in place */
void dfu_state_stopped(struct dfu_state *state, bool ok);

/**
 * @brief How much of the image came in, 0 to 100
 *
 * Without a size from the client there is no percentage to give: the answer
 * is 0 while the transfer runs and 100 once it is done.
 */
uint8_t dfu_state_percent(const struct dfu_state *state);

/** @brief Whether an update is going on or waiting for the reset */
bool dfu_state_is_busy(const struct dfu_state *state);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_DFU_STATE_H */
