/**
 * @file test_dfu_state.c
 * @brief Rules and progress of an update over the air (src/model/dfu_state.c)
 *
 * There is nothing to compare with in the legacy: the stravaV10 had no
 * update over the air. What the tests pin down are the rules of this port
 * (`docs/07-radio-ant-ble.md`, seção DFU): no update while the rider is on
 * an activity, none on a low battery off the charger, and a progress that
 * never walks backwards.
 */

#include "unity.h"

#include "model/dfu_state.h"

static struct dfu_state st;

void setUp(void)
{
    dfu_state_init(&st);
}

void tearDown(void) {}

static void test_a_new_state_has_nothing_going_on(void)
{
    TEST_ASSERT_EQUAL_INT(DFU_PHASE_IDLE, st.phase);
    TEST_ASSERT_EQUAL_UINT32(0U, st.total);
    TEST_ASSERT_EQUAL_UINT32(0U, st.written);
    TEST_ASSERT_FALSE(dfu_state_is_busy(&st));
    TEST_ASSERT_EQUAL_UINT8(0U, dfu_state_percent(&st));
}

static void test_an_update_is_refused_during_an_activity(void)
{
    struct dfu_conditions cond = {
        .ride_active = true,
        .usb_present = true,
        .battery_pct = 100U,
    };

    /* a reset in the middle of a ride loses the ride */
    TEST_ASSERT_FALSE(dfu_state_allow(&cond));
}

static void test_an_update_is_refused_on_a_low_battery(void)
{
    struct dfu_conditions cond = {
        .ride_active = false,
        .usb_present = false,
        .battery_pct = DFU_MIN_BATTERY_PCT - 1U,
    };

    TEST_ASSERT_FALSE(dfu_state_allow(&cond));

    cond.battery_pct = DFU_MIN_BATTERY_PCT;
    TEST_ASSERT_TRUE(dfu_state_allow(&cond));
}

static void test_on_the_charger_any_battery_takes_the_update(void)
{
    struct dfu_conditions cond = {
        .ride_active = false,
        .usb_present = true,
        .battery_pct = 0U,
    };

    TEST_ASSERT_TRUE(dfu_state_allow(&cond));
    TEST_ASSERT_FALSE(dfu_state_allow(NULL));
}

static void test_the_progress_follows_the_image(void)
{
    dfu_state_started(&st);
    TEST_ASSERT_TRUE(dfu_state_is_busy(&st));

    dfu_state_progress(&st, 0U, 400000U);
    TEST_ASSERT_EQUAL_UINT8(0U, dfu_state_percent(&st));

    dfu_state_progress(&st, 100000U, 400000U);
    TEST_ASSERT_EQUAL_UINT8(25U, dfu_state_percent(&st));

    dfu_state_progress(&st, 399999U, 400000U);
    TEST_ASSERT_EQUAL_UINT8(99U, dfu_state_percent(&st));
}

static void test_a_chunk_sent_again_does_not_walk_the_bar_backwards(void)
{
    dfu_state_started(&st);
    dfu_state_progress(&st, 200000U, 400000U);
    dfu_state_progress(&st, 100000U, 400000U); /* the client went back */

    TEST_ASSERT_EQUAL_UINT32(200000U, st.written);
    TEST_ASSERT_EQUAL_UINT8(50U, dfu_state_percent(&st));
}

static void test_more_bytes_than_announced_stop_at_the_end(void)
{
    dfu_state_started(&st);
    dfu_state_progress(&st, 500000U, 400000U);

    TEST_ASSERT_EQUAL_UINT32(400000U, st.written);
    TEST_ASSERT_EQUAL_UINT8(100U, dfu_state_percent(&st));
}

static void test_without_a_size_there_is_no_percentage(void)
{
    dfu_state_started(&st);
    dfu_state_progress(&st, 50000U, 0U);

    TEST_ASSERT_EQUAL_UINT32(50000U, st.written);
    TEST_ASSERT_EQUAL_UINT8(0U, dfu_state_percent(&st));

    /* the size comes in a later chunk, as the client counts it */
    dfu_state_progress(&st, 60000U, 120000U);
    TEST_ASSERT_EQUAL_UINT8(50U, dfu_state_percent(&st));
}

static void test_a_chunk_before_the_start_still_counts(void)
{
    /* the transport may hand a chunk before the started event */
    dfu_state_progress(&st, 1000U, 100000U);

    TEST_ASSERT_EQUAL_INT(DFU_PHASE_RUNNING, st.phase);
    TEST_ASSERT_TRUE(dfu_state_is_busy(&st));
}

static void test_the_image_marked_to_boot_is_done(void)
{
    dfu_state_started(&st);
    dfu_state_progress(&st, 300000U, 400000U);
    dfu_state_pending(&st);

    TEST_ASSERT_EQUAL_INT(DFU_PHASE_DONE, st.phase);
    TEST_ASSERT_EQUAL_UINT8(100U, dfu_state_percent(&st));
    TEST_ASSERT_TRUE(dfu_state_is_busy(&st));

    /* the end of the transfer comes after, and does not undo it */
    dfu_state_stopped(&st, false);
    TEST_ASSERT_EQUAL_INT(DFU_PHASE_DONE, st.phase);
}

static void test_a_transfer_that_gave_up_fails(void)
{
    dfu_state_started(&st);
    dfu_state_progress(&st, 10000U, 400000U);
    dfu_state_stopped(&st, false);

    TEST_ASSERT_EQUAL_INT(DFU_PHASE_FAILED, st.phase);
    TEST_ASSERT_FALSE(dfu_state_is_busy(&st));
    TEST_ASSERT_EQUAL_UINT8(0U, dfu_state_percent(&st));

    /* and a new attempt starts from zero */
    dfu_state_started(&st);
    TEST_ASSERT_EQUAL_UINT32(0U, st.written);
    TEST_ASSERT_EQUAL_UINT32(0U, st.total);
}

static void test_nothing_blows_up_without_a_state(void)
{
    dfu_state_init(NULL);
    dfu_state_started(NULL);
    dfu_state_progress(NULL, 1U, 2U);
    dfu_state_pending(NULL);
    dfu_state_stopped(NULL, true);

    TEST_ASSERT_EQUAL_UINT8(0U, dfu_state_percent(NULL));
    TEST_ASSERT_FALSE(dfu_state_is_busy(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_new_state_has_nothing_going_on);
    RUN_TEST(test_an_update_is_refused_during_an_activity);
    RUN_TEST(test_an_update_is_refused_on_a_low_battery);
    RUN_TEST(test_on_the_charger_any_battery_takes_the_update);
    RUN_TEST(test_the_progress_follows_the_image);
    RUN_TEST(test_a_chunk_sent_again_does_not_walk_the_bar_backwards);
    RUN_TEST(test_more_bytes_than_announced_stop_at_the_end);
    RUN_TEST(test_without_a_size_there_is_no_percentage);
    RUN_TEST(test_a_chunk_before_the_start_still_counts);
    RUN_TEST(test_the_image_marked_to_boot_is_done);
    RUN_TEST(test_a_transfer_that_gave_up_fails);
    RUN_TEST(test_nothing_blows_up_without_a_state);

    return UNITY_END();
}
