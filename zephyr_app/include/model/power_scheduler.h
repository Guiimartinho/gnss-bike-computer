/**
 * @file power_scheduler.h
 * @brief Auto-off after a period without activity, and deliberate power-off
 *
 * Port of legacy/source/scheduling/power_scheduler.cpp: the model pings the
 * scheduler on each location processed in CRS or PRC mode and on each
 * trainer update in FEC mode; after 15 minutes without a ping the device
 * turns itself off through the STC3100 power latch.
 */

#ifndef POWER_SCHEDULER_H
#define POWER_SCHEDULER_H

#include <stdint.h>

/** Minutes without a ping before the device turns off (legacy POWER_SCHEDULER_MAX_IDLE_MIN) */
#define POWER_SCHEDULER_MAX_IDLE_MIN    15U

/** Activity that keeps the device on */
typedef enum {
    POWER_PING_CRS = 0,     /**< Location processed in CRS or PRC mode */
    POWER_PING_FEC,         /**< Trainer data processed in FEC mode */
} power_ping_t;

/**
 * @brief Start counting the idle time from now
 */
void power_scheduler_init(void);

/**
 * @brief Report activity: restarts the idle time
 * @param type Source of the activity
 */
void power_scheduler_ping(power_ping_t type);

/**
 * @brief Turn the device off once the idle time passes the limit
 *
 * Call it periodically from main_loop. If the device is still running after
 * the power-off (USB power), the next attempt comes one limit later.
 */
void power_scheduler_run(void);

/**
 * @brief Power the device off now
 *
 * Forgets the saved activity state, so the next boot does not restore it,
 * and releases the power latch through the STC3100.
 */
void power_scheduler_shutdown(void);

#endif /* POWER_SCHEDULER_H */
