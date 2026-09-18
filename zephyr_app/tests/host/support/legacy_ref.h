/**
 * @file legacy_ref.h
 * @brief Reference formulas copied from the legacy stravaV10 firmware.
 *
 * The port must reproduce the behaviour of legacy/ (Vincent Golle's
 * stravaV10, CC BY-NC 4.0). The functions below are transcriptions of the
 * legacy code, kept as the test oracle; each one names its origin.
 */

#ifndef LEGACY_REF_H
#define LEGACY_REF_H

#include <math.h>

/** libraries/utils/utils.h: toRadians() */
static inline float legacy_to_radians(float angle)
{
    return (3.14159265358979323846f * angle / 180.0f);
}

/**
 * libraries/utils/utils.h: distance_between() - equirectangular
 * approximation with the mean Earth radius 6371008 m.
 */
static inline float legacy_distance_between(float lat1, float lon1, float lat2, float lon2)
{
    const float two_r = 2.0f * 6371008.0f;
    const float sdlat = (legacy_to_radians(lat2 - lat1) / 2.0f);
    const float sdlon = (legacy_to_radians(lon2 - lon1) / 2.0f);
    const float q = sdlat * sdlat +
                    0.5f * (1.0f + cosf(legacy_to_radians(lat1 + lat2))) * sdlon * sdlon;

    return two_r * sqrtf(q);
}

#endif /* LEGACY_REF_H */
