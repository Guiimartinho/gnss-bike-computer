/**
 * @file test_map_project.c
 * @brief Map windows of the interface (src/model/map_project.c)
 *
 * The rider sits in the middle of the window (500, 500 in per mille) and a
 * metre is the same length on both axes, which is what
 * `legacy/source/display/Zoom.cpp:52-88` does with its two spans. Positions
 * around Nancy (48.6921, 6.1844), as in the other tests.
 */

#include <math.h>

#include "unity.h"

#include "model/map_project.h"

#define BASE_LAT        48.6921f
#define BASE_LON        6.1844f
#define M_PER_DEG_LAT   111132.0f
#define DEG_LAT(m)      ((float)(m) / M_PER_DEG_LAT)
#define DEG_LON(m)      ((float)(m) / (M_PER_DEG_LAT * cosf(BASE_LAT * 0.0174532925f)))

/** The map of the PRC screen: 240 by 107 pixels */
#define WINDOW_ASPECT_PM    2243U

void setUp(void) {}
void tearDown(void) {}

static void test_the_rider_is_in_the_middle(void)
{
    struct map_point p = map_project(BASE_LAT, BASE_LON, BASE_LAT, BASE_LON, 250U,
                                     WINDOW_ASPECT_PM);

    TEST_ASSERT_EQUAL_INT16(500, p.x);
    TEST_ASSERT_EQUAL_INT16(500, p.y);
}

static void test_the_edge_of_the_window_is_the_span(void)
{
    /* the half-span to the east lands on the right edge */
    struct map_point p = map_project(BASE_LAT, BASE_LON + DEG_LON(250.0f), BASE_LAT, BASE_LON,
                                     250U, WINDOW_ASPECT_PM);

    TEST_ASSERT_INT16_WITHIN(10, 1000, p.x);
    TEST_ASSERT_INT16_WITHIN(10, 500, p.y);

    /* and to the west, on the left edge */
    p = map_project(BASE_LAT, BASE_LON - DEG_LON(250.0f), BASE_LAT, BASE_LON, 250U,
                    WINDOW_ASPECT_PM);
    TEST_ASSERT_INT16_WITHIN(10, 0, p.x);
}

static void test_a_metre_is_the_same_on_both_axes(void)
{
    /*
     * The screen stretches the per mille of each axis over its own side, so
     * a hundred metres to the north takes more per mille than a hundred to
     * the east, in the proportion of the window.
     */
    struct map_point east = map_project(BASE_LAT, BASE_LON + DEG_LON(100.0f), BASE_LAT, BASE_LON,
                                        250U, WINDOW_ASPECT_PM);
    struct map_point north = map_project(BASE_LAT + DEG_LAT(100.0f), BASE_LON, BASE_LAT, BASE_LON,
                                         250U, WINDOW_ASPECT_PM);

    int32_t dx = east.x - 500;
    int32_t dy = north.y - 500;

    TEST_ASSERT_INT_WITHIN(5, 200, dx);
    TEST_ASSERT_INT_WITHIN(10, (200 * 2243) / 1000, dy);
}

static void test_the_north_goes_up_and_the_east_goes_right(void)
{
    struct map_point ne = map_project(BASE_LAT + DEG_LAT(50.0f), BASE_LON + DEG_LON(50.0f),
                                      BASE_LAT, BASE_LON, 250U, WINDOW_ASPECT_PM);
    struct map_point sw = map_project(BASE_LAT - DEG_LAT(50.0f), BASE_LON - DEG_LON(50.0f),
                                      BASE_LAT, BASE_LON, 250U, WINDOW_ASPECT_PM);

    TEST_ASSERT_TRUE(ne.x > 500);
    TEST_ASSERT_TRUE(ne.y > 500);
    TEST_ASSERT_TRUE(sw.x < 500);
    TEST_ASSERT_TRUE(sw.y < 500);
}

static void test_a_point_far_away_stays_outside(void)
{
    /* ten kilometres north with a window of 250 m: far above the window */
    struct map_point p = map_project(BASE_LAT + DEG_LAT(10000.0f), BASE_LON, BASE_LAT, BASE_LON,
                                     250U, WINDOW_ASPECT_PM);

    TEST_ASSERT_TRUE(p.y > 1000);
    /* and it still fits in the int16 of the interface */
    TEST_ASSERT_TRUE(p.y <= 32000);
}

static void test_the_zoom_levels_of_the_port(void)
{
    TEST_ASSERT_EQUAL_UINT16(100U, map_span_m(1U));
    /* level 2 is the 250 m of the legacy (BASE_ZOOM_LEVEL) */
    TEST_ASSERT_EQUAL_UINT16(250U, map_span_m(2U));
    TEST_ASSERT_EQUAL_UINT16(2500U, map_span_m(MAP_ZOOM_LEVELS));
    /* out of range falls back to the default of the legacy */
    TEST_ASSERT_EQUAL_UINT16(250U, map_span_m(0U));
    TEST_ASSERT_EQUAL_UINT16(250U, map_span_m(200U));
}

static void test_the_scale_bar_is_a_round_number(void)
{
    uint16_t pm = 0U;

    TEST_ASSERT_EQUAL_UINT16(250U, map_scale_bar(250U, &pm));
    TEST_ASSERT_EQUAL_UINT16(500U, pm); /* half of the width */

    TEST_ASSERT_EQUAL_UINT16(100U, map_scale_bar(100U, &pm));
    TEST_ASSERT_EQUAL_UINT16(500U, pm);

    TEST_ASSERT_EQUAL_UINT16(2000U, map_scale_bar(2500U, &pm));
    TEST_ASSERT_EQUAL_UINT16(400U, pm);

    /* a window smaller than the smallest bar still gives a bar */
    TEST_ASSERT_EQUAL_UINT16(10U, map_scale_bar(5U, &pm));
    /* and the caller may not want the per mille */
    TEST_ASSERT_EQUAL_UINT16(250U, map_scale_bar(250U, NULL));
}

static void test_a_long_list_is_walked_with_a_step(void)
{
    TEST_ASSERT_EQUAL_UINT16(1U, map_stride(50U, 160U));
    TEST_ASSERT_EQUAL_UINT16(1U, map_stride(160U, 160U));
    TEST_ASSERT_EQUAL_UINT16(2U, map_stride(320U, 160U));
    TEST_ASSERT_EQUAL_UINT16(4U, map_stride(500U, 160U));
    TEST_ASSERT_EQUAL_UINT16(1U, map_stride(0U, 160U));
    TEST_ASSERT_EQUAL_UINT16(1U, map_stride(50U, 0U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_rider_is_in_the_middle);
    RUN_TEST(test_the_edge_of_the_window_is_the_span);
    RUN_TEST(test_a_metre_is_the_same_on_both_axes);
    RUN_TEST(test_the_north_goes_up_and_the_east_goes_right);
    RUN_TEST(test_a_point_far_away_stays_outside);
    RUN_TEST(test_the_zoom_levels_of_the_port);
    RUN_TEST(test_the_scale_bar_is_a_round_number);
    RUN_TEST(test_a_long_list_is_walked_with_a_step);

    return UNITY_END();
}
