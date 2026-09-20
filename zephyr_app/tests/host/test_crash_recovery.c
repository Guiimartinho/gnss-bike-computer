/**
 * @file test_crash_recovery.c
 * @brief Recovery of a ride after a reset (src/model/crash_recovery.c)
 *
 * The FDIR of the legacy (`legacy/source/model/Attitude.cpp:393-417` and
 * `:480-481`) keeps the ride in memory that the reset does not clear,
 * protected by a CRC-8, and takes it back when the sea level reference
 * appears, only if the block carries the same date. Then it clears the
 * block, so one reset restores once.
 *
 * Two defects of the port died here: the CRC covered its own field, so it
 * never matched what it protected, and `crash_recovery_has_data()` asked
 * for a logged fault, which a reset by the watchdog or a flat battery does
 * not leave.
 */

#include <stddef.h>

#include "unity.h"

#include "model/crash_recovery.h"

static loc_data_t loc;
static date_data_t date;

void setUp(void)
{
    (void)crash_recovery_init();
    crash_recovery_clear_saved_state();

    loc = (loc_data_t){.lat = 48.6921f, .lon = 6.1844f, .alt = 202.0f, .speed = 28.0f};
    date = (date_data_t){.date = 190926U, .secj = 52507U};
}

void tearDown(void) {}

static void test_a_fresh_start_has_nothing_to_restore(void)
{
    TEST_ASSERT_FALSE(crash_recovery_has_data());
}

static void test_a_saved_ride_comes_back_whole(void)
{
    crash_recovery_save_state(&loc, &date, 12345.5f, 678.25f, 4321U, 2500U, 1U);

    TEST_ASSERT_TRUE(crash_recovery_has_data());

    saved_data_t saved;

    TEST_ASSERT_TRUE(crash_recovery_get_saved_state(&saved));
    TEST_ASSERT_EQUAL_FLOAT(12345.5f, saved.dist);
    TEST_ASSERT_EQUAL_FLOAT(678.25f, saved.climb);
    TEST_ASSERT_EQUAL_UINT16(4321U, saved.nbpts);
    TEST_ASSERT_EQUAL_UINT16(2500U, saved.nbsec_act);
    TEST_ASSERT_EQUAL_UINT8(1U, saved.pr);
    TEST_ASSERT_EQUAL_UINT32(190926U, saved.date.date);
    TEST_ASSERT_EQUAL_FLOAT(48.6921f, saved.loc.lat);
}

static void test_a_saved_ride_does_not_need_a_logged_fault(void)
{
    /* a reset by the watchdog leaves no fault behind, and the ride must survive */
    crash_recovery_save_state(&loc, &date, 900.0f, 30.0f, 100U, 60U, 0U);
    crash_recovery_clear(); /* clears the fault, not the ride */

    TEST_ASSERT_TRUE(crash_recovery_has_data());
}

static void test_a_changed_byte_breaks_the_check(void)
{
    crash_recovery_save_state(&loc, &date, 900.0f, 30.0f, 100U, 60U, 0U);

    saved_data_t saved;

    TEST_ASSERT_TRUE(crash_recovery_get_saved_state(&saved));

    /* the same block with another distance no longer matches its CRC */
    saved.dist = 1000.0f;
    TEST_ASSERT_NOT_EQUAL_UINT8(saved.crc,
                                crash_recovery_crc8((const uint8_t *)&saved,
                                                    offsetof(saved_data_t, crc)));
}

static void test_clearing_the_block_leaves_nothing_to_restore(void)
{
    crash_recovery_save_state(&loc, &date, 900.0f, 30.0f, 100U, 60U, 0U);
    crash_recovery_clear_saved_state();

    /* a block of zeros has a CRC of zero: the sentinel has to say no */
    TEST_ASSERT_FALSE(crash_recovery_has_data());
}

static void test_saving_twice_keeps_the_last_ride(void)
{
    crash_recovery_save_state(&loc, &date, 100.0f, 10.0f, 10U, 5U, 0U);
    crash_recovery_save_state(&loc, &date, 200.0f, 20.0f, 20U, 10U, 0U);

    saved_data_t saved;

    TEST_ASSERT_TRUE(crash_recovery_get_saved_state(&saved));
    TEST_ASSERT_EQUAL_FLOAT(200.0f, saved.dist);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_fresh_start_has_nothing_to_restore);
    RUN_TEST(test_a_saved_ride_comes_back_whole);
    RUN_TEST(test_a_saved_ride_does_not_need_a_logged_fault);
    RUN_TEST(test_a_changed_byte_breaks_the_check);
    RUN_TEST(test_clearing_the_block_leaves_nothing_to_restore);
    RUN_TEST(test_saving_twice_keeps_the_last_ride);

    return UNITY_END();
}
