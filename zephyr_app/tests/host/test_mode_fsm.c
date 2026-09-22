/**
 * @file test_mode_fsm.c
 * @brief The mode machine (src/model/mode_fsm.c)
 *
 * The machine is not a port of `boucle__change_mode()`
 * (`legacy/source/model/Boucle.cpp:101-142`), so this is not a fidelity
 * test: it is the test of a design. Each of the three rules of
 * `model/mode_fsm.h` has cases here, and each of them was a way to lose a
 * ride before the rule existed:
 *
 * 1. entering PRC without a route left a navigation screen with nothing
 *    to navigate;
 * 2. going from the road to the trainer in the middle of a recording mixed
 *    trainer data into the file of a real ride;
 * 3. entering FEC wiped the power zones and the suffer score the rider had
 *    built over the whole ride. Nothing here touches them any more, and
 *    the test at the end of the file is what keeps it that way.
 *
 * The families (outdoor: CRS, PRC, DBG; indoor: FEC, Zwift) are parent
 * states, so the tests also check that the membership follows the mode.
 */

#include <string.h>

#include "unity.h"

#include "model/mode_fsm.h"

/* ---- what the machine asked of us, written down ---- */

static struct {
    int publishes;
    enum app_mode last_published;
    int route_starts;
    int route_stops;
    int refusals;
    enum app_mode last_refused;
    enum mode_refusal last_why;
} seen;

static void on_publish(enum app_mode mode, void *user)
{
    (void)user;
    seen.publishes++;
    seen.last_published = mode;
}

static void on_route_start(void *user)
{
    (void)user;
    seen.route_starts++;
}

static void on_route_stop(void *user)
{
    (void)user;
    seen.route_stops++;
}

static void on_refused(enum app_mode wanted, enum mode_refusal why, void *user)
{
    (void)user;
    seen.refusals++;
    seen.last_refused = wanted;
    seen.last_why = why;
}

static const struct mode_fsm_ops ops = {
    .publish = on_publish,
    .route_start = on_route_start,
    .route_stop = on_route_stop,
    .refused = on_refused,
};

static struct mode_fsm f;

void setUp(void)
{
    (void)memset(&seen, 0, sizeof(seen));
    mode_fsm_init(&f, &ops);
}

void tearDown(void) {}

/* ---- where it starts ---- */

static void test_it_starts_on_the_free_ride(void)
{
    /* CRS is the only mode that needs nothing: no route, no trainer, no PC */
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_CRS, mode_fsm_mode(&f));
    TEST_ASSERT_TRUE(mode_fsm_is_outdoor(&f));
}

static void test_the_first_entry_publishes_the_mode(void)
{
    /* the interface must not have to ask what mode it is in */
    TEST_ASSERT_EQUAL_INT(1, seen.publishes);
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_CRS, seen.last_published);
}

/* ---- the plain moves ---- */

static void test_going_to_the_trainer_and_back(void)
{
    TEST_ASSERT_TRUE(mode_fsm_select(&f, APP_MODE_ID_FEC));
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_FEC, mode_fsm_mode(&f));
    TEST_ASSERT_FALSE(mode_fsm_is_outdoor(&f));

    TEST_ASSERT_TRUE(mode_fsm_select(&f, APP_MODE_ID_CRS));
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_CRS, mode_fsm_mode(&f));
    TEST_ASSERT_TRUE(mode_fsm_is_outdoor(&f));
}

static void test_the_diagnostics_screen_rides_outdoors(void)
{
    /* DBG is the free ride with another page, not another kind of ride */
    TEST_ASSERT_TRUE(mode_fsm_select(&f, APP_MODE_ID_DBG));
    TEST_ASSERT_TRUE(mode_fsm_is_outdoor(&f));
}

static void test_the_ride_driven_by_a_pc_counts_as_indoors(void)
{
    TEST_ASSERT_TRUE(mode_fsm_select(&f, APP_MODE_ID_ZWIFT));
    TEST_ASSERT_FALSE(mode_fsm_is_outdoor(&f));
}

static void test_each_move_publishes_once(void)
{
    (void)mode_fsm_select(&f, APP_MODE_ID_FEC);
    (void)mode_fsm_select(&f, APP_MODE_ID_ZWIFT);
    (void)mode_fsm_select(&f, APP_MODE_ID_CRS);

    TEST_ASSERT_EQUAL_INT(4, seen.publishes);   /* the first one plus three */
}

/* ---- what is refused ---- */

static void test_a_mode_that_does_not_exist_is_refused(void)
{
    TEST_ASSERT_FALSE(mode_fsm_select(&f, 99));
    TEST_ASSERT_FALSE(mode_fsm_select(&f, -1));
    TEST_ASSERT_EQUAL_INT(2, seen.refusals);
    TEST_ASSERT_EQUAL_INT(MODE_REFUSED_UNKNOWN, seen.last_why);
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_CRS, mode_fsm_mode(&f));
}

static void test_asking_for_the_mode_it_is_already_in_changes_nothing(void)
{
    TEST_ASSERT_FALSE(mode_fsm_select(&f, APP_MODE_ID_CRS));
    TEST_ASSERT_EQUAL_INT(MODE_REFUSED_SAME, seen.last_why);
    TEST_ASSERT_EQUAL_INT(1, seen.publishes);   /* no second publish */
}

/* ---- rule 1: a route mode needs a route ---- */

static void test_the_route_mode_is_refused_without_a_route(void)
{
    TEST_ASSERT_FALSE(mode_fsm_select(&f, APP_MODE_ID_PRC));
    TEST_ASSERT_EQUAL_INT(MODE_REFUSED_NO_ROUTE, seen.last_why);
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_PRC, seen.last_refused);
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_CRS, mode_fsm_mode(&f));
    TEST_ASSERT_EQUAL_INT(0, seen.route_starts);
}

static void test_with_a_route_loaded_the_route_mode_opens_and_starts_it(void)
{
    mode_fsm_set_route_loaded(&f, true);

    TEST_ASSERT_TRUE(mode_fsm_select(&f, APP_MODE_ID_PRC));
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_PRC, mode_fsm_mode(&f));
    TEST_ASSERT_EQUAL_INT(1, seen.route_starts);
}

static void test_leaving_the_route_mode_stops_following_it(void)
{
    mode_fsm_set_route_loaded(&f, true);
    (void)mode_fsm_select(&f, APP_MODE_ID_PRC);

    TEST_ASSERT_TRUE(mode_fsm_select(&f, APP_MODE_ID_CRS));
    TEST_ASSERT_EQUAL_INT(1, seen.route_stops);
}

static void test_the_route_going_away_drops_the_rider_to_the_free_ride(void)
{
    mode_fsm_set_route_loaded(&f, true);
    (void)mode_fsm_select(&f, APP_MODE_ID_PRC);

    /* the rider unloaded it, or the card went away */
    mode_fsm_set_route_loaded(&f, false);

    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_CRS, mode_fsm_mode(&f));
    TEST_ASSERT_EQUAL_INT(1, seen.route_stops);     /* and it stopped following */
}

static void test_a_route_going_away_in_another_mode_moves_nothing(void)
{
    mode_fsm_set_route_loaded(&f, true);
    (void)mode_fsm_select(&f, APP_MODE_ID_FEC);
    mode_fsm_set_route_loaded(&f, false);

    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_FEC, mode_fsm_mode(&f));
}

/* ---- rule 2: a recorded ride does not cross families ---- */

static void test_a_recorded_ride_does_not_jump_to_the_trainer(void)
{
    mode_fsm_set_recording(&f, true);

    TEST_ASSERT_FALSE(mode_fsm_select(&f, APP_MODE_ID_FEC));
    TEST_ASSERT_EQUAL_INT(MODE_REFUSED_RECORDING, seen.last_why);
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_CRS, mode_fsm_mode(&f));
}

static void test_a_recorded_ride_does_not_jump_to_the_road_either(void)
{
    (void)mode_fsm_select(&f, APP_MODE_ID_FEC);
    mode_fsm_set_recording(&f, true);

    TEST_ASSERT_FALSE(mode_fsm_select(&f, APP_MODE_ID_CRS));
    TEST_ASSERT_EQUAL_INT(MODE_REFUSED_RECORDING, seen.last_why);
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_FEC, mode_fsm_mode(&f));
}

static void test_inside_a_family_a_recorded_ride_moves_freely(void)
{
    /*
     * This is the point of drawing the line at the family and not at the
     * mode: a rider recording on the road may want the route or the
     * diagnostics page, and neither changes where the numbers come from.
     */
    mode_fsm_set_route_loaded(&f, true);
    mode_fsm_set_recording(&f, true);

    TEST_ASSERT_TRUE(mode_fsm_select(&f, APP_MODE_ID_PRC));
    TEST_ASSERT_TRUE(mode_fsm_select(&f, APP_MODE_ID_DBG));
    TEST_ASSERT_TRUE(mode_fsm_select(&f, APP_MODE_ID_CRS));
    TEST_ASSERT_EQUAL_INT(0, seen.refusals);

    /* and indoors the same */
    mode_fsm_set_recording(&f, false);
    (void)mode_fsm_select(&f, APP_MODE_ID_FEC);
    mode_fsm_set_recording(&f, true);
    TEST_ASSERT_TRUE(mode_fsm_select(&f, APP_MODE_ID_ZWIFT));
}

static void test_when_the_ride_ends_the_families_open_again(void)
{
    mode_fsm_set_recording(&f, true);
    TEST_ASSERT_FALSE(mode_fsm_select(&f, APP_MODE_ID_FEC));

    mode_fsm_set_recording(&f, false);
    TEST_ASSERT_TRUE(mode_fsm_select(&f, APP_MODE_ID_FEC));
}

static void test_a_refused_move_leaves_the_route_alone(void)
{
    /* a refusal must not run the exit of the mode it is in */
    mode_fsm_set_route_loaded(&f, true);
    (void)mode_fsm_select(&f, APP_MODE_ID_PRC);
    mode_fsm_set_recording(&f, true);

    TEST_ASSERT_FALSE(mode_fsm_select(&f, APP_MODE_ID_FEC));
    TEST_ASSERT_EQUAL_INT(0, seen.route_stops);
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_PRC, mode_fsm_mode(&f));
}

/* ---- rule 3: the machine does not own the numbers of the ride ---- */

static void test_the_machine_asks_for_nothing_but_the_route(void)
{
    /*
     * The whole of rule 3, as a test: across every move the machine can
     * make, the only operations it ever calls are the two of the route and
     * the publish. If someone adds a `zones_reset` to an entry action, this
     * file no longer compiles, which is the point. Before, entering FEC
     * called `power_zone_reset()` and `suffer_score_reset()`, and a rider
     * who put the bike on the trainer lost the zones of the whole ride.
     */
    mode_fsm_set_route_loaded(&f, true);

    (void)mode_fsm_select(&f, APP_MODE_ID_PRC);
    (void)mode_fsm_select(&f, APP_MODE_ID_FEC);
    (void)mode_fsm_select(&f, APP_MODE_ID_ZWIFT);
    (void)mode_fsm_select(&f, APP_MODE_ID_DBG);
    (void)mode_fsm_select(&f, APP_MODE_ID_CRS);

    TEST_ASSERT_EQUAL_INT(6, seen.publishes);       /* the first plus five */
    TEST_ASSERT_EQUAL_INT(1, seen.route_starts);
    TEST_ASSERT_EQUAL_INT(1, seen.route_stops);
    TEST_ASSERT_EQUAL_INT(0, seen.refusals);
}

/* ---- the edges ---- */

static void test_the_family_of_each_mode(void)
{
    TEST_ASSERT_TRUE(mode_is_outdoor(APP_MODE_ID_CRS));
    TEST_ASSERT_TRUE(mode_is_outdoor(APP_MODE_ID_PRC));
    TEST_ASSERT_TRUE(mode_is_outdoor(APP_MODE_ID_DBG));
    TEST_ASSERT_FALSE(mode_is_outdoor(APP_MODE_ID_FEC));
    TEST_ASSERT_FALSE(mode_is_outdoor(APP_MODE_ID_ZWIFT));
}

static void test_nothing_blows_up_without_a_machine(void)
{
    mode_fsm_init(NULL, &ops);
    TEST_ASSERT_FALSE(mode_fsm_select(NULL, APP_MODE_ID_FEC));
    mode_fsm_set_route_loaded(NULL, true);
    mode_fsm_set_recording(NULL, true);
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_CRS, mode_fsm_mode(NULL));
    TEST_ASSERT_FALSE(mode_fsm_is_outdoor(NULL));
}

static void test_nothing_blows_up_without_operations(void)
{
    struct mode_fsm bare;

    mode_fsm_init(&bare, NULL);
    TEST_ASSERT_TRUE(mode_fsm_select(&bare, APP_MODE_ID_FEC));
    TEST_ASSERT_EQUAL_INT(APP_MODE_ID_FEC, mode_fsm_mode(&bare));

    mode_fsm_set_route_loaded(&bare, true);
    TEST_ASSERT_TRUE(mode_fsm_select(&bare, APP_MODE_ID_PRC));
    TEST_ASSERT_TRUE(mode_fsm_select(&bare, APP_MODE_ID_CRS));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_it_starts_on_the_free_ride);
    RUN_TEST(test_the_first_entry_publishes_the_mode);
    RUN_TEST(test_going_to_the_trainer_and_back);
    RUN_TEST(test_the_diagnostics_screen_rides_outdoors);
    RUN_TEST(test_the_ride_driven_by_a_pc_counts_as_indoors);
    RUN_TEST(test_each_move_publishes_once);
    RUN_TEST(test_a_mode_that_does_not_exist_is_refused);
    RUN_TEST(test_asking_for_the_mode_it_is_already_in_changes_nothing);
    RUN_TEST(test_the_route_mode_is_refused_without_a_route);
    RUN_TEST(test_with_a_route_loaded_the_route_mode_opens_and_starts_it);
    RUN_TEST(test_leaving_the_route_mode_stops_following_it);
    RUN_TEST(test_the_route_going_away_drops_the_rider_to_the_free_ride);
    RUN_TEST(test_a_route_going_away_in_another_mode_moves_nothing);
    RUN_TEST(test_a_recorded_ride_does_not_jump_to_the_trainer);
    RUN_TEST(test_a_recorded_ride_does_not_jump_to_the_road_either);
    RUN_TEST(test_inside_a_family_a_recorded_ride_moves_freely);
    RUN_TEST(test_when_the_ride_ends_the_families_open_again);
    RUN_TEST(test_a_refused_move_leaves_the_route_alone);
    RUN_TEST(test_the_machine_asks_for_nothing_but_the_route);
    RUN_TEST(test_the_family_of_each_mode);
    RUN_TEST(test_nothing_blows_up_without_a_machine);
    RUN_TEST(test_nothing_blows_up_without_operations);

    return UNITY_END();
}
