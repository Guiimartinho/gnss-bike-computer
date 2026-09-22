/**
 * @file test_attitude.c
 * @brief The epoch of the ride model (src/model/attitude.c)
 *
 * The pieces `attitude.c` leans on each have their own test —
 * `test_distance`, `test_power_estimate`, `test_kalman_altitude`,
 * `test_crash_recovery`. What had none was the orchestration: what happens,
 * and in what order, when one position arrives. That is where the bugs
 * were, so that is what this file covers.
 *
 * The oracle is `legacy/source/model/Attitude.cpp`, mainly
 * `addNewLocation()` at line 498:
 *
 * ```
 * if (loc_.speed > 7.f) att.nbsec_act++;   // 509, once per location
 * att.nbpts++;                             // 510
 * att.climb = computeElevation(...);       // 513
 * att.pwr   = computePower(loc_.speed);    // 516, the speed of the
 * computeDistance(loc_, date_);            // 518   PREVIOUS epoch
 * m_speed_ms = loc_.speed / 3.6f;          // 522, only now
 * ```
 *
 * The last two lines are the subtle one and have a case of their own: the
 * power of an epoch is computed with the speed the bicycle had when it
 * entered the epoch, not with the one the receiver has just reported.
 *
 * `user_settings` is faked here — two functions — because the real one
 * talks to the settings subsystem, which has no host shim. Nothing else is
 * faked: `distance.c`, `power_estimate.c`, `kalman_altitude.c`,
 * `udmatrix.c`, `vecteur.c` and `crash_recovery.c` are the real files.
 */

#include <math.h>
#include <string.h>

#include "unity.h"

#include "model/attitude.h"
#include "model/crash_recovery.h"
#include "model/user_settings.h"
#include "support/host_kernel.h"

/* ---- the fake settings ---- */

static user_settings_t fake_settings;

user_settings_t *user_settings_get_global(void)
{
    return &fake_settings;
}

uint16_t user_settings_get_weight(const user_settings_t *settings)
{
    (void)settings;
    return 79U;     /* USER_WEIGHT of the legacy */
}

/* ---- the ride ---- */

/** Nancy, where the simulation traces of the legacy were recorded */
#define LAT0    48.6921f
#define LON0    6.1844f

void setUp(void)
{
    host_uptime_set(0U);
    crash_recovery_clear();
    attitude_reset();
    (void)attitude_init();
    attitude_reset();
}

void tearDown(void) {}

/** One epoch at a given speed, moving north by `dlat` degrees */
static void epoch(float dlat, float speed_kmh, uint32_t ms)
{
    loc_data_t loc = {
        .lat = LAT0 + dlat,
        .lon = LON0,
        .alt = 200.0f,
        .speed = speed_kmh,
        .course = 0.0f,
        .timestamp = ms,
    };

    host_uptime_set(ms);
    (void)attitude_update_gps(&loc);
}

/** About 111 m of latitude per 0,001 degree */
#define DLAT_100M   0.0009f

/* ==========================================================================
 * The active seconds: `if (loc_.speed > 7.f) att.nbsec_act++;`
 * ========================================================================== */

static void test_a_ridden_epoch_counts_one_active_second(void)
{
    attitude_t a;

    for (unsigned int i = 1U; i <= 10U; i++) {
        epoch((float)i * DLAT_100M, 25.0f, i * 1000U);
    }

    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_EQUAL_UINT16(10U, a.nbsec_act);
    TEST_ASSERT_EQUAL_UINT32(10U, attitude_get_moving_time());
}

static void test_the_threshold_is_the_seven_of_the_legacy(void)
{
    attitude_t a;

    /* seven exactly does not count: the legacy asks for strictly more */
    epoch(1.0f * DLAT_100M, 7.0f, 1000U);
    epoch(2.0f * DLAT_100M, 6.9f, 2000U);
    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_EQUAL_UINT16(0U, a.nbsec_act);

    epoch(3.0f * DLAT_100M, 7.1f, 3000U);
    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_EQUAL_UINT16(1U, a.nbsec_act);
}

static void test_epochs_a_little_under_a_second_apart_still_all_count(void)
{
    /*
     * The defect this replaces. The count used to be gated on
     * `(now - last_update) >= 1000` against the uptime at the moment the
     * thread reached the epoch, and `last_update` only moved when the test
     * passed. Epochs 998 ms apart therefore went count, miss, count,
     * miss — half the real number — and the average speed of page 1, which
     * is `dist x 3,6 / nbsec_act`, came out at twice the truth.
     */
    attitude_t a;
    uint32_t t = 0U;

    for (unsigned int i = 1U; i <= 20U; i++) {
        t += 998U;
        epoch((float)i * DLAT_100M, 25.0f, t);
    }

    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_EQUAL_UINT16(20U, a.nbsec_act);
}

static void test_a_stopped_bicycle_adds_no_active_second(void)
{
    attitude_t a;

    for (unsigned int i = 1U; i <= 10U; i++) {
        /* standing still: the same place, no speed */
        epoch(0.0f, 0.0f, i * 1000U);
    }

    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_EQUAL_UINT16(0U, a.nbsec_act);
    TEST_ASSERT_EQUAL_UINT16(10U, a.nbpts);     /* but the points count */
}

static void test_a_silent_receiver_adds_nothing(void)
{
    /*
     * No position means no evidence of movement. A rider going into a
     * tunnel at 30 km/h and out of it a minute later books nothing for the
     * minute, which is what the legacy does: it counts per location.
     */
    attitude_t a;

    epoch(1.0f * DLAT_100M, 30.0f, 1000U);
    host_uptime_set(61000U);        /* a minute of silence */
    epoch(2.0f * DLAT_100M, 30.0f, 61000U);

    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_EQUAL_UINT16(2U, a.nbsec_act);
}

/* ==========================================================================
 * The position that does not enter the model
 * ========================================================================== */

static void test_a_zeroed_coordinate_is_refused(void)
{
    attitude_t a;
    loc_data_t bad = {.lat = 0.0f, .lon = 0.0f, .alt = 0.0f, .speed = 30.0f};

    epoch(1.0f * DLAT_100M, 30.0f, 1000U);

    host_uptime_set(2000U);
    TEST_ASSERT_EQUAL_INT(APP_ERR_INVALID_PARAM, attitude_update_gps(&bad));

    bad.lat = LAT0;         /* only the longitude zeroed: still refused */
    TEST_ASSERT_EQUAL_INT(APP_ERR_INVALID_PARAM, attitude_update_gps(&bad));

    /* and nothing of it reached the model */
    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_EQUAL_UINT16(1U, a.nbpts);
    TEST_ASSERT_EQUAL_UINT16(1U, a.nbsec_act);
}

static void test_a_position_off_the_planet_is_refused(void)
{
    loc_data_t bad = {.lat = 95.0f, .lon = LON0, .speed = 30.0f};

    host_uptime_set(1000U);
    TEST_ASSERT_EQUAL_INT(APP_ERR_INVALID_PARAM, attitude_update_gps(&bad));
    TEST_ASSERT_EQUAL_INT(APP_ERR_INVALID_PARAM, attitude_update_gps(NULL));
}

/* ==========================================================================
 * Distance: the rule of `Attitude::computeDistance`
 * ========================================================================== */

static void test_the_first_jump_is_thrown_away_and_then_it_counts(void)
{
    /*
     * `distance.c` has the rule and its own test; what is checked here is
     * that the epoch runs it and publishes the total.
     *
     * The rule costs the first stretch: nothing counts until the total
     * passes 25 m, and when it does the total is **thrown away** and the
     * counting starts from there (`Attitude::computeDistance`, the receiver
     * settling down). With steps of 100 m the first whole step goes, so
     * eleven positions leave nine steps on the screen and not ten.
     */
    attitude_t a;

    epoch(0.0f, 30.0f, 1000U);
    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, a.dist);

    for (unsigned int i = 1U; i <= 10U; i++) {
        epoch((float)i * DLAT_100M, 30.0f, (i + 1U) * 1000U);
    }

    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 900.5f, a.dist);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 900.5f, attitude_get_distance());
}

static void test_the_distance_counts_whatever_the_speed(void)
{
    /*
     * `computeDistance` of the legacy does not look at the speed: a rider
     * pushing the bicycle uphill at 4 km/h still covers ground. Only
     * `nbsec_act` has the 7 km/h threshold.
     */
    attitude_t a;

    epoch(0.0f, 0.0f, 1000U);
    for (unsigned int i = 1U; i <= 10U; i++) {
        epoch((float)i * DLAT_100M, 4.0f, (i + 1U) * 1000U);
    }

    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_EQUAL_UINT16(0U, a.nbsec_act);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 900.5f, a.dist);
}

/* ==========================================================================
 * Power: computed with the speed of the previous epoch
 * ========================================================================== */

static void test_the_power_uses_the_speed_the_bicycle_came_in_with(void)
{
    /*
     * `Attitude.cpp:516` computes the power and only then, at 522, updates
     * `m_speed_ms`. So the first epoch of a ride, which starts from a
     * standstill, reports zero watts however fast the receiver says the
     * bicycle is going; the watts of that speed appear one epoch later.
     */
    epoch(0.0f, 0.0f, 1000U);
    epoch(1.0f * DLAT_100M, 30.0f, 2000U);
    TEST_ASSERT_EQUAL_UINT16(0U, attitude_get_power());

    epoch(2.0f * DLAT_100M, 30.0f, 3000U);
    TEST_ASSERT_TRUE(attitude_get_power() > 0U);
}

/* ==========================================================================
 * The barometer ring: ten samples before the altitude means anything
 * ========================================================================== */

static void test_the_ring_gives_nothing_until_it_is_full(void)
{
    attitude_ext_t e;

    for (unsigned int i = 0U; i < 9U; i++) {
        (void)attitude_update_baro(101000.0f, 20.0f);
    }
    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get_ext(&e));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, e.baro_roughness);

    (void)attitude_update_baro(101000.0f, 20.0f);   /* the tenth */
    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get_ext(&e));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, e.baro_roughness);  /* full but flat */
}

static void test_the_roughness_is_the_mean_absolute_deviation(void)
{
    /*
     * `AltiBaro::getRoughness()`: five at 101000 and five at 101010 give an
     * average of 101005 and a deviation of 5 Pa in every sample.
     */
    attitude_ext_t e;

    for (unsigned int i = 0U; i < 5U; i++) {
        (void)attitude_update_baro(101000.0f, 20.0f);
    }
    for (unsigned int i = 0U; i < 5U; i++) {
        (void)attitude_update_baro(101010.0f, 20.0f);
    }

    /*
     * The ring is filled by the barometer, but what the screens read is
     * only written when a position goes through the epoch: the roughness
     * belongs to the published copy, not to the sensor.
     */
    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get_ext(&e));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, e.baro_roughness);

    epoch(DLAT_100M, 25.0f, 1000U);
    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get_ext(&e));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5.0f, e.baro_roughness);
}

/* ==========================================================================
 * The sea level reference, and the ride picked up after a crash
 * ========================================================================== */

/** Enough epochs and pressures for the sea level reference to be taken */
static void ride_until_sea_level_ref(void)
{
    for (unsigned int i = 0U; i < 10U; i++) {
        (void)attitude_update_baro(101000.0f, 20.0f);
    }
    for (unsigned int i = 1U; i <= 20U; i++) {
        epoch((float)i * DLAT_100M, 25.0f, i * 1000U);
    }
}

static void test_the_reference_waits_for_fifteen_points(void)
{
    attitude_ext_t e;

    for (unsigned int i = 0U; i < 10U; i++) {
        (void)attitude_update_baro(101000.0f, 20.0f);
    }
    for (unsigned int i = 1U; i <= 15U; i++) {
        epoch((float)i * DLAT_100M, 25.0f, i * 1000U);
    }

    /* fifteen is not more than fifteen: still no correction being filtered */
    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get_ext(&e));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, e.baro_correction);

    epoch(16.0f * DLAT_100M, 25.0f, 16000U);
    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get_ext(&e));
    TEST_ASSERT_TRUE(attitude_get_elevation() != 0.0f);
}

static void test_a_ride_picked_up_after_a_crash_keeps_its_seconds(void)
{
    /*
     * A ride is saved, the device resets, and the rider sets off again.
     * What must come back is the ride from **before** the reset, not the
     * one that has just started — `crash_recovery.c` latches the retained
     * block at boot for exactly this, because the new ride overwrites the
     * live one every 15 m, long before the restore happens.
     *
     * And the seconds have to come back with the counter behind them.
     * `restore_from_crash()` used to put `saved.nbsec_act` into the
     * published structure and leave `moving_seconds` at zero, so the first
     * epoch ridden wrote 1 over the seconds of the whole ride and the
     * average speed was computed over one second.
     */
    loc_data_t here = {.lat = LAT0, .lon = LON0, .alt = 200.0f, .speed = 25.0f};
    date_data_t when = {.date = 210926U, .secj = 43200U, .timestamp = 0U};

    attitude_update_datetime(&when);
    crash_recovery_save_state(&here, &when, 12345.0f, 456.0f, 900U, 800U, 0U);

    /* the reset: the retained block survives it and the boot latches it */
    (void)crash_recovery_init();
    attitude_reset();
    TEST_ASSERT_TRUE(crash_recovery_has_data());

    attitude_update_datetime(&when);
    ride_until_sea_level_ref();

    attitude_t a;

    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_TRUE(attitude_take_fdir_notice());

    /* the ride came back, and the seconds went on from 800, not from 1 */
    TEST_ASSERT_TRUE(a.nbsec_act > 800U);
    TEST_ASSERT_TRUE(a.dist > 12000.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 456.0f, a.climb);
}

static void test_a_ride_of_another_day_is_refused(void)
{
    /* `Attitude.cpp:395`: a block from another date is not this ride */
    loc_data_t here = {.lat = LAT0, .lon = LON0, .alt = 200.0f, .speed = 25.0f};
    date_data_t old_day = {.date = 200926U, .secj = 43200U, .timestamp = 0U};
    date_data_t today = {.date = 210926U, .secj = 43200U, .timestamp = 0U};

    attitude_update_datetime(&old_day);
    crash_recovery_save_state(&here, &old_day, 12345.0f, 456.0f, 900U, 800U, 0U);

    (void)crash_recovery_init();
    attitude_reset();
    attitude_update_datetime(&today);
    ride_until_sea_level_ref();

    attitude_t a;

    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_FALSE(attitude_take_fdir_notice());
    TEST_ASSERT_EQUAL_UINT16(20U, a.nbsec_act);     /* only today's epochs */
    TEST_ASSERT_TRUE(a.dist < 2000.0f);
}

/* ==========================================================================
 * Starting over
 * ========================================================================== */

static void test_a_reset_empties_everything(void)
{
    attitude_t a;

    ride_until_sea_level_ref();
    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_TRUE(a.nbpts > 0U);

    attitude_reset();

    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_EQUAL_UINT16(0U, a.nbpts);
    TEST_ASSERT_EQUAL_UINT16(0U, a.nbsec_act);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, a.dist);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, a.climb);
    TEST_ASSERT_EQUAL_UINT32(0U, attitude_get_moving_time());
}

static void test_a_second_init_is_refused_and_changes_nothing(void)
{
    ride_until_sea_level_ref();

    TEST_ASSERT_EQUAL_INT(APP_ERR_ALREADY_INIT, attitude_init());

    attitude_t a;

    TEST_ASSERT_EQUAL_INT(APP_OK, attitude_get(&a));
    TEST_ASSERT_TRUE(a.nbpts > 0U);
}

static void test_nothing_blows_up_without_somewhere_to_write(void)
{
    TEST_ASSERT_EQUAL_INT(APP_ERR_INVALID_PARAM, attitude_get(NULL));
    TEST_ASSERT_EQUAL_INT(APP_ERR_INVALID_PARAM, attitude_get_ext(NULL));
    TEST_ASSERT_EQUAL_INT(APP_ERR_INVALID_PARAM, attitude_get_location(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_ridden_epoch_counts_one_active_second);
    RUN_TEST(test_the_threshold_is_the_seven_of_the_legacy);
    RUN_TEST(test_epochs_a_little_under_a_second_apart_still_all_count);
    RUN_TEST(test_a_stopped_bicycle_adds_no_active_second);
    RUN_TEST(test_a_silent_receiver_adds_nothing);
    RUN_TEST(test_a_zeroed_coordinate_is_refused);
    RUN_TEST(test_a_position_off_the_planet_is_refused);
    RUN_TEST(test_the_first_jump_is_thrown_away_and_then_it_counts);
    RUN_TEST(test_the_distance_counts_whatever_the_speed);
    RUN_TEST(test_the_power_uses_the_speed_the_bicycle_came_in_with);
    RUN_TEST(test_the_ring_gives_nothing_until_it_is_full);
    RUN_TEST(test_the_roughness_is_the_mean_absolute_deviation);
    RUN_TEST(test_the_reference_waits_for_fifteen_points);
    RUN_TEST(test_a_ride_picked_up_after_a_crash_keeps_its_seconds);
    RUN_TEST(test_a_ride_of_another_day_is_refused);
    RUN_TEST(test_a_reset_empties_everything);
    RUN_TEST(test_a_second_init_is_refused_and_changes_nothing);
    RUN_TEST(test_nothing_blows_up_without_somewhere_to_write);

    return UNITY_END();
}
