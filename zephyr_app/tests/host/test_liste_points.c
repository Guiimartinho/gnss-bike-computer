/**
 * @file test_liste_points.c
 * @brief Point lists of the segments and of the history (model/liste_points.c)
 *
 * `legacy/source/routes/ListePoints.cpp`: the history of the rider grows at
 * the front and is trimmed to a maximum (`ajouteIso`), a segment keeps its
 * points in order, and the relative position of the rider to a list comes
 * from the two nearest points (`updateRelativePosition`, lines 239-331,
 * rules in `docs/06-algoritmos.md#vetores-e-posição-relativa`).
 *
 * Positions around Nancy (48.6921, 6.1844), as in the other tests.
 */

#include <math.h>

#include "unity.h"

#include "model/liste_points.h"

#define BASE_LAT        48.6921f
#define BASE_LON        6.1844f
#define M_PER_DEG_LAT   111132.0f
#define DEG_LAT(m)      ((float)(m) / M_PER_DEG_LAT)
#define DEG_LON(m)      ((float)(m) / (M_PER_DEG_LAT * cosf(BASE_LAT * 0.0174532925f)))

static liste_points_t liste;

void setUp(void)
{
    liste_init(&liste, 64U);
}

void tearDown(void) {}

/** Add a point @p east_m metres east and @p north_m north of the base */
static void add_back_at(float east_m, float north_m, float alt, float rtime)
{
    liste_add_back(&liste, BASE_LAT + DEG_LAT(north_m), BASE_LON + DEG_LON(east_m), alt, rtime);
}

static void test_a_new_list_is_empty(void)
{
    TEST_ASSERT_EQUAL_UINT16(0U, liste_size(&liste));
    TEST_ASSERT_NULL(liste_get_first(&liste));
    TEST_ASSERT_NULL(liste_get_last(&liste));
    TEST_ASSERT_NULL(liste_get_at(&liste, 0));
    TEST_ASSERT_EQUAL_UINT16(0U, liste_size(NULL));
}

static void test_points_added_at_the_back_keep_their_order(void)
{
    add_back_at(0.0f, 0.0f, 200.0f, 0.0f);
    add_back_at(10.0f, 0.0f, 201.0f, 1.0f);
    add_back_at(20.0f, 0.0f, 202.0f, 2.0f);

    TEST_ASSERT_EQUAL_UINT16(3U, liste_size(&liste));
    TEST_ASSERT_EQUAL_FLOAT(200.0f, liste_get_first(&liste)->alt);
    TEST_ASSERT_EQUAL_FLOAT(202.0f, liste_get_last(&liste)->alt);
    TEST_ASSERT_EQUAL_FLOAT(201.0f, liste_get_at(&liste, 1)->alt);
}

static void test_points_added_at_the_front_come_out_newest_first(void)
{
    liste_add_front(&liste, BASE_LAT, BASE_LON, 200.0f, 0.0f);
    liste_add_front(&liste, BASE_LAT, BASE_LON, 201.0f, 1.0f);
    liste_add_front(&liste, BASE_LAT, BASE_LON, 202.0f, 2.0f);

    /* the history of the rider: the newest is the first */
    TEST_ASSERT_EQUAL_UINT16(3U, liste_size(&liste));
    TEST_ASSERT_EQUAL_FLOAT(202.0f, liste_get_at(&liste, 0)->alt);
    TEST_ASSERT_EQUAL_FLOAT(200.0f, liste_get_at(&liste, 2)->alt);
}

static void test_the_history_is_trimmed_to_its_maximum(void)
{
    for (unsigned int i = 0U; i < 40U; i++) {
        liste_add_iso(&liste, BASE_LAT + DEG_LAT((float)i), BASE_LON, 200.0f + (float)i,
                      (float)i, 20U);
    }

    TEST_ASSERT_EQUAL_UINT16(20U, liste_size(&liste));
    /* the newest point stayed at the front */
    TEST_ASSERT_EQUAL_FLOAT(239.0f, liste_get_at(&liste, 0)->alt);
    /*
     * The window moves with the rider: the legacy drops the oldest point
     * (`ListePoints::ajouteFinIso()`, `pop_back`), so the twenty newest are
     * the ones left, in order.
     */
    TEST_ASSERT_EQUAL_FLOAT(238.0f, liste_get_at(&liste, 1)->alt);
    TEST_ASSERT_EQUAL_FLOAT(220.0f, liste_get_last(&liste)->alt);
}

static void test_a_full_history_keeps_following_the_rider(void)
{
    /* the maximum of the rider history, `HISTO_POINT_SIZE` of app_types.h */
    for (unsigned int i = 0U; i < 60U; i++) {
        liste_add_iso(&liste, BASE_LAT + DEG_LAT((float)i), BASE_LON, 200.0f + (float)i,
                      (float)i, 15U);

        const point_t *newest = liste_get_at(&liste, 0);

        /* every single epoch, the current position is the one just added */
        TEST_ASSERT_NOT_NULL(newest);
        TEST_ASSERT_EQUAL_FLOAT(200.0f + (float)i, newest->alt);
    }

    TEST_ASSERT_EQUAL_UINT16(15U, liste_size(&liste));
    TEST_ASSERT_EQUAL_FLOAT(245.0f, liste_get_last(&liste)->alt);
}

static void test_halving_a_list_keeps_one_point_out_of_two(void)
{
    for (unsigned int i = 0U; i < 9U; i++) {
        add_back_at(10.0f * (float)i, 0.0f, 200.0f + (float)i, (float)i);
    }

    TEST_ASSERT_EQUAL_UINT16(5U, liste_decimate(&liste));
    TEST_ASSERT_EQUAL_UINT16(5U, liste_size(&liste));
    /* the first one stays, and the order with it */
    TEST_ASSERT_EQUAL_FLOAT(200.0f, liste_get_at(&liste, 0)->alt);
    TEST_ASSERT_EQUAL_FLOAT(202.0f, liste_get_at(&liste, 1)->alt);
    TEST_ASSERT_EQUAL_FLOAT(208.0f, liste_get_last(&liste)->alt);

    /* nothing to halve in a list of one point, and none in an empty one */
    liste_clear(&liste);
    TEST_ASSERT_EQUAL_UINT16(0U, liste_decimate(&liste));
    TEST_ASSERT_EQUAL_UINT16(0U, liste_decimate(NULL));
}

static void test_a_list_can_live_on_an_array_of_the_caller(void)
{
    static point_t storage[300];
    liste_points_t big;

    liste_init_static(&big, storage, 300U);
    for (unsigned int i = 0U; i < 300U; i++) {
        liste_add_back(&big, BASE_LAT, BASE_LON, (float)i, (float)i);
    }

    /* over the LISTE_MAX_HISTORY of a list of its own */
    TEST_ASSERT_EQUAL_UINT16(300U, liste_size(&big));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, liste_get_first(&big)->alt);
    TEST_ASSERT_EQUAL_FLOAT(299.0f, liste_get_last(&big)->alt);

    /* without an array it falls back to its own storage */
    liste_init_static(&big, NULL, 300U);
    TEST_ASSERT_EQUAL_UINT16(0U, liste_size(&big));
    liste_add_back(&big, BASE_LAT, BASE_LON, 1.0f, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, liste_get_first(&big)->alt);
}

static void test_clearing_leaves_nothing(void)
{
    add_back_at(0.0f, 0.0f, 200.0f, 0.0f);
    add_back_at(10.0f, 0.0f, 201.0f, 1.0f);

    liste_clear(&liste);

    TEST_ASSERT_EQUAL_UINT16(0U, liste_size(&liste));
    TEST_ASSERT_NULL(liste_get_first(&liste));
}

static void test_an_index_outside_the_list_gives_nothing(void)
{
    add_back_at(0.0f, 0.0f, 200.0f, 0.0f);

    TEST_ASSERT_NULL(liste_get_at(&liste, 5));
    TEST_ASSERT_NULL(liste_get_at(&liste, -2)); /* -1 is the last, and it exists */
    TEST_ASSERT_NOT_NULL(liste_get_at(&liste, -1));
    TEST_ASSERT_NULL(liste_get_at(NULL, 0));
}

static void test_the_distance_to_the_list_is_the_nearest_point(void)
{
    add_back_at(0.0f, 0.0f, 200.0f, 0.0f);
    add_back_at(100.0f, 0.0f, 200.0f, 10.0f);
    add_back_at(200.0f, 0.0f, 200.0f, 20.0f);

    /* ten metres north of the middle point */
    float d = liste_distance_to(&liste, BASE_LAT + DEG_LAT(10.0f), BASE_LON + DEG_LON(100.0f));

    TEST_ASSERT_FLOAT_WITHIN(1.0f, 10.0f, d);

    /* an empty list is far away, as the legacy answers */
    liste_clear(&liste);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100000.0f, liste_distance_to(&liste, BASE_LAT, BASE_LON));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100000.0f, liste_distance_to(NULL, BASE_LAT, BASE_LON));
}

static void test_the_delta_covers_the_whole_list(void)
{
    add_back_at(0.0f, 0.0f, 200.0f, 0.0f);
    add_back_at(100.0f, 0.0f, 210.0f, 10.0f);
    add_back_at(100.0f, 50.0f, 220.0f, 20.0f);

    liste_update_delta(&liste);

    const vecteur_t *delta = liste_get_delta(&liste);
    const point2d_t *center = liste_get_center(&liste);

    TEST_ASSERT_NOT_NULL(delta);
    TEST_ASSERT_NOT_NULL(center);

    /*
     * The delta is in degrees, as `ListePoints::updateDelta()` of the
     * legacy keeps it (`m_delta_l._x = max_lon - min_lon`): the box here is
     * 100 m east by 50 m north.
     */
    TEST_ASSERT_FLOAT_WITHIN(DEG_LON(3.0f), DEG_LON(100.0f), fabsf(delta->x));
    TEST_ASSERT_FLOAT_WITHIN(DEG_LAT(3.0f), DEG_LAT(50.0f), fabsf(delta->y));

    /* the centre sits between the ends */
    TEST_ASSERT_FLOAT_WITHIN(DEG_LAT(2.0f), BASE_LAT + (DEG_LAT(50.0f) / 2.0f), center->lat);

    TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.0f, liste_get_elev_total(&liste));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 20.0f, liste_get_time_total(&liste));
}

static void test_the_relative_position_needs_five_points(void)
{
    point_t me;

    point_init(&me, BASE_LAT, BASE_LON, 200.0f, 0.0f);

    for (unsigned int i = 0U; i < 4U; i++) {
        add_back_at(10.0f * (float)i, 0.0f, 200.0f, (float)i);
    }

    /* the legacy does nothing below five points (`ListePoints.cpp:239-331`) */
    pos_relative_t rel = liste_compute_pos_relative(&liste, &me);

    TEST_ASSERT_EQUAL_FLOAT(0.0f, rel.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rel.y);
}

static void test_the_relative_position_projects_onto_the_two_nearest(void)
{
    point_t me;

    for (unsigned int i = 0U; i < 8U; i++) {
        add_back_at(20.0f * (float)i, 0.0f, 200.0f + (2.0f * (float)i), 10.0f * (float)i);
    }

    /* between the third and the fourth point, five metres to the side */
    point_init(&me, BASE_LAT + DEG_LAT(5.0f), BASE_LON + DEG_LON(50.0f), 200.0f, 0.0f);

    pos_relative_t rel = liste_compute_pos_relative(&liste, &me);

    /* ten metres past the third point, which is at 40 m */
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 10.0f, rel.x);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 5.0f, fabsf(rel.y));
    /* altitude interpolated between 204 and 206 */
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 205.0f, rel.z);

    liste_update_relative_position(&liste, &me);
    TEST_ASSERT_NOT_NULL(liste_get_pos_relative(&liste));
    TEST_ASSERT_FLOAT_WITHIN(2.0f, rel.x, liste_get_pos_relative(&liste)->x);
    TEST_ASSERT_TRUE(liste_get_idx_p1(&liste) > 0U);
}

static void test_a_point_far_from_the_list_falls_back_to_the_nearest(void)
{
    point_t me;

    for (unsigned int i = 0U; i < 8U; i++) {
        add_back_at(20.0f * (float)i, 0.0f, 200.0f, 10.0f * (float)i);
    }

    /* 300 m to the north: over the 50 m of the legacy */
    point_init(&me, BASE_LAT + DEG_LAT(300.0f), BASE_LON, 200.0f, 0.0f);

    pos_relative_t rel = liste_compute_pos_relative(&liste, &me);

    TEST_ASSERT_EQUAL_FLOAT(0.0f, rel.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rel.y);
}

static void test_a_segment_list_uses_the_array_it_is_given(void)
{
    static point_t storage[8];
    seg_liste_points_t seg;

    seg_liste_init(&seg, storage, 8U);

    TEST_ASSERT_EQUAL_UINT16(0U, seg.count);
    TEST_ASSERT_EQUAL_UINT16(8U, seg.capacity);
    TEST_ASSERT_EQUAL_PTR(storage, seg.points);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_new_list_is_empty);
    RUN_TEST(test_points_added_at_the_back_keep_their_order);
    RUN_TEST(test_points_added_at_the_front_come_out_newest_first);
    RUN_TEST(test_the_history_is_trimmed_to_its_maximum);
    RUN_TEST(test_a_full_history_keeps_following_the_rider);
    RUN_TEST(test_halving_a_list_keeps_one_point_out_of_two);
    RUN_TEST(test_a_list_can_live_on_an_array_of_the_caller);
    RUN_TEST(test_clearing_leaves_nothing);
    RUN_TEST(test_an_index_outside_the_list_gives_nothing);
    RUN_TEST(test_the_distance_to_the_list_is_the_nearest_point);
    RUN_TEST(test_the_delta_covers_the_whole_list);
    RUN_TEST(test_the_relative_position_needs_five_points);
    RUN_TEST(test_the_relative_position_projects_onto_the_two_nearest);
    RUN_TEST(test_a_point_far_from_the_list_falls_back_to_the_nearest);
    RUN_TEST(test_a_segment_list_uses_the_array_it_is_given);

    return UNITY_END();
}
