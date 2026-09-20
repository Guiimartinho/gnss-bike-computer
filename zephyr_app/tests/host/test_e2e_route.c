/**
 * @file test_e2e_route.c
 * @brief End to end: a course arrives from the phone and the rider follows it
 *
 * Walks the whole way a route goes through the firmware, with the real
 * modules and the fake storage of `support/host_fs.c`:
 *
 *   1. the phone asks to write a file, and the rules of `file_policy.c`
 *      decide whether it may (`rf/file_xfer.c` does this over mcumgr);
 *   2. the file lands on the storage in the text of the legacy;
 *   3. `parcours.c` opens it, with the decimation of a long course;
 *   4. the rider starts and moves, and the navigation follows;
 *   5. `map_project.c` puts the route in the window of the screen;
 *   6. `route_profile.c` builds what the profile screen draws.
 *
 * What is left out is what needs hardware: the radio, the USB and the
 * service threads.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "host_fs.h"
#include "host_kernel.h"
#include "model/file_policy.h"
#include "model/map_project.h"
#include "model/parcours.h"
#include "model/route_file.h"
#include "model/route_profile.h"

#define BASE_LAT        48.6921f
#define BASE_LON        6.1844f
#define M_PER_DEG_LAT   111132.0f
#define DEG_LAT(m)      ((float)(m) / M_PER_DEG_LAT)

/** The map window of the PRC screen, 240 by 107 pixels */
#define WINDOW_ASPECT_PM    2243U

/** Where the phone puts the course */
#define ROUTE_PATH      "/SD:/SERRA.PAR"

/**
 * A course as Strava or Komoot exports it: @p km kilometres going north
 * with a point every @p step_m metres, climbing to the middle and coming
 * back down.
 */
static unsigned int send_course(const char *path, float km, float step_m)
{
    static char body[400000];
    size_t len = 0U;
    unsigned int points = (unsigned int)((km * 1000.0f) / step_m);

    len += (size_t)snprintf(&body[len], sizeof(body) - len, "<name>SERRA</name>\r\n");
    for (unsigned int i = 0U; i < points; i++) {
        float along = step_m * (float)i;

        TEST_ASSERT_TRUE(len < (sizeof(body) - 64U));
        float half = (km * 1000.0f) / 2.0f;
        float alt = (along <= half) ? (300.0f + (along / 10.0f))
                                    : (300.0f + ((half - (along - half)) / 10.0f));

        len += (size_t)snprintf(&body[len], sizeof(body) - len, "%.8f %.8f %.1f\r\n",
                                (double)(BASE_LAT + DEG_LAT(along)), (double)BASE_LON,
                                (double)alt);
    }

    TEST_ASSERT_TRUE(host_fs_add_file(path, body));

    return points;
}

static float route_alt(uint16_t index, void *user)
{
    const point_t *pt = parcours_get_point(index);

    (void)user;

    return (pt != NULL) ? pt->alt : 0.0f;
}

void setUp(void)
{
    host_fs_reset();
    host_uptime_set(0U);
    parcours_unload();
    (void)parcours_init();
}

void tearDown(void) {}

static void test_the_phone_may_send_a_course_but_not_anything(void)
{
    /* step 1: the rules the radio applies before the first byte is written */
    TEST_ASSERT_TRUE(file_policy_allows(ROUTE_PATH, FILE_ACCESS_WRITE));
    TEST_ASSERT_EQUAL_INT(FILE_KIND_ROUTE, file_policy_kind(ROUTE_PATH));

    TEST_ASSERT_FALSE(file_policy_allows("/SD:/@50925.txt", FILE_ACCESS_WRITE));
    TEST_ASSERT_FALSE(file_policy_allows("/SD:/config/ROTA.PAR", FILE_ACCESS_WRITE));
}

static void test_a_course_of_a_hundred_kilometres_goes_all_the_way(void)
{
    /* step 2: 10.000 points, as a course exported with one point every 10 m */
    unsigned int sent = send_course(ROUTE_PATH, 100.0f, 10.0f);

    TEST_ASSERT_EQUAL_UINT(10000U, sent);

    /* step 3: it opens, halved to what fits in memory, ends kept */
    TEST_ASSERT_EQUAL(APP_OK, parcours_load(ROUTE_PATH));
    TEST_ASSERT_TRUE(parcours_is_loaded());

    uint16_t kept = parcours_get_num_points();

    TEST_ASSERT_TRUE(kept > 1U);
    TEST_ASSERT_TRUE(kept <= PARCOURS_MAX_POINTS);

    const point_t *first = parcours_get_point(0U);
    const point_t *last = parcours_get_point((uint16_t)(kept - 1U));

    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(last);
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, BASE_LAT, first->lat);
    /* the end of the course is a hundred kilometres north of the start */
    TEST_ASSERT_FLOAT_WITHIN(0.02f, BASE_LAT + DEG_LAT(99990.0f), last->lat);

    parcours_info_t info;

    TEST_ASSERT_EQUAL(APP_OK, parcours_get_info(&info));
    TEST_ASSERT_FLOAT_WITHIN(3000.0f, 100000.0f, info.total_distance);
    /* five thousand metres of climb: half the course rising a metre per 10 m */
    TEST_ASSERT_FLOAT_WITHIN(300.0f, 5000.0f, info.total_climb);
}

static void test_the_rider_follows_it_and_the_screen_shows_it(void)
{
    (void)send_course(ROUTE_PATH, 20.0f, 10.0f);
    TEST_ASSERT_EQUAL(APP_OK, parcours_load(ROUTE_PATH));
    TEST_ASSERT_EQUAL(APP_OK, parcours_start());

    /* step 4: the rider rides the first two kilometres */
    for (unsigned int m = 0U; m <= 2000U; m += 100U) {
        host_uptime_set(1000U * (m / 10U));
        parcours_update(BASE_LAT + DEG_LAT((float)m), BASE_LON, 300.0f + ((float)m / 10.0f));
    }

    nav_info_t nav;

    TEST_ASSERT_EQUAL(APP_OK, parcours_get_nav_info(&nav));
    TEST_ASSERT_TRUE(nav.on_route);
    TEST_ASSERT_TRUE(nav.current_idx > 0U);
    TEST_ASSERT_FLOAT_WITHIN(400.0f, 2000.0f, nav.dist_completed);
    TEST_ASSERT_FLOAT_WITHIN(600.0f, 18000.0f, nav.dist_remaining);

    /* step 5: the map of the screen, centred on the rider */
    float here_lat = BASE_LAT + DEG_LAT(2000.0f);
    uint16_t span = map_span_m(2U); /* 250 m, the zoom of the legacy */
    struct map_point rider = map_project(here_lat, BASE_LON, here_lat, BASE_LON, span,
                                         WINDOW_ASPECT_PM);

    TEST_ASSERT_EQUAL_INT16(500, rider.x);
    TEST_ASSERT_EQUAL_INT16(500, rider.y);

    /* a point a hundred metres ahead is above the middle and inside */
    struct map_point ahead = map_project(BASE_LAT + DEG_LAT(2100.0f), BASE_LON, here_lat,
                                         BASE_LON, span, WINDOW_ASPECT_PM);

    TEST_ASSERT_TRUE(ahead.y > 500);
    TEST_ASSERT_TRUE(ahead.y < 1500);

    /* and the start, two kilometres behind, is far below: the screen clips */
    struct map_point behind = map_project(BASE_LAT, BASE_LON, here_lat, BASE_LON, span,
                                          WINDOW_ASPECT_PM);

    TEST_ASSERT_TRUE(behind.y < 0);
}

static void test_the_profile_of_the_screen_comes_out_of_the_same_route(void)
{
    (void)send_course(ROUTE_PATH, 20.0f, 10.0f);
    TEST_ASSERT_EQUAL(APP_OK, parcours_load(ROUTE_PATH));
    TEST_ASSERT_EQUAL(APP_OK, parcours_start());

    /* at the start, the whole climb of the course is ahead */
    struct route_profile prof;

    TEST_ASSERT_TRUE(route_profile_build(&prof, parcours_get_num_points(),
                                         parcours_get_current_index(), route_alt, NULL));
    TEST_ASSERT_TRUE(prof.n > 1U);
    TEST_ASSERT_TRUE(prof.n <= ROUTE_PROFILE_MAX);
    TEST_ASSERT_EQUAL_UINT8(0U, prof.here);
    TEST_ASSERT_INT16_WITHIN(5, 300, prof.min_m);
    TEST_ASSERT_INT16_WITHIN(20, 1300, prof.max_m);
    TEST_ASSERT_INT_WITHIN(150, 1000, (int)prof.climb_left_m);

    /* step 6: past the top there is nothing left to climb */
    for (unsigned int m = 0U; m <= 12000U; m += 200U) {
        host_uptime_set(1000U * (m / 10U));
        parcours_update(BASE_LAT + DEG_LAT((float)m), BASE_LON, 300.0f);
    }

    TEST_ASSERT_TRUE(route_profile_build(&prof, parcours_get_num_points(),
                                         parcours_get_current_index(), route_alt, NULL));
    TEST_ASSERT_TRUE(prof.here > 0U);
    TEST_ASSERT_EQUAL_UINT16(0U, prof.climb_left_m);
}

static void test_a_second_course_replaces_the_first(void)
{
    /* the rider sends another one from the phone, as with a bike computer */
    (void)send_course(ROUTE_PATH, 20.0f, 10.0f);
    TEST_ASSERT_EQUAL(APP_OK, parcours_load(ROUTE_PATH));

    uint16_t first = parcours_get_num_points();

    (void)send_course("/SD:/CURTA.PAR", 2.0f, 10.0f);
    TEST_ASSERT_TRUE(file_policy_allows("/SD:/CURTA.PAR", FILE_ACCESS_WRITE));
    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/CURTA.PAR"));

    TEST_ASSERT_EQUAL_UINT16(200U, parcours_get_num_points());
    TEST_ASSERT_TRUE(parcours_get_num_points() != first);

    /* and the storage still holds both, for the menu to list */
    TEST_ASSERT_NOT_NULL(host_fs_file_content(ROUTE_PATH));
    TEST_ASSERT_NOT_NULL(host_fs_file_content("/SD:/CURTA.PAR"));
}

/* ==========================================================================
 * The same course in the three formats the device takes
 * ========================================================================== */

/** A GPX as Strava exports it, straight from the phone, no conversion */
static unsigned int send_gpx(const char *path, unsigned int points)
{
    static char body[400000];
    size_t len = 0U;

    len += (size_t)snprintf(&body[len], sizeof(body) - len,
                            "<?xml version=\"1.0\"?>\n"
                            "<gpx creator=\"StravaGPX\" "
                            "xmlns=\"http://www.topografix.com/GPX/1/1\">\n"
                            "<trk><name>SERRA</name><trkseg>\n");
    for (unsigned int i = 0U; i < points; i++) {
        TEST_ASSERT_TRUE(len < (sizeof(body) - 128U));
        len += (size_t)snprintf(&body[len], sizeof(body) - len,
                                "<trkpt lat=\"%.7f\" lon=\"%.7f\"><ele>%.1f</ele>"
                                "<time>2026-09-20T07:42:00Z</time></trkpt>\n",
                                (double)(BASE_LAT + DEG_LAT(10.0f * (float)i)), (double)BASE_LON,
                                (double)(300.0f + ((float)i / 10.0f)));
    }
    len += (size_t)snprintf(&body[len], sizeof(body) - len, "</trkseg></trk></gpx>\n");

    TEST_ASSERT_TRUE(host_fs_add_file(path, body));

    return (unsigned int)len;
}

/** The same course as a `.RTE`, as `tools/route_convert.py` writes it */
static void put_le32(uint8_t *at, uint32_t v)
{
    at[0] = (uint8_t)(v & 0xFFU);
    at[1] = (uint8_t)((v >> 8) & 0xFFU);
    at[2] = (uint8_t)((v >> 16) & 0xFFU);
    at[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static size_t send_rte(const char *path, unsigned int points, unsigned int cues)
{
    static uint8_t body[200000];
    size_t off = ROUTE_FILE_HEADER_SIZE;

    (void)memset(body, 0, sizeof(body));
    (void)memcpy(body, "RTE1", 4U);
    body[4] = (uint8_t)ROUTE_FILE_VERSION;
    body[6] = (cues > 0U) ? (uint8_t)ROUTE_FILE_FLAG_CUES : 0U;
    put_le32(&body[8], points);
    put_le32(&body[12], cues);
    put_le32(&body[16], 10U * points);
    put_le32(&body[20], (uint32_t)(points / 10U));
    put_le32(&body[24], (uint32_t)(int32_t)(BASE_LAT * 1e7f));
    put_le32(&body[28], (uint32_t)(int32_t)((BASE_LAT + DEG_LAT(10.0f * (float)points)) * 1e7f));
    put_le32(&body[32], (uint32_t)(int32_t)(BASE_LON * 1e7f));
    put_le32(&body[36], (uint32_t)(int32_t)(BASE_LON * 1e7f));
    (void)memcpy(&body[40], "SERRA DO MAR", 12U);

    for (unsigned int i = 0U; i < points; i++) {
        int32_t lat = (int32_t)((double)(BASE_LAT + DEG_LAT(10.0f * (float)i)) * 1e7);
        int16_t alt = (int16_t)(300 + (int)(i / 10U));

        put_le32(&body[off], (uint32_t)lat);
        put_le32(&body[off + 4U], (uint32_t)(int32_t)((double)BASE_LON * 1e7));
        body[off + 8U] = (uint8_t)((uint16_t)alt & 0xFFU);
        body[off + 9U] = (uint8_t)(((uint16_t)alt >> 8) & 0xFFU);
        off += ROUTE_FILE_POINT_SIZE;
    }

    for (unsigned int i = 0U; i < cues; i++) {
        put_le32(&body[off], (uint32_t)((i + 1U) * (points / (cues + 1U))));
        body[off + 4U] = (uint8_t)ROUTE_TURN_RIGHT;
        (void)memcpy(&body[off + 6U], "RUA DAS FLORES", 14U);
        off += ROUTE_FILE_CUE_SIZE;
    }

    put_le32(&body[60], route_file_crc32(0U, &body[ROUTE_FILE_HEADER_SIZE],
                                         off - ROUTE_FILE_HEADER_SIZE));

    TEST_ASSERT_TRUE(host_fs_add_bytes(path, body, off));

    return off;
}

static void test_a_gpx_from_the_phone_needs_no_conversion(void)
{
    unsigned int bytes = send_gpx("/SD:/SERRA.GPX", 2000U);

    TEST_ASSERT_TRUE(file_policy_allows("/SD:/SERRA.GPX", FILE_ACCESS_WRITE));
    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/SERRA.GPX"));
    TEST_ASSERT_TRUE(parcours_get_num_points() > 100U);

    const point_t *first = parcours_get_point(0U);

    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, BASE_LAT, first->lat);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 300.0f, first->alt);

    /* the XML costs what it costs: about 100 bytes a point */
    TEST_ASSERT_TRUE(bytes > (2000U * 90U));
}

static void test_the_converted_course_is_smaller_and_checked(void)
{
    size_t rte = send_rte("/SD:/SERRA.RTE", 2000U, 3U);
    unsigned int gpx = send_gpx("/SD:/SERRA.GPX", 2000U);

    /* ten bytes a point against a hundred of the XML */
    TEST_ASSERT_TRUE(rte < (size_t)(gpx / 5U));

    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/SERRA.RTE"));
    TEST_ASSERT_TRUE(parcours_get_num_points() > 100U);
    /* the name comes from the header, not from the file name */
    TEST_ASSERT_EQUAL_STRING("SERRA DO MAR", parcours_get_name());
}

static void test_a_course_cut_in_half_over_the_radio_is_refused(void)
{
    static uint8_t half[8000];
    size_t whole = send_rte("/SD:/SERRA.RTE", 2000U, 0U);
    const char *content = host_fs_file_content("/SD:/SERRA.RTE");

    TEST_ASSERT_NOT_NULL(content);
    (void)memcpy(half, content, sizeof(half));
    /* what arrived is a piece of the file: the CRC of the body will not match */
    TEST_ASSERT_TRUE(host_fs_add_bytes("/SD:/CORTADA.RTE", half, sizeof(half)));
    TEST_ASSERT_TRUE(whole > sizeof(half));

    TEST_ASSERT_EQUAL(APP_ERR_CHECKSUM, parcours_load("/SD:/CORTADA.RTE"));
    TEST_ASSERT_FALSE(parcours_is_loaded());
}

static void test_the_turns_of_the_course_come_out_in_order(void)
{
    (void)send_rte("/SD:/SERRA.RTE", 2000U, 3U);
    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/SERRA.RTE"));
    TEST_ASSERT_EQUAL(APP_OK, parcours_start());

    parcours_cue_t cue;
    float dist = 0.0f;

    TEST_ASSERT_TRUE(parcours_get_next_cue(&cue, &dist));
    TEST_ASSERT_EQUAL_UINT8(ROUTE_TURN_RIGHT, cue.turn);
    TEST_ASSERT_EQUAL_STRING("RUA DAS FLORES", cue.street);
    TEST_ASSERT_TRUE(dist > 0.0f);

    /* past the first turn, the next one is the one that comes */
    uint16_t first_point = cue.point;

    for (unsigned int m = 0U; m <= 8000U; m += 200U) {
        host_uptime_set(1000U * (m / 10U));
        parcours_update(BASE_LAT + DEG_LAT((float)m), BASE_LON, 300.0f);
    }

    TEST_ASSERT_TRUE(parcours_get_next_cue(&cue, &dist));
    TEST_ASSERT_TRUE(cue.point > first_point);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_phone_may_send_a_course_but_not_anything);
    RUN_TEST(test_a_course_of_a_hundred_kilometres_goes_all_the_way);
    RUN_TEST(test_the_rider_follows_it_and_the_screen_shows_it);
    RUN_TEST(test_the_profile_of_the_screen_comes_out_of_the_same_route);
    RUN_TEST(test_a_second_course_replaces_the_first);
    RUN_TEST(test_a_gpx_from_the_phone_needs_no_conversion);
    RUN_TEST(test_the_converted_course_is_smaller_and_checked);
    RUN_TEST(test_a_course_cut_in_half_over_the_radio_is_refused);
    RUN_TEST(test_the_turns_of_the_course_come_out_in_order);

    return UNITY_END();
}
