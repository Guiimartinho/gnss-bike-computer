/**
 * @file alerts.h
 * @brief The alerts a rider sets, and when they go off
 *
 * Every head unit lets the rider say "tell me when my heart rate goes over
 * 165", "tell me every 10 km", "remind me to drink every 20 minutes". This
 * port had none of it. The alerts themselves are trivial; what is not
 * trivial, and what this module exists for, is **not nagging**:
 *
 * - a threshold alert fires when the value crosses the line, and then
 *   stays quiet until the value has come back past the line by a margin.
 *   Without that margin a rider sitting exactly on 165 bpm gets an alert
 *   every epoch, and stops looking at the screen;
 * - even after coming back, it will not fire again within
 *   `ALERT_MIN_REPEAT_MS`, so a hard effort that oscillates around the
 *   limit gives one warning a minute and not twenty;
 * - an interval alert fires on each whole multiple, and a long epoch that
 *   crosses two multiples at once fires **once**, not twice.
 *
 * ## The margins
 *
 * They are absolute and per quantity, because a percentage behaves badly
 * at both ends: 5 % of 40 bpm is 2 and 5 % of 1000 W is 50.
 *
 * | Alert | Margin to re-arm |
 * |---|---|
 * | Heart rate | 3 bpm |
 * | Power | 15 W |
 * | Speed | 1,0 km/h |
 * | Cadence | 3 rpm |
 *
 * ## What it is not
 *
 * It does not know how to make a sound or draw anything. It answers with a
 * bitmask of what went off, and the caller turns that into a notification
 * on the screen and, one day, into a buzz.
 */

#ifndef MODEL_ALERTS_H
#define MODEL_ALERTS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** What the rider can be told about */
enum alert_id {
    ALERT_HR_HIGH = 0,      /**< above the value, in bpm */
    ALERT_HR_LOW,           /**< below the value, in bpm */
    ALERT_POWER_HIGH,       /**< above the value, in watts */
    ALERT_POWER_LOW,
    ALERT_SPEED_HIGH,       /**< above the value, in 0,1 km/h */
    ALERT_SPEED_LOW,
    ALERT_CADENCE_HIGH,     /**< above the value, in rpm */
    ALERT_CADENCE_LOW,
    ALERT_DISTANCE,         /**< every so many hundred metres */
    ALERT_TIME,             /**< every so many minutes of moving time */
    ALERT_DRINK,            /**< every so many minutes */
    ALERT_EAT,
    ALERT_COUNT
};

/** Shortest gap between two firings of the same threshold alert */
#define ALERT_MIN_REPEAT_MS     60000U

/* How far back inside the limit the value must come before it can fire again */
#define ALERT_HYST_BPM          3U
#define ALERT_HYST_W            15U
#define ALERT_HYST_KMH10        10U     /**< 1,0 km/h */
#define ALERT_HYST_RPM          3U

/** One alert as the rider set it */
struct alert_cfg {
    uint16_t value;         /**< the limit, or the interval; unit per alert */
    bool on;
};

/** What the ride looks like right now */
struct alert_sample {
    uint8_t hr_bpm;
    uint16_t power_w;
    uint16_t speed_kmh10;   /**< 0,1 km/h */
    uint8_t cadence_rpm;
    float dist_m;           /**< of the ride so far */
    uint32_t moving_s;      /**< moving time of the ride so far */
};

/** What the module has to remember */
struct alerts {
    struct alert_cfg cfg[ALERT_COUNT];
    bool armed[ALERT_COUNT];        /**< the value is back inside the limit */
    uint32_t last_ms[ALERT_COUNT];  /**< when it last went off */
    uint32_t done[ALERT_COUNT];     /**< multiples already fired, for intervals */
    bool started;
};

/** No alerts at all */
void alerts_init(struct alerts *a);

/**
 * @brief Set one alert
 * @param value the limit or the interval; 0 turns it off whatever @p on says
 */
void alerts_set(struct alerts *a, enum alert_id id, uint16_t value, bool on);

/** How the rider set one, for the settings screen */
struct alert_cfg alerts_get(const struct alerts *a, enum alert_id id);

/**
 * @brief One epoch of the ride
 *
 * @param now_ms uptime
 * @return bitmask of `1U << enum alert_id` for everything that went off,
 * 0 when nothing did
 */
uint32_t alerts_update(struct alerts *a, const struct alert_sample *s, uint32_t now_ms);

/** The ride ended or started over: forget what has fired */
void alerts_reset(struct alerts *a);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_ALERTS_H */
