/**
 * @file test_battery.c
 * @brief Low and critical battery (src/svc/power/battery.c)
 *
 * New in the port, so the reference is docs/16 (Sistema e energia): the
 * legacy has no low-battery rule. Low at 10 % once, again only after 15 %;
 * critical at 0 % while discharging, never while charging.
 */

#include "unity.h"

#include "svc/battery.h"

static battery_t b;

void setUp(void)
{
    battery_init(&b);
}

void tearDown(void)
{
}

static battery_event_t discharge(uint8_t pct)
{
    battery_reading_t r = {.pct = pct, .avg_ua = -80000, .vbus = false};

    return battery_update(&b, &r);
}

static battery_event_t charge(uint8_t pct, bool vbus)
{
    battery_reading_t r = {.pct = pct, .avg_ua = 300000, .vbus = vbus};

    return battery_update(&b, &r);
}

static void test_a_full_battery_says_nothing(void)
{
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, discharge(80U));
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, discharge(11U));
}

static void test_low_is_said_once_at_10_percent(void)
{
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, discharge(11U));
    TEST_ASSERT_EQUAL(BATTERY_EV_LOW, discharge(10U));
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, discharge(9U));
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, discharge(5U));
}

static void test_low_comes_back_only_after_15_percent(void)
{
    (void)discharge(10U);
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, charge(14U, true));
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, discharge(10U));
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, charge(15U, true));
    TEST_ASSERT_EQUAL(BATTERY_EV_LOW, discharge(10U));
}

static void test_starting_low_says_it_at_once(void)
{
    TEST_ASSERT_EQUAL(BATTERY_EV_LOW, discharge(4U));
}

static void test_critical_at_zero_once(void)
{
    (void)discharge(3U);
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, discharge(1U));
    TEST_ASSERT_EQUAL(BATTERY_EV_CRITICAL, discharge(0U));
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, discharge(0U));
}

static void test_critical_wins_over_low(void)
{
    TEST_ASSERT_EQUAL(BATTERY_EV_CRITICAL, discharge(0U));
}

static void test_never_critical_while_plugged_in(void)
{
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, charge(0U, true));
    /* an empty cell on USB may even draw a little: VBUS still counts */
    battery_reading_t r = {.pct = 0U, .avg_ua = -2000, .vbus = true};

    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, battery_update(&b, &r));
}

static void test_never_critical_while_the_sun_charges(void)
{
    TEST_ASSERT_EQUAL(BATTERY_EV_NONE, charge(0U, false));
}

static void test_unplugged_at_zero_turns_critical(void)
{
    (void)charge(0U, true);
    TEST_ASSERT_EQUAL(BATTERY_EV_CRITICAL, discharge(0U));
}

static void test_charging_rearms_critical(void)
{
    (void)discharge(0U);
    (void)charge(0U, true);
    TEST_ASSERT_EQUAL(BATTERY_EV_CRITICAL, discharge(0U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_full_battery_says_nothing);
    RUN_TEST(test_low_is_said_once_at_10_percent);
    RUN_TEST(test_low_comes_back_only_after_15_percent);
    RUN_TEST(test_starting_low_says_it_at_once);
    RUN_TEST(test_critical_at_zero_once);
    RUN_TEST(test_critical_wins_over_low);
    RUN_TEST(test_never_critical_while_plugged_in);
    RUN_TEST(test_never_critical_while_the_sun_charges);
    RUN_TEST(test_unplugged_at_zero_turns_critical);
    RUN_TEST(test_charging_rearms_critical);
    return UNITY_END();
}
