/**
 * @file test_kalman_altitude.c
 * @brief 3-state altitude filter (src/model/kalman_altitude.c)
 *
 * The filter of `legacy/source/model/Attitude.cpp:113-288`: state
 * `[elevation, pitch, alpha zero]`, transition `A = [[1, dl, -dl], [0, 1, 0],
 * [0, 0, 1]]` with `dl = v x dt`, `Q = diag(0.03, 0.10, 0.0002)`,
 * `R = diag(1000, 600)` and `P0` with 900 in every element
 * (`matP.ones(900)`), bounded to [1e-15, 1e12] in absolute value.
 *
 * What matters here is that the offset of the accelerometer (alpha zero) is
 * estimated: with the covariances killed, as the port did before
 * 2026-09-19, it stayed at zero for ever and the slope carried the mounting
 * angle of the board.
 */

#include "unity.h"

#include <math.h>

#include "model/kalman_altitude.h"

#define SPEED_MS        8.0f    /* about 29 km/h */
#define EPOCH_MS        1000U   /* one location per second */

static kalman_altitude_t kf;

void setUp(void)
{
    kalman_altitude_init(&kf, 200.0f);
}

void tearDown(void) {}

/**
 * Feed @p epochs seconds of a ride with a given slope, where the
 * accelerometer reads the slope plus a fixed mounting offset.
 */
static kalman_alt_output_t ride(float slope, float mounting_offset, unsigned int epochs)
{
    kalman_alt_output_t out = {0};
    float elevation = 200.0f;
    uint32_t t = EPOCH_MS;

    for (unsigned int i = 0U; i < epochs; i++) {
        elevation += SPEED_MS * slope; /* one second of climbing */

        kalman_alt_feed_t feed = {
            .baro_altitude = elevation,
            .pitch_rad = slope + mounting_offset,
            .speed_ms = SPEED_MS,
            .timestamp_ms = t,
        };

        (void)kalman_altitude_update(&kf, &feed, &out);
        t += EPOCH_MS;
    }

    return out;
}

static void test_the_elevation_follows_the_barometer(void)
{
    kalman_alt_output_t out = ride(0.04f, 0.0f, 120U);

    /* 120 s climbing at 4 % and 8 m/s: 200 m plus 38,4 m */
    TEST_ASSERT_FLOAT_WITHIN(3.0f, 238.4f, out.elevation);
}

static void test_the_slope_comes_out_of_the_filter(void)
{
    kalman_alt_output_t out = ride(0.05f, 0.0f, 180U);

    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.05f, out.slope);
    /* vertical speed is the slope times the speed */
    TEST_ASSERT_FLOAT_WITHIN(0.15f, 0.05f * SPEED_MS, out.vit_asc);
}

static void test_the_mounting_offset_is_estimated(void)
{
    /* the accelerometer reads 6 degrees more than the real slope */
    const float offset = 0.105f;
    kalman_alt_output_t out = ride(0.03f, offset, 600U);

    /*
     * With the covariances killed, alpha zero stayed at zero and the slope
     * came out 6 degrees too steep. It has to move towards the offset and
     * leave the slope near the real one.
     */
    TEST_ASSERT_TRUE(fabsf(out.alpha_zero) > 0.02f);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.03f, out.slope);
}

static void test_standing_still_does_not_update(void)
{
    kalman_alt_output_t out = {0};
    kalman_alt_feed_t feed = {
        .baro_altitude = 500.0f,
        .pitch_rad = 0.0f,
        .speed_ms = 0.5f, /* below the 1,5 m/s of the legacy */
        .timestamp_ms = EPOCH_MS,
    };

    TEST_ASSERT_FALSE(kalman_altitude_update(&kf, &feed, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 200.0f, kalman_altitude_get_elevation(&kf));
}

static void test_a_flat_ride_gives_no_slope(void)
{
    kalman_alt_output_t out = ride(0.0f, 0.0f, 120U);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, out.slope);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, out.vit_asc);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 200.0f, out.elevation);
}

static void test_going_down_gives_a_negative_vertical_speed(void)
{
    kalman_alt_output_t out = ride(-0.06f, 0.0f, 180U);

    TEST_ASSERT_TRUE(out.vit_asc < -0.2f);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, -0.06f, out.slope);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_elevation_follows_the_barometer);
    RUN_TEST(test_the_slope_comes_out_of_the_filter);
    RUN_TEST(test_the_mounting_offset_is_estimated);
    RUN_TEST(test_standing_still_does_not_update);
    RUN_TEST(test_a_flat_ride_gives_no_slope);
    RUN_TEST(test_going_down_gives_a_negative_vertical_speed);

    return UNITY_END();
}
