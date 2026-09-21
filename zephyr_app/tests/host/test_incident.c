/**
 * @file test_incident.c
 * @brief Bike alarm and crash detection (src/model/incident.c)
 *
 * The legacy has neither. What these tests hold up is mostly the *no*
 * side: a kerb is not a crash, a pothole is not a crash, a bike that
 * keeps rolling is not a crash, and a rider who presses a key is fine.
 * A detector that cries wolf is worse than none, because the rider turns
 * it off and then it is not there on the day it matters.
 */

#include <math.h>

#include "unity.h"

#include "model/incident.h"

#define SEC     1000U

static struct incident in;

void setUp(void)
{
    incident_init(&in, true);
}

void tearDown(void) {}

/** One second: peak acceleration, how still it is, and the speed */
static enum incident_event sec(float peak_g, float still_g, float kmh)
{
    struct incident_sample s = {.peak_g = peak_g, .still_g = still_g, .speed_kmh = kmh};

    return incident_update(&in, &s, SEC);
}

/** Seconds of ordinary riding */
static void riding(unsigned int n)
{
    for (unsigned int i = 0U; i < n; i++) {
        TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, sec(1.4f, 0.4f, 28.0f));
    }
}

/** Seconds of lying still, stopped */
static enum incident_event still(unsigned int n)
{
    enum incident_event ev = INCIDENT_EVENT_NONE;

    for (unsigned int i = 0U; i < n; i++) {
        enum incident_event e = sec(1.02f, 0.02f, 0.0f);

        if (e != INCIDENT_EVENT_NONE) {
            ev = e;
        }
    }

    return ev;
}

static void test_riding_along_is_not_an_incident(void)
{
    riding(600U);

    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
    TEST_ASSERT_FALSE(incident_is_armed(&in));
    TEST_ASSERT_EQUAL_UINT32(0U, incident_countdown_s(&in));
}

static void test_a_kerb_is_not_a_crash(void)
{
    /* a good bang, and the rider carries straight on */
    riding(10U);
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, sec(9.0f, 0.8f, 26.0f));
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_SHAKEN, incident_state(&in));

    /* still rolling: the question closes itself when the window runs out */
    unsigned int cleared = 0U;

    for (unsigned int i = 0U; i < 20U; i++) {
        if (sec(1.4f, 0.4f, 26.0f) == INCIDENT_EVENT_CLEARED) {
            cleared++;
        }
    }
    TEST_ASSERT_EQUAL_UINT(1U, cleared);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
}

static void test_stopping_at_a_light_is_not_a_crash(void)
{
    /* stopped and still for a long time, but nothing shook the bike */
    riding(10U);
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, still(120U));
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
    TEST_ASSERT_EQUAL_UINT32(0U, incident_countdown_s(&in));
}

static void test_a_fall_starts_a_countdown(void)
{
    riding(10U);
    /* the impact */
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, sec(11.0f, 2.0f, 24.0f));
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_SHAKEN, incident_state(&in));

    /* and then nothing moves */
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_COUNTING, still(INCIDENT_CRASH_STILL_MS / SEC));
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_COUNTING, incident_state(&in));
    TEST_ASSERT_UINT32_WITHIN(1U, INCIDENT_CRASH_COUNT_MS / SEC, incident_countdown_s(&in));
}

static void test_the_countdown_runs_out_into_a_crash(void)
{
    riding(5U);
    (void)sec(11.0f, 2.0f, 24.0f);
    (void)still(INCIDENT_CRASH_STILL_MS / SEC);

    enum incident_event ev = still(INCIDENT_CRASH_COUNT_MS / SEC);

    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_CRASH, ev);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_CRASHED, incident_state(&in));
    TEST_ASSERT_EQUAL_UINT32(0U, incident_countdown_s(&in));
}

static void test_the_countdown_ticks_down_to_zero(void)
{
    riding(5U);
    (void)sec(11.0f, 2.0f, 24.0f);
    (void)still(INCIDENT_CRASH_STILL_MS / SEC);

    uint32_t before = incident_countdown_s(&in);

    (void)still(10U);
    TEST_ASSERT_UINT32_WITHIN(1U, before - 10U, incident_countdown_s(&in));
}

static void test_a_key_cancels_the_countdown(void)
{
    riding(5U);
    (void)sec(11.0f, 2.0f, 24.0f);
    (void)still(INCIDENT_CRASH_STILL_MS / SEC);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_COUNTING, incident_state(&in));

    /* the rider is sitting on the kerb, annoyed but fine */
    incident_cancel(&in);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
    TEST_ASSERT_EQUAL_UINT32(0U, incident_countdown_s(&in));

    /* and it does not come straight back */
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, still(60U));
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
}

static void test_riding_off_cancels_the_countdown(void)
{
    riding(5U);
    (void)sec(11.0f, 2.0f, 24.0f);
    (void)still(INCIDENT_CRASH_STILL_MS / SEC);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_COUNTING, incident_state(&in));

    /* the rider got back on: that is answer enough */
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_CLEARED, sec(1.3f, 0.3f, 18.0f));
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
}

static void test_a_shake_the_bike_walks_away_from_is_forgotten(void)
{
    riding(5U);
    (void)sec(11.0f, 2.0f, 24.0f);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_SHAKEN, incident_state(&in));

    /* stopped but not still: someone is picking the bike up */
    for (unsigned int i = 0U; i < (INCIDENT_CRASH_WINDOW_MS / SEC) + 1U; i++) {
        (void)sec(1.6f, 0.6f, 0.0f);
    }
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
}

static void test_crash_detection_turned_off_detects_nothing(void)
{
    incident_init(&in, false);
    riding(5U);
    (void)sec(15.0f, 3.0f, 30.0f);
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, still(120U));
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
}

/* ==========================================================================
 * The alarm
 * ========================================================================== */

static void test_arming_waits_for_the_rider_to_walk_away(void)
{
    incident_arm(&in, true);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_SETTLING, incident_state(&in));
    TEST_ASSERT_TRUE(incident_is_armed(&in));

    /* the rider is still fiddling with the lock: nothing rings */
    for (unsigned int i = 0U; i < (INCIDENT_ARM_SETTLE_MS / SEC); i++) {
        TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, sec(1.5f, 0.5f, 0.0f));
    }
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_ARMED, incident_state(&in));
}

static void test_the_bike_being_moved_rings_the_alarm(void)
{
    incident_arm(&in, true);
    (void)still(INCIDENT_ARM_SETTLE_MS / SEC);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_ARMED, incident_state(&in));

    /* someone takes hold of it */
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_ALARM, sec(1.4f, 0.4f, 0.0f));
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_RINGING, incident_state(&in));
}

static void test_wind_does_not_ring_the_alarm(void)
{
    incident_arm(&in, true);
    (void)still(INCIDENT_ARM_SETTLE_MS / SEC);

    /* a nudge under the threshold, over and over */
    for (unsigned int i = 0U; i < 300U; i++) {
        TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, sec(1.1f, 0.10f, 0.0f));
    }
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_ARMED, incident_state(&in));
}

static void test_a_key_silences_and_disarms_the_alarm(void)
{
    incident_arm(&in, true);
    (void)still(INCIDENT_ARM_SETTLE_MS / SEC);
    (void)sec(1.4f, 0.4f, 0.0f);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_RINGING, incident_state(&in));

    incident_cancel(&in);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
    TEST_ASSERT_FALSE(incident_is_armed(&in));
    /* and it stays off, because the owner is the one who came back */
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, sec(2.0f, 1.0f, 0.0f));
}

static void test_disarming_by_hand_stops_the_watching(void)
{
    incident_arm(&in, true);
    (void)still(INCIDENT_ARM_SETTLE_MS / SEC);
    incident_arm(&in, false);

    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, sec(1.4f, 0.4f, 0.0f));
}

static void test_a_crash_while_the_alarm_is_armed_wins(void)
{
    /* the bike is parked, armed, and a car hits it */
    incident_arm(&in, true);
    (void)still(INCIDENT_ARM_SETTLE_MS / SEC);
    (void)sec(12.0f, 3.0f, 0.0f);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_SHAKEN, incident_state(&in));

    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_COUNTING, still(INCIDENT_CRASH_STILL_MS / SEC));
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_COUNTING, incident_state(&in));
}

static void test_the_alarm_comes_back_after_a_shake_that_was_nothing(void)
{
    incident_arm(&in, true);
    (void)still(INCIDENT_ARM_SETTLE_MS / SEC);
    (void)sec(12.0f, 3.0f, 0.0f);

    /* someone shook it and walked off: back to watching, still armed */
    for (unsigned int i = 0U; i < (INCIDENT_CRASH_WINDOW_MS / SEC) + 1U; i++) {
        (void)sec(1.5f, 0.5f, 0.0f);
    }
    TEST_ASSERT_TRUE(incident_is_armed(&in));
}

static void test_turning_crash_detection_off_mid_countdown_drops_it(void)
{
    riding(5U);
    (void)sec(11.0f, 2.0f, 24.0f);
    (void)still(INCIDENT_CRASH_STILL_MS / SEC);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_COUNTING, incident_state(&in));

    incident_set_crash(&in, false);
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
}

static void test_a_sample_with_no_numbers_does_not_start_anything(void)
{
    /* the accelerometer was not read: NAN must not look like a crash */
    riding(5U);
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, sec(NAN, NAN, NAN));
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(&in));
}

static void test_nothing_blows_up_without_a_machine(void)
{
    struct incident_sample s = {0};

    incident_init(NULL, true);
    incident_arm(NULL, true);
    incident_set_crash(NULL, true);
    incident_cancel(NULL);
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, incident_update(NULL, &s, SEC));
    TEST_ASSERT_EQUAL_INT(INCIDENT_EVENT_NONE, incident_update(&in, NULL, SEC));
    TEST_ASSERT_EQUAL_UINT32(0U, incident_countdown_s(NULL));
    TEST_ASSERT_EQUAL_UINT8(INCIDENT_OFF, incident_state(NULL));
    TEST_ASSERT_FALSE(incident_is_armed(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_riding_along_is_not_an_incident);
    RUN_TEST(test_a_kerb_is_not_a_crash);
    RUN_TEST(test_stopping_at_a_light_is_not_a_crash);
    RUN_TEST(test_a_fall_starts_a_countdown);
    RUN_TEST(test_the_countdown_runs_out_into_a_crash);
    RUN_TEST(test_the_countdown_ticks_down_to_zero);
    RUN_TEST(test_a_key_cancels_the_countdown);
    RUN_TEST(test_riding_off_cancels_the_countdown);
    RUN_TEST(test_a_shake_the_bike_walks_away_from_is_forgotten);
    RUN_TEST(test_crash_detection_turned_off_detects_nothing);
    RUN_TEST(test_arming_waits_for_the_rider_to_walk_away);
    RUN_TEST(test_the_bike_being_moved_rings_the_alarm);
    RUN_TEST(test_wind_does_not_ring_the_alarm);
    RUN_TEST(test_a_key_silences_and_disarms_the_alarm);
    RUN_TEST(test_disarming_by_hand_stops_the_watching);
    RUN_TEST(test_a_crash_while_the_alarm_is_armed_wins);
    RUN_TEST(test_the_alarm_comes_back_after_a_shake_that_was_nothing);
    RUN_TEST(test_turning_crash_detection_off_mid_countdown_drops_it);
    RUN_TEST(test_a_sample_with_no_numbers_does_not_start_anything);
    RUN_TEST(test_nothing_blows_up_without_a_machine);

    return UNITY_END();
}
