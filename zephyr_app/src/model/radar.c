/**
 * @file radar.c
 * @brief Vehicles coming from behind, as a rear radar reports them
 *
 * Rules in model/radar.h. Pure C: the wire formats live with each radio.
 */

#include <string.h>

#include "model/radar.h"

void radar_init(struct radar *r)
{
    if (r != NULL) {
        (void)memset(r, 0, sizeof(*r));
    }
}

uint8_t radar_level_of(uint16_t range_m, uint16_t closing_kmh)
{
    if ((range_m == 0U) || (range_m > RADAR_RANGE_MAX_M)) {
        return (uint8_t)RADAR_LEVEL_NONE;
    }

    /*
     * The ANT+ profile carries a level of its own; a radio that does not
     * gets this, which is the rule a rider would use: close, or fast.
     */
    if ((range_m <= 30U) || (closing_kmh >= 80U)) {
        return (uint8_t)RADAR_LEVEL_DANGER;
    }
    if (closing_kmh >= RADAR_FAST_KMH) {
        return (uint8_t)RADAR_LEVEL_FAST;
    }

    return (uint8_t)RADAR_LEVEL_APPROACHING;
}

/** Sort the list by distance, nearest first; eight targets, so insertion */
static void sort_by_range(struct radar *r)
{
    for (uint8_t i = 1U; i < r->n; i++) {
        struct radar_target key = r->t[i];
        uint8_t j = i;

        while ((j > 0U) && (r->t[j - 1U].range_m > key.range_m)) {
            r->t[j] = r->t[j - 1U];
            j--;
        }
        r->t[j] = key;
    }
}

void radar_feed(struct radar *r, const struct radar_frame *f, uint32_t now_ms)
{
    if ((r == NULL) || (f == NULL)) {
        return;
    }

    r->last_ms = now_ms;

    for (uint8_t i = 0U; (i < f->n) && (i < RADAR_TARGETS_MAX); i++) {
        const struct radar_target *in = &f->t[i];

        if ((in->range_m == 0U) || (in->range_m > RADAR_RANGE_MAX_M)) {
            continue;   /* nothing there, or further than the radar reaches */
        }

        /* the same vehicle keeps its slot, so the screen does not jump */
        struct radar_target *slot = NULL;

        for (uint8_t j = 0U; j < r->n; j++) {
            if (r->t[j].id == in->id) {
                slot = &r->t[j];
                break;
            }
        }
        if ((slot == NULL) && (r->n < RADAR_TARGETS_MAX)) {
            slot = &r->t[r->n];
            r->n++;
        }
        if (slot == NULL) {
            continue;   /* more vehicles than the model holds */
        }

        slot->range_m = in->range_m;
        slot->closing_kmh = in->closing_kmh;
        slot->side = in->side;
        slot->id = in->id;
        slot->seen_ms = now_ms;
        slot->level = (in->level != (uint8_t)RADAR_LEVEL_NONE)
                          ? in->level
                          : radar_level_of(in->range_m, in->closing_kmh);
    }

    radar_tick(r, now_ms);
}

void radar_tick(struct radar *r, uint32_t now_ms)
{
    if (r == NULL) {
        return;
    }

    uint8_t kept = 0U;

    for (uint8_t i = 0U; i < r->n; i++) {
        /*
         * A radar drops a frame now and then, and a mark that blinks is
         * worse than one that lingers: a target only goes after
         * RADAR_HOLD_MS with nothing said about it.
         */
        uint32_t age = now_ms - r->t[i].seen_ms;

        if (age > RADAR_HOLD_MS) {
            continue;
        }
        /* drawn solid while it is being reported, fading after that */
        r->t[i].live = (age <= RADAR_FADE_MS);
        if (kept != i) {
            r->t[kept] = r->t[i];
        }
        kept++;
    }
    r->n = kept;
    sort_by_range(r);
}

void radar_set_link(struct radar *r, bool linked, uint32_t now_ms)
{
    if (r == NULL) {
        return;
    }

    if (!linked && r->linked) {
        /* the radar went away: what it saw is no longer true */
        r->n = 0U;
    }
    r->linked = linked;
    r->last_ms = now_ms;
}

const struct radar_target *radar_nearest(const struct radar *r)
{
    if ((r == NULL) || (r->n == 0U)) {
        return NULL;
    }

    /* the list is kept sorted by distance */
    return &r->t[0];
}

uint8_t radar_count(const struct radar *r)
{
    return (r != NULL) ? r->n : 0U;
}

uint8_t radar_worst(const struct radar *r)
{
    if (r == NULL) {
        return (uint8_t)RADAR_LEVEL_NONE;
    }

    uint8_t worst = (uint8_t)RADAR_LEVEL_NONE;

    for (uint8_t i = 0U; i < r->n; i++) {
        if (r->t[i].level > worst) {
            worst = r->t[i].level;
        }
    }

    return worst;
}
