/**
 * @file loc_arbiter.c
 * @brief Which of the position sources the model listens to
 *
 * The rule, line by line from the legacy, in model/loc_arbiter.h.
 */

#include <string.h>

#include "model/loc_arbiter.h"

void loc_arbiter_init(struct loc_arbiter *a)
{
    if (a != NULL) {
        (void)memset(a, 0, sizeof(*a));
    }
}

void loc_arbiter_feed(struct loc_arbiter *a, enum loc_arb_src src, uint32_t now_ms)
{
    if ((a == NULL) || (src == LOC_ARB_NONE) || (src > LOC_ARB_SIM)) {
        return;
    }

    a->seen_ms[src] = now_ms;
    a->fresh[src] = true;
    a->started[src] = true;
}

uint32_t loc_arbiter_age(const struct loc_arbiter *a, enum loc_arb_src src, uint32_t now_ms)
{
    if ((a == NULL) || (src == LOC_ARB_NONE) || (src > LOC_ARB_SIM) || !a->started[src]) {
        return UINT32_MAX;
    }

    return now_ms - a->seen_ms[src];
}

/** Take the "arrived and not yet used" mark of one source */
static bool take(struct loc_arbiter *a, enum loc_arb_src src)
{
    bool was = a->fresh[src];

    a->fresh[src] = false;

    return was;
}

enum loc_arb_src loc_arbiter_pick(struct loc_arbiter *a, uint32_t now_ms, bool gps_has_fix)
{
    if (a == NULL) {
        return LOC_ARB_NONE;
    }

    /* the simulated ride wins, and holds the floor between its frames */
    if (take(a, LOC_ARB_SIM)) {
        return LOC_ARB_SIM;
    }
    if (loc_arbiter_age(a, LOC_ARB_SIM, now_ms) < LOC_ARB_SIM_BLOCK_MS) {
        /*
         * Recent but not new: nothing at all this epoch. Falling through
         * to the receiver here would make the bike jump between the
         * simulated track and where it really is.
         */
        (void)take(a, LOC_ARB_GPS);
        (void)take(a, LOC_ARB_LNS);

        return LOC_ARB_NONE;
    }

    if (take(a, LOC_ARB_GPS)) {
        return LOC_ARB_GPS;
    }
    if (loc_arbiter_age(a, LOC_ARB_GPS, now_ms) < LOC_ARB_GPS_BLOCK_MS) {
        (void)take(a, LOC_ARB_LNS);

        return LOC_ARB_NONE;
    }

    /* the phone, and only while the receiver has no fix of its own */
    if (take(a, LOC_ARB_LNS)) {
        return gps_has_fix ? LOC_ARB_NONE : LOC_ARB_LNS;
    }

    return LOC_ARB_NONE;
}
