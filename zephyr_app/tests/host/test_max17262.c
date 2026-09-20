/**
 * @file test_max17262.c
 * @brief Register units of the MAX17262 fuel gauge (max17262_regs.h)
 *
 * Reference: MAX17262 datasheet, Table 2 (capacity 0.5 mAh, percentage
 * 1/256 %, voltage 78.125 uV, current 156.25 uA, temperature 1/256 degC,
 * time 5.625 s), the VEmpty format (Table 3: 10 mV and 40 mV steps, reset
 * value 0xA561 for 3.3 V and 3.88 V) and the ModelCfg format (Table 4).
 */

#include "unity.h"

#include "drivers/fuel_gauge/max17262_regs.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_voltage_step_is_78_125_microvolts(void)
{
    TEST_ASSERT_EQUAL_INT32(4160000, max17262_microvolts(0xD000U));    /* 53248 steps */
    TEST_ASSERT_EQUAL_INT32(78, max17262_microvolts(1U));
    TEST_ASSERT_EQUAL_INT32(5119921, max17262_microvolts(0xFFFFU));
}

static void test_current_is_signed_in_156_25_microamp_steps(void)
{
    TEST_ASSERT_EQUAL_INT32(-100000, max17262_microamps((uint16_t)-640));
    TEST_ASSERT_EQUAL_INT32(600000, max17262_microamps(3840U));
    TEST_ASSERT_EQUAL_INT32(-5120000, max17262_microamps(0x8000U));
}

static void test_percentage_rounds_the_1_256_steps(void)
{
    TEST_ASSERT_EQUAL_UINT8(100U, max17262_percent(0x6400U));
    TEST_ASSERT_EQUAL_UINT8(51U, max17262_percent(0x3280U));       /* 50.5 % */
    TEST_ASSERT_EQUAL_UINT8(0U, max17262_percent(127U));            /* below 0.5 % */
    TEST_ASSERT_EQUAL_UINT8(1U, max17262_percent(128U));
    TEST_ASSERT_EQUAL_UINT8(100U, max17262_percent(0x6600U));      /* 102 %: capped */
}

static void test_capacity_step_is_half_a_milliamp_hour(void)
{
    TEST_ASSERT_EQUAL_UINT32(2000000U, max17262_microamp_hours(4000U));
    TEST_ASSERT_EQUAL_UINT32(500U, max17262_microamp_hours(1U));
}

static void test_time_step_is_5_625_seconds(void)
{
    TEST_ASSERT_EQUAL_UINT32(60U, max17262_minutes(640U));          /* 3600 s */
    TEST_ASSERT_EQUAL_UINT32(6143U, max17262_minutes(0xFFFEU));     /* 102.4 h */
    TEST_ASSERT_EQUAL_UINT32(MAX17262_MINUTES_UNKNOWN, max17262_minutes(0xFFFFU));
}

static void test_temperature_in_tenths_of_a_kelvin(void)
{
    TEST_ASSERT_EQUAL_UINT16(2981U, max17262_deci_kelvin(25U * 256U));    /* 298.15 K */
    TEST_ASSERT_EQUAL_UINT16(2631U, max17262_deci_kelvin((uint16_t)(-10 * 256))); /* 263.15 K */
    TEST_ASSERT_EQUAL_INT16(25, max17262_celsius(25U * 256U + 200U));
    TEST_ASSERT_EQUAL_INT16(-10, max17262_celsius((uint16_t)(-10 * 256)));
}

static void test_design_capacity_counts_half_milliamp_hours(void)
{
    TEST_ASSERT_EQUAL_HEX16(0x0FA0U, max17262_designcap(2000U));
    TEST_ASSERT_EQUAL_HEX16(0x0BB8U, max17262_designcap(1500U));   /* reset value */
}

static void test_termination_current_counts_156_25_microamps(void)
{
    TEST_ASSERT_EQUAL_HEX16(0x0180U, max17262_ichgterm(60U));
    TEST_ASSERT_EQUAL_HEX16(0x0640U, max17262_ichgterm(250U));     /* reset value */
}

static void test_vempty_packs_10_and_40_millivolt_steps(void)
{
    TEST_ASSERT_EQUAL_HEX16(0xA561U, max17262_vempty(3300U, 3880U));   /* reset value */
    TEST_ASSERT_EQUAL_HEX16(0x9661U, max17262_vempty(3000U, 3880U));
}

static void test_modelcfg_sets_vchg_only_above_4_25_volts(void)
{
    TEST_ASSERT_EQUAL_HEX16(0x8000U, max17262_modelcfg(4200U));
    TEST_ASSERT_EQUAL_HEX16(0x8000U, max17262_modelcfg(4250U));
    TEST_ASSERT_EQUAL_HEX16(0x8400U, max17262_modelcfg(4350U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_voltage_step_is_78_125_microvolts);
    RUN_TEST(test_current_is_signed_in_156_25_microamp_steps);
    RUN_TEST(test_percentage_rounds_the_1_256_steps);
    RUN_TEST(test_capacity_step_is_half_a_milliamp_hour);
    RUN_TEST(test_time_step_is_5_625_seconds);
    RUN_TEST(test_temperature_in_tenths_of_a_kelvin);
    RUN_TEST(test_design_capacity_counts_half_milliamp_hours);
    RUN_TEST(test_termination_current_counts_156_25_microamps);
    RUN_TEST(test_vempty_packs_10_and_40_millivolt_steps);
    RUN_TEST(test_modelcfg_sets_vchg_only_above_4_25_volts);
    return UNITY_END();
}
