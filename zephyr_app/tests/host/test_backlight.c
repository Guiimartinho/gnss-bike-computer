/**
 * @file test_backlight.c
 * @brief Light of the display (src/svc/ui/backlight.c)
 *
 * The state machine of docs/16 (Luz do display): Off, Temporary for 10 s
 * after a key, Automatic with little ambient light, with hysteresis between
 * 20 and 50 lux. The legacy V3 has no light: there is no legacy oracle, the
 * reference is the diagram of docs/16.
 */

#include "unity.h"

#include "svc/backlight.h"

static backlight_t b;

void setUp(void)
{
    backlight_init(&b, true);
}

void tearDown(void)
{
}

static void test_starts_off_with_enough_light_assumed(void)
{
    TEST_ASSERT_FALSE(backlight_is_on(&b));
    TEST_ASSERT_EQUAL(BACKLIGHT_OFF, b.state);
    TEST_ASSERT_FALSE(b.dark);
}

static void test_a_key_lights_it_for_ten_seconds(void)
{
    backlight_key(&b, 1000U);
    TEST_ASSERT_TRUE(backlight_is_on(&b));
    backlight_tick(&b, 1000U + BACKLIGHT_TEMP_MS - 1U);
    TEST_ASSERT_TRUE(backlight_is_on(&b));
    backlight_tick(&b, 1000U + BACKLIGHT_TEMP_MS);
    TEST_ASSERT_FALSE(backlight_is_on(&b));
}

static void test_a_new_key_restarts_the_ten_seconds(void)
{
    backlight_key(&b, 0U);
    backlight_key(&b, 8000U);
    backlight_tick(&b, 12000U);
    TEST_ASSERT_TRUE(backlight_is_on(&b));
    backlight_tick(&b, 18000U);
    TEST_ASSERT_FALSE(backlight_is_on(&b));
}

static void test_little_light_keeps_it_on_until_the_light_comes_back(void)
{
    backlight_lux(&b, 5.0f);
    TEST_ASSERT_EQUAL(BACKLIGHT_AUTO, b.state);
    backlight_tick(&b, 3600000U);
    TEST_ASSERT_TRUE(backlight_is_on(&b));
    backlight_lux(&b, 60.0f);
    TEST_ASSERT_FALSE(backlight_is_on(&b));
}

static void test_between_the_thresholds_nothing_changes(void)
{
    /* from dark: stays on */
    backlight_lux(&b, 19.9f);
    backlight_lux(&b, 20.0f);
    backlight_lux(&b, 50.0f);
    TEST_ASSERT_EQUAL(BACKLIGHT_AUTO, b.state);
    /* from bright: stays off */
    backlight_lux(&b, 50.1f);
    TEST_ASSERT_EQUAL(BACKLIGHT_OFF, b.state);
    backlight_lux(&b, 20.0f);
    TEST_ASSERT_EQUAL(BACKLIGHT_OFF, b.state);
}

static void test_temporary_turns_automatic_when_it_gets_dark(void)
{
    backlight_key(&b, 0U);
    backlight_lux(&b, 1.0f);
    TEST_ASSERT_EQUAL(BACKLIGHT_AUTO, b.state);
    backlight_tick(&b, BACKLIGHT_TEMP_MS * 3U);
    TEST_ASSERT_TRUE(backlight_is_on(&b));
}

static void test_a_key_in_automatic_keeps_it_automatic(void)
{
    backlight_lux(&b, 1.0f);
    backlight_key(&b, 100U);
    TEST_ASSERT_EQUAL(BACKLIGHT_AUTO, b.state);
}

static void test_turned_off_in_the_menu_nothing_lights_it(void)
{
    backlight_enable(&b, false);
    backlight_key(&b, 0U);
    TEST_ASSERT_FALSE(backlight_is_on(&b));
    backlight_lux(&b, 1.0f);
    TEST_ASSERT_FALSE(backlight_is_on(&b));
    /* the light level is still followed, for when it comes back on */
    TEST_ASSERT_TRUE(b.dark);
}

static void test_turning_off_in_the_menu_puts_it_out_at_once(void)
{
    backlight_key(&b, 0U);
    backlight_enable(&b, false);
    TEST_ASSERT_FALSE(backlight_is_on(&b));
    backlight_lux(&b, 1.0f);
    backlight_enable(&b, true);
    TEST_ASSERT_EQUAL(BACKLIGHT_AUTO, b.state);
    backlight_enable(&b, false);
    TEST_ASSERT_FALSE(backlight_is_on(&b));
}

static void test_turning_on_in_the_light_stays_off_until_a_key(void)
{
    backlight_enable(&b, false);
    backlight_enable(&b, true);
    TEST_ASSERT_FALSE(backlight_is_on(&b));
    backlight_key(&b, 0U);
    TEST_ASSERT_EQUAL(BACKLIGHT_TEMP, b.state);
}

static void test_the_ten_seconds_survive_the_uptime_wrap(void)
{
    uint32_t t = 0xFFFFF000U;

    backlight_key(&b, t);
    backlight_tick(&b, t + 5000U);          /* wraps past zero */
    TEST_ASSERT_TRUE(backlight_is_on(&b));
    backlight_tick(&b, t + BACKLIGHT_TEMP_MS);
    TEST_ASSERT_FALSE(backlight_is_on(&b));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_off_with_enough_light_assumed);
    RUN_TEST(test_a_key_lights_it_for_ten_seconds);
    RUN_TEST(test_a_new_key_restarts_the_ten_seconds);
    RUN_TEST(test_little_light_keeps_it_on_until_the_light_comes_back);
    RUN_TEST(test_between_the_thresholds_nothing_changes);
    RUN_TEST(test_temporary_turns_automatic_when_it_gets_dark);
    RUN_TEST(test_a_key_in_automatic_keeps_it_automatic);
    RUN_TEST(test_turned_off_in_the_menu_nothing_lights_it);
    RUN_TEST(test_turning_off_in_the_menu_puts_it_out_at_once);
    RUN_TEST(test_turning_on_in_the_light_stays_off_until_a_key);
    RUN_TEST(test_the_ten_seconds_survive_the_uptime_wrap);
    return UNITY_END();
}
