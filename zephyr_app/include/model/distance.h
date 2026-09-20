/**
 * @file distance.h
 * @brief Distance ridden, accumulated as the legacy does
 *
 * `legacy/source/model/Attitude.cpp:431-490` (`Attitude::computeDistance`):
 *
 * 1. Add to `att.dist` the distance between the previous **raw** position
 *    and the new one, whatever the speed.
 * 2. While the accumulation has not started, the first jump above 25 m
 *    throws the total away (`att.dist = m_last_save_dist`) and starts it:
 *    the first fix of the receiver comes from nowhere, and the position
 *    stored before it is (0, 0).
 * 3. Once started, every 15 m of total distance is a snapshot: the legacy
 *    saves the point in a buffer of five, writes the buffer to the SD card
 *    and copies the state for the crash recovery.
 *
 * Pure logic, without Zephyr, so the host tests cover it; the model calls
 * it once per epoch and the storage service keeps the buffer of five.
 */

#ifndef MODEL_DISTANCE_H
#define MODEL_DISTANCE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Distance of the first jump that starts the accumulation, in metres */
#define DISTANCE_START_M        25.0f

/** Distance between two snapshots, in metres */
#define DISTANCE_SNAPSHOT_M     15.0f

/** State of the accumulation; the model keeps one */
struct distance_acc {
    float total_m;      /**< att.dist of the legacy */
    float last_save_m;  /**< m_last_save_dist */
    float prev_lat;
    float prev_lon;
    bool started;       /**< m_is_acc_init: the first 25 m are gone */
    bool has_prev;      /**< a position already arrived */
};

/** Start from zero, without a previous position */
void distance_init(struct distance_acc *d);

/**
 * One position of the epoch, raw, as the receiver gave it.
 *
 * @param lat latitude in degrees
 * @param lon longitude in degrees
 * @return true when this epoch completes another 15 m and the caller has to
 *         save the point and the state for the crash recovery
 */
bool distance_add(struct distance_acc *d, float lat, float lon);

/** Total in metres */
float distance_total(const struct distance_acc *d);

/** Put a total back, as the crash recovery does after a reset */
void distance_restore(struct distance_acc *d, float total_m);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_DISTANCE_H */
