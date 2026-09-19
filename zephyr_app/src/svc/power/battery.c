/**
 * @file battery.c
 * @brief When the battery is low or at its end (battery.h)
 */

#include "svc/battery.h"

void battery_init(battery_t *b)
{
    b->low_armed = true;
    b->critical = false;
}

battery_event_t battery_update(battery_t *b, const battery_reading_t *r)
{
    bool charging = r->vbus || (r->avg_ua > 0);

    if (r->pct >= BATTERY_LOW_REARM_PCT) {
        b->low_armed = true;
    }
    if (charging) {
        /* plugged in or charging from the sun: the end moves away */
        b->critical = false;
        return BATTERY_EV_NONE;
    }
    if ((r->pct == 0U) && !b->critical) {
        b->critical = true;
        return BATTERY_EV_CRITICAL;
    }
    if ((r->pct <= BATTERY_LOW_PCT) && b->low_armed) {
        b->low_armed = false;
        return BATTERY_EV_LOW;
    }
    return BATTERY_EV_NONE;
}
