/**
 * @file test_power_zone.c
 * @brief Host tests for model/power_zone.c against legacy PowerZone.cpp.
 *
 * Legacy rules (legacy/source/model/PowerZone.cpp): the first sample only
 * stores the timestamp; samples below 50 W or above 1950 W are not binned;
 * zone i holds [lim[i] * FTP, lim[i+1] * FTP) with lim = -100, 0.55, 0.75,
 * 0.90, 1.05, 1.20, 1.50, 100.
 */

#include "unity.h"

#include "model/power_zone.h"

#define FTP 250U

static power_zone_t pz;

void setUp(void)
{
    power_zone_init(&pz, FTP);
}

void tearDown(void)
{
}

static void test_the_first_sample_only_starts_the_clock(void)
{
    power_zone_add_data(&pz, 200U, 1000U);

    TEST_ASSERT_EQUAL_UINT32(0U, power_zone_get_total_time(&pz));
}

static void test_each_power_lands_in_the_legacy_zone(void)
{
    /* Upper bounds at FTP 250 W: 137.5, 187.5, 225, 262.5, 300, 375 W. */
    static const struct {
        uint16_t watts;
        uint8_t zone;
    } cases[] = {
        { 60U, 0U }, { 137U, 0U }, { 138U, 1U }, { 187U, 1U }, { 188U, 2U },
        { 224U, 2U }, { 225U, 3U }, { 262U, 3U }, { 263U, 4U }, { 299U, 4U },
        { 300U, 5U }, { 374U, 5U }, { 375U, 6U }, { 1900U, 6U },
    };

    for (unsigned int i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++) {
        power_zone_init(&pz, FTP);
        power_zone_add_data(&pz, cases[i].watts, 1000U);
        power_zone_add_data(&pz, cases[i].watts, 3000U);

        TEST_ASSERT_EQUAL_UINT8_MESSAGE(cases[i].zone, power_zone_get_current(&pz), "zone");
        TEST_ASSERT_EQUAL_UINT32(2U, power_zone_get_time(&pz, cases[i].zone));
    }
}

static void test_power_outside_50_to_1950_watts_is_not_binned_but_moves_the_clock(void)
{
    power_zone_add_data(&pz, 200U, 1000U);
    power_zone_add_data(&pz, 40U, 5000U);    /* coasting: not binned */
    power_zone_add_data(&pz, 2000U, 9000U);  /* spike: not binned */
    power_zone_add_data(&pz, 200U, 10000U);  /* only the last second counts */

    TEST_ASSERT_EQUAL_UINT32(1U, power_zone_get_total_time(&pz));
}

static void test_time_accumulates_per_zone_and_in_total(void)
{
    uint32_t t = 1000U;

    power_zone_add_data(&pz, 100U, t);
    for (int i = 0; i < 10; i++) {
        t += 1000U;
        power_zone_add_data(&pz, 100U, t);   /* zone 1 */
    }
    for (int i = 0; i < 5; i++) {
        t += 1000U;
        power_zone_add_data(&pz, 320U, t);   /* zone 6 */
    }

    TEST_ASSERT_EQUAL_UINT32(10U, power_zone_get_time(&pz, 0U));
    TEST_ASSERT_EQUAL_UINT32(5U, power_zone_get_time(&pz, 5U));
    TEST_ASSERT_EQUAL_UINT32(15U, power_zone_get_total_time(&pz));
    TEST_ASSERT_EQUAL_UINT32(10U, power_zone_get_max_time(&pz));
}

static void test_reset_keeps_the_ftp(void)
{
    power_zone_add_data(&pz, 100U, 1000U);
    power_zone_add_data(&pz, 100U, 2000U);
    power_zone_reset(&pz);

    TEST_ASSERT_EQUAL_UINT16(FTP, pz.ftp);
    TEST_ASSERT_EQUAL_UINT32(0U, power_zone_get_total_time(&pz));
}

static void test_there_are_seven_zones(void)
{
    TEST_ASSERT_EQUAL_UINT8(7U, power_zone_get_count());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_first_sample_only_starts_the_clock);
    RUN_TEST(test_each_power_lands_in_the_legacy_zone);
    RUN_TEST(test_power_outside_50_to_1950_watts_is_not_binned_but_moves_the_clock);
    RUN_TEST(test_time_accumulates_per_zone_and_in_total);
    RUN_TEST(test_reset_keeps_the_ftp);
    RUN_TEST(test_there_are_seven_zones);
    return UNITY_END();
}
