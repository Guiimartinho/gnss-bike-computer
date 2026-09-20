/**
 * @file map_project.h
 * @brief Positions into the map windows of the interface
 *
 * The screens draw the route and the segments in per mille of their window
 * (`include/ui/ui_model.h`, `ui_pt_t`), with the rider in the middle. This
 * module turns a position into that, keeping the metres of the two axes at
 * the same scale, and picks the scale bar.
 *
 * The zoom follows `legacy/source/display/Zoom.cpp`: a half-span in metres
 * that the rider changes with the side keys, and the vertical span comes
 * from the shape of the window.
 *
 * Pure C: no Zephyr, no hardware.
 */

#ifndef MODEL_MAP_PROJECT_H
#define MODEL_MAP_PROJECT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Levels of the zoom of the port, half-span of the window in metres */
#define MAP_ZOOM_LEVELS     5U

/** A point of a map window, in per mille, as `ui_pt_t` carries it */
struct map_point {
    int16_t x;
    int16_t y;
};

/**
 * @brief Half-span of the window, in metres, for a zoom level
 *
 * The legacy has a hundred levels of `level² × 250 / 100` metres
 * (`Zoom.cpp:63`, with `BASE_ZOOM_LEVEL` 10 giving 250 m); the port keeps
 * five steps of the same idea, and level 2 is the 250 m of the legacy.
 *
 * @param zoom Level, 1 to MAP_ZOOM_LEVELS; anything else gives the default
 */
uint16_t map_span_m(uint8_t zoom);

/**
 * @brief Project a position into the window, with the rider in the middle
 *
 * @param lat Latitude of the point
 * @param lon Longitude of the point
 * @param lat0 Latitude of the rider (middle of the window)
 * @param lon0 Longitude of the rider
 * @param span_m Half-span of the window, in metres (map_span_m())
 * @param aspect_pm Width over height of the window, in per mille, so that a
 *                  metre is the same length on both axes
 * @return The point in per mille, 500 being the middle; a point outside the
 *         window comes back outside, for whoever draws it to clip
 */
struct map_point map_project(float lat, float lon, float lat0, float lon0, uint16_t span_m,
                             uint16_t aspect_pm);

/**
 * @brief Length of the scale bar of a window
 *
 * A round number of metres that takes at most half of the width.
 *
 * @param span_m Half-span of the window, in metres
 * @param bar_pm Where to put the length of the bar in per mille of the width
 * @return The length of the bar in metres
 */
uint16_t map_scale_bar(uint16_t span_m, uint16_t *bar_pm);

/**
 * @brief Step to walk a list of points so that at most @p max of them are used
 *
 * @param count Points of the list
 * @param max Points that fit on the screen
 * @return 1 when everything fits, or the step to take
 */
uint16_t map_stride(uint16_t count, uint16_t max);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_MAP_PROJECT_H */
