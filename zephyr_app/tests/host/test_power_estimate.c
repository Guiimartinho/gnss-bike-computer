/**
 * @file test_power_estimate.c
 * @brief Estimated power against the legacy (src/model/power_estimate.c)
 *
 * Oracle: `legacy_compute_power()` of `support/legacy_ref.h`, transcribed
 * from `legacy/source/model/Attitude.cpp:556-575`:
 *
 *     P = 1,025 x [9,81 x W x vit_asc + 0,004 x 9,81 x W x v + 0,204 x v^3]
 *
 * W is the rider alone, 79 kg by default (`legacy/source/parameters.h:62`),
 * and the result lives in an int16_t (`SAtt.pwr`), negative going down.
 */

#include "unity.h"

#include "legacy_ref.h"
#include "model/power_estimate.h"

#define WEIGHT_KG   79.0f

void setUp(void) {}
void tearDown(void) {}

static void test_matches_the_legacy_over_the_range_of_a_ride(void)
{
    static const float speeds_kmh[] = {0.0f, 1.0f, 5.0f, 12.0f, 20.0f, 30.0f, 45.0f, 70.0f};
    static const float slopes[] = {-0.12f, -0.05f, -0.01f, 0.0f, 0.02f, 0.05f, 0.10f, 0.18f};

    for (unsigned int i = 0U; i < (sizeof(speeds_kmh) / sizeof(speeds_kmh[0])); i++) {
        for (unsigned int j = 0U; j < (sizeof(slopes) / sizeof(slopes[0])); j++) {
            float speed_ms = speeds_kmh[i] / 3.6f;
            float vit_asc = speed_ms * slopes[j]; /* as Attitude.cpp:247 does */
            float expected = legacy_compute_power(WEIGHT_KG, vit_asc, speed_ms);

            TEST_ASSERT_INT_WITHIN(1, (int)expected,
                                   power_estimate_w(WEIGHT_KG, speed_ms, vit_asc));
        }
    }
}

static void test_thirty_kilometres_per_hour_on_the_flat(void)
{
    /* 147 W in docs/06: the port gave 88 W with its own formula */
    TEST_ASSERT_INT_WITHIN(1, 147, power_estimate_w(WEIGHT_KG, 30.0f / 3.6f, 0.0f));
}

static void test_twelve_kilometres_per_hour_at_five_percent(void)
{
    float speed_ms = 12.0f / 3.6f;

    TEST_ASSERT_INT_WITHIN(1, 151, power_estimate_w(WEIGHT_KG, speed_ms, speed_ms * 0.05f));
}

static void test_going_down_gives_negative_power(void)
{
    float speed_ms = 40.0f / 3.6f;
    int16_t power = power_estimate_w(WEIGHT_KG, speed_ms, speed_ms * -0.08f);

    TEST_ASSERT_LESS_THAN_INT16(0, power);
    TEST_ASSERT_INT_WITHIN(1, (int)legacy_compute_power(WEIGHT_KG, speed_ms * -0.08f, speed_ms),
                           power);
}

static void test_standing_still_gives_nothing(void)
{
    TEST_ASSERT_EQUAL_INT16(0, power_estimate_w(WEIGHT_KG, 0.0f, 0.0f));
}

static void test_a_heavier_rider_needs_more_power_uphill(void)
{
    float speed_ms = 15.0f / 3.6f;
    float vit_asc = speed_ms * 0.06f;

    TEST_ASSERT_GREATER_THAN_INT16(power_estimate_w(60.0f, speed_ms, vit_asc),
                                   power_estimate_w(95.0f, speed_ms, vit_asc));
}

static void test_the_result_saturates_instead_of_wrapping(void)
{
    /* absurd values: the legacy would wrap the int16_t */
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, power_estimate_w(WEIGHT_KG, 200.0f, 50.0f));
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, power_estimate_w(WEIGHT_KG, 1.0f, -100.0f));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_matches_the_legacy_over_the_range_of_a_ride);
    RUN_TEST(test_thirty_kilometres_per_hour_on_the_flat);
    RUN_TEST(test_twelve_kilometres_per_hour_at_five_percent);
    RUN_TEST(test_going_down_gives_negative_power);
    RUN_TEST(test_standing_still_gives_nothing);
    RUN_TEST(test_a_heavier_rider_needs_more_power_uphill);
    RUN_TEST(test_the_result_saturates_instead_of_wrapping);

    return UNITY_END();
}
