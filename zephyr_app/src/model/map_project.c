/**
 * @file map_project.c
 * @brief Positions into the map windows of the interface
 */

#include <math.h>
#include <stddef.h>

#include "model/map_project.h"
#include "model/vecteur.h"

/** Half-spans of the five levels, in metres (see map_span_m()) */
static const uint16_t zoom_spans_m[MAP_ZOOM_LEVELS] = {100U, 250U, 500U, 1000U, 2500U};

/** Round lengths a scale bar may take, in metres */
static const uint16_t bar_steps_m[] = {10U, 25U, 50U, 100U, 250U, 500U, 1000U, 2000U};

uint16_t map_span_m(uint8_t zoom)
{
    if ((zoom < 1U) || (zoom > MAP_ZOOM_LEVELS)) {
        return zoom_spans_m[1]; /* the 250 m of the legacy */
    }

    return zoom_spans_m[zoom - 1U];
}

struct map_point map_project(float lat, float lon, float lat0, float lon0, uint16_t span_m,
                             uint16_t aspect_pm)
{
    struct map_point p = {.x = 500, .y = 500};

    if ((span_m == 0U) || (aspect_pm == 0U)) {
        return p;
    }

    /*
     * Metres to the east and to the north of the rider, with the distance
     * of the legacy (`utils.h:36-48`, equirectangular), and the sign by
     * hand because a distance has none.
     */
    float east = distance_between(lat0, lon0, lat0, lon);
    float north = distance_between(lat0, lon0, lat, lon0);

    if (lon < lon0) {
        east = -east;
    }
    if (lat < lat0) {
        north = -north;
    }

    /*
     * Half the width of the window is span_m, and the screen stretches the
     * per mille of each axis over its own side, so the vertical takes the
     * shape of the window to keep a metre the same length on both.
     */
    float x = 500.0f + ((east / (float)span_m) * 500.0f);
    float y = 500.0f + ((north / (float)span_m) * 500.0f * ((float)aspect_pm / 1000.0f));

    /* well outside the window is still outside: whoever draws it clips */
    if (x > 32000.0f) {
        x = 32000.0f;
    } else if (x < -32000.0f) {
        x = -32000.0f;
    }
    if (y > 32000.0f) {
        y = 32000.0f;
    } else if (y < -32000.0f) {
        y = -32000.0f;
    }

    p.x = (int16_t)lroundf(x);
    p.y = (int16_t)lroundf(y);

    return p;
}

uint16_t map_scale_bar(uint16_t span_m, uint16_t *bar_pm)
{
    uint16_t bar = bar_steps_m[0];

    /* the bar takes at most half of the width, which is span_m itself */
    for (uint8_t i = 0U; i < (sizeof(bar_steps_m) / sizeof(bar_steps_m[0])); i++) {
        if (bar_steps_m[i] <= span_m) {
            bar = bar_steps_m[i];
        }
    }

    if (bar_pm != NULL) {
        *bar_pm = (span_m > 0U) ? (uint16_t)(((uint32_t)bar * 500U) / span_m) : 0U;
    }

    return bar;
}

uint16_t map_stride(uint16_t count, uint16_t max)
{
    if ((max == 0U) || (count <= max)) {
        return 1U;
    }

    uint16_t stride = (uint16_t)((count + max - 1U) / max);

    return (stride == 0U) ? 1U : stride;
}
