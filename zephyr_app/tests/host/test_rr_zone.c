/**
 * @file test_rr_zone.c
 * @brief RMSSD of the heart rate, binned by zone (src/model/rr_zone.c)
 *
 * A fidelity test against `legacy/source/model/RRZone.cpp`. What the
 * original does, line by line:
 *
 * - a sample with `timestamp == 0` is ignored (`RRZone.cpp:41`);
 * - the **first** sample only sets the clock; its RR is thrown away
 *   (`RRZone.cpp:44-47`);
 * - RR values pile up until there are `VAR_NB_ELEM` of them, which is 20
 *   (`RRZone.cpp:12`, `RRZone.cpp:54-56`);
 * - then RMSSD is `sqrt(sum of the 19 squared successive differences / 20)`
 *   — the denominator is the **buffer length**, not the number of
 *   differences, which is not the textbook RMSSD but is what the legacy
 *   computes (`RRZone.cpp:69`);
 * - the buffer starts over, and the RMSSD is added to the bin of the heart
 *   rate of that last sample, with limits 70, 108, 143, 161 and 178 bpm
 *   (`RRZone.cpp:15-23`, `RRZone.cpp:82-96`);
 * - a zone reads back as its sum divided by how many times it was fed, or
 *   zero (`getValZX`, `RRZone.cpp:126-135`).
 *
 * One thing is **deliberately** not ported. The legacy takes the sample by
 * reference and writes `hrm_info.timestamp = 0` into it when the buffer
 * fills (`RRZone.cpp:79`), because `Model.cpp:378` hands it the same global
 * structure on every turn of the loop, new sample or not; the zero is what
 * stops one sample being counted many times. Here the caller
 * (`svc/model/model_svc.c:472`) builds a fresh structure for each heart
 * rate notification that carries an RR, so the module never sees the same
 * sample twice and takes its argument as `const`.
 */

#include <math.h>
#include <string.h>

#include "unity.h"

#include "model/rr_zone.h"

static rr_zone_t rz;

void setUp(void)
{
    rr_zone_init(&rz);
}

void tearDown(void) {}

/** One sample, as the radio would deliver it */
static void feed(uint8_t bpm, uint16_t rr_ms, uint32_t ms)
{
    hrm_info_t h = {.bpm = bpm, .rr_interval = rr_ms, .timestamp = ms,
                    .connected = true};

    rr_zone_add_data(&rz, &h);
}

/**
 * Fill the buffer once with a constant heart rate and constant RR, which
 * makes every successive difference zero and so RMSSD zero. `ms` only has
 * to be non-zero and is not otherwise used by the rule.
 */
static void feed_flat(uint8_t bpm, uint16_t rr_ms)
{
    for (unsigned int i = 0U; i < RR_VAR_NB_ELEM; i++) {
        feed(bpm, rr_ms, 1000U + i);
    }
}

/* ---- what it starts as ---- */

static void test_a_fresh_one_holds_nothing(void)
{
    for (uint8_t z = 0U; z < RR_ZONES_NB; z++) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, rr_zone_get_value(&rz, z));
        TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_time(&rz, z));
    }
    TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_total_time(&rz));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rr_zone_get_max_value(&rz));
    TEST_ASSERT_EQUAL_UINT8(RR_ZONES_NB, rr_zone_get_count());
}

/* ---- the samples that do not count ---- */

static void test_a_sample_without_a_clock_is_ignored(void)
{
    /* `if (!hrm_info.timestamp) return;` */
    for (unsigned int i = 0U; i < (RR_VAR_NB_ELEM * 2U); i++) {
        feed(120U, 800U, 0U);
    }

    TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_total_time(&rz));
}

static void test_a_sample_without_a_clock_is_ignored_mid_ride_too(void)
{
    /*
     * The case above cannot tell the guard from its absence: with the
     * clock still at zero every sample takes the "first call" branch
     * anyway. This is the one that matters, and the one the radio can
     * actually produce — the sensor is running, and one notification
     * arrives with no timestamp on it. It must not reach the buffer.
     */
    feed(120U, 800U, 900U);         /* the clock is started */

    for (unsigned int i = 0U; i < (RR_VAR_NB_ELEM * 2U); i++) {
        feed(120U, 800U, 0U);
    }
    TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_total_time(&rz));

    /* and the buffer was left untouched, so a full one still takes 20 */
    for (unsigned int i = 0U; i < (RR_VAR_NB_ELEM - 1U); i++) {
        feed(120U, 800U, 2000U + i);
    }
    TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_total_time(&rz));

    feed(120U, 800U, 3000U);
    TEST_ASSERT_EQUAL_UINT32(1U, rr_zone_get_total_time(&rz));
}

static void test_the_very_first_sample_only_starts_the_clock(void)
{
    /*
     * Its RR is thrown away, so a buffer takes the first sample plus
     * RR_VAR_NB_ELEM more before a bin moves.
     */
    feed(120U, 800U, 1000U);
    for (unsigned int i = 1U; i < RR_VAR_NB_ELEM; i++) {
        feed(120U, 800U, 1000U + i);
    }
    TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_total_time(&rz));

    feed(120U, 800U, 2000U);
    TEST_ASSERT_EQUAL_UINT32(1U, rr_zone_get_total_time(&rz));
}

/* ---- the RMSSD itself ---- */

static void test_a_steady_heart_gives_no_variability(void)
{
    feed(120U, 800U, 900U);     /* the one that only starts the clock */
    feed_flat(120U, 800U);

    /* 120 bpm is [108, 143): zone 2 */
    TEST_ASSERT_EQUAL_UINT32(1U, rr_zone_get_total_time(&rz));
    TEST_ASSERT_EQUAL_UINT32(1U, rr_zone_get_time(&rz, 2U));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rr_zone_get_value(&rz, 2U));
}

static void test_the_denominator_is_the_buffer_and_not_the_differences(void)
{
    /*
     * This is the one number that separates the legacy from a textbook
     * RMSSD, so it gets a case of its own. With the RR alternating by
     * 40 ms, all 19 differences are +/-40, so
     *
     *   sum_sq = 19 x 1600 = 30400
     *   legacy: sqrt(30400 / 20) = sqrt(1520) = 38.9872
     *   textbook (19 differences): sqrt(30400 / 19) = 40 exactly
     *
     * If the divisor ever becomes the difference count, the value below
     * turns into 40 and this fails.
     */
    feed(120U, 800U, 900U);
    for (unsigned int i = 0U; i < RR_VAR_NB_ELEM; i++) {
        feed(120U, ((i % 2U) == 0U) ? 800U : 840U, 1000U + i);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 38.9872f, rr_zone_get_value(&rz, 2U));
}

static void test_a_single_jump_in_the_middle(void)
{
    /*
     * One step of 100 ms and nothing else: two differences of +/-100,
     * sum_sq = 20000, sqrt(20000 / 20) = sqrt(1000) = 31.6228.
     */
    feed(120U, 800U, 900U);
    for (unsigned int i = 0U; i < RR_VAR_NB_ELEM; i++) {
        feed(120U, (i == 10U) ? 900U : 800U, 1000U + i);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 31.6228f, rr_zone_get_value(&rz, 2U));
}

/* ---- the zones ---- */

static void test_each_limit_of_the_legacy(void)
{
    /* -1000, 70, 108, 143, 161, 178, 1000: the bin holds [low, high) */
    static const struct {
        uint8_t bpm;
        uint8_t zone;
    } cases[] = {
        {0U, 0U}, {69U, 0U},        /* below 70 */
        {70U, 1U}, {107U, 1U},      /* 70 to 107 */
        {108U, 2U}, {142U, 2U},     /* 108 to 142 */
        {143U, 3U}, {160U, 3U},     /* 143 to 160 */
        {161U, 4U}, {177U, 4U},     /* 161 to 177 */
        {178U, 5U}, {220U, 5U},     /* 178 and up */
    };

    for (unsigned int c = 0U; c < (sizeof(cases) / sizeof(cases[0])); c++) {
        rr_zone_init(&rz);
        feed(cases[c].bpm, 800U, 900U);
        feed_flat(cases[c].bpm, 800U);

        TEST_ASSERT_EQUAL_UINT32(1U, rr_zone_get_time(&rz, cases[c].zone));
        TEST_ASSERT_EQUAL_UINT8(cases[c].zone, rr_zone_get_current(&rz));
    }
}

static void test_the_bin_is_the_heart_rate_of_the_last_sample(void)
{
    /*
     * The legacy bins on `hrm_info.bpm` of the sample that filled the
     * buffer, not on an average of the twenty. A rider whose heart climbs
     * through the whole buffer books the RMSSD where they ended up.
     */
    feed(100U, 800U, 900U);
    for (unsigned int i = 0U; i < (RR_VAR_NB_ELEM - 1U); i++) {
        feed(100U, 800U, 1000U + i);
    }
    feed(150U, 800U, 2000U);    /* zone 3, not zone 1 */

    TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_time(&rz, 1U));
    TEST_ASSERT_EQUAL_UINT32(1U, rr_zone_get_time(&rz, 3U));
}

static void test_a_zone_reads_back_as_its_average(void)
{
    /* two buffers in the same zone, 0 and then 31.6228: the average */
    feed(120U, 800U, 900U);
    feed_flat(120U, 800U);
    for (unsigned int i = 0U; i < RR_VAR_NB_ELEM; i++) {
        feed(120U, (i == 10U) ? 900U : 800U, 2000U + i);
    }

    TEST_ASSERT_EQUAL_UINT32(2U, rr_zone_get_time(&rz, 2U));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 31.6228f / 2.0f, rr_zone_get_value(&rz, 2U));
}

/* ---- reading the whole thing ---- */

static void test_the_totals_across_the_zones(void)
{
    feed(80U, 800U, 900U);
    feed_flat(80U, 800U);                   /* zone 1 */
    feed_flat(150U, 800U);                  /* zone 3 */
    for (unsigned int i = 0U; i < RR_VAR_NB_ELEM; i++) {
        feed(150U, (i == 10U) ? 900U : 800U, 3000U + i);
    }                                       /* zone 3 again, with a jump */

    TEST_ASSERT_EQUAL_UINT32(3U, rr_zone_get_total_time(&rz));
    TEST_ASSERT_EQUAL_UINT32(2U, rr_zone_get_max_time(&rz));   /* zone 3, fed twice */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 31.6228f / 2.0f, rr_zone_get_max_value(&rz));
}

static void test_reset_empties_it(void)
{
    feed(120U, 800U, 900U);
    feed_flat(120U, 800U);
    TEST_ASSERT_EQUAL_UINT32(1U, rr_zone_get_total_time(&rz));

    rr_zone_reset(&rz);

    TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_total_time(&rz));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rr_zone_get_value(&rz, 3U));

    /* and the clock too: the first sample after a reset is thrown away */
    feed_flat(120U, 800U);
    TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_total_time(&rz));
}

static void test_the_buffer_never_runs_past_its_end(void)
{
    /* many buffers in a row must not walk off rr_buffer[RR_VAR_NB_ELEM] */
    feed(120U, 800U, 900U);
    for (unsigned int i = 0U; i < (RR_VAR_NB_ELEM * 10U); i++) {
        feed(120U, (uint16_t)(700U + (i % 200U)), 1000U + i);
    }

    TEST_ASSERT_EQUAL_UINT32(10U, rr_zone_get_total_time(&rz));
}

static void test_nothing_blows_up_without_a_zone_set(void)
{
    hrm_info_t h = {.bpm = 120U, .rr_interval = 800U, .timestamp = 1000U};

    rr_zone_init(NULL);
    rr_zone_reset(NULL);
    rr_zone_add_data(NULL, &h);
    rr_zone_add_data(&rz, NULL);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rr_zone_get_value(NULL, 0U));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rr_zone_get_value(&rz, RR_ZONES_NB));
    TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_time(&rz, RR_ZONES_NB));
    TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_total_time(NULL));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rr_zone_get_max_value(NULL));
    TEST_ASSERT_EQUAL_UINT32(0U, rr_zone_get_max_time(NULL));
    TEST_ASSERT_EQUAL_UINT8(0U, rr_zone_get_current(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_fresh_one_holds_nothing);
    RUN_TEST(test_a_sample_without_a_clock_is_ignored);
    RUN_TEST(test_a_sample_without_a_clock_is_ignored_mid_ride_too);
    RUN_TEST(test_the_very_first_sample_only_starts_the_clock);
    RUN_TEST(test_a_steady_heart_gives_no_variability);
    RUN_TEST(test_the_denominator_is_the_buffer_and_not_the_differences);
    RUN_TEST(test_a_single_jump_in_the_middle);
    RUN_TEST(test_each_limit_of_the_legacy);
    RUN_TEST(test_the_bin_is_the_heart_rate_of_the_last_sample);
    RUN_TEST(test_a_zone_reads_back_as_its_average);
    RUN_TEST(test_the_totals_across_the_zones);
    RUN_TEST(test_reset_empties_it);
    RUN_TEST(test_the_buffer_never_runs_past_its_end);
    RUN_TEST(test_nothing_blows_up_without_a_zone_set);

    return UNITY_END();
}
