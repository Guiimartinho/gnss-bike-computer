/**
 * @file test_power_scheduler.c
 * @brief Host tests for model/power_scheduler.c.
 *
 * Legacy rules (legacy/source/scheduling/power_scheduler.cpp): a location
 * processed in CRS or PRC mode, or a trainer update in FEC mode, restarts the
 * idle time; once it is strictly greater than 15 minutes, the device turns
 * off. The shutdown itself is the system state machine's (test_sys_fsm).
 */

#include "unity.h"

#include "model/power_scheduler.h"
#include "host_kernel.h"

#define START_MS    1000
#define LIMIT_MS    (15 * 60 * 1000)

void setUp(void)
{
    host_uptime_set(START_MS);
    power_scheduler_init();
}

void tearDown(void)
{
}

static bool run_at(int64_t since_start_ms)
{
    host_uptime_set(START_MS + since_start_ms);
    return power_scheduler_run();
}

static void test_exactly_15_minutes_idle_keeps_the_device_on(void)
{
    TEST_ASSERT_FALSE(run_at(LIMIT_MS));
}

static void test_past_15_minutes_idle_asks_to_turn_off(void)
{
    TEST_ASSERT_TRUE(run_at(LIMIT_MS + 1));
}

static void test_a_location_restarts_the_idle_time(void)
{
    host_uptime_set(START_MS + (10 * 60 * 1000));
    power_scheduler_ping(POWER_PING_CRS);

    TEST_ASSERT_FALSE(run_at(20 * 60 * 1000));
    TEST_ASSERT_TRUE(run_at((10 * 60 * 1000) + LIMIT_MS + 1));
}

static void test_a_trainer_update_restarts_the_idle_time(void)
{
    host_uptime_set(START_MS + (14 * 60 * 1000));
    power_scheduler_ping(POWER_PING_FEC);

    TEST_ASSERT_FALSE(run_at(LIMIT_MS + 1));
}

static void test_an_unknown_ping_does_not_count_as_activity(void)
{
    host_uptime_set(START_MS + (10 * 60 * 1000));
    power_scheduler_ping((power_ping_t)42);

    TEST_ASSERT_TRUE(run_at(LIMIT_MS + 1));
}

static void test_still_running_it_asks_again_one_limit_later(void)
{
    /* USB power, or a board without a power switch: nothing turned off */
    TEST_ASSERT_TRUE(run_at(LIMIT_MS + 1));
    TEST_ASSERT_FALSE(run_at(LIMIT_MS + 2));
    TEST_ASSERT_FALSE(run_at((2 * LIMIT_MS) + 1));
    TEST_ASSERT_TRUE(run_at((2 * LIMIT_MS) + 2));
}

static void test_the_32_bit_uptime_wrap_is_not_idle_time(void)
{
    /* k_uptime_get_32() wraps after 49.7 days */
    host_uptime_set(0xFFFFF000LL);
    power_scheduler_init();

    host_uptime_set(0xFFFFF000LL + 0x2000LL);
    TEST_ASSERT_FALSE(power_scheduler_run());

    host_uptime_set(0xFFFFF000LL + LIMIT_MS + 1);
    TEST_ASSERT_TRUE(power_scheduler_run());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_exactly_15_minutes_idle_keeps_the_device_on);
    RUN_TEST(test_past_15_minutes_idle_asks_to_turn_off);
    RUN_TEST(test_a_location_restarts_the_idle_time);
    RUN_TEST(test_a_trainer_update_restarts_the_idle_time);
    RUN_TEST(test_an_unknown_ping_does_not_count_as_activity);
    RUN_TEST(test_still_running_it_asks_again_one_limit_later);
    RUN_TEST(test_the_32_bit_uptime_wrap_is_not_idle_time);
    return UNITY_END();
}
