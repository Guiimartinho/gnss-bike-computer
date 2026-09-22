/**
 * @file test_power_metrics.c
 * @brief Normalised power, IF, TSS and VI (src/model/power_metrics.c)
 *
 * There is no legacy to check against: the stravaV10 has power zones and a
 * suffer score but none of these. The oracle is the definition itself
 * (Andrew Coggan's), written out in `model/power_metrics.h`, and every
 * expected number below was computed from it in Python before the test was
 * written, not read off the module.
 *
 * The anchor is the one every rider knows: **one hour exactly at threshold
 * is 100 points of training stress**. Everything else hangs off that.
 */

#include <string.h>

#include "unity.h"

#include "model/power_metrics.h"

static struct power_metrics pm;

void setUp(void)
{
    power_metrics_init(&pm, 200U);
}

void tearDown(void) {}

/** `n` seconds at a steady power */
static void feed(uint16_t watts, unsigned int n)
{
    for (unsigned int i = 0U; i < n; i++) {
        power_metrics_add(&pm, watts);
    }
}

/* ==========================================================================
 * Where it starts
 * ========================================================================== */

static void test_a_fresh_ride_has_nothing(void)
{
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_avg(&pm));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_tss(&pm));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_vi100(&pm));
}

static void test_under_thirty_seconds_there_is_no_normalised_power(void)
{
    /*
     * The rolling average needs thirty seconds before it means anything.
     * The plain average is there from the first second, which is why the
     * two are separate numbers.
     */
    feed(200U, 29U);

    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(200U, power_metrics_avg(&pm));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_tss(&pm));
}

static void test_at_the_thirtieth_second_it_appears(void)
{
    feed(200U, 30U);

    TEST_ASSERT_EQUAL_UINT16(200U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(100U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(1U, power_metrics_tss(&pm));    /* 30 s of it */
}

/* ==========================================================================
 * The anchor: an hour at threshold is 100
 * ========================================================================== */

static void test_an_hour_exactly_at_threshold_scores_one_hundred(void)
{
    feed(200U, 3600U);

    TEST_ASSERT_EQUAL_UINT16(200U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(200U, power_metrics_avg(&pm));
    TEST_ASSERT_EQUAL_UINT16(100U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(100U, power_metrics_tss(&pm));
    TEST_ASSERT_EQUAL_UINT16(100U, power_metrics_vi100(&pm));
}

static void test_half_an_hour_at_threshold_is_half_the_stress(void)
{
    feed(200U, 1800U);

    TEST_ASSERT_EQUAL_UINT16(100U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(50U, power_metrics_tss(&pm));
}

static void test_an_hour_at_half_the_threshold(void)
{
    /* IF 0,5 for an hour: TSS = 1 x 0,5^2 x 100 = 25 */
    feed(100U, 3600U);

    TEST_ASSERT_EQUAL_UINT16(100U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(50U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(25U, power_metrics_tss(&pm));
}

/* ==========================================================================
 * Why the number exists at all
 * ========================================================================== */

static void test_a_ragged_ride_costs_more_than_its_average_says(void)
{
    /*
     * Five minutes at 100 W and five at 300 W average 200 W, the same as
     * ten minutes steady at 200 W — and cost the body far more. The fourth
     * power is what says so: NP comes out at 252, a variability index of
     * 1,26, and the stress is 26 points against the 17 of the steady ride.
     */
    feed(100U, 300U);
    feed(300U, 300U);

    TEST_ASSERT_EQUAL_UINT16(252U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(200U, power_metrics_avg(&pm));
    TEST_ASSERT_EQUAL_UINT16(126U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(26U, power_metrics_tss(&pm));
    TEST_ASSERT_EQUAL_UINT16(126U, power_metrics_vi100(&pm));
}

static void test_a_flicker_faster_than_the_window_does_not_count(void)
{
    /*
     * The other half of the same idea, and the reason the window is thirty
     * seconds and not one: alternating 100 and 300 W every second is what a
     * pedal stroke and a gust of wind do, not what an interval does. The
     * rolling average flattens it, and NP stays at the plain average.
     */
    for (unsigned int i = 0U; i < 600U; i++) {
        power_metrics_add(&pm, ((i % 2U) == 0U) ? 300U : 100U);
    }

    TEST_ASSERT_EQUAL_UINT16(200U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(200U, power_metrics_avg(&pm));
    TEST_ASSERT_EQUAL_UINT16(100U, power_metrics_vi100(&pm));
}

static void test_an_hour_of_blocks_of_twenty_minutes(void)
{
    /* a real-looking session, checked against the definition in Python */
    power_metrics_init(&pm, 250U);
    feed(150U, 1200U);
    feed(250U, 1200U);
    feed(200U, 1200U);

    TEST_ASSERT_EQUAL_UINT16(212U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(200U, power_metrics_avg(&pm));
    TEST_ASSERT_EQUAL_UINT16(85U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(72U, power_metrics_tss(&pm));
    TEST_ASSERT_EQUAL_UINT16(106U, power_metrics_vi100(&pm));
}

/* ==========================================================================
 * The window itself
 * ========================================================================== */

static void test_the_window_holds_exactly_thirty_seconds(void)
{
    /*
     * Thirty seconds of 300 W and then thirty of nothing: by the end the
     * rolling average has to be back at zero, which only happens if the
     * oldest sample really leaves the ring.
     */
    feed(300U, 30U);
    TEST_ASSERT_EQUAL_UINT16(300U, power_metrics_np(&pm));

    feed(0U, 30U);

    /* the last rolling average was 0, so NP has fallen but is not zero */
    TEST_ASSERT_TRUE(power_metrics_np(&pm) < 300U);
    TEST_ASSERT_TRUE(power_metrics_np(&pm) > 0U);
    TEST_ASSERT_EQUAL_UINT16(150U, power_metrics_avg(&pm));

    /* one more second of nothing and the window is all zeros */
    feed(0U, 1U);
    TEST_ASSERT_EQUAL_UINT16(0U, pm.ring_sum);
}

static void test_a_spike_no_rider_makes_counts_as_a_zero_second(void)
{
    /*
     * A sensor glitch of 9000 W would otherwise dominate the fourth power
     * and ruin the whole ride. It is counted as zero rather than dropped,
     * because dropping a second would slide the window and make the
     * average cover more than thirty seconds.
     */
    feed(200U, 29U);
    power_metrics_add(&pm, 9000U);
    feed(200U, 30U);

    TEST_ASSERT_EQUAL_UINT16(194U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(197U, power_metrics_avg(&pm));
    TEST_ASSERT_EQUAL_UINT32(60U, pm.seconds);   /* the second still counts */
}

static void test_the_limit_of_a_believable_power(void)
{
    feed(0U, 29U);
    power_metrics_add(&pm, PM_POWER_MAX_W);         /* taken as it is */
    TEST_ASSERT_EQUAL_UINT16((uint16_t)(PM_POWER_MAX_W / 30U), power_metrics_np(&pm));

    power_metrics_init(&pm, 200U);
    feed(0U, 29U);
    power_metrics_add(&pm, PM_POWER_MAX_W + 1U);    /* one more is noise */
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_np(&pm));
}

/* ==========================================================================
 * The threshold of the rider
 * ========================================================================== */

static void test_a_real_sprint_is_not_mistaken_for_noise(void)
{
    /*
     * The limit has to sit above what a rider can actually produce. A
     * sprint of 1200 W is ordinary for a strong rider and a track sprinter
     * passes 2000 W, so those seconds must be counted, not thrown away as
     * a sensor glitch. This is what keeps PM_POWER_MAX_W from being
     * lowered to something that would quietly erase the best seconds of a
     * ride.
     */
    feed(0U, 29U);
    power_metrics_add(&pm, 1200U);
    TEST_ASSERT_EQUAL_UINT16(40U, power_metrics_np(&pm));    /* 1200 / 30 */

    power_metrics_init(&pm, 200U);
    feed(0U, 29U);
    power_metrics_add(&pm, 2000U);
    TEST_ASSERT_EQUAL_UINT16(67U, power_metrics_np(&pm));    /* 2000 / 30 */
}

static void test_without_a_threshold_there_is_no_intensity_or_stress(void)
{
    power_metrics_init(&pm, 0U);
    feed(200U, 3600U);

    /* the ride still has its power, but nothing to measure it against */
    TEST_ASSERT_EQUAL_UINT16(200U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(200U, power_metrics_avg(&pm));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_tss(&pm));
    TEST_ASSERT_EQUAL_UINT16(100U, power_metrics_vi100(&pm));
}

static void test_changing_the_threshold_changes_what_the_ride_was_worth(void)
{
    feed(200U, 3600U);
    TEST_ASSERT_EQUAL_UINT16(100U, power_metrics_tss(&pm));

    /* the rider tested and found they were stronger than they thought */
    power_metrics_set_ftp(&pm, 250U);

    TEST_ASSERT_EQUAL_UINT16(80U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(64U, power_metrics_tss(&pm));
}

static void test_a_ride_above_threshold(void)
{
    /* an hour at 250 W with a threshold of 200: IF 1,25, TSS 156 */
    feed(250U, 3600U);

    TEST_ASSERT_EQUAL_UINT16(250U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(125U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(156U, power_metrics_tss(&pm));
}

/* ==========================================================================
 * The long ride, where a float accumulator would have given up
 * ========================================================================== */

static void test_eight_hours_at_a_high_power_still_adds_up(void)
{
    /*
     * Eight hours of 400 W: the sum of fourth powers reaches 7,4e14 and the
     * stress passes what a uint8 would hold. A float accumulator would
     * survive this too, to about a tenth of a percent (see the note in the
     * header); what this pins down is that a long ride keeps adding up and
     * that TSS scales with the hours.
     */
    power_metrics_init(&pm, 400U);
    feed(400U, 8U * 3600U);

    TEST_ASSERT_EQUAL_UINT16(400U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT16(400U, power_metrics_avg(&pm));
    TEST_ASSERT_EQUAL_UINT16(100U, power_metrics_if100(&pm));
    TEST_ASSERT_EQUAL_UINT16(800U, power_metrics_tss(&pm));   /* 8 x 100 */
}

static void test_starting_over_forgets_the_ride_but_keeps_the_threshold(void)
{
    feed(300U, 600U);
    TEST_ASSERT_TRUE(power_metrics_np(&pm) > 0U);

    power_metrics_init(&pm, 250U);

    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_np(&pm));
    TEST_ASSERT_EQUAL_UINT32(0U, pm.seconds);
    TEST_ASSERT_EQUAL_UINT16(250U, pm.ftp_w);
}

static void test_nothing_blows_up_without_a_ride(void)
{
    power_metrics_init(NULL, 200U);
    power_metrics_set_ftp(NULL, 200U);
    power_metrics_add(NULL, 200U);
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_np(NULL));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_avg(NULL));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_if100(NULL));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_tss(NULL));
    TEST_ASSERT_EQUAL_UINT16(0U, power_metrics_vi100(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_fresh_ride_has_nothing);
    RUN_TEST(test_under_thirty_seconds_there_is_no_normalised_power);
    RUN_TEST(test_at_the_thirtieth_second_it_appears);
    RUN_TEST(test_an_hour_exactly_at_threshold_scores_one_hundred);
    RUN_TEST(test_half_an_hour_at_threshold_is_half_the_stress);
    RUN_TEST(test_an_hour_at_half_the_threshold);
    RUN_TEST(test_a_ragged_ride_costs_more_than_its_average_says);
    RUN_TEST(test_a_flicker_faster_than_the_window_does_not_count);
    RUN_TEST(test_an_hour_of_blocks_of_twenty_minutes);
    RUN_TEST(test_the_window_holds_exactly_thirty_seconds);
    RUN_TEST(test_a_spike_no_rider_makes_counts_as_a_zero_second);
    RUN_TEST(test_the_limit_of_a_believable_power);
    RUN_TEST(test_a_real_sprint_is_not_mistaken_for_noise);
    RUN_TEST(test_without_a_threshold_there_is_no_intensity_or_stress);
    RUN_TEST(test_changing_the_threshold_changes_what_the_ride_was_worth);
    RUN_TEST(test_a_ride_above_threshold);
    RUN_TEST(test_eight_hours_at_a_high_power_still_adds_up);
    RUN_TEST(test_starting_over_forgets_the_ride_but_keeps_the_threshold);
    RUN_TEST(test_nothing_blows_up_without_a_ride);

    return UNITY_END();
}
