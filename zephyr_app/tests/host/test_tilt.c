/**
 * @file test_tilt.c
 * @brief Host tests for svc/sensors/tilt.c.
 *
 * Pitch, roll and heading are checked against readings built here, apart
 * from the code under test: the device is turned by heading, pitch and roll
 * (aerospace Z-Y-X order in north-east-down axes), the gravity reaction and
 * a magnetic field are taken into the body, and then into the board frame
 * of the firmware (X forward, Y left, Z up). The roughness is compared with
 * the loop of legacy/source/sensors/fxos.cpp:761-766.
 */

#include <math.h>

#include "unity.h"

#include "svc/tilt.h"
#include "legacy_ref.h"

#define DEG 0.017453293f

static struct tilt_window win;

void setUp(void)
{
    tilt_window_reset(&win);
}

void tearDown(void)
{
}

/** v_body = (Rz(yaw) Ry(pitch) Rx(roll))^T v_world, north-east-down */
static void world_to_body(float yaw, float pitch, float roll, const float w[3], float b[3])
{
    float cy = cosf(yaw * DEG), sy = sinf(yaw * DEG);
    float cp = cosf(pitch * DEG), sp = sinf(pitch * DEG);
    float cr = cosf(roll * DEG), sr = sinf(roll * DEG);
    /* Rz^T */
    const float t1[3] = {(cy * w[0]) + (sy * w[1]), (-sy * w[0]) + (cy * w[1]), w[2]};
    /* Ry^T */
    const float t2[3] = {(cp * t1[0]) - (sp * t1[2]), t1[1], (sp * t1[0]) + (cp * t1[2])};
    /* Rx^T */
    b[0] = t2[0];
    b[1] = (cr * t2[1]) + (sr * t2[2]);
    b[2] = (-sr * t2[1]) + (cr * t2[2]);
}

/** Readings of the board (X forward, Y left, Z up) at a given attitude */
static void readings(float yaw, float pitch, float roll, float acc[3], float mag[3])
{
    /* the accelerometer measures the reaction to gravity: up, -g along down */
    const float force_world[3] = {0.0f, 0.0f, -TILT_G};
    /* a field pointing north and down, as in the northern hemisphere */
    const float field_world[3] = {20.0f, 0.0f, 43.0f};
    float f[3];
    float m[3];

    world_to_body(yaw, pitch, roll, force_world, f);
    world_to_body(yaw, pitch, roll, field_world, m);
    acc[0] = f[0];
    acc[1] = -f[1];
    acc[2] = -f[2];
    mag[0] = m[0];
    mag[1] = -m[1];
    mag[2] = -m[2];
}

static void check_attitude(float yaw, float pitch, float roll)
{
    float acc[3];
    float mag[3];
    struct tilt_out out;

    tilt_window_reset(&win);
    readings(yaw, pitch, roll, acc, mag);
    for (unsigned int i = 0U; i < TILT_WINDOW; i++) {
        (void)tilt_window_add(&win, acc[0], acc[1], acc[2]);
    }
    TEST_ASSERT_TRUE(tilt_compute(&win, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, pitch, out.pitch_deg);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, roll, out.roll_deg);

    float h = tilt_heading_deg(out.pitch_deg, out.roll_deg, mag[0], mag[1], mag[2]);
    float err = fmodf(fabsf(h - yaw), 360.0f);

    if (err > 180.0f) {
        err = 360.0f - err;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, err);
}

static void test_level_and_facing_north_is_all_zero(void)
{
    check_attitude(0.0f, 0.0f, 0.0f);
}

static void test_facing_east_and_west_read_90_and_270(void)
{
    check_attitude(90.0f, 0.0f, 0.0f);
    check_attitude(270.0f, 0.0f, 0.0f);
}

static void test_nose_up_is_a_positive_pitch(void)
{
    check_attitude(0.0f, 12.0f, 0.0f);
}

static void test_right_side_down_is_a_positive_roll(void)
{
    check_attitude(0.0f, 0.0f, 20.0f);
}

static void test_the_heading_survives_pitch_and_roll_together(void)
{
    check_attitude(200.0f, 15.0f, -10.0f);
    check_attitude(45.0f, -20.0f, 25.0f);
    check_attitude(315.0f, 8.0f, 35.0f);
}

static void test_the_window_fills_after_50_samples_and_then_slides(void)
{
    for (unsigned int i = 0U; i < (TILT_WINDOW - 1U); i++) {
        TEST_ASSERT_FALSE(tilt_window_add(&win, 0.0f, 0.0f, TILT_G));
    }
    TEST_ASSERT_TRUE(tilt_window_add(&win, 0.0f, 0.0f, TILT_G));
    TEST_ASSERT_TRUE(tilt_window_add(&win, 0.0f, 0.0f, TILT_G));
    TEST_ASSERT_EQUAL_UINT8(TILT_WINDOW, win.n);
}

static void test_an_empty_window_gives_nothing(void)
{
    struct tilt_out out;

    TEST_ASSERT_FALSE(tilt_compute(&win, &out));
}

static void test_the_roughness_is_the_legacy_mean_deviation_in_counts(void)
{
    int16_t counts[TILT_WINDOW];
    struct tilt_out out;

    /* a bumpy road on X, in FXOS counts at +-4 g, and the same in m/s2 */
    for (unsigned int i = 0U; i < TILT_WINDOW; i++) {
        counts[i] = (int16_t)(((i % 5U) * 37U) - 60);
        (void)tilt_window_add(&win, ((float)counts[i] / TILT_COUNTS_PER_G) * TILT_G, 0.0f, TILT_G);
    }
    TEST_ASSERT_TRUE(tilt_compute(&win, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, legacy_fxos_roughness(counts, TILT_WINDOW), out.rough[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out.rough[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out.rough[2]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_level_and_facing_north_is_all_zero);
    RUN_TEST(test_facing_east_and_west_read_90_and_270);
    RUN_TEST(test_nose_up_is_a_positive_pitch);
    RUN_TEST(test_right_side_down_is_a_positive_roll);
    RUN_TEST(test_the_heading_survives_pitch_and_roll_together);
    RUN_TEST(test_the_window_fills_after_50_samples_and_then_slides);
    RUN_TEST(test_an_empty_window_gives_nothing);
    RUN_TEST(test_the_roughness_is_the_legacy_mean_deviation_in_counts);
    return UNITY_END();
}
