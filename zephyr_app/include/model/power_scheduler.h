/**
 * @file power_scheduler.h
 * @brief Automatic power-off after a period without activity
 *
 * Port of legacy/source/scheduling/power_scheduler.cpp: each location in CRS
 * or PRC and each trainer update in FEC pings the scheduler; 15 minutes
 * without a ping turn the device off. The system state machine
 * (svc/sys_fsm.h) pings it and runs the shutdown when it expires; the
 * legacy called power_scheduler__shutdown() from here.
 */

#ifndef POWER_SCHEDULER_H
#define POWER_SCHEDULER_H

#include <stdbool.h>
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
 * @brief Check the idle time
 *
 * Call it periodically. Past the limit it returns true once and counts a
 * whole limit again, so a device still running afterwards (on USB, as the
 * legacy retried on every loop) tries again one limit later.
 *
 * @return true when the device should turn off
 */
bool power_scheduler_run(void);

#endif /* POWER_SCHEDULER_H */
