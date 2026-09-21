/**
 * @file activity.h
 * @brief Totals of the ride, auto-pause and laps
 *
 * The legacy has none of this: it logs points into `@DDMMYY.txt` from the
 * moment it boots until it shuts down, with no start, no pause and no lap
 * (`legacy/source/sd/sd_functions.cpp:605`). The only thing close is
 * `att.nbsec_act`, the seconds above 7 km/h
 * (`legacy/source/model/Attitude.cpp:509`), which this module keeps as it
 * is, apart, because the screens of the legacy show it.
 *
 * What this adds is what every computer on the market does and the FIT
 * format asks for:
 *
 * - **auto-pause**: the timer stops when the bike stops, so the average
 *   speed is the speed of the ride and not of the traffic light;
 * - **laps**, by hand or every so many kilometres, each with its own
 *   totals;
 * - the **totals** a `session` message carries: moving time, distance,
 *   ascent, descent, averages and maximums.
 *
 * Pure logic on a structure the caller owns: no Zephyr, no clock of its
 * own, no file. The caller feeds one sample per second and applies what
 * comes back.
 */

#ifndef MODEL_ACTIVITY_H
#define MODEL_ACTIVITY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Speed below which the timer pauses, in km/h.
 *
 * Not the 7 km/h of `att.nbsec_act`: that one says whether the rider is
 * pedalling in earnest, and a climb at 6 km/h is still riding. This one
 * only has to tell a stopped bike from a moving one.
 */
#define ACTIVITY_PAUSE_KMH      1.5f

/** Speed that takes the timer back, above the pause one so it does not flap */
#define ACTIVITY_RESUME_KMH     3.0f

/** Time below ACTIVITY_PAUSE_KMH before the timer stops */
#define ACTIVITY_PAUSE_HOLD_MS  3000U

/** Hysteresis of the descent, the same the climb of `attitude.c` uses */
#define ACTIVITY_DESCENT_HYST_M 2.0f

/** What one update asks the caller to do */
enum activity_event {
    ACTIVITY_EVENT_NONE = 0,
    ACTIVITY_EVENT_PAUSED,      /**< the bike stopped: the timer is held */
    ACTIVITY_EVENT_RESUMED,     /**< it is moving again */
    ACTIVITY_EVENT_LAP          /**< a lap closed; read it with activity_lap() */
};

/** Totals of a lap or of the whole ride */
struct activity_totals {
    uint32_t start_time;        /**< FIT date_time of the first sample */
    uint32_t end_time;          /**< FIT date_time of the last one */
    uint32_t elapsed_ms;        /**< wall time, pauses included */
    uint32_t timer_ms;          /**< moving time, pauses taken out */
    float dist_m;
    float ascent_m;
    float descent_m;
    float max_speed_kmh;
    uint32_t power_sum_ws;      /**< watt-seconds, for the average and the calories */
    uint32_t hr_sum;
    uint32_t cadence_sum;
    uint32_t samples;           /**< samples counted while the timer ran */
    uint32_t hr_samples;        /**< of those, the ones a strap answered */
    uint16_t max_power_w;
    uint8_t max_hr_bpm;
};

/** One second of the ride, as the model sees it */
struct activity_sample {
    uint32_t time;              /**< FIT date_time; 0 while the GNSS has no date */
    float speed_kmh;
    float dist_m;               /**< distance of the ride so far */
    float climb_m;              /**< climb of the ride so far, from `attitude.c` */
    float alt_m;                /**< filtered altitude, for the descent */
    int16_t power_w;
    uint8_t hr_bpm;
    uint8_t cadence_rpm;
};

/** State of one ride */
struct activity {
    struct activity_totals ride;
    struct activity_totals lap;     /**< the lap being ridden */
    struct activity_totals closed;  /**< the last lap that closed */
    uint32_t autolap_m;             /**< distance between automatic laps; 0 turns it off */
    uint32_t below_ms;              /**< time under the pause speed */
    float last_alt_m;               /**< reference of the descent */
    float lap_start_dist_m;
    uint16_t laps;                  /**< laps already closed */
    bool running;                   /**< the timer counts */
    bool started;                   /**< a first sample arrived */
    bool auto_pause;                /**< the rider wants the timer to stop by itself */
    bool alt_valid;
};

/**
 * Start a ride.
 *
 * @param autolap_m metres between automatic laps, 0 for none
 * @param auto_pause whether the timer stops by itself when the bike stops
 */
void activity_init(struct activity *a, uint32_t autolap_m, bool auto_pause);

/**
 * Feed one sample.
 *
 * @param dt_ms time since the previous sample
 * @return what the caller has to act on; only one event per call, and a lap
 * wins over a pause because the pause shows on the next one anyway
 */
enum activity_event activity_update(struct activity *a, const struct activity_sample *s,
                                    uint32_t dt_ms);

/**
 * Close the lap being ridden and start another, as the rider asking for it
 * on the device does.
 *
 * @return false when there is no ride yet
 */
bool activity_lap_now(struct activity *a);

/** Totals of the last lap that closed, valid after ACTIVITY_EVENT_LAP */
const struct activity_totals *activity_lap(const struct activity *a);

/** Totals of the whole ride */
const struct activity_totals *activity_ride(const struct activity *a);

/**
 * Close the ride: the lap being ridden becomes the last one, so a FIT file
 * always carries at least one lap.
 */
void activity_finish(struct activity *a);

/** Average speed of a set of totals, in km/h, over the moving time */
float activity_avg_speed(const struct activity_totals *t);

/** Average power, in watts, over the samples counted while the timer ran */
uint16_t activity_avg_power(const struct activity_totals *t);

/** Average heart rate; 0 when no strap ever answered */
uint8_t activity_avg_hr(const struct activity_totals *t);

/** Average cadence; 0 without a sensor */
uint8_t activity_avg_cadence(const struct activity_totals *t);

/**
 * Energy of the ride, in kcal.
 *
 * The work in kilojoules and the food energy in kilocalories very nearly
 * cancel for a cyclist: a kilojoule of work costs about 4,18 kJ of food, and
 * a kilocalorie is 4,184 kJ, so the number of kilojoules is the number of
 * kilocalories to within a percent. That is what every head unit does.
 */
uint16_t activity_calories(const struct activity_totals *t);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_ACTIVITY_H */
