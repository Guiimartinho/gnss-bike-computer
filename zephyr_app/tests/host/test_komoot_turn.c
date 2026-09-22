/**
 * @file test_komoot_turn.c
 * @brief Komoot's directions on this screen (src/model/komoot_turn.c)
 *
 * Twenty-four names and nine arrows. The mapping is a table plus a handful
 * of judgements, and the judgements are what the tests are about, because
 * getting one wrong points a rider the wrong way at a junction:
 *
 * - a **fork** is a choice of lane and not a corner, so it takes the
 *   slight arrow and not the plain one;
 * - **all three u-turns** are one arrow, because a rider turning round
 *   does not care which side the application thought of;
 * - a **roundabout** takes the plain arrow of the side it is ridden on,
 *   with the exit number left to the text; the counter-clockwise ones are
 *   the left-hand-traffic case;
 * - **out of route** and **take the ferry** get no arrow at all. They are
 *   messages, and an arrow would point somewhere.
 *
 * The last one has its own test because it is the only case where drawing
 * nothing is the right answer, and a table written without thinking would
 * give both of them an arrow.
 */

#include "unity.h"

#include "app/app_events.h"
#include "model/komoot_turn.h"

void setUp(void) {}
void tearDown(void) {}

/* the numbers of komoot_direction_t, as the application sends them */
#define K_NONE 0U
#define K_STRAIGHT 1U
#define K_START 2U
#define K_FINISH 3U
#define K_SLIGHT_LEFT 4U
#define K_LEFT 5U
#define K_SHARP_LEFT 6U
#define K_SLIGHT_RIGHT 7U
#define K_RIGHT 8U
#define K_SHARP_RIGHT 9U
#define K_FORK_LEFT 10U
#define K_FORK_RIGHT 11U
#define K_U_TURN 12U
#define K_U_TURN_LEFT 13U
#define K_U_TURN_RIGHT 14U
#define K_RB_EXIT1 15U
#define K_RB_EXIT2 16U
#define K_RB_EXIT3 17U
#define K_RB_CCW1 18U
#define K_RB_CCW2 19U
#define K_RB_CCW3 20U
#define K_RB_FALLBACK 21U
#define K_OUT_OF_ROUTE 22U
#define K_FERRY 23U

/* ==========================================================================
 * The plain ones
 * ========================================================================== */

static void test_the_turns_that_are_simply_turns(void)
{
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_STRAIGHT, komoot_turn_of(K_STRAIGHT));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_SLIGHT_LEFT, komoot_turn_of(K_SLIGHT_LEFT));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_LEFT, komoot_turn_of(K_LEFT));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_SHARP_LEFT, komoot_turn_of(K_SHARP_LEFT));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_SLIGHT_RIGHT, komoot_turn_of(K_SLIGHT_RIGHT));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_RIGHT, komoot_turn_of(K_RIGHT));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_SHARP_RIGHT, komoot_turn_of(K_SHARP_RIGHT));
}

static void test_the_start_of_a_route_points_straight_on(void)
{
    /* a rider setting off has nothing to turn */
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_STRAIGHT, komoot_turn_of(K_START));
}

static void test_the_end_of_a_route_says_arrive(void)
{
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_ARRIVE, komoot_turn_of(K_FINISH));
}

/* ==========================================================================
 * The judgements
 * ========================================================================== */

static void test_a_fork_is_not_a_corner(void)
{
    /*
     * A fork is a choice of lane at speed. The plain arrow would have the
     * rider braking for a corner that is not there.
     */
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_SLIGHT_LEFT, komoot_turn_of(K_FORK_LEFT));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_SLIGHT_RIGHT, komoot_turn_of(K_FORK_RIGHT));
}

static void test_every_u_turn_is_the_same_arrow(void)
{
    /* the rider turns round; which side the application thought of is noise */
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_UTURN, komoot_turn_of(K_U_TURN));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_UTURN, komoot_turn_of(K_U_TURN_LEFT));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_UTURN, komoot_turn_of(K_U_TURN_RIGHT));
}

static void test_a_roundabout_takes_the_side_it_is_ridden_on(void)
{
    /* which exit goes in the text beside the arrow, not in the arrow */
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_RIGHT, komoot_turn_of(K_RB_EXIT1));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_RIGHT, komoot_turn_of(K_RB_EXIT2));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_RIGHT, komoot_turn_of(K_RB_EXIT3));

    /* counter-clockwise is the left-hand-traffic case */
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_LEFT, komoot_turn_of(K_RB_CCW1));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_LEFT, komoot_turn_of(K_RB_CCW2));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_LEFT, komoot_turn_of(K_RB_CCW3));
}

static void test_a_roundabout_with_no_exit_named_still_gets_an_arrow(void)
{
    /*
     * The fallback means Komoot knew there was a roundabout and not which
     * exit. The rider is still at a roundabout and still needs to know.
     */
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_RIGHT, komoot_turn_of(K_RB_FALLBACK));
    TEST_ASSERT_TRUE(komoot_turn_is_navigation(K_RB_FALLBACK));
}

static void test_the_two_that_are_messages_and_not_turns(void)
{
    /*
     * The case this module exists for. "Out of route" and "take the ferry"
     * are things to read, and an arrow for either would point the rider
     * somewhere they should not go.
     */
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_NONE, komoot_turn_of(K_OUT_OF_ROUTE));
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_NONE, komoot_turn_of(K_FERRY));
    TEST_ASSERT_FALSE(komoot_turn_is_navigation(K_OUT_OF_ROUTE));
    TEST_ASSERT_FALSE(komoot_turn_is_navigation(K_FERRY));
}

static void test_nothing_is_nothing(void)
{
    TEST_ASSERT_EQUAL_UINT8(APP_TURN_NONE, komoot_turn_of(K_NONE));
    TEST_ASSERT_FALSE(komoot_turn_is_navigation(K_NONE));
}

static void test_a_direction_this_firmware_never_heard_of(void)
{
    /* Komoot may add one; an unknown name draws nothing rather than guess */
    for (unsigned int d = 24U; d < 256U; d++) {
        TEST_ASSERT_EQUAL_UINT8(APP_TURN_NONE, komoot_turn_of((uint8_t)d));
        TEST_ASSERT_FALSE(komoot_turn_is_navigation((uint8_t)d));
    }
}

static void test_every_direction_that_is_navigation_has_an_arrow(void)
{
    /*
     * The two halves have to agree: anything the module calls navigation
     * must draw something, or the screen would say a turn is coming and
     * show no arrow.
     */
    for (unsigned int d = 0U; d < 24U; d++) {
        if (komoot_turn_is_navigation((uint8_t)d)) {
            TEST_ASSERT_NOT_EQUAL_UINT8(APP_TURN_NONE, komoot_turn_of((uint8_t)d));
        }
    }
}

/* ==========================================================================
 * The distance
 * ========================================================================== */

static void test_the_distance_passes_through(void)
{
    TEST_ASSERT_EQUAL_UINT16(0U, komoot_turn_distance(0U));
    TEST_ASSERT_EQUAL_UINT16(250U, komoot_turn_distance(250U));
    TEST_ASSERT_EQUAL_UINT16(9999U, komoot_turn_distance(9999U));
}

static void test_a_turn_further_than_the_screen_can_say(void)
{
    /*
     * Komoot sends the distance to the next turn from the moment a route
     * starts, which can be thirty kilometres. Four digits is what there is
     * room for, and a number that wrapped would read as a turn just ahead.
     */
    TEST_ASSERT_EQUAL_UINT16(9999U, komoot_turn_distance(10000U));
    TEST_ASSERT_EQUAL_UINT16(9999U, komoot_turn_distance(30000U));
    TEST_ASSERT_EQUAL_UINT16(9999U, komoot_turn_distance(4294967295U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_turns_that_are_simply_turns);
    RUN_TEST(test_the_start_of_a_route_points_straight_on);
    RUN_TEST(test_the_end_of_a_route_says_arrive);
    RUN_TEST(test_a_fork_is_not_a_corner);
    RUN_TEST(test_every_u_turn_is_the_same_arrow);
    RUN_TEST(test_a_roundabout_takes_the_side_it_is_ridden_on);
    RUN_TEST(test_a_roundabout_with_no_exit_named_still_gets_an_arrow);
    RUN_TEST(test_the_two_that_are_messages_and_not_turns);
    RUN_TEST(test_nothing_is_nothing);
    RUN_TEST(test_a_direction_this_firmware_never_heard_of);
    RUN_TEST(test_every_direction_that_is_navigation_has_an_arrow);
    RUN_TEST(test_the_distance_passes_through);
    RUN_TEST(test_a_turn_further_than_the_screen_can_say);

    return UNITY_END();
}
