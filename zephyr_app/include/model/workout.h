/**
 * @file workout.h
 * @brief A structured session: the steps, and what the trainer is told
 *
 * "Ten minutes easy, then four times five minutes at 280 W with three
 * minutes between, then ten minutes easy." Every other head unit can be
 * given that and will count it down; this port could not, and the
 * `ble_fec_client_set_target_power()` it already had was called by nobody,
 * so the trainer was watched and never driven.
 *
 * Two things live here and are tested separately:
 *
 * 1. the **file**, read line by line and flattened into steps, repeats and
 *    all, so that nothing walks a tree while the rider is pedalling;
 * 2. the **engine**, which given the clock and the distance says which
 *    step is running, how much of it is left, whether the rider is inside
 *    the target and what watts to ask the trainer for.
 *
 * ## The file
 *
 * Plain text, one instruction a line, in the shape of the route files of
 * this project (`model/route_file.h`): easy to write by hand, easy to
 * generate, and no parser worth the name.
 *
 * ```
 * NAME Sweet spot 2x20
 * S T 600 P 120 150 Aquecimento
 * REPEAT 2
 * S T 1200 P 250 270 Bloco
 * S T 300 P 120 150 Solto
 * END
 * S T 600 P 120 150 Volta a calma
 * ```
 *
 * | Word | Meaning |
 * |---|---|
 * | `NAME <text>` | what the session is called |
 * | `S <dur> <n> <tgt> <lo> <hi> [label]` | one step |
 * | `REPEAT <n>` | the steps up to `END` run `n` times in all |
 * | `END` | closes the repeat |
 * | `#` or an empty line | ignored |
 *
 * `<dur>` is `T` for seconds, `D` for metres or `L` for "until the rider
 * presses the lap key". `<tgt>` is `P` watts, `H` beats, `C` turns a
 * minute or `-` for no target, and then `<lo>` and `<hi>` bound it; with
 * `-` both are written as 0.
 *
 * ## Repeats are flattened
 *
 * `REPEAT 4` over two steps becomes eight steps in the array. It costs
 * memory and buys a countdown that never has to ask where it is, and a
 * "step 6 of 11" that means what it says. A session that does not fit in
 * `WORKOUT_MAX_STEPS` is refused whole, with nothing half loaded.
 *
 * ## ERG
 *
 * On a power step the trainer is asked for the middle of the range, which
 * is what ERG means: the trainer holds those watts whatever the rider does
 * with the gears. Off a power step, and outside the trainer modes, nothing
 * is asked and the trainer keeps whatever it had.
 */

#ifndef MODEL_WORKOUT_H
#define MODEL_WORKOUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Steps after the repeats are flattened */
#define WORKOUT_MAX_STEPS   48U

/** Letters of a step label, the last one being the terminator */
#define WORKOUT_LABEL_LEN   16U

/** Letters of the session name */
#define WORKOUT_NAME_LEN    24U

/** How a step ends */
enum wk_duration {
    WK_DUR_TIME = 0,    /**< after so many seconds */
    WK_DUR_DIST,        /**< after so many metres */
    WK_DUR_LAP          /**< when the rider presses the lap key */
};

/** What the rider is asked to hold */
enum wk_target {
    WK_TGT_NONE = 0,
    WK_TGT_POWER,       /**< watts */
    WK_TGT_HR,          /**< beats a minute */
    WK_TGT_CADENCE      /**< turns a minute */
};

/** Where the rider is against the target */
enum wk_zone {
    WK_ZONE_UNDER = -1,
    WK_ZONE_IN = 0,
    WK_ZONE_OVER = 1
};

/** What happened in this epoch */
enum wk_event {
    WK_EVENT_NONE = 0,
    WK_EVENT_STEP,      /**< a new step started */
    WK_EVENT_DONE       /**< the last step ended */
};

/** One step of the session */
struct workout_step {
    uint32_t dur;                   /**< seconds or metres; unused for LAP */
    uint16_t lo;
    uint16_t hi;
    uint8_t dur_kind;               /**< enum wk_duration */
    uint8_t tgt_kind;               /**< enum wk_target */
    char label[WORKOUT_LABEL_LEN];
};

/** A session, loaded and then ridden */
struct workout {
    struct workout_step step[WORKOUT_MAX_STEPS];
    char name[WORKOUT_NAME_LEN];
    uint8_t n;                      /**< steps loaded */
    uint8_t cur;                    /**< step being ridden */
    bool loaded;
    bool running;
    bool done;
    bool lap_asked;                 /**< the rider pressed the key */

    uint32_t step_start_s;          /**< ride second the step began */
    float step_start_m;             /**< ride metre the step began */

    /* while the file is being read */
    uint8_t rep_first;              /**< first step of the open repeat */
    uint8_t rep_left;               /**< repeats still to write out */
    bool rep_open;
    bool bad;                       /**< a line did not parse: refuse it all */
};

/** Empty, with no session in it */
void workout_init(struct workout *w);

/**
 * @brief Read one line of a session file
 * @return false when the line is wrong or the session no longer fits; from
 * then on the session is refused whole and `workout_is_loaded()` stays false
 */
bool workout_parse_line(struct workout *w, const char *line);

/** The file ended: close the session and check it holds at least one step */
bool workout_parse_end(struct workout *w);

/** Whether a whole session is in memory */
bool workout_is_loaded(const struct workout *w);

/** Its name, or an empty string */
const char *workout_name(const struct workout *w);

/** Steps in it */
uint8_t workout_steps(const struct workout *w);

/** The step being ridden, or NULL */
const struct workout_step *workout_current(const struct workout *w);

/** Index of the step being ridden, counting from 0 */
uint8_t workout_index(const struct workout *w);

/**
 * @brief Begin the session
 * @param ride_s seconds of the ride so far
 * @param ride_m metres of the ride so far
 */
void workout_start(struct workout *w, uint32_t ride_s, float ride_m);

/** Stop it where it is; a second start begins from the top */
void workout_stop(struct workout *w);

/** The rider pressed the lap key: a LAP step ends on the next update */
void workout_lap(struct workout *w);

/**
 * @brief One epoch
 * @param ride_s seconds of the ride so far
 * @param ride_m metres of the ride so far
 */
enum wk_event workout_update(struct workout *w, uint32_t ride_s, float ride_m);

/**
 * @brief What is left of the step
 * @return seconds for a time step, metres for a distance step, 0 for a lap
 * step and for a session that is not running
 */
uint32_t workout_remaining(const struct workout *w, uint32_t ride_s, float ride_m);

/** Watts to ask the trainer for, or 0 when this step does not set power */
uint16_t workout_target_power(const struct workout *w);

/** Where a value sits against the target of the step */
enum wk_zone workout_zone(const struct workout *w, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_WORKOUT_H */
