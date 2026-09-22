/**
 * @file route_profile.h
 * @brief Elevation profile of the route, as the screen draws it
 *
 * A route holds thousands of points; the profile screen has a couple of
 * hundred columns. This turns one into the other: the altitude of each
 * column, the lowest and the highest of the whole route, where the rider
 * is and how much climb is left.
 *
 * Pure C: no Zephyr, no hardware. The caller hands the points one by one,
 * so this works over any storage.
 */

#ifndef MODEL_ROUTE_PROFILE_H
#define MODEL_ROUTE_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Columns of the profile, which is also what the screen draws */
#define ROUTE_PROFILE_MAX   120U

/** The profile of a route */
struct route_profile {
    uint8_t n;                          /**< columns filled */
    uint8_t here;                       /**< column of the rider */
    int16_t alt_m[ROUTE_PROFILE_MAX];   /**< altitude of each column, metres */
    int16_t min_m;                      /**< lowest point of the route */
    int16_t max_m;                      /**< highest point of the route */
    uint16_t climb_left_m;              /**< climb still ahead of the rider */
};

/**
 * @brief Altitude of a point of the route
 *
 * @param index Point, from 0 to count - 1
 * @param user What the caller gave to route_profile_build()
 * @return The altitude in metres
 */
typedef float (*route_profile_alt_fn)(uint16_t index, void *user);

/**
 * @brief Build the profile of a route
 *
 * Walks the points with a step so that at most ROUTE_PROFILE_MAX columns
 * come out, keeping the first and the last. The climb left adds only the
 * rises from the rider onwards, as the legacy counts a climb
 * (`Parcours.cpp`, positive differences only).
 *
 * @param prof Where to write it
 * @param count Points of the route
 * @param here Point the rider is on
 * @param alt_of Altitude of a point
 * @param user Handed back to @p alt_of
 * @return true when there was a route to profile
 */
bool route_profile_build(struct route_profile *prof, uint16_t count, uint16_t here,
                         route_profile_alt_fn alt_of, void *user);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_ROUTE_PROFILE_H */
