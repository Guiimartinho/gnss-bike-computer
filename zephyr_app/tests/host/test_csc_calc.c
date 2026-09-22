/**
 * @file test_csc_calc.c
 * @brief Speed and cadence from a CSC sensor (src/model/csc_calc.c)
 *
 * This arithmetic was wrong in the port until 2026-09-21: the speed came
 * out **3600 times too small**, which on a 700x25c wheel turning once a
 * second meant the screen showed zero instead of 7,6 km/h. The first test
 * here is that number, worked out by hand, so the mistake cannot come
 * back quietly.
 *
 * The rest is what a real sensor does that a first attempt forgets: both
 * counters wrap around, a stopped bike keeps sending, and the very first
 * notification has nothing to compare against.
 */

#include "unity.h"

#include "model/csc_calc.h"

static struct csc_calc c;

void setUp(void)
{
    csc_calc_init(&c, 0U);      /* the 700x25c of the port */
}

void tearDown(void) {}

static void test_the_first_reading_has_nothing_to_compare_against(void)
{
    TEST_ASSERT_EQUAL_UINT16(0U, csc_calc_speed(&c, 1000U, 5000U));
    TEST_ASSERT_EQUAL_UINT8(0U, csc_calc_cadence(&c, 500U, 5000U));
}

static void test_one_turn_a_second_is_seven_and_a_half_kilometres_an_hour(void)
{
    /*
     * 2105 mm in one second is 2,105 m/s, which is 7,578 km/h. This is the
     * number the port used to report as zero.
     */
    (void)csc_calc_speed(&c, 100U, 1000U);

    uint16_t v = csc_calc_speed(&c, 101U, 1000U + CSC_TICKS_PER_S);

    TEST_ASSERT_UINT16_WITHIN(2U, 757U, v);
}

static void test_a_real_riding_speed(void)
{
    /* five turns a second is 37,9 km/h */
    (void)csc_calc_speed(&c, 0U, 0U);
    TEST_ASSERT_UINT16_WITHIN(5U, 3789U, csc_calc_speed(&c, 5U, CSC_TICKS_PER_S));

    /* and the same distance over two seconds is half of it */
    csc_calc_init(&c, 0U);
    (void)csc_calc_speed(&c, 0U, 0U);
    TEST_ASSERT_UINT16_WITHIN(5U, 1894U, csc_calc_speed(&c, 5U, 2U * CSC_TICKS_PER_S));
}

static void test_a_bigger_wheel_goes_faster_for_the_same_turns(void)
{
    /* a 29 inch mountain bike wheel, 2326 mm */
    csc_calc_init(&c, 2326U);
    (void)csc_calc_speed(&c, 0U, 0U);

    uint16_t v = csc_calc_speed(&c, 1U, CSC_TICKS_PER_S);

    TEST_ASSERT_UINT16_WITHIN(2U, 837U, v);     /* 8,37 km/h */

    /* and changing the wheel takes effect without losing the counters */
    csc_calc_set_wheel(&c, CSC_WHEEL_MM);
    TEST_ASSERT_UINT16_WITHIN(2U, 757U, csc_calc_speed(&c, 2U, 2U * CSC_TICKS_PER_S));
}

static void test_a_bike_standing_still_reads_zero(void)
{
    (void)csc_calc_speed(&c, 400U, 12000U);

    /* the sensor keeps notifying with the counters frozen */
    for (unsigned int i = 0U; i < 10U; i++) {
        TEST_ASSERT_EQUAL_UINT16(0U, csc_calc_speed(&c, 400U, 12000U));
    }
}

static void test_the_event_time_wraps_every_sixty_four_seconds(void)
{
    /* 1/1024 s in sixteen bits runs out at 63,999 s and starts again */
    (void)csc_calc_speed(&c, 10U, 65000U);

    /* 65000 + 1024 is 66024, which wraps to 488 */
    uint16_t v = csc_calc_speed(&c, 11U, (uint16_t)(65000U + CSC_TICKS_PER_S));

    TEST_ASSERT_UINT16_WITHIN(2U, 757U, v);
}

static void test_the_wheel_counter_wraps_too(void)
{
    (void)csc_calc_speed(&c, 0xFFFFFFFEUL, 1000U);

    uint16_t v = csc_calc_speed(&c, 1U, 1000U + CSC_TICKS_PER_S);   /* three turns */

    TEST_ASSERT_UINT16_WITHIN(6U, 2273U, v);    /* 22,7 km/h */
}

static void test_an_impossible_speed_is_thrown_away(void)
{
    /* a gap the counter cannot really represent: a hundred turns in a
     * hundredth of a second would be 2400 km/h */
    (void)csc_calc_speed(&c, 0U, 0U);
    TEST_ASSERT_EQUAL_UINT16(0U, csc_calc_speed(&c, 100U, 10U));

    /* and the sensor is still usable afterwards */
    TEST_ASSERT_UINT16_WITHIN(5U, 3789U, csc_calc_speed(&c, 105U, 10U + CSC_TICKS_PER_S));
}

static void test_a_cadence_of_ninety(void)
{
    (void)csc_calc_cadence(&c, 0U, 0U);

    /* ninety turns a minute is three turns in two seconds */
    uint8_t rpm = csc_calc_cadence(&c, 3U, 2U * CSC_TICKS_PER_S);

    TEST_ASSERT_UINT8_WITHIN(1U, 90U, rpm);
}

static void test_coasting_reads_no_cadence(void)
{
    (void)csc_calc_cadence(&c, 700U, 4000U);
    TEST_ASSERT_EQUAL_UINT8(0U, csc_calc_cadence(&c, 700U, 4000U));
    /* and time passing with the crank still is still no cadence */
    TEST_ASSERT_EQUAL_UINT8(0U, csc_calc_cadence(&c, 700U, 4000U + CSC_TICKS_PER_S));
}

static void test_the_crank_counter_wraps(void)
{
    (void)csc_calc_cadence(&c, 0xFFFFU, 0U);

    /* 0xFFFF to 2 is three turns */
    uint8_t rpm = csc_calc_cadence(&c, 2U, 2U * CSC_TICKS_PER_S);

    TEST_ASSERT_UINT8_WITHIN(1U, 90U, rpm);
}

static void test_an_impossible_cadence_is_thrown_away(void)
{
    (void)csc_calc_cadence(&c, 0U, 0U);
    TEST_ASSERT_EQUAL_UINT8(0U, csc_calc_cadence(&c, 200U, 100U));
}

static void test_nothing_blows_up_without_a_sensor(void)
{
    csc_calc_init(NULL, 0U);
    csc_calc_set_wheel(NULL, 2105U);
    csc_calc_set_wheel(&c, 0U);     /* a wheel of zero is refused */
    TEST_ASSERT_EQUAL_UINT16(CSC_WHEEL_MM, c.wheel_mm);
    TEST_ASSERT_EQUAL_UINT16(0U, csc_calc_speed(NULL, 1U, 1U));
    TEST_ASSERT_EQUAL_UINT8(0U, csc_calc_cadence(NULL, 1U, 1U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_first_reading_has_nothing_to_compare_against);
    RUN_TEST(test_one_turn_a_second_is_seven_and_a_half_kilometres_an_hour);
    RUN_TEST(test_a_real_riding_speed);
    RUN_TEST(test_a_bigger_wheel_goes_faster_for_the_same_turns);
    RUN_TEST(test_a_bike_standing_still_reads_zero);
    RUN_TEST(test_the_event_time_wraps_every_sixty_four_seconds);
    RUN_TEST(test_the_wheel_counter_wraps_too);
    RUN_TEST(test_an_impossible_speed_is_thrown_away);
    RUN_TEST(test_a_cadence_of_ninety);
    RUN_TEST(test_coasting_reads_no_cadence);
    RUN_TEST(test_the_crank_counter_wraps);
    RUN_TEST(test_an_impossible_cadence_is_thrown_away);
    RUN_TEST(test_nothing_blows_up_without_a_sensor);

    return UNITY_END();
}
