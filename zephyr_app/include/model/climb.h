/**
 * @file climb.h
 * @brief The climbs of a route, and where the rider is on the one ahead
 *
 * The legacy shows the whole route and the total climb, and nothing about
 * the climb the rider is actually on (`legacy/source/vue/VuePRC.cpp`). What
 * a rider wants on a mountain road is the opposite: forget the route, tell
 * me how much of **this** climb is left, how steep it is, and what the next
 * few hundred metres look like. Garmin calls it ClimbPro; this is the same
 * idea from the elevation of the loaded route.
 *
 * Two steps, both pure C on the caller's data:
 *
 * 1. `climb_find()` walks the route once, when it loads, and writes the
 *    list of climbs. It is the expensive one and runs once.
 * 2. `climb_update()` runs each epoch and only says which climb the rider
 *    is on and what is left of it.
 *
 * Finding a climb is a matter of taste, so the rules are written down:
 *
 * | Rule | Value | Why |
 * |---|---|---|
 * | A rise opens when the altitude gains | `CLIMB_HYST_M` over a low point | below that it is noise of the file, not a hill |
 * | It closes when the altitude drops | `CLIMB_HYST_M` under a high point | the same, going down |
 * | Two rises merge when the dip between them is short and shallow | `CLIMB_MERGE_M` and a third of the gain | a false flat in the middle of a pass is not two climbs |
 * | A rise counts as a climb from | `CLIMB_MIN_LEN_M`, `CLIMB_MIN_GAIN_M` and `CLIMB_MIN_GRADE_PCT` | a bridge is not a climb |
 * | Category | by the gain, on the thresholds cycling uses | 80 m is a fourth category, 800 m is above category |
 */

#ifndef MODEL_CLIMB_H
#define MODEL_CLIMB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Climbs of a route kept in memory; a route with more loses the smallest */
#define CLIMB_MAX           16U

/** Altitude that has to change before a rise or a fall is real, in metres */
#define CLIMB_HYST_M        10.0f

/** Longest dip that still leaves two rises as one climb, in metres */
#define CLIMB_MERGE_M       1000.0f

/** Shortest thing that counts as a climb, in metres */
#define CLIMB_MIN_LEN_M     500.0f

/** Smallest gain that counts as a climb, in metres */
#define CLIMB_MIN_GAIN_M    30.0f

/** Gentlest average that counts as a climb, in percent */
#define CLIMB_MIN_GRADE_PCT 3.0f

/** Distance the "what comes next" gradient looks ahead, in metres */
#define CLIMB_AHEAD_M       200.0f

/** How hard, on the scale cycling uses; the number is the gain in metres */
enum climb_cat {
    CLIMB_CAT_NONE = 0,     /**< under 80 m: not worth a category */
    CLIMB_CAT_4,            /**< from 80 m */
    CLIMB_CAT_3,            /**< from 160 m */
    CLIMB_CAT_2,            /**< from 320 m */
    CLIMB_CAT_1,            /**< from 640 m */
    CLIMB_CAT_HC            /**< from 800 m: above category */
};

/** One climb of the route */
struct climb {
    float start_m;          /**< where it begins, along the route */
    float end_m;            /**< where the top is */
    float bottom_alt_m;
    float top_alt_m;
    uint16_t start_idx;     /**< point of the route it begins at */
    uint16_t end_idx;
    uint8_t cat;            /**< enum climb_cat */
};

/** The climbs of the route, in the order they are ridden */
struct climb_list {
    struct climb c[CLIMB_MAX];
    uint8_t n;
    bool overflowed;        /**< the route had more climbs than fit */
};

/** Where the rider stands, as the screen shows it */
struct climb_state {
    bool on_climb;          /**< riding one right now */
    uint8_t index;          /**< which one of the list */
    float remain_m;         /**< to the top of it */
    float remain_gain_m;
    float grade_pct;        /**< average of what is left */
    float ahead_grade_pct;  /**< of the next CLIMB_AHEAD_M */
    float done_pct;         /**< of the climb, by distance */
    float to_next_m;        /**< to the foot of the next climb; 0 when on one */
    uint8_t next_index;     /**< the next climb, valid when to_next_m is above 0 */
    bool have_next;
};

/**
 * @brief Distance and altitude of a point of the route
 *
 * @param index Point, from 0 to count - 1
 * @param dist_m Distance from the start of the route, in metres
 * @param alt_m Altitude in metres
 * @param user What the caller gave to climb_find()
 */
typedef void (*climb_point_fn)(uint16_t index, float *dist_m, float *alt_m, void *user);

/**
 * Find the climbs of a route. Call it once, when the route loads.
 *
 * @return how many climbs were found
 */
uint8_t climb_find(struct climb_list *out, uint16_t count, climb_point_fn point_of, void *user);

/**
 * Where the rider is, from the distance already ridden along the route.
 *
 * Cheap enough for every epoch: it walks the list of climbs, never the
 * route. @p alt_m is the altitude of the rider, used for what is left to
 * climb; without it, pass the altitude of the route at that distance.
 */
void climb_update(struct climb_state *st, const struct climb_list *l, float dist_m, float alt_m);

/**
 * The gradient of the next CLIMB_AHEAD_M metres, which needs the route.
 *
 * Kept apart from climb_update() because it is the only part that walks
 * the points again; a caller that does not want it can leave it out.
 */
float climb_grade_ahead(uint16_t count, climb_point_fn point_of, void *user, float dist_m);

/** Total gain of a climb, in metres */
float climb_gain(const struct climb *c);

/** Length of a climb, in metres */
float climb_length(const struct climb *c);

/** Average gradient of a climb, in percent */
float climb_grade(const struct climb *c);

/** The category of a gain, on the thresholds cycling uses */
uint8_t climb_cat_of(float gain_m);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_CLIMB_H */
