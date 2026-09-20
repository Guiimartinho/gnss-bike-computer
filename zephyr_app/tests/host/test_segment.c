/**
 * @file test_segment.c
 * @brief Segments on the card (src/model/segment.c)
 *
 * The rules of `legacy/source/routes/Segment.h:21-38` and of the allocator
 * of `legacy/source/sd/sd_functions.cpp:518-596`: the card is scanned by
 * name, a segment is loaded when the rider comes within DIST_ALLOC (300 m)
 * of its start, unloaded past that while it is off and past twice that
 * while it is on, and it activates when the rider arrives at the start in
 * the direction of the segment.
 *
 * The card is the fake file system of `support/host_fs.c`, with segments
 * written in the text of the legacy.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "host_fs.h"
#include "host_kernel.h"
#include "model/segment.h"

#define BASE_LAT        48.6921f
#define BASE_LON        6.1844f
#define M_PER_DEG_LAT   111132.0f
#define DEG_LAT(m)      ((float)(m) / M_PER_DEG_LAT)
#define DEG_LON(m)      ((float)(m) / (M_PER_DEG_LAT * cosf(BASE_LAT * 0.0174532925f)))

/** Name of the segment that starts at the base position */
static char base_name[16];

static void make_name(char *out, size_t size, float lat, float lon)
{
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    uint32_t ilat = (uint32_t)(((double)lat + 90.0) * 100000.0 + 0.5);
    uint32_t ilon = (uint32_t)(((double)lon + 180.0) * 100000.0 + 0.5);
    char la[5];
    char lo[5];

    for (int i = 4; i >= 0; i--) {
        la[i] = digits[ilat % 36U];
        ilat /= 36U;
        lo[i] = digits[ilon % 36U];
        ilon /= 36U;
    }
    (void)snprintf(out, size, "%c%c%c%c%c#%c%c.%c%c%c", la[0], la[1], la[2], la[3], la[4],
                   lo[0], lo[1], lo[2], lo[3], lo[4]);
}

/**
 * Write a segment of @p points points going east from (@p lat, @p lon),
 * one point every 20 m and 5 s, in the text of the legacy.
 */
static void add_segment(float lat, float lon, unsigned int points, char *name_out)
{
    char name[16];
    char path[32];
    static char body[65536]; /* a long segment does not fit on the stack */
    size_t len = 0U;

    make_name(name, sizeof(name), lat, lon);
    (void)snprintf(path, sizeof(path), "/SD:/%s", name);

    len += (size_t)snprintf(&body[len], sizeof(body) - len, "<Name>TEST SEGMENT</Name>\r\n");
    for (unsigned int i = 0U; i < points; i++) {
        /* the columns of the legacy: lat ; lon ; rtime ; alt */
        len += (size_t)snprintf(&body[len], sizeof(body) - len, "%.6f ; %.6f ; %.1f ; %.1f\r\n",
                                (double)lat, (double)(lon + DEG_LON(20.0f * (float)i)),
                                (double)(200.0f + (5.0f * (float)i)),
                                (double)(100.0f + (5.0f * (float)i)));
    }

    TEST_ASSERT_TRUE(host_fs_add_file(path, body));
    if (name_out != NULL) {
        (void)memcpy(name_out, name, strlen(name) + 1U);
    }
}

static loc_data_t at(float east_m, float north_m, uint32_t ms)
{
    loc_data_t loc = {
        .lat = BASE_LAT + DEG_LAT(north_m),
        .lon = BASE_LON + DEG_LON(east_m),
        .alt = 100.0f,
        .speed = 25.0f,
        .course = 90.0f,
        .timestamp = ms,
    };

    return loc;
}

void setUp(void)
{
    host_fs_reset();
    host_uptime_set(0U);
    segment_unload_all();
    (void)segment_init();
}

void tearDown(void) {}

static void test_a_card_without_segments_loads_nothing(void)
{
    TEST_ASSERT_EQUAL_INT(0, segment_load_all());
    TEST_ASSERT_EQUAL_UINT16(0U, segment_get_total_count());
}

static void test_the_scan_takes_the_names_of_the_legacy(void)
{
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);
    add_segment(BASE_LAT + DEG_LAT(5000.0f), BASE_LON, 10U, NULL);
    TEST_ASSERT_TRUE(host_fs_add_file("/SD:/MJ_40.PAR", "48.69 6.18 200\r\n"));
    TEST_ASSERT_TRUE(host_fs_add_file("/SD:/@50925.txt", "48.69;6.18;200.00;1;\r\n"));

    /* only the two segments count: the route and the log stay out */
    TEST_ASSERT_EQUAL_INT(2, segment_load_all());
    TEST_ASSERT_EQUAL_UINT16(2U, segment_get_total_count());

    segment_t seg;

    TEST_ASSERT_EQUAL(APP_OK, segment_get(0U, &seg));
    TEST_ASSERT_EQUAL_STRING(base_name, seg.name);
    TEST_ASSERT_EQUAL_UINT8(SEG_OFF, seg.status);
    TEST_ASSERT_EQUAL_UINT16(0U, seg.num_points); /* the points come later */
}

static void test_the_scan_does_not_open_the_files(void)
{
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);

    unsigned int before = host_fs_open_count();

    (void)segment_load_all();

    /* the position comes from the name: the legacy opens nothing here */
    TEST_ASSERT_EQUAL_UINT(before, host_fs_open_count());
}

static void test_a_segment_far_away_stays_closed(void)
{
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);
    (void)segment_load_all();

    /* one kilometre north: over the 300 m of the legacy */
    float dist = segment_run_allocator(BASE_LAT + DEG_LAT(1000.0f), BASE_LON);

    TEST_ASSERT_FLOAT_WITHIN(50.0f, 1000.0f, dist);

    segment_t seg;

    TEST_ASSERT_EQUAL(APP_OK, segment_get(0U, &seg));
    TEST_ASSERT_EQUAL_UINT16(0U, seg.num_points);
}

static void test_a_segment_within_three_hundred_metres_is_loaded(void)
{
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);
    (void)segment_load_all();

    (void)segment_run_allocator(BASE_LAT + DEG_LAT(100.0f), BASE_LON);

    segment_t seg;

    TEST_ASSERT_EQUAL(APP_OK, segment_get(0U, &seg));
    TEST_ASSERT_EQUAL_UINT16(30U, seg.num_points);
    /* 29 steps of 5 s and of 5 m of climb */
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 145.0f, seg.total_time);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 145.0f, seg.total_elev);
}

static void test_a_segment_longer_than_the_slot_is_halved(void)
{
    /* over the 256 points of a slot of the pool (`SEG_SLOTS` in segment.c) */
    add_segment(BASE_LAT, BASE_LON, 600U, base_name);
    (void)segment_load_all();

    (void)segment_run_allocator(BASE_LAT + DEG_LAT(100.0f), BASE_LON);

    segment_t seg;

    TEST_ASSERT_EQUAL(APP_OK, segment_get(0U, &seg));
    TEST_ASSERT_TRUE(seg.num_points > 0U);
    TEST_ASSERT_TRUE(seg.num_points <= 256U);
    /*
     * The end of the file survives the halving, so the segment still knows
     * how long it is: 599 steps of 5 s and of 5 m of climb.
     */
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2995.0f, seg.total_time);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2995.0f, seg.total_elev);
}

static void test_a_fourth_segment_waits_for_a_free_slot(void)
{
    /* the pool holds three segments at a time */
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);
    add_segment(BASE_LAT + DEG_LAT(30.0f), BASE_LON, 30U, NULL);
    add_segment(BASE_LAT + DEG_LAT(60.0f), BASE_LON, 30U, NULL);
    add_segment(BASE_LAT + DEG_LAT(90.0f), BASE_LON, 30U, NULL); /* no slot left */

    TEST_ASSERT_EQUAL_INT(4, segment_load_all());
    (void)segment_run_allocator(BASE_LAT, BASE_LON);

    uint8_t loaded = 0U;

    for (uint8_t i = 0U; i < 4U; i++) {
        segment_t seg;

        TEST_ASSERT_EQUAL(APP_OK, segment_get(i, &seg));
        if (seg.num_points > 0U) {
            loaded++;
        }
    }

    TEST_ASSERT_EQUAL_UINT8(3U, loaded);

    /* the fourth is the only one left out */
    segment_t fourth;

    TEST_ASSERT_EQUAL(APP_OK, segment_get(3U, &fourth));
    TEST_ASSERT_EQUAL_UINT16(0U, fourth.num_points);

    /*
     * Its slot comes when the others are behind: the rider goes 350 m north,
     * where the first two are past the 300 m of the legacy and the fourth,
     * 260 m away, is not.
     */
    (void)segment_run_allocator(BASE_LAT + DEG_LAT(350.0f), BASE_LON);

    TEST_ASSERT_EQUAL(APP_OK, segment_get(3U, &fourth));
    TEST_ASSERT_EQUAL_UINT16(30U, fourth.num_points);

    segment_t first;

    TEST_ASSERT_EQUAL(APP_OK, segment_get(0U, &first));
    TEST_ASSERT_EQUAL_UINT16(0U, first.num_points);
}

static void test_going_away_unloads_the_segment(void)
{
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);
    (void)segment_load_all();

    (void)segment_run_allocator(BASE_LAT + DEG_LAT(100.0f), BASE_LON);
    (void)segment_run_allocator(BASE_LAT + DEG_LAT(2000.0f), BASE_LON);

    segment_t seg;

    TEST_ASSERT_EQUAL(APP_OK, segment_get(0U, &seg));
    TEST_ASSERT_EQUAL_UINT16(0U, seg.num_points);
}

static void test_the_nearest_distance_is_reported(void)
{
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);
    add_segment(BASE_LAT + DEG_LAT(400.0f), BASE_LON, 10U, NULL);
    (void)segment_load_all();

    (void)segment_run_allocator(BASE_LAT + DEG_LAT(50.0f), BASE_LON);

    TEST_ASSERT_FLOAT_WITHIN(20.0f, 50.0f, segment_get_nearest_distance());
}

static void test_riding_into_a_segment_activates_it(void)
{
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);
    (void)segment_load_all();
    (void)segment_run_allocator(BASE_LAT, BASE_LON - DEG_LON(100.0f));

    /* arrive from the west and cross the start heading east */
    for (unsigned int i = 0U; i < 12U; i++) {
        loc_data_t loc = at(-60.0f + (10.0f * (float)i), 0.0f, 1000U * (i + 1U));

        host_uptime_set(1000U * (i + 1U));
        TEST_ASSERT_EQUAL(APP_OK, segment_update(&loc));
    }

    TEST_ASSERT_TRUE(segment_is_any_active());
    TEST_ASSERT_GREATER_THAN_UINT8(0U, segment_get_active_count());

    segment_t active[2];

    TEST_ASSERT_GREATER_THAN_UINT8(0U, segment_get_active(active, 2U));
    TEST_ASSERT_EQUAL_STRING(base_name, active[0].name);
}

static void test_riding_to_the_end_finishes_the_segment(void)
{
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);
    (void)segment_load_all();
    (void)segment_run_allocator(BASE_LAT, BASE_LON - DEG_LON(100.0f));

    /* the whole segment: 30 points of 20 m, plus the run-up and the run-out */
    for (unsigned int i = 0U; i < 80U; i++) {
        loc_data_t loc = at(-60.0f + (10.0f * (float)i), 0.0f, 1000U * (i + 1U));

        host_uptime_set(1000U * (i + 1U));
        (void)segment_update(&loc);
    }

    segment_t seg;

    TEST_ASSERT_EQUAL(APP_OK, segment_get(0U, &seg));
    /* off again, or counting down at the end, but no longer running */
    TEST_ASSERT_NOT_EQUAL_UINT8(SEG_ON, seg.status);
}

static void test_a_bad_position_is_refused(void)
{
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);
    (void)segment_load_all();

    TEST_ASSERT_EQUAL(APP_ERR_INVALID_PARAM, segment_update(NULL));
}

static void test_unloading_everything_empties_the_list(void)
{
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);
    (void)segment_load_all();
    TEST_ASSERT_EQUAL_UINT16(1U, segment_get_total_count());

    segment_unload_all();
    TEST_ASSERT_EQUAL_UINT16(0U, segment_get_total_count());
    TEST_ASSERT_FALSE(segment_is_any_active());
}

static void test_the_nearby_list_comes_sorted(void)
{
    add_segment(BASE_LAT + DEG_LAT(500.0f), BASE_LON, 10U, NULL);  /* far */
    add_segment(BASE_LAT, BASE_LON, 30U, base_name);               /* near */
    (void)segment_load_all();

    segment_t near[4];
    uint8_t n = segment_get_nearby(near, 4U, BASE_LAT, BASE_LON);

    TEST_ASSERT_GREATER_THAN_UINT8(0U, n);
    TEST_ASSERT_EQUAL_STRING(base_name, near[0].name);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_card_without_segments_loads_nothing);
    RUN_TEST(test_the_scan_takes_the_names_of_the_legacy);
    RUN_TEST(test_the_scan_does_not_open_the_files);
    RUN_TEST(test_a_segment_far_away_stays_closed);
    RUN_TEST(test_a_segment_within_three_hundred_metres_is_loaded);
    RUN_TEST(test_a_segment_longer_than_the_slot_is_halved);
    RUN_TEST(test_a_fourth_segment_waits_for_a_free_slot);
    RUN_TEST(test_going_away_unloads_the_segment);
    RUN_TEST(test_the_nearest_distance_is_reported);
    RUN_TEST(test_riding_into_a_segment_activates_it);
    RUN_TEST(test_riding_to_the_end_finishes_the_segment);
    RUN_TEST(test_a_bad_position_is_refused);
    RUN_TEST(test_unloading_everything_empties_the_list);
    RUN_TEST(test_the_nearby_list_comes_sorted);

    return UNITY_END();
}
