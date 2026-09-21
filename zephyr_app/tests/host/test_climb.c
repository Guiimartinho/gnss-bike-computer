/**
 * @file test_climb.c
 * @brief The climbs of a route (src/model/climb.c)
 *
 * The legacy shows the whole route and the total climb, and says nothing
 * about the climb the rider is on. These tests cover what the port adds:
 * where a climb starts and stops, what counts as one and what does not,
 * and what is left of the one being ridden.
 *
 * The routes here are built by hand so that every number is known before
 * the code runs: a bridge that is not a climb, a pass with a false flat
 * in the middle, a col that ends the route, and a mountain stage.
 */

#include <math.h>
#include <string.h>

#include "unity.h"

#include "model/climb.h"

#define PTS_MAX     4096U

static float pt_dist[PTS_MAX];
static float pt_alt[PTS_MAX];
static uint16_t pt_n;
static struct climb_list list;
static struct climb_state st;

static void point_of(uint16_t index, float *dist_m, float *alt_m, void *user)
{
    (void)user;
    *dist_m = pt_dist[index];
    *alt_m = pt_alt[index];
}

void setUp(void)
{
    pt_n = 0U;
    (void)memset(&list, 0, sizeof(list));
    (void)memset(&st, 0, sizeof(st));
}

void tearDown(void) {}

/** Add a stretch: @p len_m long, rising (or falling) by @p rise_m */
static void leg(float len_m, float rise_m)
{
    float d0 = (pt_n > 0U) ? pt_dist[pt_n - 1U] : 0.0f;
    float a0 = (pt_n > 0U) ? pt_alt[pt_n - 1U] : 0.0f;

    if (pt_n == 0U) {
        pt_dist[0] = 0.0f;
        pt_alt[0] = 0.0f;
        pt_n = 1U;
        a0 = 0.0f;
    }

    /* a point every 25 m, which is about what a route from Strava has */
    unsigned int steps = (unsigned int)(len_m / 25.0f);

    if (steps < 1U) {
        steps = 1U;
    }
    for (unsigned int i = 1U; i <= steps; i++) {
        TEST_ASSERT_TRUE(pt_n < PTS_MAX);
        pt_dist[pt_n] = d0 + ((len_m * (float)i) / (float)steps);
        pt_alt[pt_n] = a0 + ((rise_m * (float)i) / (float)steps);
        pt_n++;
    }
}

/** Start a route at a given altitude */
static void start_at(float alt_m)
{
    pt_dist[0] = 0.0f;
    pt_alt[0] = alt_m;
    pt_n = 1U;
}

static uint8_t find(void)
{
    return climb_find(&list, pt_n, point_of, NULL);
}

static void test_a_flat_route_has_no_climb(void)
{
    start_at(100.0f);
    leg(20000.0f, 0.0f);

    TEST_ASSERT_EQUAL_UINT8(0U, find());
    climb_update(&st, &list, 5000.0f, 100.0f);
    TEST_ASSERT_FALSE(st.on_climb);
    TEST_ASSERT_FALSE(st.have_next);
}

static void test_a_bridge_is_not_a_climb(void)
{
    /* 200 m up at 5 %: steep enough, far too short */
    start_at(50.0f);
    leg(2000.0f, 0.0f);
    leg(200.0f, 10.0f);
    leg(200.0f, -10.0f);
    leg(2000.0f, 0.0f);

    TEST_ASSERT_EQUAL_UINT8(0U, find());
}

static void test_a_long_drag_at_one_percent_is_not_a_climb(void)
{
    /* five kilometres and fifty metres of gain, but only 1 % */
    start_at(0.0f);
    leg(5000.0f, 50.0f);
    leg(2000.0f, -50.0f);

    TEST_ASSERT_EQUAL_UINT8(0U, find());
}

static void test_a_real_climb_is_found_with_its_numbers(void)
{
    /* two kilometres of flat, then five kilometres at 6 %, then down */
    start_at(200.0f);
    leg(2000.0f, 0.0f);
    leg(5000.0f, 300.0f);
    leg(3000.0f, -300.0f);

    TEST_ASSERT_EQUAL_UINT8(1U, find());

    const struct climb *c = &list.c[0];

    TEST_ASSERT_FLOAT_WITHIN(60.0f, 2000.0f, c->start_m);
    TEST_ASSERT_FLOAT_WITHIN(60.0f, 7000.0f, c->end_m);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 200.0f, c->bottom_alt_m);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 500.0f, c->top_alt_m);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 300.0f, climb_gain(c));
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 6.0f, climb_grade(c));
    /* 300 m of gain is a third category */
    TEST_ASSERT_EQUAL_UINT8(CLIMB_CAT_3, c->cat);
}

static void test_a_false_flat_does_not_split_a_pass_in_two(void)
{
    /*
     * The Galibier has a dip in the middle. Three kilometres at 7 %, a
     * shallow half kilometre down, then three more at 7 %: one climb.
     */
    start_at(1000.0f);
    leg(1000.0f, 0.0f);
    leg(3000.0f, 210.0f);
    leg(500.0f, -30.0f);
    leg(3000.0f, 210.0f);
    leg(2000.0f, -390.0f);

    TEST_ASSERT_EQUAL_UINT8(1U, find());

    const struct climb *c = &list.c[0];

    TEST_ASSERT_FLOAT_WITHIN(60.0f, 1000.0f, c->start_m);
    TEST_ASSERT_FLOAT_WITHIN(60.0f, 7500.0f, c->end_m);
    /* the top is the top, not the sum of the two rises */
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 1390.0f, c->top_alt_m);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 390.0f, climb_gain(c));
}

static void test_a_long_valley_does_split_two_climbs(void)
{
    /* the same two rises, with three kilometres of descent between them */
    start_at(1000.0f);
    leg(1000.0f, 0.0f);
    leg(3000.0f, 210.0f);
    leg(3000.0f, -200.0f);
    leg(3000.0f, 210.0f);
    leg(2000.0f, -220.0f);

    TEST_ASSERT_EQUAL_UINT8(2U, find());
    TEST_ASSERT_FLOAT_WITHIN(15.0f, 210.0f, climb_gain(&list.c[0]));
    TEST_ASSERT_FLOAT_WITHIN(15.0f, 210.0f, climb_gain(&list.c[1]));
    /* and they come in the order they are ridden */
    TEST_ASSERT_TRUE(list.c[0].start_m < list.c[1].start_m);
}

static void test_a_route_that_ends_on_the_top_still_has_the_climb(void)
{
    start_at(600.0f);
    leg(1000.0f, 0.0f);
    leg(8000.0f, 700.0f);

    TEST_ASSERT_EQUAL_UINT8(1U, find());
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 700.0f, climb_gain(&list.c[0]));
    /* 700 m of gain is a first category */
    TEST_ASSERT_EQUAL_UINT8(CLIMB_CAT_1, list.c[0].cat);
}

static void test_the_categories_follow_the_gain(void)
{
    TEST_ASSERT_EQUAL_UINT8(CLIMB_CAT_NONE, climb_cat_of(79.0f));
    TEST_ASSERT_EQUAL_UINT8(CLIMB_CAT_4, climb_cat_of(80.0f));
    TEST_ASSERT_EQUAL_UINT8(CLIMB_CAT_3, climb_cat_of(160.0f));
    TEST_ASSERT_EQUAL_UINT8(CLIMB_CAT_2, climb_cat_of(320.0f));
    TEST_ASSERT_EQUAL_UINT8(CLIMB_CAT_1, climb_cat_of(640.0f));
    TEST_ASSERT_EQUAL_UINT8(CLIMB_CAT_HC, climb_cat_of(800.0f));
    TEST_ASSERT_EQUAL_UINT8(CLIMB_CAT_NONE, climb_cat_of(NAN));
}

static void test_the_rider_sees_what_is_left_of_the_climb(void)
{
    start_at(200.0f);
    leg(2000.0f, 0.0f);
    leg(5000.0f, 300.0f);
    leg(3000.0f, -300.0f);
    TEST_ASSERT_EQUAL_UINT8(1U, find());

    /* at the foot, nothing done */
    climb_update(&st, &list, 2000.0f, 200.0f);
    TEST_ASSERT_TRUE(st.on_climb);
    TEST_ASSERT_FLOAT_WITHIN(60.0f, 5000.0f, st.remain_m);
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 300.0f, st.remain_gain_m);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 6.0f, st.grade_pct);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 0.0f, st.done_pct);

    /* halfway up */
    climb_update(&st, &list, 4500.0f, 350.0f);
    TEST_ASSERT_TRUE(st.on_climb);
    TEST_ASSERT_FLOAT_WITHIN(60.0f, 2500.0f, st.remain_m);
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 150.0f, st.remain_gain_m);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 50.0f, st.done_pct);

    /* over the top */
    climb_update(&st, &list, 8000.0f, 400.0f);
    TEST_ASSERT_FALSE(st.on_climb);
    TEST_ASSERT_FALSE(st.have_next);
}

static void test_a_rider_slower_than_the_route_still_gets_a_positive_gain(void)
{
    /* the altitude of the rider is above the top: nothing left to climb,
     * and never a negative number on the screen */
    start_at(200.0f);
    leg(2000.0f, 0.0f);
    leg(5000.0f, 300.0f);
    leg(3000.0f, -300.0f);
    (void)find();

    climb_update(&st, &list, 6500.0f, 520.0f);
    TEST_ASSERT_TRUE(st.on_climb);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, st.remain_gain_m);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, st.grade_pct);
}

static void test_without_an_altitude_the_gain_left_comes_from_the_route(void)
{
    start_at(200.0f);
    leg(2000.0f, 0.0f);
    leg(5000.0f, 300.0f);
    leg(3000.0f, -300.0f);
    (void)find();

    climb_update(&st, &list, 4500.0f, NAN);
    TEST_ASSERT_TRUE(st.on_climb);
    /* half the distance left is half the gain left */
    TEST_ASSERT_FLOAT_WITHIN(15.0f, 150.0f, st.remain_gain_m);
}

static void test_between_two_climbs_the_next_one_is_announced(void)
{
    start_at(1000.0f);
    leg(1000.0f, 0.0f);
    leg(3000.0f, 210.0f);
    leg(3000.0f, -200.0f);
    leg(3000.0f, 210.0f);
    leg(2000.0f, -220.0f);
    TEST_ASSERT_EQUAL_UINT8(2U, find());

    /* down in the valley between the two */
    climb_update(&st, &list, 5500.0f, 1110.0f);
    TEST_ASSERT_FALSE(st.on_climb);
    TEST_ASSERT_TRUE(st.have_next);
    TEST_ASSERT_EQUAL_UINT8(1U, st.next_index);
    TEST_ASSERT_FLOAT_WITHIN(100.0f, 1500.0f, st.to_next_m);

    /* and on the first climb, the second is still the next one */
    climb_update(&st, &list, 2000.0f, 1070.0f);
    TEST_ASSERT_TRUE(st.on_climb);
    TEST_ASSERT_EQUAL_UINT8(0U, st.index);
    TEST_ASSERT_TRUE(st.have_next);
    TEST_ASSERT_EQUAL_UINT8(1U, st.next_index);
}

static void test_the_gradient_of_the_next_two_hundred_metres(void)
{
    /* flat, then a steep ramp: the average of the climb hides the ramp */
    start_at(100.0f);
    leg(1000.0f, 0.0f);
    leg(1000.0f, 20.0f);    /* 2 % */
    leg(1000.0f, 120.0f);   /* 12 % */
    leg(2000.0f, -140.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.6f, 0.0f, climb_grade_ahead(pt_n, point_of, NULL, 500.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.6f, 2.0f, climb_grade_ahead(pt_n, point_of, NULL, 1500.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.8f, 12.0f, climb_grade_ahead(pt_n, point_of, NULL, 2500.0f));
    /* past the end of the route there is nothing to look at */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, climb_grade_ahead(pt_n, point_of, NULL, 99000.0f));
}

static void test_a_mountain_stage_keeps_the_climbs_in_order(void)
{
    /* five passes, each bigger than the last */
    start_at(300.0f);
    for (unsigned int i = 1U; i <= 5U; i++) {
        leg(2000.0f, 0.0f);
        leg(1000.0f * (float)i, 70.0f * (float)i);
        leg(1000.0f * (float)i, -70.0f * (float)i);
    }

    TEST_ASSERT_EQUAL_UINT8(5U, find());
    TEST_ASSERT_FALSE(list.overflowed);
    for (uint8_t i = 0U; i < 5U; i++) {
        TEST_ASSERT_FLOAT_WITHIN(10.0f, 70.0f * (float)(i + 1U), climb_gain(&list.c[i]));
        if (i > 0U) {
            TEST_ASSERT_TRUE(list.c[i - 1U].end_m < list.c[i].start_m);
        }
    }
}

static void test_a_route_with_more_climbs_than_fit_keeps_the_big_ones(void)
{
    /* twenty passes, growing: only the sixteen biggest survive */
    start_at(0.0f);
    for (unsigned int i = 1U; i <= 20U; i++) {
        leg(1000.0f, 0.0f);
        leg(1500.0f, 50.0f + (5.0f * (float)i));
        leg(1500.0f, -(50.0f + (5.0f * (float)i)));
    }

    TEST_ASSERT_EQUAL_UINT8(CLIMB_MAX, find());
    TEST_ASSERT_TRUE(list.overflowed);
    /* the smallest kept is bigger than the smallest of the route */
    TEST_ASSERT_TRUE(climb_gain(&list.c[0]) > 55.0f);
    /* and they are still in the order they are ridden */
    for (uint8_t i = 1U; i < list.n; i++) {
        TEST_ASSERT_TRUE(list.c[i - 1U].start_m < list.c[i].start_m);
    }
}

static void test_nothing_blows_up_without_a_route(void)
{
    TEST_ASSERT_EQUAL_UINT8(0U, climb_find(NULL, 10U, point_of, NULL));
    TEST_ASSERT_EQUAL_UINT8(0U, climb_find(&list, 10U, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT8(0U, climb_find(&list, 1U, point_of, NULL));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, climb_grade_ahead(0U, point_of, NULL, 0.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, climb_gain(NULL));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, climb_length(NULL));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, climb_grade(NULL));

    climb_update(NULL, &list, 0.0f, 0.0f);
    climb_update(&st, NULL, 0.0f, 0.0f);
    TEST_ASSERT_FALSE(st.on_climb);
    climb_update(&st, &list, NAN, 0.0f);
    TEST_ASSERT_FALSE(st.on_climb);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_flat_route_has_no_climb);
    RUN_TEST(test_a_bridge_is_not_a_climb);
    RUN_TEST(test_a_long_drag_at_one_percent_is_not_a_climb);
    RUN_TEST(test_a_real_climb_is_found_with_its_numbers);
    RUN_TEST(test_a_false_flat_does_not_split_a_pass_in_two);
    RUN_TEST(test_a_long_valley_does_split_two_climbs);
    RUN_TEST(test_a_route_that_ends_on_the_top_still_has_the_climb);
    RUN_TEST(test_the_categories_follow_the_gain);
    RUN_TEST(test_the_rider_sees_what_is_left_of_the_climb);
    RUN_TEST(test_a_rider_slower_than_the_route_still_gets_a_positive_gain);
    RUN_TEST(test_without_an_altitude_the_gain_left_comes_from_the_route);
    RUN_TEST(test_between_two_climbs_the_next_one_is_announced);
    RUN_TEST(test_the_gradient_of_the_next_two_hundred_metres);
    RUN_TEST(test_a_mountain_stage_keeps_the_climbs_in_order);
    RUN_TEST(test_a_route_with_more_climbs_than_fit_keeps_the_big_ones);
    RUN_TEST(test_nothing_blows_up_without_a_route);

    return UNITY_END();
}
