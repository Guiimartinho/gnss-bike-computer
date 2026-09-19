/**
 * @file test_ui_fmt.c
 * @brief Host tests for ui/ui_fmt.c against legacy Screenutils.cpp.
 *
 * Legacy rules (legacy/source/vue/Screenutils.cpp): _fmkstr() truncates each
 * decimal in float arithmetic and prints "---" above 100000 in absolute
 * value; _secjmkstr() prints HH:MM:SS and " --:--:--" from one day on.
 * Vue::cadran() prints "---" for texts longer than 6 characters and
 * Vue::cadranH() "-----" past 9 (legacy/source/vue/Vue.cpp).
 *
 * Deliberate differences, kept by the tests below:
 *  - NaN prints "---" (the legacy casts it to int, which is undefined);
 *  - an unknown time prints "--:--:--" without the legacy leading space.
 */

#include <math.h>
#include <string.h>

#include "unity.h"

#include "legacy_ref.h"
#include "ui/ui_fmt.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void check_like_legacy(float value, unsigned int digits)
{
    char port[48];
    char legacy[48];

    (void)ui_fmt_float(port, sizeof(port), value, digits);
    legacy_fmkstr(legacy, sizeof(legacy), value, digits);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(legacy, port, "differs from legacy _fmkstr");
}

static void test_floats_print_as_the_legacy_over_a_sweep(void)
{
    for (int32_t i = -25000; i <= 25000; i += 37) {
        float v = (float)i / 100.0f;

        check_like_legacy(v, 0U);
        check_like_legacy(v, 1U);
        check_like_legacy(v, 2U);
    }
}

static void test_decimals_are_truncated_in_float_as_the_legacy(void)
{
    char buf[16];

    /* 0.21f is 0.2099999..., so two digits print 0.20 */
    TEST_ASSERT_EQUAL_STRING("0.20", ui_fmt_float(buf, sizeof(buf), 0.21f, 2U));
    /* 23.4f is 23.3999996... */
    TEST_ASSERT_EQUAL_STRING("23.39", ui_fmt_float(buf, sizeof(buf), 23.4f, 2U));
    TEST_ASSERT_EQUAL_STRING("18.2", ui_fmt_float(buf, sizeof(buf), 18.2f, 1U));
    TEST_ASSERT_EQUAL_STRING("7", ui_fmt_float(buf, sizeof(buf), 7.9f, 0U));
}

static void test_small_negative_values_keep_their_sign(void)
{
    char buf[16];

    TEST_ASSERT_EQUAL_STRING("-0.5", ui_fmt_float(buf, sizeof(buf), -0.5f, 1U));
    TEST_ASSERT_EQUAL_STRING("-1.2", ui_fmt_float(buf, sizeof(buf), -1.25f, 1U));
    check_like_legacy(-0.05f, 2U);
}

static void test_values_past_100000_print_three_dashes(void)
{
    char buf[16];

    TEST_ASSERT_EQUAL_STRING("100000.0", ui_fmt_float(buf, sizeof(buf), 100000.0f, 1U));
    TEST_ASSERT_EQUAL_STRING("---", ui_fmt_float(buf, sizeof(buf), 100001.0f, 1U));
    TEST_ASSERT_EQUAL_STRING("---", ui_fmt_float(buf, sizeof(buf), -250000.0f, 0U));
    check_like_legacy(100001.0f, 1U);
}

static void test_nan_prints_three_dashes_unlike_the_legacy(void)
{
    char buf[16];

    TEST_ASSERT_EQUAL_STRING("---", ui_fmt_float(buf, sizeof(buf), NAN, 1U));
}

static void test_a_small_buffer_is_cut_and_terminated(void)
{
    char buf[4];

    (void)ui_fmt_float(buf, sizeof(buf), 12345.678f, 2U);
    TEST_ASSERT_EQUAL_UINT32(3U, (uint32_t)strlen(buf));
}

static void test_times_print_as_the_legacy_within_the_day(void)
{
    char port[16];
    char legacy[16];

    for (uint32_t s = 0U; s < 86400U; s += 97U) {
        (void)ui_fmt_hms(port, sizeof(port), s, ':');
        legacy_secjmkstr(legacy, sizeof(legacy), s, ':');
        TEST_ASSERT_EQUAL_STRING(legacy, port);
    }
}

static void test_an_unknown_time_prints_dashes_without_the_leading_space(void)
{
    char buf[16];

    TEST_ASSERT_EQUAL_STRING("--:--:--", ui_fmt_hms(buf, sizeof(buf), 86400U, ':'));
    TEST_ASSERT_EQUAL_STRING("--:--", ui_fmt_hm(buf, sizeof(buf), 90000U));
    TEST_ASSERT_EQUAL_STRING("07:42", ui_fmt_hm(buf, sizeof(buf), (7U * 3600U) + (42U * 60U) + 59U));
}

static void test_cadran_texts_past_their_width_print_dashes(void)
{
    TEST_ASSERT_EQUAL_STRING("123456", ui_fmt_cadran("123456", 6U));
    TEST_ASSERT_EQUAL_STRING("---", ui_fmt_cadran("1234567", 6U));
    TEST_ASSERT_EQUAL_STRING("00:42:17", ui_fmt_cadran("00:42:17", 9U));
    TEST_ASSERT_EQUAL_STRING("-----", ui_fmt_cadran("0123456789", 9U));
    TEST_ASSERT_EQUAL_STRING("", ui_fmt_cadran(NULL, 6U));
}

static void test_signed_values_carry_their_sign_and_unit(void)
{
    char buf[24];

    TEST_ASSERT_EQUAL_STRING("+12.3 s", ui_fmt_signed(buf, sizeof(buf), 12.4f, 1U, "s"));
    TEST_ASSERT_EQUAL_STRING("-72.1 s", ui_fmt_signed(buf, sizeof(buf), -72.2f, 1U, "s"));
    TEST_ASSERT_EQUAL_STRING("+0.0", ui_fmt_signed(buf, sizeof(buf), 0.0f, 1U, ""));
    TEST_ASSERT_EQUAL_STRING("---", ui_fmt_signed(buf, sizeof(buf), 1.0e6f, 1U, "s"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_floats_print_as_the_legacy_over_a_sweep);
    RUN_TEST(test_decimals_are_truncated_in_float_as_the_legacy);
    RUN_TEST(test_small_negative_values_keep_their_sign);
    RUN_TEST(test_values_past_100000_print_three_dashes);
    RUN_TEST(test_nan_prints_three_dashes_unlike_the_legacy);
    RUN_TEST(test_a_small_buffer_is_cut_and_terminated);
    RUN_TEST(test_times_print_as_the_legacy_within_the_day);
    RUN_TEST(test_an_unknown_time_prints_dashes_without_the_leading_space);
    RUN_TEST(test_cadran_texts_past_their_width_print_dashes);
    RUN_TEST(test_signed_values_carry_their_sign_and_unit);
    return UNITY_END();
}
