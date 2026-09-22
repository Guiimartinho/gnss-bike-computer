/**
 * @file test_loc_arbiter.c
 * @brief Which position source wins (src/model/loc_arbiter.c)
 *
 * A fidelity test: the rule is `legacy/source/model/Locator.cpp:111-134`,
 * transcribed in `model/loc_arbiter.h`, and every case here is a line of
 * it. The two that a plain "take the newest" gets wrong have tests of
 * their own, because they are the reason the rule is shaped like that:
 *
 * - a source that is recent but not new gives **nothing**, so that between
 *   two frames of a simulated ride the receiver cannot slip a position in
 *   and make the bike jump;
 * - the phone is refused while the receiver has a fix, however old.
 */

#include "unity.h"

#include "model/loc_arbiter.h"

static struct loc_arbiter a;

void setUp(void)
{
    loc_arbiter_init(&a);
}

void tearDown(void) {}

static void test_with_nothing_arriving_there_is_no_source(void)
{
    TEST_ASSERT_EQUAL_INT(LOC_ARB_NONE, loc_arbiter_pick(&a, 1000U, false));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, loc_arbiter_age(&a, LOC_ARB_GPS, 1000U));
}

static void test_the_receiver_alone_is_taken(void)
{
    loc_arbiter_feed(&a, LOC_ARB_GPS, 1000U);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_GPS, loc_arbiter_pick(&a, 1000U, true));
}

static void test_a_sample_is_offered_only_once(void)
{
    /* `isUpdated()` of the legacy is a one-shot, not an age test */
    loc_arbiter_feed(&a, LOC_ARB_GPS, 1000U);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_GPS, loc_arbiter_pick(&a, 1000U, true));
    TEST_ASSERT_EQUAL_INT(LOC_ARB_NONE, loc_arbiter_pick(&a, 1100U, true));

    loc_arbiter_feed(&a, LOC_ARB_GPS, 2000U);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_GPS, loc_arbiter_pick(&a, 2000U, true));
}

static void test_the_simulated_ride_wins_over_the_receiver(void)
{
    loc_arbiter_feed(&a, LOC_ARB_GPS, 1000U);
    loc_arbiter_feed(&a, LOC_ARB_SIM, 1000U);

    TEST_ASSERT_EQUAL_INT(LOC_ARB_SIM, loc_arbiter_pick(&a, 1000U, true));
}

static void test_between_two_frames_of_a_simulation_nothing_slips_in(void)
{
    /*
     * This is the line `else if (sim_loc.getAge() < 2000) return None;`.
     * A receiver position arriving in the gap must not be used: the bike
     * would jump from the simulated track to where the device really is.
     */
    loc_arbiter_feed(&a, LOC_ARB_SIM, 1000U);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_SIM, loc_arbiter_pick(&a, 1000U, true));

    loc_arbiter_feed(&a, LOC_ARB_GPS, 1500U);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_NONE, loc_arbiter_pick(&a, 1500U, true));

    /* and the receiver sample was used up, not held over */
    TEST_ASSERT_EQUAL_INT(LOC_ARB_NONE, loc_arbiter_pick(&a, 1600U, true));
}

static void test_the_receiver_comes_back_when_the_simulation_stops(void)
{
    loc_arbiter_feed(&a, LOC_ARB_SIM, 1000U);
    (void)loc_arbiter_pick(&a, 1000U, true);

    /* one millisecond before the block runs out, still nothing */
    loc_arbiter_feed(&a, LOC_ARB_GPS, 1000U + LOC_ARB_SIM_BLOCK_MS - 1U);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_NONE,
                          loc_arbiter_pick(&a, 1000U + LOC_ARB_SIM_BLOCK_MS - 1U, true));

    /* at the threshold the receiver is heard again */
    loc_arbiter_feed(&a, LOC_ARB_GPS, 1000U + LOC_ARB_SIM_BLOCK_MS);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_GPS, loc_arbiter_pick(&a, 1000U + LOC_ARB_SIM_BLOCK_MS, true));
}

static void test_the_phone_is_refused_while_the_receiver_has_a_fix(void)
{
    /* the receiver has not spoken for a long time, but it still has a fix */
    loc_arbiter_feed(&a, LOC_ARB_LNS, 10000U);

    TEST_ASSERT_EQUAL_INT(LOC_ARB_NONE, loc_arbiter_pick(&a, 10000U, true));
}

static void test_the_phone_is_taken_when_the_receiver_has_no_fix(void)
{
    loc_arbiter_feed(&a, LOC_ARB_LNS, 10000U);

    TEST_ASSERT_EQUAL_INT(LOC_ARB_LNS, loc_arbiter_pick(&a, 10000U, false));
}

static void test_a_recent_receiver_blocks_the_phone_even_with_no_fix(void)
{
    /* `else if (gps_loc.getAge() < 1500) return None;` */
    loc_arbiter_feed(&a, LOC_ARB_GPS, 1000U);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_GPS, loc_arbiter_pick(&a, 1000U, false));

    loc_arbiter_feed(&a, LOC_ARB_LNS, 1400U);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_NONE, loc_arbiter_pick(&a, 1400U, false));

    /* past the block, and with no fix, the phone is heard */
    loc_arbiter_feed(&a, LOC_ARB_LNS, 1000U + LOC_ARB_GPS_BLOCK_MS);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_LNS,
                          loc_arbiter_pick(&a, 1000U + LOC_ARB_GPS_BLOCK_MS, false));
}

static void test_the_whole_order_with_all_three_talking(void)
{
    loc_arbiter_feed(&a, LOC_ARB_GPS, 1000U);
    loc_arbiter_feed(&a, LOC_ARB_LNS, 1000U);
    loc_arbiter_feed(&a, LOC_ARB_SIM, 1000U);

    /* the simulation first, and it takes the other two with it */
    TEST_ASSERT_EQUAL_INT(LOC_ARB_SIM, loc_arbiter_pick(&a, 1000U, false));
    TEST_ASSERT_EQUAL_INT(LOC_ARB_NONE, loc_arbiter_pick(&a, 1000U, false));

    /* with the simulation gone, the receiver; the phone waits its turn */
    loc_arbiter_feed(&a, LOC_ARB_GPS, 5000U);
    loc_arbiter_feed(&a, LOC_ARB_LNS, 5000U);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_GPS, loc_arbiter_pick(&a, 5000U, false));

    /* and with neither of the other two, the phone */
    loc_arbiter_feed(&a, LOC_ARB_LNS, 9000U);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_LNS, loc_arbiter_pick(&a, 9000U, false));
}

static void test_a_simulated_ride_at_one_hertz_never_lets_go(void)
{
    /* a frame a second, and the block is two: the receiver never wins */
    for (unsigned int t = 0U; t < 30U; t++) {
        uint32_t now = 1000U + (t * 1000U);

        loc_arbiter_feed(&a, LOC_ARB_SIM, now);
        loc_arbiter_feed(&a, LOC_ARB_GPS, now);
        TEST_ASSERT_EQUAL_INT(LOC_ARB_SIM, loc_arbiter_pick(&a, now, true));
    }
}

static void test_the_age_of_each_source(void)
{
    loc_arbiter_feed(&a, LOC_ARB_GPS, 1000U);
    TEST_ASSERT_EQUAL_UINT32(0U, loc_arbiter_age(&a, LOC_ARB_GPS, 1000U));
    TEST_ASSERT_EQUAL_UINT32(2500U, loc_arbiter_age(&a, LOC_ARB_GPS, 3500U));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, loc_arbiter_age(&a, LOC_ARB_LNS, 3500U));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, loc_arbiter_age(&a, LOC_ARB_NONE, 3500U));
}

static void test_the_clock_wrapping_does_not_unblock_a_source(void)
{
    /* k_uptime_get_32() wraps after 49 days; unsigned arithmetic carries */
    uint32_t late = 0xFFFFFF00UL;

    loc_arbiter_feed(&a, LOC_ARB_SIM, late);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_SIM, loc_arbiter_pick(&a, late, true));

    uint32_t after = late + 500U;   /* wrapped past zero */

    loc_arbiter_feed(&a, LOC_ARB_GPS, after);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_NONE, loc_arbiter_pick(&a, after, true));
    TEST_ASSERT_EQUAL_UINT32(500U, loc_arbiter_age(&a, LOC_ARB_SIM, after));
}

static void test_nothing_blows_up_without_an_arbiter(void)
{
    loc_arbiter_init(NULL);
    loc_arbiter_feed(NULL, LOC_ARB_GPS, 0U);
    loc_arbiter_feed(&a, LOC_ARB_NONE, 0U);
    loc_arbiter_feed(&a, (enum loc_arb_src)99, 0U);
    TEST_ASSERT_EQUAL_INT(LOC_ARB_NONE, loc_arbiter_pick(NULL, 0U, false));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, loc_arbiter_age(NULL, LOC_ARB_GPS, 0U));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, loc_arbiter_age(&a, (enum loc_arb_src)99, 0U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_with_nothing_arriving_there_is_no_source);
    RUN_TEST(test_the_receiver_alone_is_taken);
    RUN_TEST(test_a_sample_is_offered_only_once);
    RUN_TEST(test_the_simulated_ride_wins_over_the_receiver);
    RUN_TEST(test_between_two_frames_of_a_simulation_nothing_slips_in);
    RUN_TEST(test_the_receiver_comes_back_when_the_simulation_stops);
    RUN_TEST(test_the_phone_is_refused_while_the_receiver_has_a_fix);
    RUN_TEST(test_the_phone_is_taken_when_the_receiver_has_no_fix);
    RUN_TEST(test_a_recent_receiver_blocks_the_phone_even_with_no_fix);
    RUN_TEST(test_the_whole_order_with_all_three_talking);
    RUN_TEST(test_a_simulated_ride_at_one_hertz_never_lets_go);
    RUN_TEST(test_the_age_of_each_source);
    RUN_TEST(test_the_clock_wrapping_does_not_unblock_a_source);
    RUN_TEST(test_nothing_blows_up_without_an_arbiter);

    return UNITY_END();
}
