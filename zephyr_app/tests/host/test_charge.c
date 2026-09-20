/**
 * @file test_charge.c
 * @brief The charge machine (src/svc/power/charge.c)
 *
 * Reference: docs/16 (Carga) and the nPM1300 datasheet registers
 * BCHGCHARGESTATUS (bit 1 completed, 2 trickle, 3 constant current,
 * 4 constant voltage, 6 charging paused by die temperature), BCHGERRREASON
 * and NTCSTATUS (bit 0 cold, 3 hot). New in the port: the V3 had no
 * charger telemetry.
 */

#include "unity.h"

#include "app/app_events.h"
#include "svc/charge.h"

static charge_inputs_t in;

void setUp(void)
{
    in = (charge_inputs_t){0};
}

void tearDown(void)
{
}

static void test_no_vbus_no_sun_is_the_battery(void)
{
    TEST_ASSERT_EQUAL(CHARGE_BATTERY, charge_state(&in));
    TEST_ASSERT_EQUAL_UINT8(APP_CHARGE_NONE, charge_app(CHARGE_BATTERY));
}

static void test_the_sun_without_vbus_is_solar(void)
{
    in.solar = true;
    TEST_ASSERT_EQUAL(CHARGE_SOLAR, charge_state(&in));
    TEST_ASSERT_EQUAL_UINT8(APP_CHARGE_SOLAR, charge_app(CHARGE_SOLAR));
}

static void test_vbus_wins_over_the_sun(void)
{
    /* the hardware blocks the solar charger with VBUS (DIS_STO_CH) */
    in.vbus = true;
    in.solar = true;
    in.status = CHARGE_STATUS_CC;
    TEST_ASSERT_EQUAL(CHARGE_USB, charge_state(&in));
}

static void test_each_charging_phase_is_usb(void)
{
    in.vbus = true;
    in.status = CHARGE_STATUS_TRICKLE;
    TEST_ASSERT_EQUAL(CHARGE_USB, charge_state(&in));
    in.status = CHARGE_STATUS_CC;
    TEST_ASSERT_EQUAL(CHARGE_USB, charge_state(&in));
    in.status = CHARGE_STATUS_CV;
    TEST_ASSERT_EQUAL(CHARGE_USB, charge_state(&in));
    TEST_ASSERT_EQUAL_UINT8(APP_CHARGE_USB, charge_app(CHARGE_USB));
}

static void test_completed_is_full(void)
{
    in.vbus = true;
    in.status = CHARGE_STATUS_COMPLETED | 0x01U;   /* battery detected too */
    TEST_ASSERT_EQUAL(CHARGE_USB_FULL, charge_state(&in));
    TEST_ASSERT_EQUAL_UINT8(APP_CHARGE_USB_FULL, charge_app(CHARGE_USB_FULL));
}

static void test_cold_or_hot_cell_pauses(void)
{
    in.vbus = true;
    in.ntc = CHARGE_NTC_COLD;
    TEST_ASSERT_EQUAL(CHARGE_THERMAL_PAUSE, charge_state(&in));
    in.ntc = CHARGE_NTC_HOT;
    TEST_ASSERT_EQUAL(CHARGE_THERMAL_PAUSE, charge_state(&in));
}

static void test_cool_or_warm_cell_keeps_charging(void)
{
    /* JEITA cool and warm only lower the current or the voltage */
    in.vbus = true;
    in.status = CHARGE_STATUS_CC;
    in.ntc = 0x02U | 0x04U;
    TEST_ASSERT_EQUAL(CHARGE_USB, charge_state(&in));
    in.status = 0U;
    TEST_ASSERT_EQUAL(CHARGE_USB, charge_state(&in));
}

static void test_hot_die_pauses(void)
{
    in.vbus = true;
    in.status = CHARGE_STATUS_DIE_HOT;
    TEST_ASSERT_EQUAL(CHARGE_THERMAL_PAUSE, charge_state(&in));
}

static void test_an_error_is_a_fault_whatever_else(void)
{
    in.vbus = true;
    in.error = 0x20U;           /* charge timer timeout */
    in.status = CHARGE_STATUS_CC | CHARGE_STATUS_COMPLETED;
    TEST_ASSERT_EQUAL(CHARGE_FAULT, charge_state(&in));
    TEST_ASSERT_EQUAL_UINT8(APP_CHARGE_USB, charge_app(CHARGE_FAULT));
    TEST_ASSERT_EQUAL_UINT8(APP_CHARGE_USB, charge_app(CHARGE_THERMAL_PAUSE));
}

static void test_without_vbus_errors_do_not_show(void)
{
    /* the error stays latched in the nPM1300 until cleared; unplugged, it is the battery */
    in.error = 0x20U;
    TEST_ASSERT_EQUAL(CHARGE_BATTERY, charge_state(&in));
}

static void test_vbus_just_plugged_is_usb(void)
{
    in.vbus = true;
    TEST_ASSERT_EQUAL(CHARGE_USB, charge_state(&in));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_vbus_no_sun_is_the_battery);
    RUN_TEST(test_the_sun_without_vbus_is_solar);
    RUN_TEST(test_vbus_wins_over_the_sun);
    RUN_TEST(test_each_charging_phase_is_usb);
    RUN_TEST(test_completed_is_full);
    RUN_TEST(test_cold_or_hot_cell_pauses);
    RUN_TEST(test_cool_or_warm_cell_keeps_charging);
    RUN_TEST(test_hot_die_pauses);
    RUN_TEST(test_an_error_is_a_fault_whatever_else);
    RUN_TEST(test_without_vbus_errors_do_not_show);
    RUN_TEST(test_vbus_just_plugged_is_usb);
    return UNITY_END();
}
