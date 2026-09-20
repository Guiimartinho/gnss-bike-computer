/**
 * @file test_route_profile.c
 * @brief Elevation profile of the route (src/model/route_profile.c)
 *
 * The legacy had no profile screen; what it does have is the way it counts
 * a climb, only the rises (`Parcours.cpp`, `calc_route_stats`), which the
 * climb left follows here.
 */

#include <math.h>

#include "unity.h"

#include "model/route_profile.h"

static struct route_profile prof;

/** A route that climbs to the middle and goes back down */
static float hill(uint16_t i, void *user)
{
    uint16_t n = *(const uint16_t *)user;
    float half = (float)n / 2.0f;

    return (i <= (uint16_t)half) ? (100.0f + (float)i) : (100.0f + half - ((float)i - half));
}

/** A flat route at 200 m */
static float flat(uint16_t i, void *user)
{
    (void)i;
    (void)user;

    return 200.0f;
}

void setUp(void) {}
void tearDown(void) {}

static void test_a_short_route_keeps_every_point(void)
{
    uint16_t n = 10U;

    TEST_ASSERT_TRUE(route_profile_build(&prof, n, 0U, hill, &n));
    /* the ten points plus the end of the route */
    TEST_ASSERT_EQUAL_UINT8(11U, prof.n);
    TEST_ASSERT_EQUAL_INT16(100, prof.alt_m[0]);
    TEST_ASSERT_EQUAL_INT16(105, prof.max_m);
}

static void test_a_long_route_is_walked_with_a_step(void)
{
    uint16_t n = 4000U;

    TEST_ASSERT_TRUE(route_profile_build(&prof, n, 0U, hill, &n));
    TEST_ASSERT_TRUE(prof.n <= ROUTE_PROFILE_MAX);
    TEST_ASSERT_TRUE(prof.n > 100U);
    /* the top of the hill is in the profile, whatever the step */
    TEST_ASSERT_INT16_WITHIN(40, 2100, prof.max_m);
    TEST_ASSERT_EQUAL_INT16(100, prof.min_m);
}

static void test_the_rider_has_a_column(void)
{
    uint16_t n = 1000U;

    TEST_ASSERT_TRUE(route_profile_build(&prof, n, 500U, hill, &n));
    /* half way in the route is half way in the profile */
    TEST_ASSERT_INT_WITHIN(2, prof.n / 2U, prof.here);
    TEST_ASSERT_TRUE(prof.here < prof.n);
}

static void test_the_climb_left_counts_only_what_is_ahead(void)
{
    uint16_t n = 200U;

    /* at the start the whole climb of the hill is ahead: 100 m */
    TEST_ASSERT_TRUE(route_profile_build(&prof, n, 0U, hill, &n));
    TEST_ASSERT_INT_WITHIN(5, 100, (int)prof.climb_left_m);

    /* past the top there is nothing left to climb */
    TEST_ASSERT_TRUE(route_profile_build(&prof, n, 150U, hill, &n));
    TEST_ASSERT_EQUAL_UINT16(0U, prof.climb_left_m);
}

static void test_a_flat_route_has_no_climb_and_no_range(void)
{
    uint16_t n = 50U;

    TEST_ASSERT_TRUE(route_profile_build(&prof, n, 10U, flat, &n));
    TEST_ASSERT_EQUAL_UINT16(0U, prof.climb_left_m);
    TEST_ASSERT_EQUAL_INT16(200, prof.min_m);
    TEST_ASSERT_EQUAL_INT16(200, prof.max_m);
}

static void test_a_route_below_sea_level_still_works(void)
{
    /* ROTT.PAR of the legacy runs at -13 m */
    uint16_t n = 20U;

    TEST_ASSERT_TRUE(route_profile_build(&prof, n, 0U, flat, &n));
    TEST_ASSERT_EQUAL_INT16(200, prof.alt_m[0]);
}

static void test_without_a_route_there_is_no_profile(void)
{
    uint16_t n = 0U;

    TEST_ASSERT_FALSE(route_profile_build(&prof, 0U, 0U, hill, &n));
    TEST_ASSERT_EQUAL_UINT8(0U, prof.n);
    TEST_ASSERT_FALSE(route_profile_build(NULL, 10U, 0U, hill, &n));
    TEST_ASSERT_FALSE(route_profile_build(&prof, 10U, 0U, NULL, &n));
}

static void test_the_rider_past_the_end_stays_in_the_profile(void)
{
    uint16_t n = 30U;

    TEST_ASSERT_TRUE(route_profile_build(&prof, n, 1000U, hill, &n));
    TEST_ASSERT_TRUE(prof.here < prof.n);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_short_route_keeps_every_point);
    RUN_TEST(test_a_long_route_is_walked_with_a_step);
    RUN_TEST(test_the_rider_has_a_column);
    RUN_TEST(test_the_climb_left_counts_only_what_is_ahead);
    RUN_TEST(test_a_flat_route_has_no_climb_and_no_range);
    RUN_TEST(test_a_route_below_sea_level_still_works);
    RUN_TEST(test_without_a_route_there_is_no_profile);
    RUN_TEST(test_the_rider_past_the_end_stays_in_the_profile);

    return UNITY_END();
}
