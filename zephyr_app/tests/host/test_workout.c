/**
 * @file test_workout.c
 * @brief A structured session, read and ridden (src/model/workout.c)
 *
 * Two halves, tested separately because they fail differently.
 *
 * The **file** has to refuse a session it cannot hold or cannot understand,
 * and refuse it *whole*: a rider given half the plan they wrote would ride
 * the wrong session without knowing. So every bad line leaves nothing
 * loaded, and there are cases for each way a line can be wrong.
 *
 * The **engine** has to count a step down, move to the next one at the
 * right moment, and end. The cases that matter are the ones a session
 * actually runs into: a repeat block that has to come out flattened in the
 * right order, a step that ends on distance while the ride is also
 * spending time, a step that waits for the rider's thumb, and the last
 * step ending exactly once.
 *
 * There is no legacy for any of this.
 */

#include <string.h>

#include "unity.h"

#include "model/workout.h"

static struct workout w;

void setUp(void)
{
    workout_init(&w);
}

void tearDown(void) {}

/** Feed a whole file, line by line, and close it */
static bool load(const char *const *lines, unsigned int n)
{
    for (unsigned int i = 0U; i < n; i++) {
        if (!workout_parse_line(&w, lines[i])) {
            return workout_parse_end(&w);   /* which will also be false */
        }
    }

    return workout_parse_end(&w);
}

#define LOAD(arr)   load((arr), (unsigned int)(sizeof(arr) / sizeof((arr)[0])))

/* ==========================================================================
 * Reading the file
 * ========================================================================== */

static void test_the_simplest_session_there_is(void)
{
    static const char *const f[] = {
        "NAME Uma hora firme",
        "S T 3600 P 200 220 Firme",
    };

    TEST_ASSERT_TRUE(LOAD(f));
    TEST_ASSERT_TRUE(workout_is_loaded(&w));
    TEST_ASSERT_EQUAL_STRING("Uma hora firme", workout_name(&w));
    TEST_ASSERT_EQUAL_UINT8(1U, workout_steps(&w));

    workout_start(&w, 0U, 0.0f);

    const struct workout_step *st = workout_current(&w);

    TEST_ASSERT_NOT_NULL(st);
    TEST_ASSERT_EQUAL_UINT8(WK_DUR_TIME, st->dur_kind);
    TEST_ASSERT_EQUAL_UINT32(3600U, st->dur);
    TEST_ASSERT_EQUAL_UINT8(WK_TGT_POWER, st->tgt_kind);
    TEST_ASSERT_EQUAL_UINT16(200U, st->lo);
    TEST_ASSERT_EQUAL_UINT16(220U, st->hi);
    TEST_ASSERT_EQUAL_STRING("Firme", st->label);
}

static void test_blank_lines_and_comments_are_ignored(void)
{
    static const char *const f[] = {
        "# a session",
        "",
        "   ",
        "NAME Teste",
        "# the only step",
        "S T 600 - 0 0",
    };

    TEST_ASSERT_TRUE(LOAD(f));
    TEST_ASSERT_EQUAL_UINT8(1U, workout_steps(&w));
}

static void test_every_kind_of_duration_and_target(void)
{
    static const char *const f[] = {
        "S T 600 P 100 150 Tempo",
        "S D 5000 H 140 155 Distancia",
        "S L 0 C 85 95 Ate a tecla",
        "S T 300 - 0 0 Solto",
    };

    TEST_ASSERT_TRUE(LOAD(f));
    TEST_ASSERT_EQUAL_UINT8(4U, workout_steps(&w));

    workout_start(&w, 0U, 0.0f);
    TEST_ASSERT_EQUAL_UINT8(WK_DUR_TIME, w.step[0].dur_kind);
    TEST_ASSERT_EQUAL_UINT8(WK_TGT_POWER, w.step[0].tgt_kind);
    TEST_ASSERT_EQUAL_UINT8(WK_DUR_DIST, w.step[1].dur_kind);
    TEST_ASSERT_EQUAL_UINT8(WK_TGT_HR, w.step[1].tgt_kind);
    TEST_ASSERT_EQUAL_UINT8(WK_DUR_LAP, w.step[2].dur_kind);
    TEST_ASSERT_EQUAL_UINT8(WK_TGT_CADENCE, w.step[2].tgt_kind);
    TEST_ASSERT_EQUAL_UINT8(WK_TGT_NONE, w.step[3].tgt_kind);
}

static void test_a_repeat_comes_out_flattened_in_order(void)
{
    /*
     * The one that has to be right: four times "five on, three off" is
     * eight steps, alternating, and the countdown never has to ask where
     * in a tree it is.
     */
    static const char *const f[] = {
        "NAME 4x5",
        "S T 600 P 120 150 Aquecimento",
        "REPEAT 4",
        "S T 300 P 270 290 Bloco",
        "S T 180 P 120 150 Solto",
        "END",
        "S T 600 P 120 150 Volta",
    };

    TEST_ASSERT_TRUE(LOAD(f));
    TEST_ASSERT_EQUAL_UINT8(10U, workout_steps(&w));    /* 1 + 4x2 + 1 */

    TEST_ASSERT_EQUAL_STRING("Aquecimento", w.step[0].label);
    for (unsigned int r = 0U; r < 4U; r++) {
        TEST_ASSERT_EQUAL_STRING("Bloco", w.step[1U + (r * 2U)].label);
        TEST_ASSERT_EQUAL_UINT32(300U, w.step[1U + (r * 2U)].dur);
        TEST_ASSERT_EQUAL_STRING("Solto", w.step[2U + (r * 2U)].label);
        TEST_ASSERT_EQUAL_UINT32(180U, w.step[2U + (r * 2U)].dur);
    }
    TEST_ASSERT_EQUAL_STRING("Volta", w.step[9].label);
}

static void test_a_repeat_of_one_is_the_block_itself(void)
{
    static const char *const f[] = {
        "REPEAT 1",
        "S T 300 P 200 220 Bloco",
        "END",
    };

    TEST_ASSERT_TRUE(LOAD(f));
    TEST_ASSERT_EQUAL_UINT8(1U, workout_steps(&w));
}

static void test_two_repeats_one_after_the_other(void)
{
    static const char *const f[] = {
        "REPEAT 2",
        "S T 60 P 300 320 A",
        "END",
        "REPEAT 3",
        "S T 30 P 100 120 B",
        "END",
    };

    TEST_ASSERT_TRUE(LOAD(f));
    TEST_ASSERT_EQUAL_UINT8(5U, workout_steps(&w));
    TEST_ASSERT_EQUAL_STRING("A", w.step[1].label);
    TEST_ASSERT_EQUAL_STRING("B", w.step[2].label);
    TEST_ASSERT_EQUAL_STRING("B", w.step[4].label);
}

/* ==========================================================================
 * Files that are refused
 * ========================================================================== */

static void test_a_line_that_is_not_understood_refuses_the_whole_session(void)
{
    static const char *const f[] = {
        "S T 600 P 120 150 Aquecimento",
        "TRAINING harder",
        "S T 600 P 120 150 Volta",
    };

    TEST_ASSERT_FALSE(LOAD(f));
    TEST_ASSERT_FALSE(workout_is_loaded(&w));
}

static void test_the_ways_a_step_can_be_wrong(void)
{
    static const char *const bad[] = {
        "S X 600 P 100 120 duracao desconhecida",
        "S T P 100 120 sem numero",
        "S T 0 P 100 120 duracao zero",
        "S T 600 X 100 120 alvo desconhecido",
        "S T 600 P 100 nao ha limite de cima",
        "S T 600 P 150 100 faixa invertida",
        "S",
    };

    for (unsigned int i = 0U; i < (sizeof(bad) / sizeof(bad[0])); i++) {
        workout_init(&w);
        TEST_ASSERT_FALSE_MESSAGE(workout_parse_line(&w, bad[i]), bad[i]);
        TEST_ASSERT_FALSE(workout_parse_end(&w));
    }
}

static void test_a_repeat_that_is_never_closed(void)
{
    static const char *const f[] = {
        "REPEAT 3",
        "S T 300 P 200 220 Bloco",
    };

    TEST_ASSERT_FALSE(LOAD(f));
    TEST_ASSERT_FALSE(workout_is_loaded(&w));
}

static void test_an_end_without_a_repeat(void)
{
    workout_init(&w);
    TEST_ASSERT_FALSE(workout_parse_line(&w, "END"));
}

static void test_a_repeat_with_nothing_in_it(void)
{
    static const char *const f[] = {"REPEAT 3", "END"};

    TEST_ASSERT_FALSE(LOAD(f));
}

static void test_a_repeat_inside_a_repeat_is_refused(void)
{
    static const char *const f[] = {
        "REPEAT 2",
        "S T 60 P 300 320 A",
        "REPEAT 2",
    };

    TEST_ASSERT_FALSE(LOAD(f));
}

static void test_a_session_with_no_steps(void)
{
    static const char *const f[] = {"NAME So um nome"};

    TEST_ASSERT_FALSE(LOAD(f));
}

static void test_a_session_too_big_to_hold_is_refused_whole(void)
{
    /*
     * Thirty repeats of two steps is sixty, and the array holds
     * forty-eight. The count itself is allowed — it is the flattening at
     * `END` that runs out of room — and nothing must be left half loaded.
     */
    static const char *const f[] = {
        "REPEAT 30",
        "S T 60 P 300 320 A",
        "S T 60 P 100 120 B",
        "END",
    };

    TEST_ASSERT_FALSE(LOAD(f));
    TEST_ASSERT_FALSE(workout_is_loaded(&w));

    /* and a repeat count past what could ever fit is refused on its line */
    static const char *const huge[] = {"REPEAT 500", "S T 60 P 300 320 A", "END"};

    workout_init(&w);
    TEST_ASSERT_FALSE(LOAD(huge));
}

static void test_a_session_that_fits_exactly(void)
{
    static const char *const f[] = {
        "REPEAT 24",
        "S T 60 P 300 320 A",
        "S T 60 P 100 120 B",
        "END",
    };

    TEST_ASSERT_TRUE(LOAD(f));
    TEST_ASSERT_EQUAL_UINT8(WORKOUT_MAX_STEPS, workout_steps(&w));
}

/* ==========================================================================
 * Riding it
 * ========================================================================== */

/** Three steps of ten, twenty and thirty seconds */
static void load_three(void)
{
    static const char *const f[] = {
        "S T 10 P 100 120 Um",
        "S T 20 P 200 220 Dois",
        "S T 30 P 300 320 Tres",
    };

    workout_init(&w);
    TEST_ASSERT_TRUE(LOAD(f));
}

static void test_a_session_that_has_not_started_does_nothing(void)
{
    load_three();

    TEST_ASSERT_NULL(workout_current(&w));
    TEST_ASSERT_EQUAL_INT(WK_EVENT_NONE, workout_update(&w, 100U, 0.0f));
    TEST_ASSERT_EQUAL_UINT16(0U, workout_target_power(&w));
    TEST_ASSERT_EQUAL_UINT32(0U, workout_remaining(&w, 100U, 0.0f));
}

static void test_the_steps_come_one_after_the_other(void)
{
    load_three();
    workout_start(&w, 1000U, 0.0f);

    TEST_ASSERT_EQUAL_UINT8(0U, workout_index(&w));
    TEST_ASSERT_EQUAL_UINT32(10U, workout_remaining(&w, 1000U, 0.0f));

    TEST_ASSERT_EQUAL_INT(WK_EVENT_NONE, workout_update(&w, 1009U, 0.0f));
    TEST_ASSERT_EQUAL_UINT32(1U, workout_remaining(&w, 1009U, 0.0f));

    TEST_ASSERT_EQUAL_INT(WK_EVENT_STEP, workout_update(&w, 1010U, 0.0f));
    TEST_ASSERT_EQUAL_UINT8(1U, workout_index(&w));
    TEST_ASSERT_EQUAL_UINT32(20U, workout_remaining(&w, 1010U, 0.0f));

    TEST_ASSERT_EQUAL_INT(WK_EVENT_STEP, workout_update(&w, 1030U, 0.0f));
    TEST_ASSERT_EQUAL_UINT8(2U, workout_index(&w));

    TEST_ASSERT_EQUAL_INT(WK_EVENT_NONE, workout_update(&w, 1059U, 0.0f));
    TEST_ASSERT_EQUAL_INT(WK_EVENT_DONE, workout_update(&w, 1060U, 0.0f));
}

static void test_the_end_happens_once(void)
{
    load_three();
    workout_start(&w, 0U, 0.0f);

    (void)workout_update(&w, 10U, 0.0f);
    (void)workout_update(&w, 30U, 0.0f);
    TEST_ASSERT_EQUAL_INT(WK_EVENT_DONE, workout_update(&w, 60U, 0.0f));

    /* and then nothing, however long the rider goes on */
    for (uint32_t t = 61U; t < 200U; t++) {
        TEST_ASSERT_EQUAL_INT(WK_EVENT_NONE, workout_update(&w, t, 0.0f));
    }
    TEST_ASSERT_NULL(workout_current(&w));
    TEST_ASSERT_TRUE(w.done);
}

static void test_a_long_epoch_that_covers_a_whole_step(void)
{
    /*
     * The receiver went quiet and the ride jumped thirty seconds, past the
     * first step entirely. One step per update is the rule: the session
     * catches up on the following epochs rather than skipping three at
     * once, so the rider sees each step named.
     */
    load_three();
    workout_start(&w, 0U, 0.0f);

    TEST_ASSERT_EQUAL_INT(WK_EVENT_STEP, workout_update(&w, 30U, 0.0f));
    TEST_ASSERT_EQUAL_UINT8(1U, workout_index(&w));

    TEST_ASSERT_EQUAL_INT(WK_EVENT_STEP, workout_update(&w, 50U, 0.0f));
    TEST_ASSERT_EQUAL_UINT8(2U, workout_index(&w));
}

static void test_a_step_that_ends_on_distance(void)
{
    static const char *const f[] = {
        "S D 1000 P 200 220 Um quilometro",
        "S T 60 P 100 120 Solto",
    };

    workout_init(&w);
    TEST_ASSERT_TRUE(LOAD(f));
    workout_start(&w, 0U, 5000.0f);

    TEST_ASSERT_EQUAL_UINT32(1000U, workout_remaining(&w, 0U, 5000.0f));

    /* time passing does not end a distance step */
    TEST_ASSERT_EQUAL_INT(WK_EVENT_NONE, workout_update(&w, 3600U, 5500.0f));
    TEST_ASSERT_EQUAL_UINT32(500U, workout_remaining(&w, 3600U, 5500.0f));

    TEST_ASSERT_EQUAL_INT(WK_EVENT_STEP, workout_update(&w, 3700U, 6000.0f));
    TEST_ASSERT_EQUAL_UINT8(1U, workout_index(&w));
}

static void test_a_step_that_waits_for_the_rider(void)
{
    static const char *const f[] = {
        "S L 0 - 0 0 Ate a tecla",
        "S T 60 P 100 120 Depois",
    };

    workout_init(&w);
    TEST_ASSERT_TRUE(LOAD(f));
    workout_start(&w, 0U, 0.0f);

    /* neither time nor distance ends it */
    TEST_ASSERT_EQUAL_INT(WK_EVENT_NONE, workout_update(&w, 100000U, 99999.0f));
    TEST_ASSERT_EQUAL_UINT32(0U, workout_remaining(&w, 100000U, 0.0f));

    workout_lap(&w);
    TEST_ASSERT_EQUAL_INT(WK_EVENT_STEP, workout_update(&w, 100001U, 99999.0f));
    TEST_ASSERT_EQUAL_UINT8(1U, workout_index(&w));

    /* and the key does not carry over into the next step */
    TEST_ASSERT_EQUAL_INT(WK_EVENT_NONE, workout_update(&w, 100002U, 99999.0f));
}

static void test_one_press_does_not_end_two_steps_that_both_wait(void)
{
    /*
     * Two "until the key" steps in a row is how a rider walks through a
     * warm-up at their own pace. One press must move one step: a press
     * that stayed set would run the whole session away in one epoch.
     */
    static const char *const f[] = {
        "S L 0 - 0 0 Primeiro",
        "S L 0 - 0 0 Segundo",
        "S T 60 P 100 120 Fim",
    };

    workout_init(&w);
    TEST_ASSERT_TRUE(LOAD(f));
    workout_start(&w, 0U, 0.0f);

    workout_lap(&w);
    TEST_ASSERT_EQUAL_INT(WK_EVENT_STEP, workout_update(&w, 10U, 0.0f));
    TEST_ASSERT_EQUAL_UINT8(1U, workout_index(&w));

    /* still on the second one, however long the rider waits */
    TEST_ASSERT_EQUAL_INT(WK_EVENT_NONE, workout_update(&w, 20U, 0.0f));
    TEST_ASSERT_EQUAL_INT(WK_EVENT_NONE, workout_update(&w, 30U, 0.0f));
    TEST_ASSERT_EQUAL_UINT8(1U, workout_index(&w));

    workout_lap(&w);
    TEST_ASSERT_EQUAL_INT(WK_EVENT_STEP, workout_update(&w, 40U, 0.0f));
    TEST_ASSERT_EQUAL_UINT8(2U, workout_index(&w));
}

static void test_stopping_and_starting_again_begins_from_the_top(void)
{
    load_three();
    workout_start(&w, 0U, 0.0f);
    (void)workout_update(&w, 10U, 0.0f);
    TEST_ASSERT_EQUAL_UINT8(1U, workout_index(&w));

    workout_stop(&w);
    TEST_ASSERT_NULL(workout_current(&w));

    workout_start(&w, 500U, 0.0f);
    TEST_ASSERT_EQUAL_UINT8(0U, workout_index(&w));
    TEST_ASSERT_EQUAL_UINT32(10U, workout_remaining(&w, 500U, 0.0f));
}

/* ==========================================================================
 * What the trainer is told, and where the rider is
 * ========================================================================== */

static void test_the_trainer_is_asked_for_the_middle_of_the_range(void)
{
    load_three();
    workout_start(&w, 0U, 0.0f);

    TEST_ASSERT_EQUAL_UINT16(110U, workout_target_power(&w));   /* 100..120 */

    (void)workout_update(&w, 10U, 0.0f);
    TEST_ASSERT_EQUAL_UINT16(210U, workout_target_power(&w));   /* 200..220 */

    (void)workout_update(&w, 30U, 0.0f);
    TEST_ASSERT_EQUAL_UINT16(310U, workout_target_power(&w));   /* 300..320 */

    (void)workout_update(&w, 60U, 0.0f);
    TEST_ASSERT_EQUAL_UINT16(0U, workout_target_power(&w));     /* it ended */
}

static void test_a_step_with_another_target_asks_the_trainer_for_nothing(void)
{
    static const char *const f[] = {
        "S T 600 H 140 155 Por batimento",
        "S T 600 - 0 0 Solto",
    };

    workout_init(&w);
    TEST_ASSERT_TRUE(LOAD(f));
    workout_start(&w, 0U, 0.0f);

    TEST_ASSERT_EQUAL_UINT16(0U, workout_target_power(&w));
    (void)workout_update(&w, 600U, 0.0f);
    TEST_ASSERT_EQUAL_UINT16(0U, workout_target_power(&w));
}

static void test_where_the_rider_sits_against_the_target(void)
{
    load_three();
    workout_start(&w, 0U, 0.0f);

    TEST_ASSERT_EQUAL_INT(WK_ZONE_UNDER, workout_zone(&w, 99U));
    TEST_ASSERT_EQUAL_INT(WK_ZONE_IN, workout_zone(&w, 100U));
    TEST_ASSERT_EQUAL_INT(WK_ZONE_IN, workout_zone(&w, 110U));
    TEST_ASSERT_EQUAL_INT(WK_ZONE_IN, workout_zone(&w, 120U));
    TEST_ASSERT_EQUAL_INT(WK_ZONE_OVER, workout_zone(&w, 121U));
}

static void test_a_step_with_no_target_is_never_wrong(void)
{
    static const char *const f[] = {"S T 600 - 0 0 Solto"};

    workout_init(&w);
    TEST_ASSERT_TRUE(LOAD(f));
    workout_start(&w, 0U, 0.0f);

    TEST_ASSERT_EQUAL_INT(WK_ZONE_IN, workout_zone(&w, 0U));
    TEST_ASSERT_EQUAL_INT(WK_ZONE_IN, workout_zone(&w, 9999U));
}

/* ==========================================================================
 * The edges
 * ========================================================================== */

static void test_a_label_longer_than_there_is_room_for(void)
{
    static const char *const f[] = {
        "S T 600 P 100 120 Um rotulo bem mais longo do que cabe",
    };

    TEST_ASSERT_TRUE(LOAD(f));
    TEST_ASSERT_EQUAL_UINT32(WORKOUT_LABEL_LEN - 1U, strlen(w.step[0].label));
}

static void test_a_step_without_a_label(void)
{
    static const char *const f[] = {"S T 600 P 100 120"};

    TEST_ASSERT_TRUE(LOAD(f));
    TEST_ASSERT_EQUAL_STRING("", w.step[0].label);
}

static void test_lines_that_end_the_way_a_file_ends_them(void)
{
    static const char *const f[] = {
        "NAME Com CRLF\r",
        "S T 600 P 100 120 Passo\r",
    };

    TEST_ASSERT_TRUE(LOAD(f));
    TEST_ASSERT_EQUAL_STRING("Com CRLF", workout_name(&w));
    TEST_ASSERT_EQUAL_STRING("Passo", w.step[0].label);
}

static void test_nothing_blows_up_without_a_session(void)
{
    workout_init(NULL);
    TEST_ASSERT_FALSE(workout_parse_line(NULL, "S T 600 P 100 120"));
    TEST_ASSERT_FALSE(workout_parse_line(&w, NULL));
    TEST_ASSERT_FALSE(workout_parse_end(NULL));
    TEST_ASSERT_FALSE(workout_is_loaded(NULL));
    TEST_ASSERT_EQUAL_STRING("", workout_name(NULL));
    TEST_ASSERT_EQUAL_UINT8(0U, workout_steps(NULL));
    TEST_ASSERT_NULL(workout_current(NULL));
    workout_start(NULL, 0U, 0.0f);
    workout_stop(NULL);
    workout_lap(NULL);
    TEST_ASSERT_EQUAL_INT(WK_EVENT_NONE, workout_update(NULL, 0U, 0.0f));
    TEST_ASSERT_EQUAL_UINT32(0U, workout_remaining(NULL, 0U, 0.0f));
    TEST_ASSERT_EQUAL_UINT16(0U, workout_target_power(NULL));
    TEST_ASSERT_EQUAL_INT(WK_ZONE_IN, workout_zone(NULL, 100U));
}

static void test_a_session_cannot_start_before_it_is_loaded(void)
{
    workout_start(&w, 0U, 0.0f);

    TEST_ASSERT_NULL(workout_current(&w));
    TEST_ASSERT_EQUAL_INT(WK_EVENT_NONE, workout_update(&w, 100U, 0.0f));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_simplest_session_there_is);
    RUN_TEST(test_blank_lines_and_comments_are_ignored);
    RUN_TEST(test_every_kind_of_duration_and_target);
    RUN_TEST(test_a_repeat_comes_out_flattened_in_order);
    RUN_TEST(test_a_repeat_of_one_is_the_block_itself);
    RUN_TEST(test_two_repeats_one_after_the_other);
    RUN_TEST(test_a_line_that_is_not_understood_refuses_the_whole_session);
    RUN_TEST(test_the_ways_a_step_can_be_wrong);
    RUN_TEST(test_a_repeat_that_is_never_closed);
    RUN_TEST(test_an_end_without_a_repeat);
    RUN_TEST(test_a_repeat_with_nothing_in_it);
    RUN_TEST(test_a_repeat_inside_a_repeat_is_refused);
    RUN_TEST(test_a_session_with_no_steps);
    RUN_TEST(test_a_session_too_big_to_hold_is_refused_whole);
    RUN_TEST(test_a_session_that_fits_exactly);
    RUN_TEST(test_a_session_that_has_not_started_does_nothing);
    RUN_TEST(test_the_steps_come_one_after_the_other);
    RUN_TEST(test_the_end_happens_once);
    RUN_TEST(test_a_long_epoch_that_covers_a_whole_step);
    RUN_TEST(test_a_step_that_ends_on_distance);
    RUN_TEST(test_a_step_that_waits_for_the_rider);
    RUN_TEST(test_one_press_does_not_end_two_steps_that_both_wait);
    RUN_TEST(test_stopping_and_starting_again_begins_from_the_top);
    RUN_TEST(test_the_trainer_is_asked_for_the_middle_of_the_range);
    RUN_TEST(test_a_step_with_another_target_asks_the_trainer_for_nothing);
    RUN_TEST(test_where_the_rider_sits_against_the_target);
    RUN_TEST(test_a_step_with_no_target_is_never_wrong);
    RUN_TEST(test_a_label_longer_than_there_is_room_for);
    RUN_TEST(test_a_step_without_a_label);
    RUN_TEST(test_lines_that_end_the_way_a_file_ends_them);
    RUN_TEST(test_nothing_blows_up_without_a_session);
    RUN_TEST(test_a_session_cannot_start_before_it_is_loaded);

    return UNITY_END();
}
