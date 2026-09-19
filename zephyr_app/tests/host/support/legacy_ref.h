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
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * legacy/source/vue/Screenutils.cpp: _fmkstr(), with a char buffer in place
 * of the Arduino String. Decimals are truncated digit by digit in float.
 */
static inline void legacy_fmkstr(char *out, size_t size, float value, unsigned int nb_digits)
{
    char res[48] = "";

    if (fabsf(value) > 100000) {
        (void)snprintf(out, size, "---");
        return;
    }
    int ent_val = (int)value;

    if (ent_val == 0 && value < 0.0F) {
        (void)snprintf(res, sizeof(res), "-");
    }
    (void)snprintf(res + strlen(res), sizeof(res) - strlen(res), "%d", ent_val);
    if (nb_digits > 0) {
        (void)snprintf(res + strlen(res), sizeof(res) - strlen(res), ".");
        for (uint16_t i = 0; i < nb_digits; i++) {
            value = fabsf(value - (float)ent_val);
            value *= 10;
            ent_val = (int)value;
            uint32_t dec_val = (uint32_t)value;
            (void)snprintf(res + strlen(res), sizeof(res) - strlen(res), "%lu", (unsigned long)dec_val);
        }
    }
    (void)snprintf(out, size, "%s", res);
}

/** legacy/source/vue/Screenutils.cpp: _secjmkstr() */
static inline void legacy_secjmkstr(char *out, size_t size, uint32_t value, char sep)
{
    if (value >= 86400) {
        (void)snprintf(out, size, " --:--:--");
        return;
    }
    uint8_t hours = (uint8_t)(value / 3600);
    value -= hours * 3600;
    uint8_t minutes = (uint8_t)(value / 60);
    value -= minutes * 60;
    uint8_t seconds = (uint8_t)(value % 60);
    (void)snprintf(out, size, "%02u%c%02u%c%02u", hours, sep, minutes, sep, seconds);
}

/**
 * legacy/source/sensors/fxos.cpp:741-766: mean of the buffered samples, then
 * the mean absolute deviation, both in float over int16 counts.
 */
static inline float legacy_fxos_roughness(const int16_t *buff, unsigned int count)
{
    float mean = 0.0f;
    float rough = 0.0f;

    for (unsigned int i = 0; i < count; i++) {
        mean += (float)buff[i];
    }
    mean /= count;
    for (unsigned int i = 0; i < count; i++) {
        rough += fabsf((float)buff[i] - mean) / count;
    }
    return rough;
}

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
