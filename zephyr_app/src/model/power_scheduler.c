/**
 * @file power_scheduler.c
 * @brief Automatic power-off after a period without activity
 *
 * Port of legacy/source/scheduling/power_scheduler.cpp.
 */

#include <zephyr/kernel.h>

#include "model/power_scheduler.h"

/** Idle limit in milliseconds */
#define MAX_IDLE_MS     (POWER_SCHEDULER_MAX_IDLE_MIN * 60U * 1000U)

/** Uptime of the last ping; unsigned arithmetic survives the 49-day wrap */
static uint32_t last_ping_ms;

void power_scheduler_init(void)
{
    last_ping_ms = k_uptime_get_32();
}

void power_scheduler_ping(power_ping_t type)
{
    switch (type) {
    case POWER_PING_CRS:
    case POWER_PING_FEC:
        last_ping_ms = k_uptime_get_32();
        break;

    default:
        break;
    }
}

bool power_scheduler_run(void)
{
    uint32_t now = k_uptime_get_32();

    /* Strictly greater, as the legacy: exactly 15 min is still on */
    if ((now - last_ping_ms) > MAX_IDLE_MS) {
        last_ping_ms = now;
        return true;
    }
    return false;
}
