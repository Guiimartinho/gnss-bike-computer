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


/*
 * Relative position, activation and end of a segment
 * (`legacy/source/routes/ListePoints.cpp:239-331` and
 * `legacy/source/routes/Segment.cpp`), with the constants of
 * `legacy/source/routes/Segment.h:21-38`: DIST_ACT 50 m, PSCAL_LIM 0,
 * MARGE_ACT 1,5.
 */

/** A point of the segment, @p east_m metres east and @p north_m north of the base */
static point_t point_at(float east_m, float north_m, float alt, float rtime)
{
    point_t p;

    point_init(&p, BASE_LAT + DEG_LAT(north_m), BASE_LON + DEG_LON(east_m), alt, rtime);

    return p;
}

static void test_a_point_over_the_segment_projects_onto_it(void)
{
    point_t p1 = point_at(0.0f, 0.0f, 200.0f, 0.0f);
    point_t p2 = point_at(40.0f, 0.0f, 210.0f, 20.0f);
    point_t me = point_at(10.0f, 0.0f, 205.0f, 0.0f);
    pos_relative_t rel;

    TEST_ASSERT_TRUE(calculate_relative_position(&me, &p1, &p2, &rel));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 10.0f, rel.x);   /* ten metres along */
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, rel.y);    /* on the line */
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 202.5f, rel.z);  /* altitude interpolated */
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 5.0f, rel.t);    /* time interpolated */
}

static void test_a_point_beside_the_segment_gives_the_side_distance(void)
{
    point_t p1 = point_at(0.0f, 0.0f, 200.0f, 0.0f);
    point_t p2 = point_at(40.0f, 0.0f, 200.0f, 20.0f);
    point_t me = point_at(20.0f, 8.0f, 200.0f, 0.0f);
    pos_relative_t rel;

    TEST_ASSERT_TRUE(calculate_relative_position(&me, &p1, &p2, &rel));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.0f, rel.x);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 8.0f, fabsf(rel.y));
}

static void test_a_point_too_far_falls_back_to_the_first_point(void)
{
    point_t p1 = point_at(0.0f, 0.0f, 200.0f, 0.0f);
    point_t p2 = point_at(40.0f, 0.0f, 210.0f, 20.0f);
    point_t me = point_at(0.0f, 80.0f, 200.0f, 0.0f); /* 80 m away, over the 50 m */
    pos_relative_t rel;

    TEST_ASSERT_FALSE(calculate_relative_position(&me, &p1, &p2, &rel));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rel.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rel.y);
    TEST_ASSERT_EQUAL_FLOAT(200.0f, rel.z);  /* altitude of P1 */
}

static void test_a_point_behind_the_segment_is_outside_the_triangle(void)
{
    point_t p1 = point_at(0.0f, 0.0f, 200.0f, 0.0f);
    point_t p2 = point_at(40.0f, 0.0f, 200.0f, 20.0f);
    point_t me = point_at(-10.0f, 0.0f, 200.0f, 0.0f); /* before the start */
    pos_relative_t rel;

    TEST_ASSERT_FALSE(calculate_relative_position(&me, &p1, &p2, &rel));
}

static void test_null_arguments_do_not_project(void)
{
    point_t p1 = point_at(0.0f, 0.0f, 200.0f, 0.0f);
    pos_relative_t rel;

    TEST_ASSERT_FALSE(calculate_relative_position(NULL, &p1, &p1, &rel));
    TEST_ASSERT_FALSE(calculate_relative_position(&p1, NULL, &p1, &rel));
    TEST_ASSERT_FALSE(calculate_relative_position(&p1, &p1, NULL, &rel));
    TEST_ASSERT_FALSE(calculate_relative_position(&p1, &p1, &p1, NULL));
}

static void test_a_rider_arriving_in_the_right_direction_activates(void)
{
    point_t seg1 = point_at(0.0f, 0.0f, 200.0f, 0.0f);
    point_t seg2 = point_at(100.0f, 0.0f, 200.0f, 30.0f);
    point_t prev = point_at(-5.0f, 3.0f, 200.0f, 0.0f);
    point_t cur = point_at(5.0f, 2.0f, 200.0f, 0.0f);

    /*
     * Past the start and heading for the second point: the legacy asks for
     * the scalar product above the limit and for the Pythagorean geometry,
     * which only holds once the rider is inside the triangle.
     */
    TEST_ASSERT_TRUE(test_segment_activation(&cur, &prev, &seg1, &seg2, 50.0f, 0.0f));

    /* before the start, the geometry does not hold yet */
    point_t before_prev = point_at(-20.0f, 5.0f, 200.0f, 0.0f);
    point_t before_cur = point_at(-10.0f, 2.0f, 200.0f, 0.0f);

    TEST_ASSERT_FALSE(test_segment_activation(&before_cur, &before_prev, &seg1, &seg2, 50.0f,
                                              0.0f));
}

static void test_a_rider_going_the_other_way_does_not_activate(void)
{
    point_t seg1 = point_at(0.0f, 0.0f, 200.0f, 0.0f);
    point_t seg2 = point_at(100.0f, 0.0f, 200.0f, 30.0f);
    point_t prev = point_at(10.0f, 0.0f, 200.0f, 0.0f);
    point_t cur = point_at(2.0f, 0.0f, 200.0f, 0.0f); /* going west */

    TEST_ASSERT_FALSE(test_segment_activation(&cur, &prev, &seg1, &seg2, 50.0f, 0.0f));
}

static void test_a_rider_too_far_from_the_start_does_not_activate(void)
{
    point_t seg1 = point_at(0.0f, 0.0f, 200.0f, 0.0f);
    point_t seg2 = point_at(100.0f, 0.0f, 200.0f, 30.0f);
    point_t prev = point_at(-120.0f, 0.0f, 200.0f, 0.0f);
    point_t cur = point_at(-100.0f, 0.0f, 200.0f, 0.0f); /* 100 m, over the 50 m */

    TEST_ASSERT_FALSE(test_segment_activation(&cur, &prev, &seg1, &seg2, 50.0f, 0.0f));
}

static void test_standing_still_does_not_activate(void)
{
    point_t seg1 = point_at(0.0f, 0.0f, 200.0f, 0.0f);
    point_t seg2 = point_at(100.0f, 0.0f, 200.0f, 30.0f);
    point_t cur = point_at(-5.0f, 0.0f, 200.0f, 0.0f);

    /* movement below one millimetre: no direction to compare */
    TEST_ASSERT_FALSE(test_segment_activation(&cur, &cur, &seg1, &seg2, 50.0f, 0.0f));
    TEST_ASSERT_FALSE(test_segment_activation(NULL, &cur, &seg1, &seg2, 50.0f, 0.0f));
}

static void test_passing_the_last_point_ends_the_segment(void)
{
    point_t last = point_at(0.0f, 0.0f, 200.0f, 100.0f);
    point_t end = point_at(20.0f, 0.0f, 200.0f, 110.0f);
    point_t cur = point_at(25.0f, 0.0f, 200.0f, 0.0f); /* five metres past the end */

    TEST_ASSERT_TRUE(test_segment_deactivation(&cur, &last, &end, 50.0f));
}

static void test_before_the_last_point_the_segment_goes_on(void)
{
    point_t last = point_at(0.0f, 0.0f, 200.0f, 100.0f);
    point_t end = point_at(20.0f, 0.0f, 200.0f, 110.0f);
    point_t cur = point_at(10.0f, 0.0f, 200.0f, 0.0f);

    TEST_ASSERT_FALSE(test_segment_deactivation(&cur, &last, &end, 50.0f));
    TEST_ASSERT_FALSE(test_segment_deactivation(NULL, &last, &end, 50.0f));
}

static void test_far_from_the_end_the_segment_goes_on(void)
{
    point_t last = point_at(0.0f, 0.0f, 200.0f, 100.0f);
    point_t end = point_at(20.0f, 0.0f, 200.0f, 110.0f);
    point_t cur = point_at(20.0f, 80.0f, 200.0f, 0.0f); /* 80 m to the side */

    TEST_ASSERT_FALSE(test_segment_deactivation(&cur, &last, &end, 50.0f));
}

static void test_the_orthogonal_turns_the_vector_by_ninety_degrees(void)
{
    vecteur_t v;
    vecteur_t ortho;

    vecteur_init(&v, 3.0f, 4.0f, 0.0f, 0.0f);
    vecteur_orthogonal(&v, &ortho);

    /* (y, -x): perpendicular, same length */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, vecteur_scalar_product(&v, &ortho));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, vecteur_get_norm(&v), vecteur_get_norm(&ortho));
}

static void test_a_projection_lands_on_the_other_vector(void)
{
    vecteur_t v1;
    vecteur_t v2;
    vecteur_t proj;

    vecteur_init(&v1, 4.0f, 2.0f, 0.0f, 0.0f);
    vecteur_init(&v2, 1.0f, 0.0f, 0.0f, 0.0f);
    vecteur_project(&v1, &v2, &proj);

    /* the legacy multiplies component by component (`Vecteur::project()`) */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, proj.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, proj.y);
}

static void test_a_point_with_a_zero_coordinate_is_not_valid(void)
{
    point_t ok = point_at(0.0f, 0.0f, 200.0f, 0.0f);
    point_t zero;
    point_t far_north;

    point_init(&zero, 0.0f, 0.0f, 0.0f, 0.0f);
    point_init(&far_north, 91.0f, 6.0f, 0.0f, 0.0f);

    TEST_ASSERT_TRUE(point_is_valid(&ok));
    TEST_ASSERT_FALSE(point_is_valid(&zero));
    TEST_ASSERT_FALSE(point_is_valid(&far_north));
    TEST_ASSERT_FALSE(point_is_valid(NULL));
}

static void test_a_two_dimension_point_measures_the_same_distance(void)
{
    point2d_t a;
    point2d_t b;

    point2d_init(&a, BASE_LAT, BASE_LON);
    point2d_init(&b, BASE_LAT + DEG_LAT(100.0f), BASE_LON);

    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, point2d_distance(&a, &b));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, point2d_distance(&a, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_point_over_the_segment_projects_onto_it);
    RUN_TEST(test_a_point_beside_the_segment_gives_the_side_distance);
    RUN_TEST(test_a_point_too_far_falls_back_to_the_first_point);
    RUN_TEST(test_a_point_behind_the_segment_is_outside_the_triangle);
    RUN_TEST(test_null_arguments_do_not_project);
    RUN_TEST(test_a_rider_arriving_in_the_right_direction_activates);
    RUN_TEST(test_a_rider_going_the_other_way_does_not_activate);
    RUN_TEST(test_a_rider_too_far_from_the_start_does_not_activate);
    RUN_TEST(test_standing_still_does_not_activate);
    RUN_TEST(test_passing_the_last_point_ends_the_segment);
    RUN_TEST(test_before_the_last_point_the_segment_goes_on);
    RUN_TEST(test_far_from_the_end_the_segment_goes_on);
    RUN_TEST(test_the_orthogonal_turns_the_vector_by_ninety_degrees);
    RUN_TEST(test_a_projection_lands_on_the_other_vector);
    RUN_TEST(test_a_point_with_a_zero_coordinate_is_not_valid);
    RUN_TEST(test_a_two_dimension_point_measures_the_same_distance);
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
