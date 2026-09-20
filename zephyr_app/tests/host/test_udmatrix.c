/**
 * @file test_udmatrix.c
 * @brief Matrices of the altitude filter against the legacy (model/udmatrix.c)
 *
 * The 3-state Kalman of `legacy/source/model/Attitude.cpp:113-288` runs on
 * the `UDMatrix` of `libraries/kalman/`. Two of its behaviours decide
 * whether alpha zero is ever estimated:
 *
 * - `UDMatrix::ones(v)` (`UDMatrix.cpp:270-280`) writes v in **every**
 *   element, which is what `matP.ones(900)` of `Attitude.cpp:152` wants.
 * - `UDMatrix::bound(min, max)` (`UDMatrix.cpp:296-309`) compares the
 *   **absolute** value, so a covariance of -500 survives; out of range it
 *   keeps the positive bound, sign and all.
 */

#include "unity.h"

#include "model/udmatrix.h"

static udmatrix_t m;

void setUp(void)
{
    udmat_init(&m, 3U, 3U);
}

void tearDown(void) {}

static void test_ones_fills_every_element_as_the_legacy_does(void)
{
    udmat_ones(&m, 900.0f);

    for (uint8_t i = 0U; i < 3U; i++) {
        for (uint8_t j = 0U; j < 3U; j++) {
            TEST_ASSERT_EQUAL_FLOAT(900.0f, udmat_get(&m, i, j));
        }
    }
}

static void test_identity_keeps_only_the_diagonal(void)
{
    udmat_identity(&m, 2.0f);

    TEST_ASSERT_EQUAL_FLOAT(2.0f, udmat_get(&m, 1U, 1U));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, udmat_get(&m, 0U, 1U));
}

static void test_bound_keeps_a_negative_covariance(void)
{
    udmat_zeros(&m);
    udmat_set(&m, 0U, 1U, -500.0f);
    udmat_set(&m, 1U, 0U, -500.0f);

    udmat_bound(&m, 1e-15f, 1e12f);

    /* the port used to turn these into +1e-15 and lose alpha zero */
    TEST_ASSERT_EQUAL_FLOAT(-500.0f, udmat_get(&m, 0U, 1U));
    TEST_ASSERT_EQUAL_FLOAT(-500.0f, udmat_get(&m, 1U, 0U));
}

static void test_bound_lifts_what_is_too_small_in_absolute_value(void)
{
    udmat_zeros(&m);
    udmat_set(&m, 2U, 2U, -1e-20f);

    udmat_bound(&m, 1e-15f, 1e12f);

    /* the legacy writes the minimum, positive, whatever the sign was */
    TEST_ASSERT_EQUAL_FLOAT(1e-15f, udmat_get(&m, 2U, 2U));
    TEST_ASSERT_EQUAL_FLOAT(1e-15f, udmat_get(&m, 0U, 0U));
}

static void test_bound_cuts_what_is_too_large_in_absolute_value(void)
{
    udmat_zeros(&m);
    udmat_set(&m, 0U, 0U, 1e13f);
    udmat_set(&m, 1U, 1U, -1e13f);

    udmat_bound(&m, 1e-15f, 1e12f);

    TEST_ASSERT_EQUAL_FLOAT(1e12f, udmat_get(&m, 0U, 0U));
    TEST_ASSERT_EQUAL_FLOAT(1e12f, udmat_get(&m, 1U, 1U));
}

static void test_a_covariance_inside_the_bounds_is_untouched(void)
{
    udmat_zeros(&m);
    udmat_set(&m, 0U, 0U, 900.0f);
    udmat_set(&m, 0U, 2U, -0.75f);

    udmat_bound(&m, 1e-15f, 1e12f);

    TEST_ASSERT_EQUAL_FLOAT(900.0f, udmat_get(&m, 0U, 0U));
    TEST_ASSERT_EQUAL_FLOAT(-0.75f, udmat_get(&m, 0U, 2U));
}

static void test_a_sum_into_one_of_its_inputs_keeps_both(void)
{
    udmatrix_t q;

    udmat_init(&q, 3U, 3U);
    udmat_zeros(&q);
    udmat_set(&q, 0U, 0U, 0.03f);

    udmat_ones(&m, 900.0f);

    /* the filter does P = P + Q; the result is one of the inputs */
    TEST_ASSERT_TRUE(udmat_add(&m, &q, &m));

    TEST_ASSERT_EQUAL_FLOAT(900.03f, udmat_get(&m, 0U, 0U));
    TEST_ASSERT_EQUAL_FLOAT(900.0f, udmat_get(&m, 1U, 1U));
}

static void test_a_difference_into_one_of_its_inputs_keeps_both(void)
{
    udmatrix_t eye;

    udmat_init(&eye, 3U, 3U);
    udmat_identity(&eye, 1.0f);

    udmat_ones(&m, 2.0f);

    TEST_ASSERT_TRUE(udmat_sub(&eye, &m, &eye));

    TEST_ASSERT_EQUAL_FLOAT(-1.0f, udmat_get(&eye, 0U, 0U));
    TEST_ASSERT_EQUAL_FLOAT(-2.0f, udmat_get(&eye, 0U, 1U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ones_fills_every_element_as_the_legacy_does);
    RUN_TEST(test_identity_keeps_only_the_diagonal);
    RUN_TEST(test_bound_keeps_a_negative_covariance);
    RUN_TEST(test_bound_lifts_what_is_too_small_in_absolute_value);
    RUN_TEST(test_bound_cuts_what_is_too_large_in_absolute_value);
    RUN_TEST(test_a_covariance_inside_the_bounds_is_untouched);
    RUN_TEST(test_a_sum_into_one_of_its_inputs_keeps_both);
    RUN_TEST(test_a_difference_into_one_of_its_inputs_keeps_both);

    return UNITY_END();
}
