/**
 * @file activity.c
 * @brief Totals of the ride, auto-pause and laps
 *
 * Rules and what the legacy does (and does not) in model/activity.h.
 */

#include <math.h>
#include <string.h>

#include "model/activity.h"

/** Start a set of totals from the point the ride or the lap opens at */
static void totals_reset(struct activity_totals *t, uint32_t time)
{
    (void)memset(t, 0, sizeof(*t));
    t->start_time = time;
    t->end_time = time;
}

void activity_init(struct activity *a, uint32_t autolap_m, bool auto_pause)
{
    if (a == NULL) {
        return;
    }

    (void)memset(a, 0, sizeof(*a));
    a->autolap_m = autolap_m;
    a->auto_pause = auto_pause;
    /* without auto-pause the timer runs from the first sample and never stops */
    a->running = true;
}

/** Add one sample to a set of totals; only called while the timer runs */
static void totals_add(struct activity_totals *t, const struct activity_sample *s, uint32_t dt_ms)
{
    t->timer_ms += dt_ms;
    t->samples++;

    if (s->speed_kmh > t->max_speed_kmh) {
        t->max_speed_kmh = s->speed_kmh;
    }
    if (s->power_w > 0) {
        uint32_t w = (uint32_t)s->power_w;

        /* watt-seconds: the power of this sample over the time it covered */
        t->power_sum_ws += (w * dt_ms) / 1000U;
        if (w > (uint32_t)t->max_power_w) {
            t->max_power_w = (uint16_t)w;
        }
    }
    if (s->hr_bpm != 0U) {
        /* counted apart: a strap that drops for half the ride must not
         * halve the average of the half it did answer */
        t->hr_sum += s->hr_bpm;
        t->hr_samples++;
        if (s->hr_bpm > t->max_hr_bpm) {
            t->max_hr_bpm = s->hr_bpm;
        }
    }
    t->cadence_sum += s->cadence_rpm;
}

/** Close the lap being ridden and open the next one */
static void lap_close(struct activity *a, uint32_t time)
{
    a->lap.end_time = time;
    a->closed = a->lap;
    a->laps++;
    a->lap_start_dist_m = a->ride.dist_m;
    totals_reset(&a->lap, time);
}

enum activity_event activity_update(struct activity *a, const struct activity_sample *s,
                                    uint32_t dt_ms)
{
    if ((a == NULL) || (s == NULL)) {
        return ACTIVITY_EVENT_NONE;
    }

    if (!a->started) {
        a->started = true;
        totals_reset(&a->ride, s->time);
        totals_reset(&a->lap, s->time);
        a->last_alt_m = s->alt_m;
        a->alt_valid = isfinite(s->alt_m);
        a->ride.dist_m = s->dist_m;
        a->ride.ascent_m = s->climb_m;
        a->lap_start_dist_m = s->dist_m;

        return ACTIVITY_EVENT_NONE;
    }

    enum activity_event ev = ACTIVITY_EVENT_NONE;

    a->ride.elapsed_ms += dt_ms;
    a->lap.elapsed_ms += dt_ms;
    if (s->time != 0U) {
        a->ride.end_time = s->time;
        a->lap.end_time = s->time;
        if (a->ride.start_time == 0U) {
            /* the first samples had no date yet: the ride starts here */
            a->ride.start_time = s->time;
            a->lap.start_time = s->time;
        }
    }

    /* auto-pause: below the threshold for a while stops the timer, and any
     * real speed takes it back at once */
    if (a->auto_pause) {
        if (s->speed_kmh < ACTIVITY_PAUSE_KMH) {
            a->below_ms += dt_ms;
            if (a->running && (a->below_ms >= ACTIVITY_PAUSE_HOLD_MS)) {
                a->running = false;
                ev = ACTIVITY_EVENT_PAUSED;
            }
        } else {
            a->below_ms = 0U;
            if (!a->running && (s->speed_kmh >= ACTIVITY_RESUME_KMH)) {
                a->running = true;
                ev = ACTIVITY_EVENT_RESUMED;
            }
        }
    }

    /* the distance and the climb come from the model and only move while
     * the bike moves, so they are taken whatever the timer does */
    float dist = s->dist_m - a->ride.dist_m;

    if (isfinite(dist) && (dist > 0.0f)) {
        a->ride.dist_m = s->dist_m;
        a->lap.dist_m += dist;
    }

    float climb = s->climb_m - a->ride.ascent_m;

    if (isfinite(climb) && (climb > 0.0f)) {
        a->ride.ascent_m = s->climb_m;
        a->lap.ascent_m += climb;
    }

    /* the descent is of this port: the legacy never counted it. Same dead
     * band as the climb of `attitude.c`, so the two agree on what is noise */
    if (isfinite(s->alt_m)) {
        if (!a->alt_valid) {
            a->last_alt_m = s->alt_m;
            a->alt_valid = true;
        } else if (s->alt_m + ACTIVITY_DESCENT_HYST_M < a->last_alt_m) {
            float drop = a->last_alt_m - s->alt_m;

            a->ride.descent_m += drop;
            a->lap.descent_m += drop;
            a->last_alt_m = s->alt_m;
        } else if (s->alt_m > (a->last_alt_m + ACTIVITY_DESCENT_HYST_M)) {
            a->last_alt_m = s->alt_m;
        }
    }

    if (a->running) {
        totals_add(&a->ride, s, dt_ms);
        totals_add(&a->lap, s, dt_ms);
    }

    /* the automatic lap: every so many metres of the ride */
    if ((a->autolap_m != 0U) &&
        ((a->ride.dist_m - a->lap_start_dist_m) >= (float)a->autolap_m)) {
        lap_close(a, (s->time != 0U) ? s->time : a->ride.end_time);
        ev = ACTIVITY_EVENT_LAP;
    }

    return ev;
}

bool activity_lap_now(struct activity *a)
{
    if ((a == NULL) || !a->started) {
        return false;
    }

    lap_close(a, a->ride.end_time);

    return true;
}

const struct activity_totals *activity_lap(const struct activity *a)
{
    return (a != NULL) ? &a->closed : NULL;
}

const struct activity_totals *activity_ride(const struct activity *a)
{
    return (a != NULL) ? &a->ride : NULL;
}

void activity_finish(struct activity *a)
{
    if ((a == NULL) || !a->started) {
        return;
    }

    /* an empty lap at the end would only add a message with nothing in it */
    if ((a->lap.timer_ms != 0U) || (a->lap.dist_m > 0.0f) || (a->laps == 0U)) {
        lap_close(a, a->ride.end_time);
    }
    a->running = false;
}

float activity_avg_speed(const struct activity_totals *t)
{
    if ((t == NULL) || (t->timer_ms == 0U)) {
        return 0.0f;
    }

    /* the moving time is what the average of a ride is taken over */
    return (t->dist_m / (float)t->timer_ms) * 3600.0f;
}

uint16_t activity_avg_power(const struct activity_totals *t)
{
    if ((t == NULL) || (t->timer_ms < 1000U)) {
        return 0U;
    }

    uint32_t avg = t->power_sum_ws / (t->timer_ms / 1000U);

    return (avg > 65534U) ? 65534U : (uint16_t)avg;
}

uint8_t activity_avg_hr(const struct activity_totals *t)
{
    if ((t == NULL) || (t->hr_samples == 0U)) {
        return 0U;
    }

    uint32_t avg = t->hr_sum / t->hr_samples;

    return (avg > 254U) ? 254U : (uint8_t)avg;
}

uint8_t activity_avg_cadence(const struct activity_totals *t)
{
    if ((t == NULL) || (t->samples == 0U)) {
        return 0U;
    }

    uint32_t avg = t->cadence_sum / t->samples;

    return (avg > 254U) ? 254U : (uint8_t)avg;
}

uint16_t activity_calories(const struct activity_totals *t)
{
    if (t == NULL) {
        return 0U;
    }

    /* watt-seconds to kilojoules, and a kilojoule of work is about a
     * kilocalorie of food for a cyclist (model/activity.h) */
    uint32_t kcal = t->power_sum_ws / 1000U;

    return (kcal > 65534U) ? 65534U : (uint16_t)kcal;
}
