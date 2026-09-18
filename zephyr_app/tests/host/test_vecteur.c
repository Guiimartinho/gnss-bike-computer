/**
 * @file test_vecteur.c
 * @brief Host tests for model/vecteur.c against legacy Vecteur/utils.
 */

#include <math.h>

#include "unity.h"

#include "legacy_ref.h"
#include "model/vecteur.h"

/* Nancy, France: the legacy GPX simulation traces were recorded there. */
#define BASE_LAT 48.6921f
#define BASE_LON 6.1844f

/* One metre in degrees at BASE_LAT (approximate, for building test points). */
#define M_PER_DEG_LAT 111132.0f
#define DEG_LAT(m)    ((m) / M_PER_DEG_LAT)
#define DEG_LON(m)    ((m) / (M_PER_DEG_LAT * cosf(BASE_LAT * 0.0174532925f)))

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_distance_is_zero_for_the_same_point(void)
{
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, distance_between(BASE_LAT, BASE_LON, BASE_LAT, BASE_LON));
}

static void test_distance_matches_the_legacy_formula_within_half_a_percent(void)
{
    static const float hops_m[] = { 10.0f, 50.0f, 100.0f, 500.0f, 1000.0f, 5000.0f };

    for (unsigned int i = 0U; i < (sizeof(hops_m) / sizeof(hops_m[0])); i++) {
        /* Diagonal hop: north-east, same distance on both axes. */
        const float lat2 = BASE_LAT + DEG_LAT(hops_m[i]);
        const float lon2 = BASE_LON + DEG_LON(hops_m[i]);
        const float legacy = legacy_distance_between(BASE_LAT, BASE_LON, lat2, lon2);
        const float port = distance_between(BASE_LAT, BASE_LON, lat2, lon2);

        TEST_ASSERT_FLOAT_WITHIN(0.005f * legacy + 0.05f, legacy, port);
    }
}

static void test_distance_of_a_north_hop_is_close_to_the_expected_metres(void)
{
    const float d = distance_between(BASE_LAT, BASE_LON, BASE_LAT + DEG_LAT(100.0f), BASE_LON);

    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, d);
}

static void test_vector_points_east_and_north_with_positive_components(void)
{
    point_t p1;
    point_t p2;
    vecteur_t v;

    point_init(&p1, BASE_LAT, BASE_LON, 200.0f, 0.0f);
    point_init(&p2, BASE_LAT + DEG_LAT(30.0f), BASE_LON + DEG_LON(40.0f), 205.0f, 10.0f);
    vecteur_from_points(&v, &p1, &p2);

    TEST_ASSERT_FLOAT_WITHIN(1.0f, 40.0f, v.x);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 30.0f, v.y);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 50.0f, vecteur_get_norm(&v));
}

static void test_vector_points_west_and_south_with_negative_components(void)
{
    point_t p1;
    point_t p2;
    vecteur_t v;

    point_init(&p1, BASE_LAT, BASE_LON, 0.0f, 0.0f);
    point_init(&p2, BASE_LAT - DEG_LAT(30.0f), BASE_LON - DEG_LON(40.0f), 0.0f, 0.0f);
    vecteur_from_points(&v, &p1, &p2);

    TEST_ASSERT_TRUE(v.x < -39.0f);
    TEST_ASSERT_TRUE(v.y < -29.0f);
}

static void test_vector_components_match_the_legacy_axis_distances(void)
{
    point_t p1;
    point_t p2;
    vecteur_t v;
    const float lat2 = BASE_LAT - DEG_LAT(120.0f);
    const float lon2 = BASE_LON + DEG_LON(75.0f);

    point_init(&p1, BASE_LAT, BASE_LON, 0.0f, 0.0f);
    point_init(&p2, lat2, lon2, 0.0f, 0.0f);
    vecteur_from_points(&v, &p1, &p2);

    /* legacy/source/routes/Vecteur.cpp: Vecteur(Point&, Point&) */
    const float legacy_x = legacy_distance_between(BASE_LAT, BASE_LON, BASE_LAT, lon2);
    const float legacy_y = -legacy_distance_between(BASE_LAT, BASE_LON, lat2, BASE_LON);

    TEST_ASSERT_FLOAT_WITHIN(0.5f, legacy_x, v.x);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, legacy_y, v.y);
}

static void test_scalar_product_uses_only_x_and_y(void)
{
    vecteur_t a;
    vecteur_t b;

    vecteur_init(&a, 3.0f, 4.0f, 100.0f, 7.0f);
    vecteur_init(&b, -2.0f, 5.0f, -50.0f, 9.0f);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 14.0f, vecteur_scalar_product(&a, &b));
}

static void test_normalize_makes_a_unit_vector(void)
{
    vecteur_t v;

    vecteur_init(&v, 30.0f, -40.0f, 0.0f, 0.0f);
    vecteur_normalize(&v);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.6f, v.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -0.8f, v.y);
}

static void test_normalize_leaves_a_vector_shorter_than_1_mm_untouched(void)
{
    vecteur_t v;

    /* legacy Vecteur::norm(): returns early when the norm is below 0.001 */
    vecteur_init(&v, 0.0005f, 0.0f, 0.0f, 0.0f);
    vecteur_normalize(&v);

    TEST_ASSERT_FLOAT_WITHIN(1e-7f, 0.0005f, v.x);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_distance_is_zero_for_the_same_point);
    RUN_TEST(test_distance_matches_the_legacy_formula_within_half_a_percent);
    RUN_TEST(test_distance_of_a_north_hop_is_close_to_the_expected_metres);
    RUN_TEST(test_vector_points_east_and_north_with_positive_components);
    RUN_TEST(test_vector_points_west_and_south_with_negative_components);
    RUN_TEST(test_vector_components_match_the_legacy_axis_distances);
    RUN_TEST(test_scalar_product_uses_only_x_and_y);
    RUN_TEST(test_normalize_makes_a_unit_vector);
    RUN_TEST(test_normalize_leaves_a_vector_shorter_than_1_mm_untouched);
    return UNITY_END();
}
