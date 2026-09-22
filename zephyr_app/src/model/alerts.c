/**
 * @file alerts.c
 * @brief The alerts a rider sets, and when they go off
 *
 * The rules about not nagging, and the margins, are in model/alerts.h.
 */

#include <string.h>

#include "model/alerts.h"

void alerts_init(struct alerts *a)
{
    if (a != NULL) {
        (void)memset(a, 0, sizeof(*a));
    }
}

void alerts_set(struct alerts *a, enum alert_id id, uint16_t value, bool on)
{
    if ((a == NULL) || (id >= ALERT_COUNT)) {
        return;
    }

    a->cfg[id].value = value;
    a->cfg[id].on = on && (value > 0U);

    /* a limit that just changed starts armed, whatever the value is now */
    a->armed[id] = true;
    a->last_ms[id] = 0U;
    a->done[id] = 0U;
}

struct alert_cfg alerts_get(const struct alerts *a, enum alert_id id)
{
    struct alert_cfg none = {0U, false};

    if ((a == NULL) || (id >= ALERT_COUNT)) {
        return none;
    }

    return a->cfg[id];
}

void alerts_reset(struct alerts *a)
{
    if (a == NULL) {
        return;
    }

    for (unsigned int i = 0U; i < ALERT_COUNT; i++) {
        a->armed[i] = true;
        a->last_ms[i] = 0U;
        a->done[i] = 0U;
    }
    a->started = false;
}

/** May this alert speak now? */
static bool may_fire(const struct alerts *a, enum alert_id id, uint32_t now_ms)
{
    if (!a->armed[id]) {
        return false;
    }

    if (a->last_ms[id] == 0U) {
        return true;        /* it has never fired */
    }

    return (now_ms - a->last_ms[id]) >= ALERT_MIN_REPEAT_MS;
}

/**
 * One alert over a limit.
 *
 * @param over true for "above the limit", false for "below it"
 * @param value what the ride is doing
 * @param hyst how far back inside the limit it must come to re-arm
 */
static bool check_threshold(struct alerts *a, enum alert_id id, bool over,
                            uint32_t value, uint32_t hyst, uint32_t now_ms)
{
    if (!a->cfg[id].on) {
        return false;
    }

    uint32_t limit = a->cfg[id].value;
    bool outside = over ? (value > limit) : (value < limit);

    if (!outside) {
        /*
         * Back inside, but only past the margin does it become able to
         * speak again: right on the line it stays quiet.
         */
        bool clear = over ? ((limit >= hyst) ? (value <= (limit - hyst)) : true)
                          : (value >= (limit + hyst));

        if (clear) {
            a->armed[id] = true;
        }

        return false;
    }

    if (!may_fire(a, id, now_ms)) {
        return false;
    }

    a->armed[id] = false;
    a->last_ms[id] = (now_ms != 0U) ? now_ms : 1U;

    return true;
}

/**
 * One alert every so much.
 *
 * @param progress what has been done, in the unit of the interval
 * @return true when a new whole multiple has been passed; a leap over two
 * multiples fires once, because two notifications for one epoch are noise
 */
static bool check_interval(struct alerts *a, enum alert_id id, uint32_t progress)
{
    if (!a->cfg[id].on) {
        return false;
    }

    uint32_t reached = progress / a->cfg[id].value;

    if (reached <= a->done[id]) {
        return false;
    }

    a->done[id] = reached;

    return true;
}

uint32_t alerts_update(struct alerts *a, const struct alert_sample *s, uint32_t now_ms)
{
    if ((a == NULL) || (s == NULL)) {
        return 0U;
    }

    uint32_t fired = 0U;

    /*
     * A sensor that is not there reads zero, and a zero would set off every
     * "below" alert for ever. So a low alert only speaks once its quantity
     * has been seen at all.
     */
    if (check_threshold(a, ALERT_HR_HIGH, true, s->hr_bpm, ALERT_HYST_BPM, now_ms)) {
        fired |= 1U << ALERT_HR_HIGH;
    }
    if ((s->hr_bpm > 0U) &&
        check_threshold(a, ALERT_HR_LOW, false, s->hr_bpm, ALERT_HYST_BPM, now_ms)) {
        fired |= 1U << ALERT_HR_LOW;
    }

    if (check_threshold(a, ALERT_POWER_HIGH, true, s->power_w, ALERT_HYST_W, now_ms)) {
        fired |= 1U << ALERT_POWER_HIGH;
    }
    if ((s->power_w > 0U) &&
        check_threshold(a, ALERT_POWER_LOW, false, s->power_w, ALERT_HYST_W, now_ms)) {
        fired |= 1U << ALERT_POWER_LOW;
    }

    if (check_threshold(a, ALERT_SPEED_HIGH, true, s->speed_kmh10, ALERT_HYST_KMH10, now_ms)) {
        fired |= 1U << ALERT_SPEED_HIGH;
    }
    if ((s->speed_kmh10 > 0U) &&
        check_threshold(a, ALERT_SPEED_LOW, false, s->speed_kmh10, ALERT_HYST_KMH10, now_ms)) {
        fired |= 1U << ALERT_SPEED_LOW;
    }

    if (check_threshold(a, ALERT_CADENCE_HIGH, true, s->cadence_rpm, ALERT_HYST_RPM, now_ms)) {
        fired |= 1U << ALERT_CADENCE_HIGH;
    }
    if ((s->cadence_rpm > 0U) &&
        check_threshold(a, ALERT_CADENCE_LOW, false, s->cadence_rpm, ALERT_HYST_RPM, now_ms)) {
        fired |= 1U << ALERT_CADENCE_LOW;
    }

    /* the interval alerts, each in its own unit */
    uint32_t hundreds = (s->dist_m > 0.0f) ? (uint32_t)(s->dist_m / 100.0f) : 0U;

    if (check_interval(a, ALERT_DISTANCE, hundreds)) {
        fired |= 1U << ALERT_DISTANCE;
    }
    if (check_interval(a, ALERT_TIME, s->moving_s / 60U)) {
        fired |= 1U << ALERT_TIME;
    }
    if (check_interval(a, ALERT_DRINK, s->moving_s / 60U)) {
        fired |= 1U << ALERT_DRINK;
    }
    if (check_interval(a, ALERT_EAT, s->moving_s / 60U)) {
        fired |= 1U << ALERT_EAT;
    }

    a->started = true;

    return fired;
}
