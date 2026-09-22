/**
 * @file power_metrics.h
 * @brief Normalised power, intensity factor and training stress
 *
 * The three numbers a rider with a power meter looks at when the ride ends,
 * and the reason average power is not enough: a ride of two hours at a
 * steady 200 W and a ride of two hours alternating 100 and 300 W have the
 * same average and cost the body very different things.
 *
 * The method is Andrew Coggan's, and it is the same one every head unit and
 * every training site uses:
 *
 * 1. a **rolling average of the last 30 seconds** of power, one value per
 *    second, starting once thirty seconds exist;
 * 2. each of those values raised to the **fourth power**;
 * 3. the mean of those, and its fourth root: that is **normalised power**.
 *
 * From there:
 *
 * - **intensity factor** is `NP / FTP`: 1,0 is an hour at threshold;
 * - **training stress** is `t x NP x IF / (FTP x 3600) x 100`, so an hour
 *   exactly at threshold scores 100;
 * - **variability index** is `NP / average`, which says how ragged the ride
 *   was: near 1,0 for a time trial, well above it for a criterium.
 *
 * ## Feeding it
 *
 * One sample per second, and **only while the ride is running**. A stop at
 * the traffic lights must not feed zeros: they would drag the rolling
 * average down and lie about the effort. The caller already knows whether
 * the ride is paused (`model/activity.h`), so it simply does not call.
 *
 * ## Why the accumulator is a double
 *
 * Not because a `float` would break: the sum after `n` seconds is about
 * `n` times the fourth power of the average, and naive summation loses at
 * worst `n x eps`, which over eight hours is 4e-4 on the sum and a quarter
 * of that on the fourth root — under a tenth of a percent on NP, well
 * below what the sensor itself is worth. The accumulator is a double so
 * that nobody has to redo that reasoning when the ride gets longer or the
 * power higher. It costs one addition a second, and everything the module
 * hands back is a `float` again.
 */

#ifndef MODEL_POWER_METRICS_H
#define MODEL_POWER_METRICS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Seconds of the rolling average (Coggan) */
#define PM_WINDOW_S     30U

/** A power above this is noise from the sensor, not a rider */
#define PM_POWER_MAX_W  2500U

/** What the ride has produced so far */
struct power_metrics {
    uint16_t ring[PM_WINDOW_S];     /**< the last thirty seconds of power */
    uint8_t head;                   /**< where the next sample goes */
    uint8_t filled;                 /**< how many of the thirty exist */
    uint32_t ring_sum;              /**< sum of the ring, kept as it moves */

    double quartic_sum;             /**< sum of the rolling averages^4 */
    uint32_t quartic_n;             /**< how many went into it */

    uint32_t total_sum;             /**< every sample, for the plain average */
    uint32_t total_n;
    uint32_t seconds;               /**< seconds fed, which is the ride time */
    uint16_t ftp_w;                 /**< 0 until the rider sets one */
};

/**
 * @brief Start over
 * @param ftp_w the rider's threshold power; 0 leaves IF and TSS at zero
 */
void power_metrics_init(struct power_metrics *pm, uint16_t ftp_w);

/** The rider changed their threshold in the settings */
void power_metrics_set_ftp(struct power_metrics *pm, uint16_t ftp_w);

/**
 * @brief One second of the ride
 *
 * Call once a second while the ride runs, never while it is paused.
 * A power above PM_POWER_MAX_W is taken as noise and counted as zero
 * rather than dropped, because dropping a second would shift the window.
 */
void power_metrics_add(struct power_metrics *pm, uint16_t power_w);

/** Normalised power in watts, 0 until thirty seconds exist */
uint16_t power_metrics_np(const struct power_metrics *pm);

/** Plain average power in watts */
uint16_t power_metrics_avg(const struct power_metrics *pm);

/** Intensity factor in hundredths (100 is an hour at threshold) */
uint16_t power_metrics_if100(const struct power_metrics *pm);

/** Training stress, whole points */
uint16_t power_metrics_tss(const struct power_metrics *pm);

/** Variability index in hundredths; 100 is a perfectly steady ride */
uint16_t power_metrics_vi100(const struct power_metrics *pm);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_POWER_METRICS_H */
