/**
 * @file battery.h
 * @brief When the battery is low or at its end (docs/16, Sistema e energia)
 *
 * Plain C, no Zephyr (host test test_battery). New in the port: the legacy
 * has no low-battery rule (legacy/source/sensors/STC3100.cpp only feeds the
 * charge shown on screen, and power_scheduler.cpp shuts down on idle only).
 *
 *  - Low: one notification when the charge falls to BATTERY_LOW_PCT while
 *    discharging, and another only after it rose to BATTERY_LOW_REARM_PCT.
 *  - Critical: 0 % while discharging. The MAX17262 declares 0 % at its
 *    empty voltage (datasheet, VEmpty register), and the system machine
 *    saves and shuts down before the protection of the pack cuts the cell.
 *
 * Charging means VBUS present or a positive average current (the solar
 * charger adds current without VBUS).
 */

#ifndef SVC_BATTERY_H
#define SVC_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Notify at this charge while discharging */
#define BATTERY_LOW_PCT         10U
/** Notify again only after the charge rose to this */
#define BATTERY_LOW_REARM_PCT   15U

typedef enum {
    BATTERY_EV_NONE = 0,
    BATTERY_EV_LOW,
    BATTERY_EV_CRITICAL
} battery_event_t;

typedef struct {
    bool low_armed;
    bool critical;
} battery_t;

/** One reading of the gauge */
typedef struct {
    uint8_t pct;
    int32_t avg_ua;         /**< positive while charging */
    bool vbus;
} battery_reading_t;

void battery_init(battery_t *b);

/** A new reading: at most one event, each only once until it may happen again */
battery_event_t battery_update(battery_t *b, const battery_reading_t *r);

#ifdef __cplusplus
}
#endif

#endif /* SVC_BATTERY_H */
