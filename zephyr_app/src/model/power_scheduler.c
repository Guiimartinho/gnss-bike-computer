/**
 * @file power_scheduler.c
 * @brief Auto-off after a period without activity, and deliberate power-off
 *
 * Port of legacy/source/scheduling/power_scheduler.cpp.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "model/power_scheduler.h"
#include "model/crash_recovery.h"
#include "drivers/stc3100.h"

LOG_MODULE_REGISTER(power_scheduler, CONFIG_LOG_DEFAULT_LEVEL);

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

void power_scheduler_run(void)
{
    uint32_t now = k_uptime_get_32();

    /* Strictly greater, as the legacy: exactly 15 min is still on */
    if ((now - last_ping_ms) > MAX_IDLE_MS) {
        LOG_INF("No activity for %u min: powering off",
                (unsigned int)POWER_SCHEDULER_MAX_IDLE_MIN);
        power_scheduler_shutdown();

        /*
         * Still here: the board runs on USB power (or has no latch, as the
         * DK). The legacy retried on every loop; retry one limit later.
         */
        last_ping_ms = now;
    }
}

void power_scheduler_shutdown(void)
{
    crash_recovery_clear_saved_state();

    app_err_t err = stc3100_shutdown();

    if (err != APP_OK) {
        LOG_WRN("Power latch not released: %d", err);
    }
}
