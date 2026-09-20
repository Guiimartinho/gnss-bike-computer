/**
 * @file gnss_power.c
 * @brief Power state machine of the GNSS receiver
 *
 * Rules in svc/gnss_power.h and in docs/16-arquitetura-firmware.md (GNSS).
 */

#include "svc/gnss_power.h"

#include "app/app_events.h"

void gnss_power_init(struct gnss_power *p, bool has_leap)
{
    if (p == NULL) {
        return;
    }
    p->mode = APP_GNSS_MODE_BACKUP;
    p->started = false;
    p->had_fix = false;
    /* without LEAP the receiver only tracks at full power: the machine says
     * so from the start and never asks for a mode it does not have */
    p->full_power = !has_leap;
    p->has_leap = has_leap;
    p->no_fix = 0U;
    p->good_ms = 0U;
    p->silence_ms = 0U;
}

enum gnss_power_action gnss_power_mode_change(struct gnss_power *p, bool uses_gnss)
{
    if (p == NULL) {
        return GNSS_POWER_ACTION_NONE;
    }

    bool first = !p->started;

    p->started = true;

    if (uses_gnss) {
        if (p->mode != APP_GNSS_MODE_BACKUP) {
            /* the machine starts in backup, so the first call comes here */
            return GNSS_POWER_ACTION_NONE;
        }
        /* the receiver comes back from the standby without its configuration */
        p->mode = APP_GNSS_MODE_ACQ;
        p->had_fix = false;
        p->full_power = !p->has_leap;
        p->no_fix = 0U;
        p->good_ms = 0U;
        p->silence_ms = 0U;

        return GNSS_POWER_ACTION_WAKE;
    }

    if ((p->mode == APP_GNSS_MODE_BACKUP) && !first) {
        return GNSS_POWER_ACTION_NONE;
    }
    p->mode = APP_GNSS_MODE_BACKUP;

    return GNSS_POWER_ACTION_STANDBY;
}

enum gnss_power_action gnss_power_epoch(struct gnss_power *p, bool fix, uint32_t interval_ms)
{
    if ((p == NULL) || (p->mode == APP_GNSS_MODE_BACKUP)) {
        return GNSS_POWER_ACTION_NONE;
    }

    enum gnss_power_action action = GNSS_POWER_ACTION_NONE;

    p->silence_ms = 0U;
    if (fix) {
        p->no_fix = 0U;
        p->had_fix = true;
        if (p->full_power && p->has_leap) {
            p->good_ms += interval_ms;
            if (p->good_ms >= GNSS_POWER_GOOD_MS) {
                /* the signal came back: the receiver tracks in low power again */
                p->full_power = false;
                p->good_ms = 0U;
                action = GNSS_POWER_ACTION_LEAP;
            }
        }
        p->mode = p->full_power ? APP_GNSS_MODE_FULL : APP_GNSS_MODE_LEAP;

        return action;
    }

    p->good_ms = 0U;
    if (p->no_fix < UINT8_MAX) {
        p->no_fix++;
    }
    if (p->no_fix >= GNSS_POWER_WEAK_EPOCHS) {
        p->mode = APP_GNSS_MODE_ACQ;
        if (!p->full_power) {
            /* weak signal: acquisition and tracking at full power */
            p->full_power = true;
            action = GNSS_POWER_ACTION_FULL;
        }
    }

    return action;
}

enum gnss_power_action gnss_power_tick(struct gnss_power *p, uint32_t elapsed_ms)
{
    if ((p == NULL) || (p->mode == APP_GNSS_MODE_BACKUP)) {
        return GNSS_POWER_ACTION_NONE;
    }

    uint32_t before = p->silence_ms;

    p->silence_ms += elapsed_ms;

    if ((before < GNSS_POWER_RESET_MS) && (p->silence_ms >= GNSS_POWER_RESET_MS)) {
        /* the receiver never came back: the reset pin is the last resort */
        p->silence_ms = 0U;

        return GNSS_POWER_ACTION_RESET;
    }
    if ((before < GNSS_POWER_SILENCE_MS) && (p->silence_ms >= GNSS_POWER_SILENCE_MS)) {
        return GNSS_POWER_ACTION_CONFIGURE;
    }

    return GNSS_POWER_ACTION_NONE;
}

uint8_t gnss_power_mode(const struct gnss_power *p)
{
    return (p == NULL) ? (uint8_t)APP_GNSS_MODE_BACKUP : p->mode;
}
