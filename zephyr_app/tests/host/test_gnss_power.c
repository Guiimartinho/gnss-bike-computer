/**
 * @file test_gnss_power.c
 * @brief Power state machine of the GNSS receiver (src/svc/gnss/gnss_power.c)
 *
 * The machine of docs/16-arquitetura-firmware.md (GNSS): backup outside the
 * modes that use the receiver, acquiring until the first fix, LEAP while
 * tracking, full power when the signal goes away for
 * GNSS_POWER_WEAK_EPOCHS epochs, and back to LEAP after GNSS_POWER_GOOD_MS
 * with a fix. The legacy did the same with the standby pin of the MT3333
 * (`legacy/source/sensors/GPSMGMT.cpp:195`), woken in CRS and PRC
 * (`legacy/source/model/BoucleCRS.cpp:38`).
 *
 * The receiver of the new board, the MAX-F10S, has no low power tracking
 * mode: the last tests here cover the machine with has_leap false, where the
 * shape is the same but no power mode is ever asked for.
 */

#include "unity.h"

#include "app/app_events.h"
#include "svc/gnss_power.h"

#define EPOCH_MS    1000U

static struct gnss_power p;

void setUp(void)
{
    gnss_power_init(&p, true);
}

void tearDown(void) {}

/** Take the machine to the tracking state in LEAP, as after a first fix */
static void reach_leap(void)
{
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_WAKE, gnss_power_mode_change(&p, true));
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, true, EPOCH_MS));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_LEAP, gnss_power_mode(&p));
}

static void test_starts_in_backup(void)
{
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_BACKUP, gnss_power_mode(&p));
}

static void test_first_mode_always_acts_because_the_receiver_is_on(void)
{
    /* the device boots with the receiver powered and unconfigured */
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_STANDBY, gnss_power_mode_change(&p, false));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_BACKUP, gnss_power_mode(&p));

    gnss_power_init(&p, true);
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_WAKE, gnss_power_mode_change(&p, true));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_ACQ, gnss_power_mode(&p));
}

static void test_a_mode_that_does_not_change_the_receiver_asks_nothing(void)
{
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_WAKE, gnss_power_mode_change(&p, true));
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_mode_change(&p, true));

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_STANDBY, gnss_power_mode_change(&p, false));
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_mode_change(&p, false));
}

static void test_the_first_fix_leaves_the_acquisition(void)
{
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_WAKE, gnss_power_mode_change(&p, true));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_ACQ, gnss_power_mode(&p));

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, false, EPOCH_MS));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_ACQ, gnss_power_mode(&p));

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, true, EPOCH_MS));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_LEAP, gnss_power_mode(&p));
}

static void test_a_short_gap_does_not_change_the_mode(void)
{
    reach_leap();
    for (uint8_t i = 0U; i < (GNSS_POWER_WEAK_EPOCHS - 1U); i++) {
        TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, false, EPOCH_MS));
        TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_LEAP, gnss_power_mode(&p));
    }
}

static void test_a_long_gap_asks_for_full_power_once(void)
{
    reach_leap();
    for (uint8_t i = 0U; i < (GNSS_POWER_WEAK_EPOCHS - 1U); i++) {
        (void)gnss_power_epoch(&p, false, EPOCH_MS);
    }

    /* the tenth epoch without a fix */
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_FULL, gnss_power_epoch(&p, false, EPOCH_MS));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_ACQ, gnss_power_mode(&p));

    /* and it does not ask again while it stays without a fix */
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, false, EPOCH_MS));
}

static void test_a_fix_at_full_power_shows_full_power(void)
{
    reach_leap();
    for (uint8_t i = 0U; i < GNSS_POWER_WEAK_EPOCHS; i++) {
        (void)gnss_power_epoch(&p, false, EPOCH_MS);
    }

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, true, EPOCH_MS));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_FULL, gnss_power_mode(&p));
}

static void test_a_minute_of_fix_goes_back_to_leap(void)
{
    reach_leap();
    for (uint8_t i = 0U; i < GNSS_POWER_WEAK_EPOCHS; i++) {
        (void)gnss_power_epoch(&p, false, EPOCH_MS);
    }

    uint32_t epochs = GNSS_POWER_GOOD_MS / EPOCH_MS;

    for (uint32_t i = 0U; i < (epochs - 1U); i++) {
        TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, true, EPOCH_MS));
        TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_FULL, gnss_power_mode(&p));
    }

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_LEAP, gnss_power_epoch(&p, true, EPOCH_MS));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_LEAP, gnss_power_mode(&p));
}

static void test_a_gap_in_the_middle_restarts_the_count_of_good_time(void)
{
    reach_leap();
    for (uint8_t i = 0U; i < GNSS_POWER_WEAK_EPOCHS; i++) {
        (void)gnss_power_epoch(&p, false, EPOCH_MS);
    }

    uint32_t epochs = GNSS_POWER_GOOD_MS / EPOCH_MS;

    for (uint32_t i = 0U; i < (epochs - 1U); i++) {
        (void)gnss_power_epoch(&p, true, EPOCH_MS);
    }
    (void)gnss_power_epoch(&p, false, EPOCH_MS); /* one epoch without a fix */

    /* the minute starts again: the next fix is not enough */
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, true, EPOCH_MS));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_FULL, gnss_power_mode(&p));
}

static void test_leaving_the_mode_puts_the_receiver_down(void)
{
    reach_leap();
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_STANDBY, gnss_power_mode_change(&p, false));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_BACKUP, gnss_power_mode(&p));

    /* an epoch from before the standby changes nothing */
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, true, EPOCH_MS));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_BACKUP, gnss_power_mode(&p));
}

static void test_waking_up_starts_from_the_acquisition_in_low_power(void)
{
    reach_leap();
    for (uint8_t i = 0U; i < GNSS_POWER_WEAK_EPOCHS; i++) {
        (void)gnss_power_epoch(&p, false, EPOCH_MS); /* full power */
    }
    (void)gnss_power_mode_change(&p, false);

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_WAKE, gnss_power_mode_change(&p, true));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_ACQ, gnss_power_mode(&p));
    /* the receiver came back in the configuration of the devicetree, LEAP */
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, true, EPOCH_MS));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_LEAP, gnss_power_mode(&p));
}

static void test_silence_of_the_receiver_asks_for_the_configuration_again(void)
{
    reach_leap();

    /* nothing arrives: the machine counts the silence */
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE,
                          gnss_power_tick(&p, GNSS_POWER_SILENCE_MS - 1U));
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_CONFIGURE, gnss_power_tick(&p, 1U));

    /* and it does not ask again before the reset threshold */
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_tick(&p, 1000U));
}

static void test_a_receiver_that_never_comes_back_gets_a_reset(void)
{
    reach_leap();
    (void)gnss_power_tick(&p, GNSS_POWER_SILENCE_MS);

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE,
                          gnss_power_tick(&p, GNSS_POWER_RESET_MS - GNSS_POWER_SILENCE_MS - 1U));
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_RESET, gnss_power_tick(&p, 1U));

    /* after the reset the count starts again, and the cycle repeats */
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_CONFIGURE,
                          gnss_power_tick(&p, GNSS_POWER_SILENCE_MS));
}

static void test_an_epoch_clears_the_silence(void)
{
    reach_leap();
    (void)gnss_power_tick(&p, GNSS_POWER_SILENCE_MS - 1U);
    (void)gnss_power_epoch(&p, true, EPOCH_MS);

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE,
                          gnss_power_tick(&p, GNSS_POWER_SILENCE_MS - 1U));
}

static void test_a_receiver_in_backup_is_not_expected_to_talk(void)
{
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_STANDBY, gnss_power_mode_change(&p, false));
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_tick(&p, GNSS_POWER_RESET_MS * 2U));
}

/*
 * A receiver without LEAP: the MAX-F10S has no CFG-PM group at all (u-blox
 * F10 SPG 6.00 interface description UBX-23002975 R02, 4.8), so the machine
 * must never ask for a power mode. What saves energy there is the standby
 * between modes, which stays the same.
 */

static void test_without_leap_tracking_already_shows_full_power(void)
{
    gnss_power_init(&p, false);

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_WAKE, gnss_power_mode_change(&p, true));
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, true, EPOCH_MS));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_FULL, gnss_power_mode(&p));
}

static void test_without_leap_a_long_gap_asks_for_no_mode(void)
{
    gnss_power_init(&p, false);
    (void)gnss_power_mode_change(&p, true);
    (void)gnss_power_epoch(&p, true, EPOCH_MS);

    for (unsigned int i = 0U; i < (GNSS_POWER_WEAK_EPOCHS + 5U); i++) {
        TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, false, EPOCH_MS));
    }
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_ACQ, gnss_power_mode(&p));
}

static void test_without_leap_a_minute_of_fix_asks_for_no_mode(void)
{
    gnss_power_init(&p, false);
    (void)gnss_power_mode_change(&p, true);

    /* the machine that has LEAP would come back with ACTION_LEAP here */
    for (unsigned int ms = 0U; ms <= (GNSS_POWER_GOOD_MS + (2U * EPOCH_MS)); ms += EPOCH_MS) {
        TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, true, EPOCH_MS));
    }
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_FULL, gnss_power_mode(&p));
}

static void test_without_leap_the_receiver_still_goes_down_and_comes_back(void)
{
    gnss_power_init(&p, false);
    (void)gnss_power_mode_change(&p, true);
    (void)gnss_power_epoch(&p, true, EPOCH_MS);

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_STANDBY, gnss_power_mode_change(&p, false));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_BACKUP, gnss_power_mode(&p));

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_WAKE, gnss_power_mode_change(&p, true));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_ACQ, gnss_power_mode(&p));
    /* and the wake-up does not reset it into a mode it does not have */
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(&p, true, EPOCH_MS));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_FULL, gnss_power_mode(&p));
}

static void test_without_leap_silence_still_asks_for_the_configuration(void)
{
    gnss_power_init(&p, false);
    (void)gnss_power_mode_change(&p, true);
    (void)gnss_power_epoch(&p, true, EPOCH_MS);

    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_CONFIGURE,
                          gnss_power_tick(&p, GNSS_POWER_SILENCE_MS));
}

static void test_a_null_machine_does_nothing(void)
{
    gnss_power_init(NULL, true);
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_mode_change(NULL, true));
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_epoch(NULL, true, EPOCH_MS));
    TEST_ASSERT_EQUAL_INT(GNSS_POWER_ACTION_NONE, gnss_power_tick(NULL, 1000U));
    TEST_ASSERT_EQUAL_UINT8(APP_GNSS_MODE_BACKUP, gnss_power_mode(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_in_backup);
    RUN_TEST(test_first_mode_always_acts_because_the_receiver_is_on);
    RUN_TEST(test_a_mode_that_does_not_change_the_receiver_asks_nothing);
    RUN_TEST(test_the_first_fix_leaves_the_acquisition);
    RUN_TEST(test_a_short_gap_does_not_change_the_mode);
    RUN_TEST(test_a_long_gap_asks_for_full_power_once);
    RUN_TEST(test_a_fix_at_full_power_shows_full_power);
    RUN_TEST(test_a_minute_of_fix_goes_back_to_leap);
    RUN_TEST(test_a_gap_in_the_middle_restarts_the_count_of_good_time);
    RUN_TEST(test_leaving_the_mode_puts_the_receiver_down);
    RUN_TEST(test_waking_up_starts_from_the_acquisition_in_low_power);
    RUN_TEST(test_silence_of_the_receiver_asks_for_the_configuration_again);
    RUN_TEST(test_a_receiver_that_never_comes_back_gets_a_reset);
    RUN_TEST(test_an_epoch_clears_the_silence);
    RUN_TEST(test_a_receiver_in_backup_is_not_expected_to_talk);
    RUN_TEST(test_without_leap_tracking_already_shows_full_power);
    RUN_TEST(test_without_leap_a_long_gap_asks_for_no_mode);
    RUN_TEST(test_without_leap_a_minute_of_fix_asks_for_no_mode);
    RUN_TEST(test_without_leap_the_receiver_still_goes_down_and_comes_back);
    RUN_TEST(test_without_leap_silence_still_asks_for_the_configuration);
    RUN_TEST(test_a_null_machine_does_nothing);

    return UNITY_END();
}
