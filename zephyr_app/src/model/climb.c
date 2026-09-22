/**
 * @file climb.c
 * @brief The climbs of a route, and where the rider is on the one ahead
 *
 * Rules, thresholds and what the legacy does in model/climb.h.
 */

#include <math.h>
#include <string.h>

#include "model/climb.h"

uint8_t climb_cat_of(float gain_m)
{
    if (!isfinite(gain_m)) {
        return (uint8_t)CLIMB_CAT_NONE;
    }
    if (gain_m >= 800.0f) {
        return (uint8_t)CLIMB_CAT_HC;
    }
    if (gain_m >= 640.0f) {
        return (uint8_t)CLIMB_CAT_1;
    }
    if (gain_m >= 320.0f) {
        return (uint8_t)CLIMB_CAT_2;
    }
    if (gain_m >= 160.0f) {
        return (uint8_t)CLIMB_CAT_3;
    }
    if (gain_m >= 80.0f) {
        return (uint8_t)CLIMB_CAT_4;
    }

    return (uint8_t)CLIMB_CAT_NONE;
}

float climb_gain(const struct climb *c)
{
    return (c != NULL) ? (c->top_alt_m - c->bottom_alt_m) : 0.0f;
}

float climb_length(const struct climb *c)
{
    return (c != NULL) ? (c->end_m - c->start_m) : 0.0f;
}

float climb_grade(const struct climb *c)
{
    float len = climb_length(c);

    if (len < 1.0f) {
        return 0.0f;
    }

    return (climb_gain(c) / len) * 100.0f;
}

/** A rise long enough, high enough and steep enough to be called a climb */
static bool is_climb(const struct climb *c)
{
    return (climb_length(c) >= CLIMB_MIN_LEN_M) && (climb_gain(c) >= CLIMB_MIN_GAIN_M) &&
           (climb_grade(c) >= CLIMB_MIN_GRADE_PCT);
}

/** The climb of the list with the least gain */
static uint8_t smallest(const struct climb_list *l)
{
    uint8_t worst = 0U;
    float least = climb_gain(&l->c[0]);

    for (uint8_t i = 1U; i < l->n; i++) {
        float g = climb_gain(&l->c[i]);

        if (g < least) {
            least = g;
            worst = i;
        }
    }

    return worst;
}

/** Drop that one, closing the gap so the route order is kept */
static void drop(struct climb_list *l, uint8_t at)
{
    for (uint8_t i = at; (i + 1U) < l->n; i++) {
        l->c[i] = l->c[i + 1U];
    }
    l->n--;
    l->overflowed = true;
}

/** Take a finished rise into the list, keeping the route order */
static void push(struct climb_list *l, const struct climb *c)
{
    if (!is_climb(c)) {
        return;
    }
    if (l->n >= CLIMB_MAX) {
        /* the route has more climbs than fit: the smallest one goes, and
         * a new one smaller than all of them never gets in */
        uint8_t worst = smallest(l);

        if (climb_gain(c) <= climb_gain(&l->c[worst])) {
            l->overflowed = true;
            return;
        }
        drop(l, worst);
    }

    l->c[l->n] = *c;
    l->c[l->n].cat = climb_cat_of(climb_gain(c));
    l->n++;
}

uint8_t climb_find(struct climb_list *out, uint16_t count, climb_point_fn point_of, void *user)
{
    if ((out == NULL) || (point_of == NULL)) {
        return 0U;
    }

    (void)memset(out, 0, sizeof(*out));
    if (count < 2U) {
        return 0U;
    }

    float dist;
    float alt;

    point_of(0U, &dist, &alt, user);

    /* the rise being followed, open from the last low point */
    struct climb cur = {
        .start_m = dist,
        .end_m = dist,
        .bottom_alt_m = alt,
        .top_alt_m = alt,
        .start_idx = 0U,
        .end_idx = 0U,
    };
    /* the lowest point since the last top, which is where a rise would open */
    float low_m = dist;
    float low_alt = alt;
    uint16_t low_idx = 0U;
    bool rising = false;

    for (uint16_t i = 1U; i < count; i++) {
        point_of(i, &dist, &alt, user);
        if (!isfinite(dist) || !isfinite(alt)) {
            continue;
        }

        if (!rising) {
            if (alt <= low_alt) {
                /*
                 * `<=`, not `<`: on a flat the low point has to travel with
                 * the rider, or a climb after two kilometres of valley
                 * would be said to start where the valley did.
                 */
                low_alt = alt;
                low_m = dist;
                low_idx = i;
            } else if (alt >= (low_alt + CLIMB_HYST_M)) {
                /* a real rise started back at the low point */
                cur.start_m = low_m;
                cur.bottom_alt_m = low_alt;
                cur.start_idx = low_idx;
                cur.end_m = dist;
                cur.top_alt_m = alt;
                cur.end_idx = i;
                rising = true;
            }
            continue;
        }

        if (alt > cur.top_alt_m) {
            cur.top_alt_m = alt;
            cur.end_m = dist;
            cur.end_idx = i;
            continue;
        }
        if (alt > (cur.top_alt_m - CLIMB_HYST_M)) {
            continue;   /* still around the top: not a descent yet */
        }

        /*
         * Going down for real. The rise may still pick up again after a
         * short and shallow dip, which is a false flat in the middle of a
         * pass, not the end of the climb: hold it and see.
         */
        bool resumed = false;
        float dip_low = alt;
        float dip_low_m = dist;

        for (uint16_t j = i + 1U; j < count; j++) {
            float d2;
            float a2;

            point_of(j, &d2, &a2, user);
            if (!isfinite(d2) || !isfinite(a2)) {
                continue;
            }
            if (a2 < dip_low) {
                dip_low = a2;
                dip_low_m = d2;
            }
            if ((d2 - cur.end_m) > CLIMB_MERGE_M) {
                break;      /* the dip is too long to be part of the climb */
            }
            if ((cur.top_alt_m - dip_low) > (climb_gain(&cur) / 3.0f)) {
                break;      /* and too deep */
            }
            if (a2 >= (cur.top_alt_m + CLIMB_HYST_M / 2.0f)) {
                /* back above the old top: the same climb goes on */
                cur.top_alt_m = a2;
                cur.end_m = d2;
                cur.end_idx = j;
                i = j;
                resumed = true;
                break;
            }
        }
        if (resumed) {
            continue;
        }

        push(out, &cur);
        rising = false;
        low_alt = dip_low;
        low_m = dip_low_m;
        low_idx = i;
    }

    /* a route that ends on the way up ends on a climb */
    if (rising) {
        push(out, &cur);
    }

    return out->n;
}

void climb_update(struct climb_state *st, const struct climb_list *l, float dist_m, float alt_m)
{
    if (st == NULL) {
        return;
    }

    (void)memset(st, 0, sizeof(*st));
    if ((l == NULL) || (l->n == 0U) || !isfinite(dist_m)) {
        return;
    }

    for (uint8_t i = 0U; i < l->n; i++) {
        const struct climb *c = &l->c[i];

        if (dist_m >= c->end_m) {
            continue;   /* already over the top of this one */
        }

        if (dist_m >= c->start_m) {
            float len = climb_length(c);

            st->on_climb = true;
            st->index = i;
            st->remain_m = c->end_m - dist_m;
            /*
             * What is left to climb from where the rider actually is. With
             * no altitude, the share of the gain that matches the share of
             * the distance still to ride.
             */
            if (isfinite(alt_m)) {
                st->remain_gain_m = c->top_alt_m - alt_m;
            } else {
                st->remain_gain_m = climb_gain(c) * ((len > 1.0f) ? (st->remain_m / len) : 1.0f);
            }
            if (st->remain_gain_m < 0.0f) {
                st->remain_gain_m = 0.0f;
            }
            st->grade_pct = (st->remain_m >= 1.0f)
                                ? ((st->remain_gain_m / st->remain_m) * 100.0f)
                                : 0.0f;
            st->done_pct = (len > 1.0f) ? (((dist_m - c->start_m) / len) * 100.0f) : 0.0f;
            if ((i + 1U) < l->n) {
                st->have_next = true;
                st->next_index = (uint8_t)(i + 1U);
            }

            return;
        }

        /* the next climb is still ahead */
        st->to_next_m = c->start_m - dist_m;
        st->index = i;
        st->next_index = i;
        st->have_next = true;

        return;
    }
}

float climb_grade_ahead(uint16_t count, climb_point_fn point_of, void *user, float dist_m)
{
    if ((point_of == NULL) || (count < 2U) || !isfinite(dist_m)) {
        return 0.0f;
    }

    float here_alt = NAN;
    float there_alt = NAN;
    float there_m = dist_m;

    for (uint16_t i = 0U; i < count; i++) {
        float d;
        float a;

        point_of(i, &d, &a, user);
        if (!isfinite(d) || !isfinite(a)) {
            continue;
        }
        if (!isfinite(here_alt) && (d >= dist_m)) {
            here_alt = a;
        }
        if (isfinite(here_alt)) {
            there_alt = a;
            there_m = d;
            if (d >= (dist_m + CLIMB_AHEAD_M)) {
                break;
            }
        }
    }

    if (!isfinite(here_alt) || !isfinite(there_alt) || ((there_m - dist_m) < 1.0f)) {
        return 0.0f;
    }

    return ((there_alt - here_alt) / (there_m - dist_m)) * 100.0f;
}
