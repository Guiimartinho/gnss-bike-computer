/**
 * @file route_profile.c
 * @brief Elevation profile of the route, as the screen draws it
 */

#include <math.h>
#include <string.h>

#include "model/map_project.h"
#include "model/route_profile.h"

bool route_profile_build(struct route_profile *prof, uint16_t count, uint16_t here,
                         route_profile_alt_fn alt_of, void *user)
{
    if ((prof == NULL) || (alt_of == NULL)) {
        return false;
    }

    (void)memset(prof, 0, sizeof(*prof));

    if (count == 0U) {
        return false;
    }

    uint16_t stride = map_stride(count, ROUTE_PROFILE_MAX);
    float lowest = alt_of(0U, user);
    float highest = lowest;
    float climb = 0.0f;
    float previous = 0.0f;
    bool have_previous = false;

    for (uint16_t i = 0U; i < count; i += stride) {
        float alt = alt_of(i, user);

        if (alt < lowest) {
            lowest = alt;
        }
        if (alt > highest) {
            highest = alt;
        }

        if (prof->n < ROUTE_PROFILE_MAX) {
            prof->alt_m[prof->n] = (int16_t)lroundf(alt);
            if (i <= here) {
                prof->here = prof->n;
            }
            prof->n++;
        }

        /* only the rises ahead of the rider count, as the legacy climbs */
        if (i >= here) {
            if (have_previous && (alt > previous)) {
                climb += alt - previous;
            }
            previous = alt;
            have_previous = true;
        }
    }

    /* the end of the route is the end of the profile, whatever the step */
    if ((count > 1U) && (prof->n > 0U)) {
        float last = alt_of((uint16_t)(count - 1U), user);

        if (prof->n < ROUTE_PROFILE_MAX) {
            prof->alt_m[prof->n] = (int16_t)lroundf(last);
            prof->n++;
        } else {
            prof->alt_m[prof->n - 1U] = (int16_t)lroundf(last);
        }
        if (last < lowest) {
            lowest = last;
        }
        if (last > highest) {
            highest = last;
        }
        if (have_previous && (last > previous)) {
            climb += last - previous;
        }
    }

    prof->min_m = (int16_t)lroundf(lowest);
    prof->max_m = (int16_t)lroundf(highest);
    prof->climb_left_m = (climb > 65535.0f) ? 65535U : (uint16_t)lroundf(climb);

    if (prof->here >= prof->n) {
        prof->here = (uint8_t)(prof->n - 1U);
    }

    return true;
}
