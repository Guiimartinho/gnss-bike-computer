/**
 * @file test_sys_fsm.c
 * @brief Host tests for svc/power/sys_fsm.c on the Zephyr SMF (lib/smf/smf.c).
 *
 * The system machine of docs/16 (Sistema e energia): Partida, Ligado, MSC,
 * Desligando and Desligado. The automatic power-off keeps the legacy rules
 * (legacy/source/scheduling/power_scheduler.cpp): locations ping it in CRS
 * and PRC (BoucleCRS.cpp:197), trainer data in FEC (BoucleFEC.cpp:83), and
 * 15 min without a ping turn the device off. The shutdown waits for every
 * service to finish its part, 5 s at most, before the power goes.
 */

#include "unity.h"

#include "svc/sys_fsm.h"
#include "host_kernel.h"

#define LIMIT_MS    (15 * 60 * 1000)
#define ALL_SVC     ((1U << APP_SVC_COUNT) - 1U)

static struct sys_fsm fsm;
static enum app_sys_state published[16];
static unsigned int npublished;
static unsigned int power_off_calls;
static bool power_off_vbus;

static void publish(enum app_sys_state state, void *user)
{
    (void)user;
    if (npublished < 16U) {
        published[npublished] = state;
    }
    npublished++;
}

static void power_off(bool vbus, void *user)
{
    (void)user;
    power_off_calls++;
    power_off_vbus = vbus;
}

static const struct sys_fsm_ops ops = {publish, power_off, NULL};

void setUp(void)
{
    npublished = 0U;
    power_off_calls = 0U;
    power_off_vbus = false;
    host_uptime_set(1000);
    sys_fsm_init(&fsm, &ops, ALL_SVC);
}

void tearDown(void)
{
}

static void to_on(void)
{
    sys_fsm_event(&fsm, SYS_EV_READY, 0);
}

static void ack_all(void)
{
    for (int svc = 0; svc < (int)APP_SVC_COUNT; svc++) {
        sys_fsm_event(&fsm, SYS_EV_ACK, svc);
    }
}

static void tick_at(int64_t uptime_ms)
{
    host_uptime_set(uptime_ms);
    sys_fsm_event(&fsm, SYS_EV_TICK, 0);
}

static void test_it_starts_in_partida_and_publishes_it(void)
{
    TEST_ASSERT_EQUAL(APP_SYS_BOOT, sys_fsm_state(&fsm));
    TEST_ASSERT_EQUAL_UINT(1U, npublished);
    TEST_ASSERT_EQUAL(APP_SYS_BOOT, published[0]);
}

static void test_ready_turns_it_on(void)
{
    to_on();

    TEST_ASSERT_EQUAL(APP_SYS_ON, sys_fsm_state(&fsm));
    TEST_ASSERT_EQUAL(APP_SYS_ON, published[npublished - 1U]);
}

static void test_the_menu_shutdown_waits_for_every_service(void)
{
    to_on();
    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_SHUTDOWN);
    TEST_ASSERT_EQUAL(APP_SYS_SHUTDOWN, sys_fsm_state(&fsm));

    for (int svc = 0; svc < (int)APP_SVC_COUNT - 1; svc++) {
        sys_fsm_event(&fsm, SYS_EV_ACK, svc);
    }
    TEST_ASSERT_EQUAL_UINT(0U, power_off_calls);

    sys_fsm_event(&fsm, SYS_EV_ACK, APP_SVC_COUNT - 1);
    TEST_ASSERT_EQUAL(APP_SYS_OFF, sys_fsm_state(&fsm));
    TEST_ASSERT_EQUAL_UINT(1U, power_off_calls);
    TEST_ASSERT_FALSE(power_off_vbus);
}

static void test_a_silent_service_holds_the_shutdown_5_seconds_at_most(void)
{
    to_on();
    host_uptime_set(10000);
    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_SHUTDOWN);

    tick_at(10000 + SYS_SHUTDOWN_TIMEOUT_MS - 1);
    TEST_ASSERT_EQUAL(APP_SYS_SHUTDOWN, sys_fsm_state(&fsm));

    tick_at(10000 + SYS_SHUTDOWN_TIMEOUT_MS);
    TEST_ASSERT_EQUAL(APP_SYS_OFF, sys_fsm_state(&fsm));
    TEST_ASSERT_EQUAL_UINT(1U, power_off_calls);
}

static void test_with_usb_power_it_goes_to_system_off(void)
{
    to_on();
    sys_fsm_event(&fsm, SYS_EV_VBUS, 1);
    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_SHUTDOWN);
    ack_all();

    TEST_ASSERT_TRUE(power_off_vbus);
}

static void test_15_minutes_without_a_location_turn_it_off(void)
{
    to_on();

    tick_at(1000 + LIMIT_MS);
    TEST_ASSERT_EQUAL(APP_SYS_ON, sys_fsm_state(&fsm));

    tick_at(1000 + LIMIT_MS + 1);
    TEST_ASSERT_EQUAL(APP_SYS_SHUTDOWN, sys_fsm_state(&fsm));
}

static void test_a_location_in_crs_keeps_it_on(void)
{
    to_on();
    host_uptime_set(1000 + (10 * 60 * 1000));
    sys_fsm_event(&fsm, SYS_EV_FIX, 0);

    tick_at(1000 + LIMIT_MS + 1);
    TEST_ASSERT_EQUAL(APP_SYS_ON, sys_fsm_state(&fsm));
}

static void test_the_dbg_screen_runs_the_crs_loop_and_pings_too(void)
{
    to_on();
    sys_fsm_event(&fsm, SYS_EV_MODE, APP_MODE_ID_DBG);
    host_uptime_set(1000 + (10 * 60 * 1000));
    sys_fsm_event(&fsm, SYS_EV_FIX, 0);

    tick_at(1000 + LIMIT_MS + 1);
    TEST_ASSERT_EQUAL(APP_SYS_ON, sys_fsm_state(&fsm));
}

static void test_in_fec_only_the_trainer_keeps_it_on(void)
{
    to_on();
    sys_fsm_event(&fsm, SYS_EV_MODE, APP_MODE_ID_FEC);
    host_uptime_set(1000 + (10 * 60 * 1000));
    sys_fsm_event(&fsm, SYS_EV_FIX, 0);
    tick_at(1000 + LIMIT_MS + 1);
    TEST_ASSERT_EQUAL(APP_SYS_SHUTDOWN, sys_fsm_state(&fsm));

    setUp();
    to_on();
    sys_fsm_event(&fsm, SYS_EV_MODE, APP_MODE_ID_FEC);
    host_uptime_set(1000 + (10 * 60 * 1000));
    sys_fsm_event(&fsm, SYS_EV_TRAINER, 0);
    tick_at(1000 + LIMIT_MS + 1);
    TEST_ASSERT_EQUAL(APP_SYS_ON, sys_fsm_state(&fsm));
}

static void test_the_trainer_does_not_count_in_crs(void)
{
    to_on();
    host_uptime_set(1000 + (10 * 60 * 1000));
    sys_fsm_event(&fsm, SYS_EV_TRAINER, 0);

    tick_at(1000 + LIMIT_MS + 1);
    TEST_ASSERT_EQUAL(APP_SYS_SHUTDOWN, sys_fsm_state(&fsm));
}

static void test_a_battery_at_its_end_shuts_down(void)
{
    to_on();
    sys_fsm_event(&fsm, SYS_EV_BATT_CRITICAL, 0);

    TEST_ASSERT_EQUAL(APP_SYS_SHUTDOWN, sys_fsm_state(&fsm));
}

static void test_msc_has_no_automatic_power_off_but_accepts_shutdown(void)
{
    to_on();
    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_MSC);
    TEST_ASSERT_EQUAL(APP_SYS_MSC, sys_fsm_state(&fsm));

    tick_at(1000 + (2 * LIMIT_MS));
    TEST_ASSERT_EQUAL(APP_SYS_MSC, sys_fsm_state(&fsm));

    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_SHUTDOWN);
    TEST_ASSERT_EQUAL(APP_SYS_SHUTDOWN, sys_fsm_state(&fsm));
}

static void test_a_shutdown_during_the_boot_is_taken(void)
{
    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_SHUTDOWN);

    TEST_ASSERT_EQUAL(APP_SYS_SHUTDOWN, sys_fsm_state(&fsm));
}

static void test_commands_during_the_shutdown_do_not_restart_its_clock(void)
{
    to_on();
    host_uptime_set(10000);
    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_SHUTDOWN);

    host_uptime_set(13000);
    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_SHUTDOWN);
    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_MSC);
    TEST_ASSERT_EQUAL(APP_SYS_SHUTDOWN, sys_fsm_state(&fsm));

    tick_at(10000 + SYS_SHUTDOWN_TIMEOUT_MS);
    TEST_ASSERT_EQUAL(APP_SYS_OFF, sys_fsm_state(&fsm));
}

static void test_only_the_services_of_the_mask_are_awaited(void)
{
    sys_fsm_init(&fsm, &ops, (1U << APP_SVC_MODEL) | (1U << APP_SVC_STORAGE));
    to_on();
    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_SHUTDOWN);

    sys_fsm_event(&fsm, SYS_EV_ACK, APP_SVC_GNSS);
    sys_fsm_event(&fsm, SYS_EV_ACK, APP_SVC_MODEL);
    sys_fsm_event(&fsm, SYS_EV_ACK, APP_SVC_MODEL);
    TEST_ASSERT_EQUAL(APP_SYS_SHUTDOWN, sys_fsm_state(&fsm));

    sys_fsm_event(&fsm, SYS_EV_ACK, APP_SVC_STORAGE);
    TEST_ASSERT_EQUAL(APP_SYS_OFF, sys_fsm_state(&fsm));
}

static void test_the_power_goes_once_even_if_the_machine_keeps_running(void)
{
    to_on();
    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_SHUTDOWN);
    ack_all();
    tick_at(100000);
    sys_fsm_event(&fsm, SYS_EV_CMD, APP_CMD_SHUTDOWN);

    TEST_ASSERT_EQUAL_UINT(1U, power_off_calls);
    TEST_ASSERT_EQUAL(APP_SYS_OFF, sys_fsm_state(&fsm));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_it_starts_in_partida_and_publishes_it);
    RUN_TEST(test_ready_turns_it_on);
    RUN_TEST(test_the_menu_shutdown_waits_for_every_service);
    RUN_TEST(test_a_silent_service_holds_the_shutdown_5_seconds_at_most);
    RUN_TEST(test_with_usb_power_it_goes_to_system_off);
    RUN_TEST(test_15_minutes_without_a_location_turn_it_off);
    RUN_TEST(test_a_location_in_crs_keeps_it_on);
    RUN_TEST(test_the_dbg_screen_runs_the_crs_loop_and_pings_too);
    RUN_TEST(test_in_fec_only_the_trainer_keeps_it_on);
    RUN_TEST(test_the_trainer_does_not_count_in_crs);
    RUN_TEST(test_a_battery_at_its_end_shuts_down);
    RUN_TEST(test_msc_has_no_automatic_power_off_but_accepts_shutdown);
    RUN_TEST(test_a_shutdown_during_the_boot_is_taken);
    RUN_TEST(test_commands_during_the_shutdown_do_not_restart_its_clock);
    RUN_TEST(test_only_the_services_of_the_mask_are_awaited);
    RUN_TEST(test_the_power_goes_once_even_if_the_machine_keeps_running);
    return UNITY_END();
}
