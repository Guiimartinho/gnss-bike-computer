/**
 * @file test_aem10900.c
 * @brief Register units of the AEM10900 solar charger (aem10900_regs.h)
 *
 * Reference: AEM1090x datasheet DS-AEM1090x-v2.4.0: VOVCH (9.4, Table 20:
 * 0x32 is 4.05 V, 0x2F is 3.88 V, 2.70 V at least), VOVDIS (9.3, Table 18:
 * 0x2D is 3.04 V, 2.51 V at least), STO (9.15: 4.8 V x DATA / 256) and the
 * APM fields of the power meter mode (9.13, Table 32: POWER in bits 18:0,
 * OFFSET in bits 22:19).
 */

#include "unity.h"

#include "drivers/charger/aem10900_regs.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_charge_threshold_of_the_board_is_0x32(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x32U, aem10900_vovch_thresh(4050U));
    TEST_ASSERT_EQUAL_UINT32(4050U, aem10900_vovch_mv(0x32U));
}

static void test_charge_threshold_takes_the_nearest_step(void)
{
    /* 3.90 V of the pins has no I2C step: 3.88 V is the nearest */
    TEST_ASSERT_EQUAL_HEX8(0x2FU, aem10900_vovch_thresh(3900U));
    TEST_ASSERT_EQUAL_UINT32(3881U, aem10900_vovch_mv(0x2FU));
    TEST_ASSERT_EQUAL_HEX8(0x33U, aem10900_vovch_thresh(4100U));      /* 4.106 V */
}

static void test_charge_threshold_limits(void)
{
    TEST_ASSERT_EQUAL_UINT32(2700U, aem10900_vovch_mv(0x00U));        /* forced to 2.7 V */
    TEST_ASSERT_EQUAL_HEX8(0x3FU, aem10900_vovch_thresh(5000U));
    TEST_ASSERT_EQUAL_UINT32(4781U, aem10900_vovch_mv(0x3FU));
    TEST_ASSERT_EQUAL_HEX8(0x00U, aem10900_vovch_thresh(1000U));
}

static void test_discharge_threshold_reset_value_is_3_04_volts(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x2DU, aem10900_vovdis_thresh(3040U));
    TEST_ASSERT_EQUAL_UINT32(3037U, aem10900_vovdis_mv(0x2DU));
    TEST_ASSERT_EQUAL_UINT32(2510U, aem10900_vovdis_mv(0x00U));       /* forced to 2.51 V */
}

static void test_battery_voltage_is_4_8_volts_over_256_steps(void)
{
    TEST_ASSERT_EQUAL_UINT32(3900U, aem10900_sto_mv(208U));
    TEST_ASSERT_EQUAL_UINT32(4781U, aem10900_sto_mv(255U));
    TEST_ASSERT_EQUAL_UINT32(0U, aem10900_sto_mv(0U));
}

static void test_apm_energy_is_power_shifted_by_offset(void)
{
    /* OFFSET 2 (bit 20), POWER 1000 */
    TEST_ASSERT_EQUAL_UINT64(4000U, aem10900_apm_units(0xE8U, 0x03U, 0x10U));
    TEST_ASSERT_EQUAL_UINT64(1000U, aem10900_apm_units(0xE8U, 0x03U, 0x00U));
    /* the largest: 524287 << 15, beyond 32 bits */
    TEST_ASSERT_EQUAL_UINT64(17179836416ULL, aem10900_apm_units(0xFFU, 0xFFU, 0x7FU));
    /* bit 23 is not part of the data */
    TEST_ASSERT_EQUAL_UINT64(1000U, aem10900_apm_units(0xE8U, 0x03U, 0x80U));
}

static void test_power_is_picojoules_per_microsecond(void)
{
    TEST_ASSERT_EQUAL_UINT32(1000U, aem10900_power_uw(128000U, 1000U, 128U));
    TEST_ASSERT_EQUAL_UINT32(0U, aem10900_power_uw(128000U, 0U, 128U));   /* not calibrated */
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, aem10900_power_uw(17179836416ULL, 1000000U, 128U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_charge_threshold_of_the_board_is_0x32);
    RUN_TEST(test_charge_threshold_takes_the_nearest_step);
    RUN_TEST(test_charge_threshold_limits);
    RUN_TEST(test_discharge_threshold_reset_value_is_3_04_volts);
    RUN_TEST(test_battery_voltage_is_4_8_volts_over_256_steps);
    RUN_TEST(test_apm_energy_is_power_shifted_by_offset);
    RUN_TEST(test_power_is_picojoules_per_microsecond);
    return UNITY_END();
}
