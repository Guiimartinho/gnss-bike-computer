/**
 * @file incident.c
 * @brief Bike alarm and crash detection, from the accelerometer
 *
 * Rules, thresholds and the warning about what this is not, in
 * model/incident.h.
 */

#include <math.h>
#include <string.h>

#include "model/incident.h"

static void go(struct incident *in, enum incident_state st)
{
    in->state = (uint8_t)st;
    in->since_ms = 0U;
    in->moved_ms = 0U;
    in->still_ms = 0U;
    in->shaken_ms = 0U;
    in->count_ms = 0U;
}

void incident_init(struct incident *in, bool crash_on)
{
    if (in == NULL) {
        return;
    }

    (void)memset(in, 0, sizeof(*in));
    in->crash_on = crash_on;
    in->state = (uint8_t)INCIDENT_OFF;
}

void incident_arm(struct incident *in, bool armed)
{
    if (in == NULL) {
        return;
    }

    in->alarm_on = armed;
    if (armed) {
        /* the rider is still touching the bike: wait before watching */
        go(in, INCIDENT_SETTLING);
    } else if ((in->state == INCIDENT_SETTLING) || (in->state == INCIDENT_ARMED) ||
               (in->state == INCIDENT_RINGING)) {
        go(in, INCIDENT_OFF);
    }
}

void incident_set_crash(struct incident *in, bool on)
{
    if (in == NULL) {
        return;
    }

    in->crash_on = on;
    if (!on && ((in->state == INCIDENT_SHAKEN) || (in->state == INCIDENT_COUNTING) ||
                (in->state == INCIDENT_CRASHED))) {
        go(in, in->alarm_on ? INCIDENT_ARMED : INCIDENT_OFF);
    }
}

void incident_cancel(struct incident *in)
{
    if (in == NULL) {
        return;
    }

    /* a rider who can press a key is a rider who is there */
    in->alarm_on = false;
    go(in, INCIDENT_OFF);
}

uint32_t incident_countdown_s(const struct incident *in)
{
    if ((in == NULL) || (in->state != INCIDENT_COUNTING)) {
        return 0U;
    }

    uint32_t left = (in->count_ms < INCIDENT_CRASH_COUNT_MS)
                        ? (INCIDENT_CRASH_COUNT_MS - in->count_ms)
                        : 0U;

    return (left + 999U) / 1000U;
}

uint8_t incident_state(const struct incident *in)
{
    return (in != NULL) ? in->state : (uint8_t)INCIDENT_OFF;
}

bool incident_is_armed(const struct incident *in)
{
    if (in == NULL) {
        return false;
    }

    return (in->state == INCIDENT_SETTLING) || (in->state == INCIDENT_ARMED) ||
           (in->state == INCIDENT_RINGING);
}

/** The crash machine, which runs whatever the alarm is doing */
static enum incident_event crash_update(struct incident *in, const struct incident_sample *s,
                                        uint32_t dt_ms)
{
    bool still = isfinite(s->still_g) && (s->still_g <= INCIDENT_CRASH_STILL_G);
    bool stopped = !isfinite(s->speed_kmh) || (s->speed_kmh <= INCIDENT_CRASH_STOPPED_KMH);

    switch ((enum incident_state)in->state) {
    case INCIDENT_SHAKEN:
        in->shaken_ms += dt_ms;
        if (still && stopped) {
            in->still_ms += dt_ms;
            if (in->still_ms >= INCIDENT_CRASH_STILL_MS) {
                float peak = in->peak_g;

                go(in, INCIDENT_COUNTING);
                in->peak_g = peak;

                return INCIDENT_EVENT_COUNTING;
            }
        } else {
            /* moving again: whatever it was, it was not a crash */
            in->still_ms = 0U;
        }
        if (in->shaken_ms >= INCIDENT_CRASH_WINDOW_MS) {
            go(in, in->alarm_on ? INCIDENT_ARMED : INCIDENT_OFF);

            return INCIDENT_EVENT_CLEARED;
        }

        return INCIDENT_EVENT_NONE;

    case INCIDENT_COUNTING:
        /* riding off cancels it as surely as a key would */
        if (!stopped) {
            go(in, in->alarm_on ? INCIDENT_ARMED : INCIDENT_OFF);

            return INCIDENT_EVENT_CLEARED;
        }
        in->count_ms += dt_ms;
        if (in->count_ms >= INCIDENT_CRASH_COUNT_MS) {
            float peak = in->peak_g;

            go(in, INCIDENT_CRASHED);
            in->peak_g = peak;

            return INCIDENT_EVENT_CRASH;
        }

        return INCIDENT_EVENT_NONE;

    case INCIDENT_CRASHED:
        return INCIDENT_EVENT_NONE;     /* only a key gets out of here */

    default:
        break;
    }

    /* watching: a peak big enough opens the question */
    if (isfinite(s->peak_g) && (s->peak_g >= INCIDENT_CRASH_G)) {
        float peak = s->peak_g;

        go(in, INCIDENT_SHAKEN);
        in->peak_g = peak;
    }

    return INCIDENT_EVENT_NONE;
}

enum incident_event incident_update(struct incident *in, const struct incident_sample *s,
                                    uint32_t dt_ms)
{
    if ((in == NULL) || (s == NULL)) {
        return INCIDENT_EVENT_NONE;
    }

    in->since_ms += dt_ms;

    /*
     * The crash machine comes first: it can take over from the alarm, and
     * a crash matters more than a bike being moved.
     */
    if (in->crash_on) {
        enum incident_event ev = crash_update(in, s, dt_ms);

        if (ev != INCIDENT_EVENT_NONE) {
            return ev;
        }
        if ((in->state == INCIDENT_SHAKEN) || (in->state == INCIDENT_COUNTING) ||
            (in->state == INCIDENT_CRASHED)) {
            return INCIDENT_EVENT_NONE;
        }
    }

    if (!in->alarm_on) {
        return INCIDENT_EVENT_NONE;
    }

    switch ((enum incident_state)in->state) {
    case INCIDENT_SETTLING:
        if (in->since_ms >= INCIDENT_ARM_SETTLE_MS) {
            go(in, INCIDENT_ARMED);
        }
        break;

    case INCIDENT_ARMED:
        if (isfinite(s->still_g) && (s->still_g >= INCIDENT_ALARM_G)) {
            in->moved_ms += dt_ms;
            if (in->moved_ms >= INCIDENT_ALARM_HOLD_MS) {
                go(in, INCIDENT_RINGING);

                return INCIDENT_EVENT_ALARM;
            }
        } else {
            in->moved_ms = 0U;
        }
        break;

    case INCIDENT_RINGING:
        break;      /* only a key silences it */

    default:
        /* armed but idle: the crash machine put it back here */
        go(in, INCIDENT_SETTLING);
        break;
    }

    return INCIDENT_EVENT_NONE;
}
