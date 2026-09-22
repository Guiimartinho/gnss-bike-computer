/**
 * @file test_activity.c
 * @brief Totals, auto-pause and laps (src/model/activity.c)
 *
 * None of this exists in the legacy, which logs from boot to shutdown with
 * no start, no pause and no lap. The only thing it has is `att.nbsec_act`,
 * the seconds above 7 km/h (`legacy/source/model/Attitude.cpp:509`), which
 * stays where it is; these tests cover what the port adds around it.
 *
 * The numbers to watch are the ones a rider would notice on the screen or
 * on Strava: the average speed of a ride with a traffic light in it, the
 * distance of each lap, and the descent, which the legacy never counted.
 */

#include <math.h>

#include "unity.h"

#include "model/activity.h"

#define SEC     1000U

static struct activity act;

void setUp(void)
{
    activity_init(&act, 0U, true);
}

void tearDown(void) {}

/** One second at a given speed, carrying the distance that speed covers */
static void ride(float kmh, unsigned int seconds)
{
    struct activity_sample s = {
        .time = 900000000UL,
        .speed_kmh = kmh,
        .dist_m = act.ride.dist_m,
        .climb_m = act.ride.ascent_m,
        .alt_m = 200.0f,
        .power_w = 200,
        .hr_bpm = 150U,
        .cadence_rpm = 85U,
    };

    for (unsigned int i = 0U; i < seconds; i++) {
        s.dist_m += kmh / 3.6f;
        s.time++;
        (void)activity_update(&act, &s, SEC);
    }
}

/** The first sample, which only sets the starting point */
static void first(float alt_m)
{
    struct activity_sample s = {
        .time = 900000000UL, .speed_kmh = 0.0f, .dist_m = 0.0f,
        .climb_m = 0.0f, .alt_m = alt_m,
    };

    TEST_ASSERT_EQUAL_INT(ACTIVITY_EVENT_NONE, activity_update(&act, &s, SEC));
}

static void test_a_ride_starts_empty(void)
{
    const struct activity_totals *t = activity_ride(&act);

    TEST_ASSERT_EQUAL_UINT32(0U, t->timer_ms);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, t->dist_m);
    TEST_ASSERT_EQUAL_UINT16(0U, act.laps);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, activity_avg_speed(t));
}

static void test_the_timer_counts_while_the_bike_moves(void)
{
    first(200.0f);
    ride(30.0f, 60U);

    const struct activity_totals *t = activity_ride(&act);

    TEST_ASSERT_EQUAL_UINT32(60U * SEC, t->timer_ms);
    TEST_ASSERT_EQUAL_UINT32(60U * SEC, t->elapsed_ms);
    /* 30 km/h for a minute is 500 m */
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 500.0f, t->dist_m);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 30.0f, activity_avg_speed(t));
}

static void test_a_traffic_light_stops_the_timer_but_not_the_clock(void)
{
    first(200.0f);
    ride(30.0f, 60U);       /* a minute of riding */
    ride(0.0f, 120U);       /* two minutes at the light */
    ride(30.0f, 60U);       /* and a minute more */

    const struct activity_totals *t = activity_ride(&act);

    /* the wall clock counted the whole four minutes */
    TEST_ASSERT_EQUAL_UINT32(240U * SEC, t->elapsed_ms);
    /* the timer held after the three seconds of the hold */
    TEST_ASSERT_UINT32_WITHIN(SEC, (60U + 3U + 60U) * SEC, t->timer_ms);
    /* and the average is the average of the ride, not of the wait */
    TEST_ASSERT_FLOAT_WITHIN(1.5f, 30.0f, activity_avg_speed(t));
}

static void test_the_pause_and_the_resume_are_announced_once(void)
{
    struct activity_sample s = {
        .time = 1U, .speed_kmh = 20.0f, .dist_m = 0.0f, .alt_m = 100.0f,
    };

    (void)activity_update(&act, &s, SEC);

    unsigned int paused = 0U;
    unsigned int resumed = 0U;

    s.speed_kmh = 0.0f;
    for (unsigned int i = 0U; i < 10U; i++) {
        s.time++;
        if (activity_update(&act, &s, SEC) == ACTIVITY_EVENT_PAUSED) {
            paused++;
        }
    }
    s.speed_kmh = 20.0f;
    for (unsigned int i = 0U; i < 10U; i++) {
        s.time++;
        s.dist_m += 5.5f;
        if (activity_update(&act, &s, SEC) == ACTIVITY_EVENT_RESUMED) {
            resumed++;
        }
    }

    TEST_ASSERT_EQUAL_UINT(1U, paused);
    TEST_ASSERT_EQUAL_UINT(1U, resumed);
}

static void test_walking_the_bike_does_not_flap_the_timer(void)
{
    /* between the two thresholds the timer keeps whatever it was doing */
    struct activity_sample s = {
        .time = 1U, .speed_kmh = 20.0f, .dist_m = 0.0f, .alt_m = 100.0f,
    };

    (void)activity_update(&act, &s, SEC);

    /* stop for long enough to pause */
    s.speed_kmh = 0.0f;
    for (unsigned int i = 0U; i < 5U; i++) {
        s.time++;
        (void)activity_update(&act, &s, SEC);
    }
    TEST_ASSERT_FALSE(act.running);

    /* pushing the bike at 2 km/h is above the pause speed but below the
     * resume one: the timer stays held */
    s.speed_kmh = 2.0f;
    for (unsigned int i = 0U; i < 10U; i++) {
        s.time++;
        s.dist_m += 0.55f;
        (void)activity_update(&act, &s, SEC);
    }
    TEST_ASSERT_FALSE(act.running);

    s.speed_kmh = 4.0f;
    s.time++;
    s.dist_m += 1.1f;
    TEST_ASSERT_EQUAL_INT(ACTIVITY_EVENT_RESUMED, activity_update(&act, &s, SEC));
}

static void test_without_auto_pause_the_timer_never_stops(void)
{
    activity_init(&act, 0U, false);
    first(200.0f);
    ride(0.0f, 300U);

    const struct activity_totals *t = activity_ride(&act);

    TEST_ASSERT_EQUAL_UINT32(300U * SEC, t->timer_ms);
    TEST_ASSERT_TRUE(act.running);
}

static void test_an_automatic_lap_closes_every_so_many_metres(void)
{
    activity_init(&act, 1000U, true);   /* a lap every kilometre */
    first(200.0f);

    unsigned int laps = 0U;
    struct activity_sample s = {
        .time = 1U, .speed_kmh = 36.0f, .dist_m = 0.0f, .alt_m = 200.0f,
    };

    /* 36 km/h is 10 m/s: 250 seconds make 2,5 km */
    for (unsigned int i = 0U; i < 250U; i++) {
        s.time++;
        s.dist_m += 10.0f;
        if (activity_update(&act, &s, SEC) == ACTIVITY_EVENT_LAP) {
            laps++;
            TEST_ASSERT_FLOAT_WITHIN(15.0f, 1000.0f, activity_lap(&act)->dist_m);
        }
    }

    TEST_ASSERT_EQUAL_UINT(2U, laps);
    TEST_ASSERT_EQUAL_UINT16(2U, act.laps);
    /* half a kilometre is still being ridden */
    TEST_ASSERT_FLOAT_WITHIN(15.0f, 500.0f, act.lap.dist_m);
}

static void test_a_lap_by_hand_splits_where_the_rider_asks(void)
{
    first(200.0f);
    ride(36.0f, 100U);      /* a kilometre */

    TEST_ASSERT_TRUE(activity_lap_now(&act));
    TEST_ASSERT_EQUAL_UINT16(1U, act.laps);
    TEST_ASSERT_FLOAT_WITHIN(15.0f, 1000.0f, activity_lap(&act)->dist_m);

    ride(36.0f, 50U);       /* half a kilometre more */
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 500.0f, act.lap.dist_m);
    /* and the ride keeps the whole distance */
    TEST_ASSERT_FLOAT_WITHIN(20.0f, 1500.0f, activity_ride(&act)->dist_m);
}

static void test_a_lap_before_the_ride_starts_does_nothing(void)
{
    TEST_ASSERT_FALSE(activity_lap_now(&act));
    TEST_ASSERT_EQUAL_UINT16(0U, act.laps);
}

static void test_finishing_closes_the_lap_that_was_open(void)
{
    first(200.0f);
    ride(36.0f, 100U);
    activity_finish(&act);

    TEST_ASSERT_EQUAL_UINT16(1U, act.laps);
    TEST_ASSERT_FLOAT_WITHIN(15.0f, 1000.0f, activity_lap(&act)->dist_m);
    TEST_ASSERT_FALSE(act.running);
}

static void test_a_ride_with_no_distance_still_ends_with_one_lap(void)
{
    /* a FIT file with no lap at all is not a valid activity */
    first(200.0f);
    activity_finish(&act);

    TEST_ASSERT_EQUAL_UINT16(1U, act.laps);
}

static void test_the_descent_counts_what_the_legacy_never_did(void)
{
    struct activity_sample s = {
        .time = 1U, .speed_kmh = 30.0f, .dist_m = 0.0f, .alt_m = 1000.0f,
    };

    (void)activity_update(&act, &s, SEC);

    /* down three hundred metres, a metre at a time */
    for (unsigned int i = 0U; i < 300U; i++) {
        s.time++;
        s.dist_m += 8.3f;
        s.alt_m -= 1.0f;
        (void)activity_update(&act, &s, SEC);
    }

    const struct activity_totals *t = activity_ride(&act);

    /* the dead band of two metres holds back the last step, no more */
    TEST_ASSERT_FLOAT_WITHIN(3.0f, 300.0f, t->descent_m);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, t->ascent_m);
}

static void test_noise_of_the_barometer_is_not_a_descent(void)
{
    struct activity_sample s = {
        .time = 1U, .speed_kmh = 30.0f, .dist_m = 0.0f, .alt_m = 500.0f,
    };

    (void)activity_update(&act, &s, SEC);

    for (unsigned int i = 0U; i < 200U; i++) {
        s.time++;
        s.dist_m += 8.3f;
        /* plus and minus one metre, under the dead band */
        s.alt_m = 500.0f + (((i % 2U) == 0U) ? 1.0f : -1.0f);
        (void)activity_update(&act, &s, SEC);
    }

    TEST_ASSERT_EQUAL_FLOAT(0.0f, activity_ride(&act)->descent_m);
}

static void test_the_climb_comes_from_the_model_and_splits_by_lap(void)
{
    struct activity_sample s = {
        .time = 1U, .speed_kmh = 36.0f, .dist_m = 0.0f, .climb_m = 0.0f, .alt_m = 200.0f,
    };

    (void)activity_update(&act, &s, SEC);

    for (unsigned int i = 0U; i < 100U; i++) {
        s.time++;
        s.dist_m += 10.0f;
        s.climb_m += 1.0f;      /* the model says a metre of climb per second */
        (void)activity_update(&act, &s, SEC);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 100.0f, activity_ride(&act)->ascent_m);

    (void)activity_lap_now(&act);
    for (unsigned int i = 0U; i < 50U; i++) {
        s.time++;
        s.dist_m += 10.0f;
        s.climb_m += 1.0f;
        (void)activity_update(&act, &s, SEC);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.5f, 150.0f, activity_ride(&act)->ascent_m);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 50.0f, act.lap.ascent_m);
}

static void test_the_averages_only_count_the_time_the_timer_ran(void)
{
    struct activity_sample s = {
        .time = 1U, .speed_kmh = 30.0f, .dist_m = 0.0f, .alt_m = 100.0f,
        .power_w = 200, .hr_bpm = 150U, .cadence_rpm = 90U,
    };

    (void)activity_update(&act, &s, SEC);
    for (unsigned int i = 0U; i < 100U; i++) {
        s.time++;
        s.dist_m += 8.3f;
        (void)activity_update(&act, &s, SEC);
    }

    /* stopped, with the sensors still answering: none of it counts */
    s.speed_kmh = 0.0f;
    s.power_w = 0;
    s.hr_bpm = 90U;
    s.cadence_rpm = 0U;
    for (unsigned int i = 0U; i < 200U; i++) {
        s.time++;
        (void)activity_update(&act, &s, SEC);
    }

    const struct activity_totals *t = activity_ride(&act);

    /*
     * The seconds of the hold count: the timer only stops once the bike has
     * been still for ACTIVITY_PAUSE_HOLD_MS, and those seconds were ridden
     * as far as the device knew. Two of them reach the totals with no
     * power, so 100 s at 200 W average 196 W over 102 s, not 200 W.
     */
    TEST_ASSERT_EQUAL_UINT32(102U * SEC, t->timer_ms);
    TEST_ASSERT_EQUAL_UINT16(196U, activity_avg_power(t));
    TEST_ASSERT_UINT8_WITHIN(3U, 148U, activity_avg_hr(t));
    TEST_ASSERT_UINT8_WITHIN(3U, 87U, activity_avg_cadence(t));
    TEST_ASSERT_EQUAL_UINT16(200U, t->max_power_w);
    TEST_ASSERT_EQUAL_UINT8(150U, t->max_hr_bpm);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 30.0f, t->max_speed_kmh);
}

static void test_a_strap_that_drops_does_not_halve_the_average(void)
{
    struct activity_sample s = {
        .time = 1U, .speed_kmh = 30.0f, .dist_m = 0.0f, .alt_m = 100.0f, .hr_bpm = 160U,
    };

    (void)activity_update(&act, &s, SEC);
    for (unsigned int i = 0U; i < 100U; i++) {
        s.time++;
        s.dist_m += 8.3f;
        /* the strap answers every other second */
        s.hr_bpm = ((i % 2U) == 0U) ? 160U : 0U;
        (void)activity_update(&act, &s, SEC);
    }

    TEST_ASSERT_EQUAL_UINT8(160U, activity_avg_hr(activity_ride(&act)));
}

static void test_the_calories_come_from_the_work_done(void)
{
    struct activity_sample s = {
        .time = 1U, .speed_kmh = 30.0f, .dist_m = 0.0f, .alt_m = 100.0f, .power_w = 250,
    };

    (void)activity_update(&act, &s, SEC);
    /* 250 W for an hour is 900 kJ, which is about 900 kcal for a cyclist */
    for (unsigned int i = 0U; i < 3600U; i++) {
        s.time++;
        s.dist_m += 8.3f;
        (void)activity_update(&act, &s, SEC);
    }

    TEST_ASSERT_UINT16_WITHIN(5U, 900U, activity_calories(activity_ride(&act)));
}

static void test_a_ride_that_starts_before_the_first_fix_takes_the_first_date(void)
{
    /* the GNSS has no date until it gets a fix: the ride starts at zero and
     * the first real second is what the FIT file says it began at */
    struct activity_sample s = {
        .time = 0U, .speed_kmh = 20.0f, .dist_m = 0.0f, .alt_m = 100.0f,
    };

    (void)activity_update(&act, &s, SEC);
    for (unsigned int i = 0U; i < 5U; i++) {
        s.dist_m += 5.5f;
        (void)activity_update(&act, &s, SEC);
    }
    TEST_ASSERT_EQUAL_UINT32(0U, activity_ride(&act)->start_time);

    s.time = 900000000UL;
    s.dist_m += 5.5f;
    (void)activity_update(&act, &s, SEC);

    TEST_ASSERT_EQUAL_UINT32(900000000UL, activity_ride(&act)->start_time);
    TEST_ASSERT_EQUAL_UINT32(900000000UL, activity_ride(&act)->end_time);
}

static void test_nothing_blows_up_without_a_ride(void)
{
    struct activity_sample s = {.time = 1U};

    TEST_ASSERT_EQUAL_INT(ACTIVITY_EVENT_NONE, activity_update(NULL, &s, SEC));
    TEST_ASSERT_EQUAL_INT(ACTIVITY_EVENT_NONE, activity_update(&act, NULL, SEC));
    TEST_ASSERT_NULL(activity_lap(NULL));
    TEST_ASSERT_NULL(activity_ride(NULL));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, activity_avg_speed(NULL));
    TEST_ASSERT_EQUAL_UINT16(0U, activity_avg_power(NULL));
    TEST_ASSERT_EQUAL_UINT8(0U, activity_avg_hr(NULL));
    TEST_ASSERT_EQUAL_UINT8(0U, activity_avg_cadence(NULL));
    TEST_ASSERT_EQUAL_UINT16(0U, activity_calories(NULL));
    activity_init(NULL, 0U, true);
    activity_finish(NULL);
    activity_finish(&act);      /* never started */
    TEST_ASSERT_EQUAL_UINT16(0U, act.laps);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_ride_starts_empty);
    RUN_TEST(test_the_timer_counts_while_the_bike_moves);
    RUN_TEST(test_a_traffic_light_stops_the_timer_but_not_the_clock);
    RUN_TEST(test_the_pause_and_the_resume_are_announced_once);
    RUN_TEST(test_walking_the_bike_does_not_flap_the_timer);
    RUN_TEST(test_without_auto_pause_the_timer_never_stops);
    RUN_TEST(test_an_automatic_lap_closes_every_so_many_metres);
    RUN_TEST(test_a_lap_by_hand_splits_where_the_rider_asks);
    RUN_TEST(test_a_lap_before_the_ride_starts_does_nothing);
    RUN_TEST(test_finishing_closes_the_lap_that_was_open);
    RUN_TEST(test_a_ride_with_no_distance_still_ends_with_one_lap);
    RUN_TEST(test_the_descent_counts_what_the_legacy_never_did);
    RUN_TEST(test_noise_of_the_barometer_is_not_a_descent);
    RUN_TEST(test_the_climb_comes_from_the_model_and_splits_by_lap);
    RUN_TEST(test_the_averages_only_count_the_time_the_timer_ran);
    RUN_TEST(test_a_strap_that_drops_does_not_halve_the_average);
    RUN_TEST(test_the_calories_come_from_the_work_done);
    RUN_TEST(test_a_ride_that_starts_before_the_first_fix_takes_the_first_date);
    RUN_TEST(test_nothing_blows_up_without_a_ride);

    return UNITY_END();
}
