/**
 * @file test_radar.c
 * @brief Vehicles coming from behind (src/model/radar.c)
 *
 * The legacy has no radar, so there is nothing to be faithful to: these
 * tests cover the behaviour a rider would notice. The one that matters
 * most is the last: a radar drops a frame now and then, and a mark that
 * blinks on the screen is worse than one that lingers, so a target only
 * goes after RADAR_HOLD_MS with nothing said about it.
 */

#include "unity.h"

#include "model/radar.h"
#include "model/radar_wire.h"

static struct radar r;

void setUp(void)
{
    radar_init(&r);
    radar_set_link(&r, true, 0U);
}

void tearDown(void) {}

/** One frame with the vehicles given as {id, range, closing km/h} */
static void frame(uint32_t now_ms, const uint16_t (*v)[3], uint8_t n)
{
    struct radar_frame f = {.n = n};

    for (uint8_t i = 0U; i < n; i++) {
        f.t[i].id = (uint8_t)v[i][0];
        f.t[i].range_m = v[i][1];
        f.t[i].closing_kmh = v[i][2];
        f.t[i].side = (uint8_t)RADAR_SIDE_UNKNOWN;
        f.t[i].level = (uint8_t)RADAR_LEVEL_NONE;   /* let the model decide */
    }
    radar_feed(&r, &f, now_ms);
}

static void test_the_road_starts_empty(void)
{
    TEST_ASSERT_EQUAL_UINT8(0U, radar_count(&r));
    TEST_ASSERT_NULL(radar_nearest(&r));
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_NONE, radar_worst(&r));
}

static void test_a_car_behind_shows_up_with_its_distance(void)
{
    const uint16_t v[][3] = {{1U, 90U, 30U}};

    frame(1000U, v, 1U);

    TEST_ASSERT_EQUAL_UINT8(1U, radar_count(&r));

    const struct radar_target *t = radar_nearest(&r);

    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL_UINT16(90U, t->range_m);
    TEST_ASSERT_EQUAL_UINT16(30U, t->closing_kmh);
    TEST_ASSERT_TRUE(t->live);
}

static void test_the_nearest_one_comes_first(void)
{
    const uint16_t v[][3] = {{1U, 120U, 20U}, {2U, 40U, 25U}, {3U, 80U, 15U}};

    frame(1000U, v, 3U);

    TEST_ASSERT_EQUAL_UINT8(3U, radar_count(&r));
    TEST_ASSERT_EQUAL_UINT16(40U, radar_nearest(&r)->range_m);
    TEST_ASSERT_EQUAL_UINT16(80U, r.t[1].range_m);
    TEST_ASSERT_EQUAL_UINT16(120U, r.t[2].range_m);
}

static void test_a_car_keeps_its_slot_as_it_closes(void)
{
    const uint16_t a[][3] = {{7U, 140U, 40U}};
    const uint16_t b[][3] = {{7U, 100U, 45U}};
    const uint16_t c[][3] = {{7U, 55U, 45U}};

    frame(1000U, a, 1U);
    frame(2000U, b, 1U);
    frame(3000U, c, 1U);

    /* one vehicle all along, not three */
    TEST_ASSERT_EQUAL_UINT8(1U, radar_count(&r));
    TEST_ASSERT_EQUAL_UINT8(7U, radar_nearest(&r)->id);
    TEST_ASSERT_EQUAL_UINT16(55U, radar_nearest(&r)->range_m);
}

static void test_a_dropped_frame_does_not_blink_the_mark(void)
{
    const uint16_t v[][3] = {{1U, 90U, 30U}};

    frame(1000U, v, 1U);

    /* the radar says nothing for two seconds */
    radar_tick(&r, 3000U);
    /* still there, but no longer solid */
    TEST_ASSERT_EQUAL_UINT8(1U, radar_count(&r));
    TEST_ASSERT_FALSE(r.t[0].live);      /* fading, but still drawn */

    /* and it comes back */
    frame(3200U, v, 1U);
    TEST_ASSERT_EQUAL_UINT8(1U, radar_count(&r));
    TEST_ASSERT_TRUE(r.t[0].live);
}

static void test_a_car_that_goes_away_is_forgotten(void)
{
    const uint16_t v[][3] = {{1U, 90U, 30U}};

    frame(1000U, v, 1U);
    radar_tick(&r, 1000U + RADAR_HOLD_MS);
    TEST_ASSERT_EQUAL_UINT8(1U, radar_count(&r));

    radar_tick(&r, 1000U + RADAR_HOLD_MS + 1U);
    TEST_ASSERT_EQUAL_UINT8(0U, radar_count(&r));
    TEST_ASSERT_NULL(radar_nearest(&r));
}

static void test_a_frame_with_one_car_gone_keeps_the_other(void)
{
    const uint16_t both[][3] = {{1U, 100U, 30U}, {2U, 60U, 40U}};
    const uint16_t one[][3] = {{2U, 30U, 40U}};

    frame(1000U, both, 2U);
    TEST_ASSERT_EQUAL_UINT8(2U, radar_count(&r));

    /* the first car turned off; the second keeps coming */
    frame(2000U, one, 1U);
    TEST_ASSERT_EQUAL_UINT8(2U, radar_count(&r));    /* the first still fades */
    radar_tick(&r, 2000U + RADAR_HOLD_MS + 1U);
    TEST_ASSERT_EQUAL_UINT8(0U, radar_count(&r));
}

static void test_a_reading_out_of_range_is_dropped(void)
{
    const uint16_t v[][3] = {{1U, 0U, 30U}, {2U, RADAR_RANGE_MAX_M + 1U, 30U},
                             {3U, 70U, 20U}};

    frame(1000U, v, 3U);

    /* nothing there, and further than the radar reaches: only one is real */
    TEST_ASSERT_EQUAL_UINT8(1U, radar_count(&r));
    TEST_ASSERT_EQUAL_UINT8(3U, radar_nearest(&r)->id);
}

static void test_the_threat_level_follows_the_distance_and_the_speed(void)
{
    /* close is dangerous whatever the speed */
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_DANGER, radar_level_of(25U, 20U));
    /* and so is very fast, however far */
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_DANGER, radar_level_of(140U, 90U));
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_FAST, radar_level_of(120U, 60U));
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_APPROACHING, radar_level_of(120U, 20U));
    /* nothing there is no threat */
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_NONE, radar_level_of(0U, 60U));
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_NONE, radar_level_of(500U, 60U));
}

static void test_a_level_the_radio_gives_wins_over_the_guess(void)
{
    /* the ANT+ profile carries a level of its own: the model keeps it */
    struct radar_frame f = {.n = 1U};

    f.t[0].id = 1U;
    f.t[0].range_m = 120U;
    f.t[0].closing_kmh = 20U;       /* the guess would say "approaching" */
    f.t[0].level = (uint8_t)RADAR_LEVEL_DANGER;
    radar_feed(&r, &f, 1000U);

    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_DANGER, radar_nearest(&r)->level);
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_DANGER, radar_worst(&r));
}

static void test_the_worst_of_the_road_is_what_the_rider_is_warned_about(void)
{
    const uint16_t v[][3] = {{1U, 150U, 15U}, {2U, 20U, 30U}, {3U, 100U, 20U}};

    frame(1000U, v, 3U);

    /* the one at twenty metres is the one that matters */
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_DANGER, radar_worst(&r));
    TEST_ASSERT_EQUAL_UINT8(2U, radar_nearest(&r)->id);
}

static void test_more_cars_than_the_model_holds(void)
{
    uint16_t v[RADAR_TARGETS_MAX + 4U][3];

    for (unsigned int i = 0U; i < (RADAR_TARGETS_MAX + 4U); i++) {
        v[i][0] = (uint16_t)(i + 1U);
        v[i][1] = (uint16_t)(20U + (i * 10U));
        v[i][2] = 30U;
    }
    /* a frame never carries more than the model holds, but the radio could
     * hand over a longer one: nothing overruns */
    struct radar_frame f = {.n = RADAR_TARGETS_MAX + 4U};

    for (unsigned int i = 0U; i < RADAR_TARGETS_MAX; i++) {
        f.t[i].id = (uint8_t)v[i][0];
        f.t[i].range_m = v[i][1];
        f.t[i].closing_kmh = v[i][2];
    }
    radar_feed(&r, &f, 1000U);

    TEST_ASSERT_EQUAL_UINT8(RADAR_TARGETS_MAX, radar_count(&r));
    TEST_ASSERT_EQUAL_UINT16(20U, radar_nearest(&r)->range_m);
}

static void test_the_radar_going_away_clears_the_road(void)
{
    const uint16_t v[][3] = {{1U, 90U, 30U}};

    frame(1000U, v, 1U);
    TEST_ASSERT_EQUAL_UINT8(1U, radar_count(&r));

    /* the radio lost the radar: what it last saw is no longer true */
    radar_set_link(&r, false, 2000U);
    TEST_ASSERT_EQUAL_UINT8(0U, radar_count(&r));
    TEST_ASSERT_FALSE(r.linked);
}

/* ==========================================================================
 * The wire format of the Varia over BLE
 *
 * Not verified against a radar: Garmin publishes no specification for this
 * service, and what is tested here is the shape the open projects report
 * (`model/radar_wire.h`). The tests still pay for themselves: they hold the
 * bounds, the empty frame and the odd-length packet, which is where a
 * parser of a format nobody documented would hurt.
 * ========================================================================== */

static void test_a_varia_notification_with_two_cars(void)
{
    /* counter, then id/range/speed for each vehicle */
    static const uint8_t pkt[] = {0x10U, 0x01U, 90U, 32U, 0x02U, 45U, 55U};
    struct radar_frame f;

    TEST_ASSERT_TRUE(radar_wire_varia(pkt, sizeof(pkt), &f));
    TEST_ASSERT_EQUAL_UINT8(2U, f.n);
    TEST_ASSERT_EQUAL_UINT8(1U, f.t[0].id);
    TEST_ASSERT_EQUAL_UINT16(90U, f.t[0].range_m);
    TEST_ASSERT_EQUAL_UINT16(32U, f.t[0].closing_kmh);
    TEST_ASSERT_EQUAL_UINT16(45U, f.t[1].range_m);
    /* the service says nothing about the threat: the model decides */
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_NONE, f.t[0].level);
}

static void test_an_empty_varia_notification_is_a_clear_road(void)
{
    /* the counter alone: a valid frame with nothing in it, which is what
     * makes a mark on the screen stop fading and go */
    static const uint8_t pkt[] = {0x11U};
    struct radar_frame f;

    TEST_ASSERT_TRUE(radar_wire_varia(pkt, sizeof(pkt), &f));
    TEST_ASSERT_EQUAL_UINT8(0U, f.n);
}

static void test_a_varia_slot_with_no_distance_is_skipped(void)
{
    static const uint8_t pkt[] = {0x12U, 0x01U, 0U, 0U, 0x02U, 70U, 40U};
    struct radar_frame f;

    TEST_ASSERT_TRUE(radar_wire_varia(pkt, sizeof(pkt), &f));
    TEST_ASSERT_EQUAL_UINT8(1U, f.n);
    TEST_ASSERT_EQUAL_UINT16(70U, f.t[0].range_m);
}

static void test_a_packet_that_is_not_a_radar_frame_is_refused(void)
{
    /* a body that is not a whole number of vehicles is not ours */
    static const uint8_t odd[] = {0x13U, 0x01U, 90U};
    struct radar_frame f;

    TEST_ASSERT_FALSE(radar_wire_varia(odd, sizeof(odd), &f));
    TEST_ASSERT_FALSE(radar_wire_varia(NULL, 4U, &f));
    TEST_ASSERT_FALSE(radar_wire_varia(odd, 0U, &f));
    TEST_ASSERT_FALSE(radar_wire_varia(odd, sizeof(odd), NULL));
}

static void test_a_notification_longer_than_the_model_holds(void)
{
    uint8_t pkt[1U + (16U * 3U)];
    struct radar_frame f;

    pkt[0] = 0x14U;
    for (unsigned int i = 0U; i < 16U; i++) {
        pkt[1U + (i * 3U)] = (uint8_t)(i + 1U);
        pkt[2U + (i * 3U)] = (uint8_t)(20U + i);
        pkt[3U + (i * 3U)] = 30U;
    }

    TEST_ASSERT_TRUE(radar_wire_varia(pkt, sizeof(pkt), &f));
    TEST_ASSERT_EQUAL_UINT8(RADAR_TARGETS_MAX, f.n);
}

static void test_a_varia_frame_goes_straight_into_the_model(void)
{
    static const uint8_t pkt[] = {0x20U, 0x05U, 25U, 60U};
    struct radar_frame f;

    TEST_ASSERT_TRUE(radar_wire_varia(pkt, sizeof(pkt), &f));
    radar_feed(&r, &f, 1000U);

    /* twenty-five metres is close enough to be a threat whatever the speed */
    TEST_ASSERT_EQUAL_UINT8(1U, radar_count(&r));
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_DANGER, radar_worst(&r));
    TEST_ASSERT_EQUAL_UINT16(25U, radar_nearest(&r)->range_m);
}

static void test_nothing_blows_up_without_a_radar(void)
{
    struct radar_frame f = {.n = 1U};

    radar_init(NULL);
    radar_feed(NULL, &f, 0U);
    radar_feed(&r, NULL, 0U);
    radar_tick(NULL, 0U);
    radar_set_link(NULL, true, 0U);
    TEST_ASSERT_NULL(radar_nearest(NULL));
    TEST_ASSERT_EQUAL_UINT8(0U, radar_count(NULL));
    TEST_ASSERT_EQUAL_UINT8(RADAR_LEVEL_NONE, radar_worst(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_road_starts_empty);
    RUN_TEST(test_a_car_behind_shows_up_with_its_distance);
    RUN_TEST(test_the_nearest_one_comes_first);
    RUN_TEST(test_a_car_keeps_its_slot_as_it_closes);
    RUN_TEST(test_a_dropped_frame_does_not_blink_the_mark);
    RUN_TEST(test_a_car_that_goes_away_is_forgotten);
    RUN_TEST(test_a_frame_with_one_car_gone_keeps_the_other);
    RUN_TEST(test_a_reading_out_of_range_is_dropped);
    RUN_TEST(test_the_threat_level_follows_the_distance_and_the_speed);
    RUN_TEST(test_a_level_the_radio_gives_wins_over_the_guess);
    RUN_TEST(test_the_worst_of_the_road_is_what_the_rider_is_warned_about);
    RUN_TEST(test_more_cars_than_the_model_holds);
    RUN_TEST(test_the_radar_going_away_clears_the_road);
    RUN_TEST(test_a_varia_notification_with_two_cars);
    RUN_TEST(test_an_empty_varia_notification_is_a_clear_road);
    RUN_TEST(test_a_varia_slot_with_no_distance_is_skipped);
    RUN_TEST(test_a_packet_that_is_not_a_radar_frame_is_refused);
    RUN_TEST(test_a_notification_longer_than_the_model_holds);
    RUN_TEST(test_a_varia_frame_goes_straight_into_the_model);
    RUN_TEST(test_nothing_blows_up_without_a_radar);

    return UNITY_END();
}
