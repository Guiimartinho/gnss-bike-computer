/**
 * @file test_alerts.c
 * @brief The alerts a rider sets (src/model/alerts.c)
 *
 * The alerts themselves are a comparison. What the tests are about is the
 * part that makes them usable instead of infuriating: a rider sitting on
 * the limit must be told **once**, not every epoch, and must be told again
 * only after really coming back inside and after a decent gap. An alert
 * that cries wolf is an alert the rider turns off.
 *
 * There is no legacy for any of this; the rules are the ones written out
 * in `model/alerts.h`.
 */

#include <string.h>

#include "unity.h"

#include "model/alerts.h"

static struct alerts a;
static struct alert_sample s;

void setUp(void)
{
    alerts_init(&a);
    (void)memset(&s, 0, sizeof(s));
}

void tearDown(void) {}

/** One epoch, and what went off */
static uint32_t at(uint32_t ms)
{
    return alerts_update(&a, &s, ms);
}

#define BIT(id)     (1U << (id))

/* ==========================================================================
 * Nothing set, nothing said
 * ========================================================================== */

static void test_a_rider_who_set_nothing_is_never_interrupted(void)
{
    s.hr_bpm = 250U;
    s.power_w = 2000U;
    s.speed_kmh10 = 900U;
    s.cadence_rpm = 200U;
    s.dist_m = 100000.0f;
    s.moving_s = 10000U;

    TEST_ASSERT_EQUAL_UINT32(0U, at(1000U));
}

static void test_an_alert_with_no_value_stays_off(void)
{
    /* the settings screen can hand a zero; it must not mean "always" */
    alerts_set(&a, ALERT_HR_HIGH, 0U, true);
    s.hr_bpm = 200U;

    TEST_ASSERT_EQUAL_UINT32(0U, at(1000U));
    TEST_ASSERT_FALSE(alerts_get(&a, ALERT_HR_HIGH).on);
}

static void test_an_alert_can_be_turned_off_again(void)
{
    alerts_set(&a, ALERT_HR_HIGH, 165U, true);
    s.hr_bpm = 180U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_HIGH), at(1000U));

    alerts_set(&a, ALERT_HR_HIGH, 165U, false);
    TEST_ASSERT_EQUAL_UINT32(0U, at(200000U));
}

/* ==========================================================================
 * A limit above: the heart rate
 * ========================================================================== */

static void test_going_over_the_limit_says_so_once(void)
{
    alerts_set(&a, ALERT_HR_HIGH, 165U, true);

    s.hr_bpm = 160U;
    TEST_ASSERT_EQUAL_UINT32(0U, at(1000U));

    s.hr_bpm = 166U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_HIGH), at(2000U));

    /* and then keeps quiet while the rider stays up there */
    for (uint32_t t = 3000U; t < 100000U; t += 1000U) {
        TEST_ASSERT_EQUAL_UINT32(0U, at(t));
    }
}

static void test_the_limit_itself_is_not_over_it(void)
{
    alerts_set(&a, ALERT_HR_HIGH, 165U, true);

    s.hr_bpm = 165U;
    TEST_ASSERT_EQUAL_UINT32(0U, at(1000U));

    s.hr_bpm = 166U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_HIGH), at(2000U));
}

static void test_coming_back_to_the_line_is_not_coming_back_inside(void)
{
    /*
     * The heart of it. A rider holding 165, 166, 165, 166 must hear one
     * alert, not four: the alert only re-arms three beats below the limit.
     */
    alerts_set(&a, ALERT_HR_HIGH, 165U, true);

    s.hr_bpm = 170U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_HIGH), at(1000U));

    s.hr_bpm = 165U;                 /* on the line: not re-armed */
    TEST_ASSERT_EQUAL_UINT32(0U, at(200000U));
    s.hr_bpm = 170U;
    TEST_ASSERT_EQUAL_UINT32(0U, at(201000U));

    s.hr_bpm = 163U;                 /* two below: still not enough */
    TEST_ASSERT_EQUAL_UINT32(0U, at(202000U));
    s.hr_bpm = 170U;
    TEST_ASSERT_EQUAL_UINT32(0U, at(203000U));

    s.hr_bpm = 162U;                 /* three below: re-armed */
    TEST_ASSERT_EQUAL_UINT32(0U, at(204000U));
    s.hr_bpm = 170U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_HIGH), at(205000U));
}

static void test_even_re_armed_it_waits_a_minute(void)
{
    /*
     * A hard effort that swings wide of the limit would otherwise give an
     * alert every few seconds. One a minute is the most it can say.
     */
    alerts_set(&a, ALERT_HR_HIGH, 165U, true);

    s.hr_bpm = 180U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_HIGH), at(10000U));

    s.hr_bpm = 140U;                 /* well back inside: re-armed */
    TEST_ASSERT_EQUAL_UINT32(0U, at(20000U));

    s.hr_bpm = 180U;                 /* ten seconds later: too soon */
    TEST_ASSERT_EQUAL_UINT32(0U, at(30000U));

    s.hr_bpm = 140U;
    TEST_ASSERT_EQUAL_UINT32(0U, at(40000U));

    s.hr_bpm = 180U;                 /* past the minute: allowed */
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_HIGH), at(10000U + ALERT_MIN_REPEAT_MS));
}

/* ==========================================================================
 * A limit below
 * ========================================================================== */

static void test_dropping_under_the_limit_says_so(void)
{
    alerts_set(&a, ALERT_CADENCE_LOW, 70U, true);

    s.cadence_rpm = 90U;
    TEST_ASSERT_EQUAL_UINT32(0U, at(1000U));

    s.cadence_rpm = 65U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_CADENCE_LOW), at(2000U));
    TEST_ASSERT_EQUAL_UINT32(0U, at(3000U));
}

static void test_a_sensor_that_is_not_there_does_not_set_off_a_low_alert(void)
{
    /*
     * A strap that has not connected reads zero, and zero is below every
     * limit. Without this the rider would be told their heart had stopped
     * before they left the house.
     */
    alerts_set(&a, ALERT_HR_LOW, 100U, true);
    alerts_set(&a, ALERT_POWER_LOW, 100U, true);
    alerts_set(&a, ALERT_CADENCE_LOW, 60U, true);
    alerts_set(&a, ALERT_SPEED_LOW, 100U, true);

    s.hr_bpm = 0U;
    s.power_w = 0U;
    s.cadence_rpm = 0U;
    s.speed_kmh10 = 0U;

    for (uint32_t t = 1000U; t < 300000U; t += 10000U) {
        TEST_ASSERT_EQUAL_UINT32(0U, at(t));
    }

    /* and once the strap does report, the alert works */
    s.hr_bpm = 80U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_LOW), at(310000U));
}

static void test_the_low_alert_re_arms_above_the_limit_by_the_margin(void)
{
    alerts_set(&a, ALERT_CADENCE_LOW, 70U, true);

    s.cadence_rpm = 60U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_CADENCE_LOW), at(1000U));

    s.cadence_rpm = 72U;             /* two above: not enough */
    TEST_ASSERT_EQUAL_UINT32(0U, at(200000U));
    s.cadence_rpm = 60U;
    TEST_ASSERT_EQUAL_UINT32(0U, at(201000U));

    s.cadence_rpm = 73U;             /* three above: re-armed */
    TEST_ASSERT_EQUAL_UINT32(0U, at(202000U));
    s.cadence_rpm = 60U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_CADENCE_LOW), at(203000U));
}

/* ==========================================================================
 * Power and speed have their own margins
 * ========================================================================== */

static void test_power_uses_a_margin_of_fifteen_watts(void)
{
    alerts_set(&a, ALERT_POWER_HIGH, 300U, true);

    s.power_w = 320U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_POWER_HIGH), at(1000U));

    s.power_w = 290U;                /* ten under: not enough */
    TEST_ASSERT_EQUAL_UINT32(0U, at(200000U));
    s.power_w = 320U;
    TEST_ASSERT_EQUAL_UINT32(0U, at(201000U));

    s.power_w = 285U;                /* fifteen under: re-armed */
    TEST_ASSERT_EQUAL_UINT32(0U, at(202000U));
    s.power_w = 320U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_POWER_HIGH), at(203000U));
}

static void test_speed_is_in_tenths_of_a_kilometre(void)
{
    /* 60,0 km/h down a descent */
    alerts_set(&a, ALERT_SPEED_HIGH, 600U, true);

    s.speed_kmh10 = 595U;
    TEST_ASSERT_EQUAL_UINT32(0U, at(1000U));

    s.speed_kmh10 = 605U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_SPEED_HIGH), at(2000U));
}

static void test_a_limit_lower_than_its_own_margin(void)
{
    /*
     * A limit of 10 W with a margin of 15 would underflow if the margin
     * were simply subtracted. Coming back under the limit at all re-arms
     * it, which is the only thing that can be meant down there.
     */
    alerts_set(&a, ALERT_POWER_HIGH, 10U, true);

    s.power_w = 50U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_POWER_HIGH), at(1000U));

    s.power_w = 5U;
    TEST_ASSERT_EQUAL_UINT32(0U, at(200000U));
    s.power_w = 50U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_POWER_HIGH), at(201000U));
}

/* ==========================================================================
 * The ones that fire every so much
 * ========================================================================== */

static void test_every_ten_kilometres(void)
{
    /* the distance alert counts in hundreds of metres, so 10 km is 100 */
    alerts_set(&a, ALERT_DISTANCE, 100U, true);

    s.dist_m = 9900.0f;
    TEST_ASSERT_EQUAL_UINT32(0U, at(1000U));

    s.dist_m = 10000.0f;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_DISTANCE), at(2000U));

    s.dist_m = 10100.0f;
    TEST_ASSERT_EQUAL_UINT32(0U, at(3000U));

    s.dist_m = 20000.0f;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_DISTANCE), at(4000U));
}

static void test_a_long_gap_that_passes_two_marks_fires_once(void)
{
    /*
     * The receiver went quiet through a tunnel and the ride jumped 25 km.
     * Two notifications for one epoch would be noise, and three would be
     * worse; the rider is told once and the count moves on.
     */
    alerts_set(&a, ALERT_DISTANCE, 100U, true);

    s.dist_m = 5000.0f;
    TEST_ASSERT_EQUAL_UINT32(0U, at(1000U));

    s.dist_m = 30000.0f;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_DISTANCE), at(2000U));

    s.dist_m = 30100.0f;
    TEST_ASSERT_EQUAL_UINT32(0U, at(3000U));

    s.dist_m = 40000.0f;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_DISTANCE), at(4000U));
}

static void test_the_interval_alerts_count_moving_time_in_minutes(void)
{
    alerts_set(&a, ALERT_DRINK, 20U, true);
    alerts_set(&a, ALERT_EAT, 45U, true);

    s.moving_s = 19U * 60U;
    TEST_ASSERT_EQUAL_UINT32(0U, at(1000U));

    s.moving_s = 20U * 60U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_DRINK), at(2000U));

    s.moving_s = 40U * 60U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_DRINK), at(3000U));

    /* at 45 minutes both come due, and both are said */
    s.moving_s = 45U * 60U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_EAT), at(4000U));

    s.moving_s = 60U * 60U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_DRINK), at(5000U));
}

static void test_the_interval_alerts_do_not_count_a_stop(void)
{
    /*
     * They are fed moving time, so an hour at a café does not bring the
     * next reminder any closer. That is the caller's doing, and this test
     * records the contract: the clock only moves when `moving_s` does.
     */
    alerts_set(&a, ALERT_DRINK, 20U, true);

    s.moving_s = 20U * 60U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_DRINK), at(1000U));

    for (uint32_t t = 2000U; t < 3600000U; t += 60000U) {
        TEST_ASSERT_EQUAL_UINT32(0U, at(t));
    }
}

/* ==========================================================================
 * Several at once, and starting over
 * ========================================================================== */

static void test_more_than_one_can_go_off_in_the_same_epoch(void)
{
    alerts_set(&a, ALERT_HR_HIGH, 160U, true);
    alerts_set(&a, ALERT_POWER_HIGH, 300U, true);
    alerts_set(&a, ALERT_DISTANCE, 100U, true);

    s.hr_bpm = 175U;
    s.power_w = 350U;
    s.dist_m = 10000.0f;

    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_HIGH) | BIT(ALERT_POWER_HIGH) | BIT(ALERT_DISTANCE),
                             at(1000U));
}

static void test_changing_a_limit_gives_the_rider_a_clean_start(void)
{
    alerts_set(&a, ALERT_HR_HIGH, 165U, true);
    s.hr_bpm = 180U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_HIGH), at(1000U));
    TEST_ASSERT_EQUAL_UINT32(0U, at(2000U));

    /* they raised it in the settings: the new limit speaks straight away */
    alerts_set(&a, ALERT_HR_HIGH, 175U, true);
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_HIGH), at(3000U));
    TEST_ASSERT_EQUAL_UINT16(175U, alerts_get(&a, ALERT_HR_HIGH).value);
}

static void test_a_new_ride_starts_the_alerts_over(void)
{
    alerts_set(&a, ALERT_DISTANCE, 100U, true);
    alerts_set(&a, ALERT_HR_HIGH, 165U, true);

    s.dist_m = 10000.0f;
    s.hr_bpm = 180U;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_DISTANCE) | BIT(ALERT_HR_HIGH), at(1000U));

    alerts_reset(&a);

    /* the ride starts again from zero and the marks come round again */
    s.dist_m = 10000.0f;
    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_DISTANCE) | BIT(ALERT_HR_HIGH), at(2000U));

    /* and the settings survived the reset */
    TEST_ASSERT_EQUAL_UINT16(100U, alerts_get(&a, ALERT_DISTANCE).value);
    TEST_ASSERT_TRUE(alerts_get(&a, ALERT_DISTANCE).on);
}

static void test_the_very_first_epoch_can_fire(void)
{
    /* `last_ms` of zero means "never", not "at uptime zero" */
    alerts_set(&a, ALERT_HR_HIGH, 100U, true);
    s.hr_bpm = 150U;

    TEST_ASSERT_EQUAL_UINT32(BIT(ALERT_HR_HIGH), at(0U));
    TEST_ASSERT_EQUAL_UINT32(0U, at(1U));
}

static void test_nothing_blows_up_without_alerts(void)
{
    alerts_init(NULL);
    alerts_reset(NULL);
    alerts_set(NULL, ALERT_HR_HIGH, 100U, true);
    alerts_set(&a, ALERT_COUNT, 100U, true);
    TEST_ASSERT_EQUAL_UINT32(0U, alerts_update(NULL, &s, 0U));
    TEST_ASSERT_EQUAL_UINT32(0U, alerts_update(&a, NULL, 0U));
    TEST_ASSERT_FALSE(alerts_get(NULL, ALERT_HR_HIGH).on);
    TEST_ASSERT_FALSE(alerts_get(&a, ALERT_COUNT).on);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_rider_who_set_nothing_is_never_interrupted);
    RUN_TEST(test_an_alert_with_no_value_stays_off);
    RUN_TEST(test_an_alert_can_be_turned_off_again);
    RUN_TEST(test_going_over_the_limit_says_so_once);
    RUN_TEST(test_the_limit_itself_is_not_over_it);
    RUN_TEST(test_coming_back_to_the_line_is_not_coming_back_inside);
    RUN_TEST(test_even_re_armed_it_waits_a_minute);
    RUN_TEST(test_dropping_under_the_limit_says_so);
    RUN_TEST(test_a_sensor_that_is_not_there_does_not_set_off_a_low_alert);
    RUN_TEST(test_the_low_alert_re_arms_above_the_limit_by_the_margin);
    RUN_TEST(test_power_uses_a_margin_of_fifteen_watts);
    RUN_TEST(test_speed_is_in_tenths_of_a_kilometre);
    RUN_TEST(test_a_limit_lower_than_its_own_margin);
    RUN_TEST(test_every_ten_kilometres);
    RUN_TEST(test_a_long_gap_that_passes_two_marks_fires_once);
    RUN_TEST(test_the_interval_alerts_count_moving_time_in_minutes);
    RUN_TEST(test_the_interval_alerts_do_not_count_a_stop);
    RUN_TEST(test_more_than_one_can_go_off_in_the_same_epoch);
    RUN_TEST(test_changing_a_limit_gives_the_rider_a_clean_start);
    RUN_TEST(test_a_new_ride_starts_the_alerts_over);
    RUN_TEST(test_the_very_first_epoch_can_fire);
    RUN_TEST(test_nothing_blows_up_without_alerts);

    return UNITY_END();
}
