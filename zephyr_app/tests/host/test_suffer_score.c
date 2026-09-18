/**
 * @file test_suffer_score.c
 * @brief Host tests for model/suffer_score.c against legacy SufferScore.cpp.
 *
 * Legacy rules (legacy/source/model/SufferScore.cpp): zones by heart rate
 * (80,120], (120,144], (144,165], (165,176], above 176; nothing at or below
 * 80 bpm; 16, 33, 72, 85 and 95 points per hour in zones 1 to 5.
 */

#include "unity.h"

#include "model/suffer_score.h"

static suffer_score_t ss;

void setUp(void)
{
    suffer_score_init(&ss);
}

void tearDown(void)
{
}

/* Feeds `seconds` one-second samples at `bpm`, after a clock-start sample. */
static void ride(uint8_t bpm, uint32_t seconds)
{
    static uint32_t t;

    if (ss.last_timestamp == 0U) {
        t = 1000U;
        suffer_score_add_hrm(&ss, bpm, t);
    }
    for (uint32_t i = 0U; i < seconds; i++) {
        t += 1000U;
        suffer_score_add_hrm(&ss, bpm, t);
    }
}

static void test_the_first_sample_only_starts_the_clock(void)
{
    suffer_score_add_hrm(&ss, 150U, 1000U);

    TEST_ASSERT_EQUAL_UINT32(0U, suffer_score_get_total_time(&ss));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, suffer_score_get(&ss));
}

static void test_heart_rate_at_or_below_80_bpm_scores_nothing(void)
{
    ride(80U, 600U);

    TEST_ASSERT_EQUAL_UINT32(0U, suffer_score_get_total_time(&ss));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, suffer_score_get(&ss));
}

static void test_zone_boundaries_follow_the_legacy_limits(void)
{
    static const struct {
        uint8_t bpm;
        uint8_t zone;
    } cases[] = {
        { 81U, 0U }, { 120U, 0U }, { 121U, 1U }, { 144U, 1U }, { 145U, 2U },
        { 165U, 2U }, { 166U, 3U }, { 176U, 3U }, { 177U, 4U }, { 200U, 4U },
    };

    for (unsigned int i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++) {
        suffer_score_init(&ss);
        ride(cases[i].bpm, 10U);

        TEST_ASSERT_EQUAL_UINT32_MESSAGE(10U, suffer_score_get_zone_time(&ss, cases[i].zone), "zone");
    }
}

static void test_one_hour_in_each_zone_gives_the_legacy_points(void)
{
    static const struct {
        uint8_t bpm;
        float points;
    } cases[] = {
        { 100U, 16.0f }, { 130U, 33.0f }, { 150U, 72.0f }, { 170U, 85.0f }, { 180U, 95.0f },
    };

    for (unsigned int i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++) {
        suffer_score_init(&ss);
        ride(cases[i].bpm, 3600U);

        TEST_ASSERT_FLOAT_WITHIN(0.01f, cases[i].points, suffer_score_get(&ss));
    }
}

static void test_the_score_adds_the_time_of_every_zone(void)
{
    ride(130U, 1800U);   /* 30 min in zone 2: 16.5 points */
    ride(180U, 600U);    /* 10 min in zone 5: 15.83 points */

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 16.5f + (95.0f / 6.0f), suffer_score_get(&ss));
    TEST_ASSERT_EQUAL_UINT32(2400U, suffer_score_get_total_time(&ss));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_first_sample_only_starts_the_clock);
    RUN_TEST(test_heart_rate_at_or_below_80_bpm_scores_nothing);
    RUN_TEST(test_zone_boundaries_follow_the_legacy_limits);
    RUN_TEST(test_one_hour_in_each_zone_gives_the_legacy_points);
    RUN_TEST(test_the_score_adds_the_time_of_every_zone);
    return UNITY_END();
}
