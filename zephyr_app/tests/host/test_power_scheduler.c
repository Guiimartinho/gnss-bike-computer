/**
 * @file test_power_scheduler.c
 * @brief Host tests for model/power_scheduler.c.
 *
 * Legacy rules (legacy/source/scheduling/power_scheduler.cpp): a location
 * processed in CRS or PRC mode, or a trainer update in FEC mode, restarts the
 * idle time; once it is strictly greater than 15 minutes, the saved activity
 * is cleared and the STC3100 releases the power latch.
 */

#include "unity.h"

#include "model/power_scheduler.h"
#include "fake_shutdown.h"
#include "host_kernel.h"

#define START_MS    1000
#define LIMIT_MS    (15 * 60 * 1000)

void setUp(void)
{
    fake_shutdown_reset();
    host_uptime_set(START_MS);
    power_scheduler_init();
}

void tearDown(void)
{
}

static void run_at(int64_t since_start_ms)
{
    host_uptime_set(START_MS + since_start_ms);
    power_scheduler_run();
}

static void test_exactly_15_minutes_idle_keeps_the_device_on(void)
{
    run_at(LIMIT_MS);

    TEST_ASSERT_EQUAL_UINT(0U, fake_shutdown_latch_calls());
}

static void test_past_15_minutes_idle_clears_the_activity_and_opens_the_latch(void)
{
    run_at(LIMIT_MS + 1);

    TEST_ASSERT_EQUAL_UINT(1U, fake_shutdown_latch_calls());
    TEST_ASSERT_EQUAL_UINT(1U, fake_shutdown_clear_calls());
    TEST_ASSERT_TRUE(fake_shutdown_cleared_first());
}

static void test_a_location_restarts_the_idle_time(void)
{
    host_uptime_set(START_MS + (10 * 60 * 1000));
    power_scheduler_ping(POWER_PING_CRS);

    run_at(20 * 60 * 1000);
    TEST_ASSERT_EQUAL_UINT(0U, fake_shutdown_latch_calls());

    run_at((10 * 60 * 1000) + LIMIT_MS + 1);
    TEST_ASSERT_EQUAL_UINT(1U, fake_shutdown_latch_calls());
}

static void test_a_trainer_update_restarts_the_idle_time(void)
{
    host_uptime_set(START_MS + (14 * 60 * 1000));
    power_scheduler_ping(POWER_PING_FEC);

    run_at(LIMIT_MS + 1);
    TEST_ASSERT_EQUAL_UINT(0U, fake_shutdown_latch_calls());
}

static void test_an_unknown_ping_does_not_count_as_activity(void)
{
    host_uptime_set(START_MS + (10 * 60 * 1000));
    power_scheduler_ping((power_ping_t)42);

    run_at(LIMIT_MS + 1);
    TEST_ASSERT_EQUAL_UINT(1U, fake_shutdown_latch_calls());
}

static void test_still_powered_it_tries_again_one_limit_later(void)
{
    /* USB power, or a board without the latch: nothing turns off */
    run_at(LIMIT_MS + 1);
    run_at(LIMIT_MS + 2);
    TEST_ASSERT_EQUAL_UINT(1U, fake_shutdown_latch_calls());

    run_at((2 * LIMIT_MS) + 2);
    TEST_ASSERT_EQUAL_UINT(2U, fake_shutdown_latch_calls());
}

static void test_a_failed_latch_write_is_retried_later_too(void)
{
    fake_shutdown_set_latch_result(APP_ERR_IO);

    run_at(LIMIT_MS + 1);
    run_at(LIMIT_MS + 100);
    TEST_ASSERT_EQUAL_UINT(1U, fake_shutdown_latch_calls());

    run_at((2 * LIMIT_MS) + 2);
    TEST_ASSERT_EQUAL_UINT(2U, fake_shutdown_latch_calls());
}

static void test_the_32_bit_uptime_wrap_is_not_idle_time(void)
{
    /* k_uptime_get_32() wraps after 49.7 days */
    host_uptime_set(0xFFFFF000LL);
    power_scheduler_init();

    host_uptime_set(0xFFFFF000LL + 0x2000LL);
    power_scheduler_run();
    TEST_ASSERT_EQUAL_UINT(0U, fake_shutdown_latch_calls());

    host_uptime_set(0xFFFFF000LL + LIMIT_MS + 1);
    power_scheduler_run();
    TEST_ASSERT_EQUAL_UINT(1U, fake_shutdown_latch_calls());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_exactly_15_minutes_idle_keeps_the_device_on);
    RUN_TEST(test_past_15_minutes_idle_clears_the_activity_and_opens_the_latch);
    RUN_TEST(test_a_location_restarts_the_idle_time);
    RUN_TEST(test_a_trainer_update_restarts_the_idle_time);
    RUN_TEST(test_an_unknown_ping_does_not_count_as_activity);
    RUN_TEST(test_still_powered_it_tries_again_one_limit_later);
    RUN_TEST(test_a_failed_latch_write_is_retried_later_too);
    RUN_TEST(test_the_32_bit_uptime_wrap_is_not_idle_time);
    return UNITY_END();
}
