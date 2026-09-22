/**
 * @file incident.h
 * @brief Bike alarm and crash detection, from the accelerometer
 *
 * > [!WARNING]
 * > **This is not a safety device.** It is a convenience, like the one on
 * > a Garmin, and it fails in both directions: a crash where the bike ends
 * > up moving, or where the device comes off the bars, is not detected, and
 * > a kerb taken hard can look like one. Nobody should ride differently
 * > because it is on, and nobody should be counted on to be warned by it.
 * > The alarm is just as weak: a thief who takes the whole bike into a van
 * > sets it off, and one who cuts the lock quietly may not.
 *
 * Two machines over the same input, because both ask the same question —
 * is the bike moving, and how hard was it shaken:
 *
 * ```
 * alarm:  OFF --arm--> SETTLING --5 s--> ARMED --movement--> RINGING
 * crash:  WATCHING --spike--> SHAKEN --still and stopped--> COUNTING --30 s--> CRASHED
 * ```
 *
 * The **alarm** is armed by the rider when they walk away from the bike.
 * It waits `INCIDENT_ARM_SETTLE_MS` first, because the rider is still
 * touching it, and then any movement above `INCIDENT_ALARM_G` held for
 * `INCIDENT_ALARM_HOLD_MS` rings it.
 *
 * The **crash** needs three things in a row, because any one of them alone
 * is an everyday event: a peak above `INCIDENT_CRASH_G`, then the bike
 * stopped, then the device still for `INCIDENT_CRASH_STILL_MS`. Only then
 * does the countdown start, and any key cancels it. A rider who is fine
 * will always cancel; a rider who cannot is the case this is for.
 *
 * Pure logic on the caller's structure: no Zephyr, no clock of its own, no
 * radio. The caller feeds one sample per second with the **peak**
 * acceleration seen since the last one, not the average, because an impact
 * lasts about a tenth of a second.
 */

#ifndef MODEL_INCIDENT_H
#define MODEL_INCIDENT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Movement, as a deviation from one g, that counts as the bike being moved */
#define INCIDENT_ALARM_G            0.15f

/** How long that movement has to last before the alarm rings */
#define INCIDENT_ALARM_HOLD_MS      1000U

/** Quiet time after arming, while the rider is still walking away */
#define INCIDENT_ARM_SETTLE_MS      5000U

/** Peak acceleration that makes a candidate for a crash, in g */
#define INCIDENT_CRASH_G            6.0f

/** Deviation from one g under which the device counts as still */
#define INCIDENT_CRASH_STILL_G      0.10f

/** Speed under which the bike counts as stopped, km/h */
#define INCIDENT_CRASH_STOPPED_KMH  3.0f

/** Still and stopped for this long after the peak makes it a crash */
#define INCIDENT_CRASH_STILL_MS     8000U

/** Time the rider has to cancel before the device calls it a crash */
#define INCIDENT_CRASH_COUNT_MS     30000U

/** Longest a peak is remembered while waiting for the bike to stop */
#define INCIDENT_CRASH_WINDOW_MS    15000U

/** What one update asks the caller to do */
enum incident_event {
    INCIDENT_EVENT_NONE = 0,
    INCIDENT_EVENT_ALARM,       /**< the bike moved while armed */
    INCIDENT_EVENT_COUNTING,    /**< a crash is being counted down */
    INCIDENT_EVENT_CRASH,       /**< the countdown ran out */
    INCIDENT_EVENT_CLEARED      /**< cancelled, disarmed, or back to normal */
};

/** Where the two machines stand */
enum incident_state {
    INCIDENT_OFF = 0,           /**< alarm not armed, nothing pending */
    INCIDENT_SETTLING,          /**< armed, waiting for the rider to walk away */
    INCIDENT_ARMED,             /**< armed and watching */
    INCIDENT_RINGING,           /**< the bike moved */
    INCIDENT_SHAKEN,            /**< a peak, waiting for the bike to stop */
    INCIDENT_COUNTING,          /**< stopped and still: counting down */
    INCIDENT_CRASHED            /**< the countdown ran out */
};

/** One second of the ride, as the model sees it */
struct incident_sample {
    float peak_g;       /**< largest magnitude since the last sample, in g */
    float still_g;      /**< how far from one g the device is right now */
    float speed_kmh;
};

/** State of the two machines */
struct incident {
    uint32_t since_ms;      /**< time in the state */
    uint32_t moved_ms;      /**< movement held while armed */
    uint32_t still_ms;      /**< stillness held after a peak */
    uint32_t shaken_ms;     /**< time since the peak */
    uint32_t count_ms;      /**< countdown run so far */
    float peak_g;           /**< the peak that started it */
    uint8_t state;          /**< enum incident_state */
    bool alarm_on;          /**< the rider wants the alarm */
    bool crash_on;          /**< the rider wants crash detection */
};

/** Start with both machines idle */
void incident_init(struct incident *in, bool crash_on);

/** Arm or disarm the bike alarm */
void incident_arm(struct incident *in, bool armed);

/** Turn crash detection on or off */
void incident_set_crash(struct incident *in, bool on);

/**
 * Feed one sample.
 *
 * @param dt_ms time since the previous one
 * @return what the caller has to act on
 */
enum incident_event incident_update(struct incident *in, const struct incident_sample *s,
                                    uint32_t dt_ms);

/**
 * A key was pressed: cancel whatever is pending.
 *
 * Cancels the countdown, silences the alarm and disarms it, because a
 * rider who can press a key is a rider who is there.
 */
void incident_cancel(struct incident *in);

/** Seconds left to cancel; 0 when nothing is being counted */
uint32_t incident_countdown_s(const struct incident *in);

/** Where the machines are */
uint8_t incident_state(const struct incident *in);

/** true while the alarm is armed, settling or ringing */
bool incident_is_armed(const struct incident *in);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_INCIDENT_H */
