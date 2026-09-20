/**
 * @file power_estimate.c
 * @brief Power the rider puts out, estimated as the legacy does
 *
 * Formula and origin in model/power_estimate.h.
 */

#include "model/power_estimate.h"

int16_t power_estimate_w(float weight_kg, float speed_ms, float vit_asc_ms)
{
    float power = POWER_GRAVITY * weight_kg * vit_asc_ms;          /* gravity */

    power += POWER_ROLLING * POWER_GRAVITY * weight_kg * speed_ms; /* ground and mechanics */
    power += POWER_AIR * speed_ms * speed_ms * speed_ms;           /* air */
    power *= POWER_TRANSMISSION;                                   /* transmission */

    if (power > 32767.0f) {
        return INT16_MAX;
    }
    if (power < -32768.0f) {
        return INT16_MIN;
    }

    return (int16_t)power;
}
