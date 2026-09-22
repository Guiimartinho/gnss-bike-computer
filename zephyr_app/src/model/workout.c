/**
 * @file workout.c
 * @brief A structured session: the steps, and what the trainer is told
 *
 * The file format, the flattening of the repeats and what ERG means here
 * are in model/workout.h.
 */

#include <stdlib.h>
#include <string.h>

#include "model/workout.h"

void workout_init(struct workout *w)
{
    if (w != NULL) {
        (void)memset(w, 0, sizeof(*w));
    }
}

/* ==========================================================================
 * Reading the file
 * ========================================================================== */

/** Skip spaces and tabs */
static const char *skip_blank(const char *p)
{
    while ((*p == ' ') || (*p == '\t')) {
        p++;
    }

    return p;
}

/** The next word, and where it ended */
static const char *word_end(const char *p)
{
    while ((*p != '\0') && (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n')) {
        p++;
    }

    return p;
}

/** One unsigned number; `ok` says whether there was one */
static uint32_t take_u32(const char **p, bool *ok)
{
    const char *s = skip_blank(*p);
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 10);

    if ((end == NULL) || (end == s)) {
        *ok = false;

        return 0U;
    }

    *p = end;
    *ok = true;

    return (uint32_t)v;
}

/** Copy what is left of the line as a label, without its ending */
static void take_label(const char *p, char *dst, size_t len)
{
    p = skip_blank(p);

    size_t i = 0U;

    while ((p[i] != '\0') && (p[i] != '\r') && (p[i] != '\n') && (i < (len - 1U))) {
        dst[i] = p[i];
        i++;
    }
    dst[i] = '\0';
}

/** Append one step, or say there is no room */
static bool push_step(struct workout *w, const struct workout_step *st)
{
    if (w->n >= WORKOUT_MAX_STEPS) {
        return false;
    }

    w->step[w->n] = *st;
    w->n++;

    return true;
}

/** `S <dur> <n> <tgt> <lo> <hi> [label]` */
static bool parse_step(struct workout *w, const char *p)
{
    struct workout_step st;

    (void)memset(&st, 0, sizeof(st));

    p = skip_blank(p);
    switch (*p) {
    case 'T':
        st.dur_kind = (uint8_t)WK_DUR_TIME;
        break;
    case 'D':
        st.dur_kind = (uint8_t)WK_DUR_DIST;
        break;
    case 'L':
        st.dur_kind = (uint8_t)WK_DUR_LAP;
        break;
    default:
        return false;
    }
    p++;

    bool ok = true;

    /*
     * Every step line carries a number, so that a line always has the same
     * shape and anything generating one does not have to special-case the
     * lap step. For a lap step it means nothing and is thrown away; for the
     * other two a zero is a mistake, because a step of no length is not a
     * step.
     */
    st.dur = take_u32(&p, &ok);
    if (!ok) {
        return false;
    }
    if (st.dur_kind == (uint8_t)WK_DUR_LAP) {
        st.dur = 0U;
    } else if (st.dur == 0U) {
        return false;
    }

    p = skip_blank(p);
    switch (*p) {
    case 'P':
        st.tgt_kind = (uint8_t)WK_TGT_POWER;
        break;
    case 'H':
        st.tgt_kind = (uint8_t)WK_TGT_HR;
        break;
    case 'C':
        st.tgt_kind = (uint8_t)WK_TGT_CADENCE;
        break;
    case '-':
        st.tgt_kind = (uint8_t)WK_TGT_NONE;
        break;
    default:
        return false;
    }
    p++;

    st.lo = (uint16_t)take_u32(&p, &ok);
    if (!ok) {
        return false;
    }
    st.hi = (uint16_t)take_u32(&p, &ok);
    if (!ok) {
        return false;
    }
    if (st.hi < st.lo) {
        /* a range the wrong way round is a typing mistake, not a range */
        return false;
    }

    take_label(p, st.label, WORKOUT_LABEL_LEN);

    return push_step(w, &st);
}

/**
 * `END`: write the steps of the block out again, one time less than asked
 *
 * The block was written once as it was read, so `REPEAT 4` copies it three
 * more times.
 */
static bool close_repeat(struct workout *w)
{
    if (!w->rep_open) {
        return false;
    }

    uint8_t first = w->rep_first;
    uint8_t count = (uint8_t)(w->n - first);

    w->rep_open = false;

    if (count == 0U) {
        return false;   /* `REPEAT n` and then straight to `END` */
    }

    for (uint8_t r = 1U; r < w->rep_left; r++) {
        for (uint8_t i = 0U; i < count; i++) {
            if (!push_step(w, &w->step[first + i])) {
                return false;
            }
        }
    }

    return true;
}

bool workout_parse_line(struct workout *w, const char *line)
{
    if ((w == NULL) || (line == NULL) || w->bad) {
        return false;
    }

    const char *p = skip_blank(line);

    if ((*p == '\0') || (*p == '#') || (*p == '\r') || (*p == '\n')) {
        return true;    /* nothing on this line, and that is fine */
    }

    const char *end = word_end(p);
    size_t wlen = (size_t)(end - p);
    bool ok;

    if ((wlen == 4U) && (strncmp(p, "NAME", 4U) == 0)) {
        take_label(end, w->name, WORKOUT_NAME_LEN);
        ok = true;
    } else if ((wlen == 1U) && (*p == 'S')) {
        ok = parse_step(w, end);
    } else if ((wlen == 6U) && (strncmp(p, "REPEAT", 6U) == 0)) {
        if (w->rep_open) {
            ok = false;     /* a repeat inside a repeat is not supported */
        } else {
            bool got;
            const char *q = end;
            uint32_t n = take_u32(&q, &got);

            ok = got && (n >= 1U) && (n <= WORKOUT_MAX_STEPS);
            if (ok) {
                w->rep_first = w->n;
                w->rep_left = (uint8_t)n;
                w->rep_open = true;
            }
        }
    } else if ((wlen == 3U) && (strncmp(p, "END", 3U) == 0)) {
        ok = close_repeat(w);
    } else {
        ok = false;
    }

    if (!ok) {
        /*
         * A session half read is worse than none: the rider would pedal a
         * plan that is not the one they wrote. Everything is refused from
         * here on.
         */
        w->bad = true;
        w->loaded = false;
    }

    return ok;
}

bool workout_parse_end(struct workout *w)
{
    if ((w == NULL) || w->bad || w->rep_open || (w->n == 0U)) {
        if (w != NULL) {
            w->loaded = false;
        }

        return false;
    }

    w->loaded = true;
    w->cur = 0U;

    return true;
}

/* ==========================================================================
 * Riding it
 * ========================================================================== */

bool workout_is_loaded(const struct workout *w)
{
    return (w != NULL) && w->loaded;
}

const char *workout_name(const struct workout *w)
{
    return (w != NULL) ? w->name : "";
}

uint8_t workout_steps(const struct workout *w)
{
    return (w != NULL) ? w->n : 0U;
}

const struct workout_step *workout_current(const struct workout *w)
{
    if ((w == NULL) || !w->loaded || !w->running || (w->cur >= w->n)) {
        return NULL;
    }

    return &w->step[w->cur];
}

uint8_t workout_index(const struct workout *w)
{
    return (w != NULL) ? w->cur : 0U;
}

void workout_start(struct workout *w, uint32_t ride_s, float ride_m)
{
    if ((w == NULL) || !w->loaded) {
        return;
    }

    w->cur = 0U;
    w->running = true;
    w->done = false;
    w->lap_asked = false;
    w->step_start_s = ride_s;
    w->step_start_m = ride_m;
}

void workout_stop(struct workout *w)
{
    if (w != NULL) {
        w->running = false;
        w->lap_asked = false;
    }
}

void workout_lap(struct workout *w)
{
    if (w != NULL) {
        w->lap_asked = true;
    }
}

/** Has the step being ridden come to its end? */
static bool step_over(const struct workout *w, uint32_t ride_s, float ride_m)
{
    const struct workout_step *st = &w->step[w->cur];

    switch ((enum wk_duration)st->dur_kind) {
    case WK_DUR_TIME:
        return (ride_s - w->step_start_s) >= st->dur;
    case WK_DUR_DIST:
        return (ride_m - w->step_start_m) >= (float)st->dur;
    case WK_DUR_LAP:
    default:
        return w->lap_asked;
    }
}

enum wk_event workout_update(struct workout *w, uint32_t ride_s, float ride_m)
{
    if ((w == NULL) || !w->loaded || !w->running) {
        return WK_EVENT_NONE;
    }

    if (!step_over(w, ride_s, ride_m)) {
        return WK_EVENT_NONE;
    }

    w->lap_asked = false;
    w->cur++;
    w->step_start_s = ride_s;
    w->step_start_m = ride_m;

    if (w->cur >= w->n) {
        w->cur = w->n;      /* past the end, and it stays there */
        w->running = false;
        w->done = true;

        return WK_EVENT_DONE;
    }

    return WK_EVENT_STEP;
}

uint32_t workout_remaining(const struct workout *w, uint32_t ride_s, float ride_m)
{
    const struct workout_step *st = workout_current(w);

    if (st == NULL) {
        return 0U;
    }

    switch ((enum wk_duration)st->dur_kind) {
    case WK_DUR_TIME: {
        uint32_t gone = ride_s - w->step_start_s;

        return (gone < st->dur) ? (st->dur - gone) : 0U;
    }
    case WK_DUR_DIST: {
        float gone = ride_m - w->step_start_m;

        if (gone >= (float)st->dur) {
            return 0U;
        }

        return (uint32_t)((float)st->dur - gone);
    }
    case WK_DUR_LAP:
    default:
        return 0U;
    }
}

uint16_t workout_target_power(const struct workout *w)
{
    const struct workout_step *st = workout_current(w);

    if ((st == NULL) || (st->tgt_kind != (uint8_t)WK_TGT_POWER)) {
        return 0U;
    }

    /* the middle of the range: what ERG holds */
    return (uint16_t)(((uint32_t)st->lo + st->hi) / 2U);
}

enum wk_zone workout_zone(const struct workout *w, uint16_t value)
{
    const struct workout_step *st = workout_current(w);

    if ((st == NULL) || (st->tgt_kind == (uint8_t)WK_TGT_NONE)) {
        return WK_ZONE_IN;
    }

    if (value < st->lo) {
        return WK_ZONE_UNDER;
    }
    if (value > st->hi) {
        return WK_ZONE_OVER;
    }

    return WK_ZONE_IN;
}
