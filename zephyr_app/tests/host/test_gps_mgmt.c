/**
 * @file test_gps_mgmt.c
 * @brief Host tests for drivers/gps/gps_mgmt.c with a fake UART and GPIO.
 *
 * The rules under test: corrupted lines are dropped; the fix callback runs
 * once per epoch (on the RMC), not once per sentence; an RMC with status V
 * ends the fix at once. The legacy ran the model once per location update
 * (legacy/source/model/Locator.cpp, TASK_EVENT_LOCATION).
 */

#include <string.h>

#include "unity.h"

#include "drivers/gps_mgmt.h"
#include "fake_gps_hal.h"
#include "host_kernel.h"

/* One MediaTek epoch, in the module's output order */
#define GGA_FIX  "$GPGGA,123519.000,4841.5260,N,00611.0640,E,1,08,0.9,212.5,M,47.9,M,,*52"
#define GSA_3D   "$GPGSA,A,3,04,05,09,12,,,,,,,,,2.5,1.3,2.1*3F"
#define GSV_1    "$GPGSV,2,1,08,04,77,040,46,05,33,070,41,09,20,210,35,12,45,300,40*7C"
#define GSV_2    "$GPGSV,2,2,08,17,10,160,,20,05,020,22,25,60,250,38,29,15,330,*76"
#define RMC_FIX  "$GPRMC,123519.200,A,4841.5260,N,00611.0640,E,12.5,054.7,180926,,,A*59"
#define RMC_VOID "$GPRMC,123519.000,V,,,,,,,180926,,,N*44"

static unsigned int s_callbacks;
static gps_data_t s_last;

static void on_fix(const gps_data_t *data)
{
    s_callbacks++;
    s_last = *data;
}

static void push_epoch(void)
{
    fake_uart_push_line(GGA_FIX);
    fake_uart_push_line(GSA_3D);
    fake_uart_push_line(GSV_1);
    fake_uart_push_line(GSV_2);
    fake_uart_push_line(RMC_FIX);
}

void setUp(void)
{
    static bool initialized;

    fake_gps_hal_reset();
    /* M10578-A3 FIX output is high once a fix is obtained (datasheet pin 13) */
    fake_gpio_set_input(HAL_GPIO_GPS_FIX, true);
    host_uptime_set(1000);
    s_callbacks = 0U;
    memset(&s_last, 0, sizeof(s_last));
    /* gps_mgmt has no deinit: initialize once, then re-register each test */
    if (!initialized) {
        TEST_ASSERT_EQUAL(APP_OK, gps_mgmt_init());
        initialized = true;
    }
    (void)gps_mgmt_register_callback(on_fix);
    TEST_ASSERT_EQUAL(APP_OK, gps_mgmt_reset());
}

void tearDown(void)
{
}

static void test_a_full_epoch_triggers_the_fix_callback_once(void)
{
    push_epoch();
    gps_mgmt_process();

    TEST_ASSERT_EQUAL_UINT(1U, s_callbacks);
    TEST_ASSERT_TRUE(s_last.fix_valid);
    TEST_ASSERT_FLOAT_WITHIN(2e-5f, 48.6921f, s_last.location.lat);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 212.5f, s_last.location.alt);      /* from the GGA */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.5f * 1.852f, s_last.location.speed);
    TEST_ASSERT_EQUAL_UINT32(180926U, s_last.datetime.date);
}

static void test_three_epochs_give_three_callbacks(void)
{
    push_epoch();
    push_epoch();
    push_epoch();
    gps_mgmt_process();

    TEST_ASSERT_EQUAL_UINT(3U, s_callbacks);
}

static void test_a_line_with_a_bad_checksum_is_ignored(void)
{
    fake_uart_push_line("$GPRMC,123519.200,A,4841.5260,N,00611.0640,E,12.5,054.7,180926,,,A*58");
    gps_mgmt_process();

    TEST_ASSERT_EQUAL_UINT(0U, s_callbacks);
    TEST_ASSERT_FALSE(gps_mgmt_has_fix());
}

static void test_a_void_rmc_ends_the_fix_at_once(void)
{
    push_epoch();
    gps_mgmt_process();
    TEST_ASSERT_TRUE(gps_mgmt_has_fix());

    fake_uart_push_line(RMC_VOID);
    gps_mgmt_process();

    TEST_ASSERT_FALSE(gps_mgmt_has_fix());
    TEST_ASSERT_EQUAL_UINT(1U, s_callbacks);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_full_epoch_triggers_the_fix_callback_once);
    RUN_TEST(test_three_epochs_give_three_callbacks);
    RUN_TEST(test_a_line_with_a_bad_checksum_is_ignored);
    RUN_TEST(test_a_void_rmc_ends_the_fix_at_once);
    return UNITY_END();
}
