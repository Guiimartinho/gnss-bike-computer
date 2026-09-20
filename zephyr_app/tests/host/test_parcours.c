/**
 * @file test_parcours.c
 * @brief Routes of the card (src/model/parcours.c)
 *
 * The routes of the legacy are text files of `lat lon alt` separated by
 * spaces, with CRLF at the end of each line and `<...>` lines of metadata
 * (`legacy/source/parsers/file_parser.cpp:79-123`, `chargerPointPar()`, and
 * `legacy/source/sd/sd_functions.cpp:451-507`, `load_parcours()`). A point
 * without altitude still counts, with zero.
 *
 * The card is the fake file system of `support/host_fs.c`; the last case
 * walks the two real routes of `tools/TDD/DB`.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "host_fs.h"
#include "host_kernel.h"
#include "model/parcours.h"

#ifndef PARCOURS_DB_DIR
#define PARCOURS_DB_DIR "."
#endif

#define BASE_LAT        48.6921f
#define BASE_LON        6.1844f
#define M_PER_DEG_LAT   111132.0f
#define DEG_LAT(m)      ((float)(m) / M_PER_DEG_LAT)

void setUp(void)
{
    host_fs_reset();
    host_uptime_set(0U);
    parcours_unload();
    (void)parcours_init();
}

void tearDown(void) {}

/** A route going north, one point every @p step_m metres, in the legacy text */
static void add_route(const char *path, unsigned int points, float step_m)
{
    static char body[65536];
    size_t len = 0U;

    len += (size_t)snprintf(&body[len], sizeof(body) - len, "<name>TESTE</name>\r\n");
    for (unsigned int i = 0U; i < points; i++) {
        len += (size_t)snprintf(&body[len], sizeof(body) - len, "%.8f %.8f %.1f\r\n",
                                (double)(BASE_LAT + DEG_LAT(step_m * (float)i)), (double)BASE_LON,
                                (double)(200.0f + (float)i));
    }

    TEST_ASSERT_TRUE(host_fs_add_file(path, body));
}

static void test_a_route_of_the_legacy_loads(void)
{
    add_route("/SD:/MJ_40.PAR", 10U, 20.0f);

    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/MJ_40.PAR"));
    TEST_ASSERT_TRUE(parcours_is_loaded());
    /* the line of metadata does not become a point */
    TEST_ASSERT_EQUAL_UINT16(10U, parcours_get_num_points());

    const point_t *first = parcours_get_point(0U);

    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, BASE_LAT, first->lat);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, BASE_LON, first->lon);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 200.0f, first->alt);
}

static void test_the_end_of_line_of_the_legacy_does_not_stop_the_loader(void)
{
    /*
     * CRLF leaves an empty read between two points; the loader used to take
     * that for the end of the file and kept only the first point.
     */
    TEST_ASSERT_TRUE(host_fs_add_file("/SD:/CRLF.PAR",
                                      "48.68770900 6.128876000 341.9\r\n"
                                      "48.68771600 6.128894000 341.9\r\n"
                                      "48.68779900 6.128768000 342.6\r\n"));

    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/CRLF.PAR"));
    TEST_ASSERT_EQUAL_UINT16(3U, parcours_get_num_points());
}

static void test_a_point_without_altitude_still_counts(void)
{
    TEST_ASSERT_TRUE(host_fs_add_file("/SD:/NOALT.PAR",
                                      "48.6921 6.1844\r\n"
                                      "48.6931 6.1844 210.5\r\n"));

    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/NOALT.PAR"));
    TEST_ASSERT_EQUAL_UINT16(2U, parcours_get_num_points());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, parcours_get_point(0U)->alt);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 210.5f, parcours_get_point(1U)->alt);
}

static void test_the_last_line_without_an_end_of_line_counts(void)
{
    TEST_ASSERT_TRUE(host_fs_add_file("/SD:/NOEOL.PAR",
                                      "48.6921 6.1844 200\r\n"
                                      "48.6931 6.1844 210"));

    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/NOEOL.PAR"));
    TEST_ASSERT_EQUAL_UINT16(2U, parcours_get_num_points());
}

static void test_a_route_with_one_point_is_refused(void)
{
    TEST_ASSERT_TRUE(host_fs_add_file("/SD:/ONE.PAR", "48.6921 6.1844 200\r\n"));

    TEST_ASSERT_EQUAL(APP_ERR_INVALID_PARAM, parcours_load("/SD:/ONE.PAR"));
    TEST_ASSERT_FALSE(parcours_is_loaded());
}

static void test_a_file_that_is_not_there_gives_an_error(void)
{
    TEST_ASSERT_EQUAL(APP_ERR_IO, parcours_load("/SD:/SUMIU.PAR"));
    TEST_ASSERT_EQUAL(APP_ERR_INVALID_PARAM, parcours_load(NULL));
    TEST_ASSERT_FALSE(parcours_is_loaded());
}

static void test_loading_again_replaces_the_route(void)
{
    add_route("/SD:/A.PAR", 10U, 20.0f);
    add_route("/SD:/B.PAR", 4U, 20.0f);

    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/A.PAR"));
    TEST_ASSERT_EQUAL_UINT16(10U, parcours_get_num_points());
    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/B.PAR"));
    TEST_ASSERT_EQUAL_UINT16(4U, parcours_get_num_points());
}

static void test_the_route_knows_its_length_and_climb(void)
{
    add_route("/SD:/DIST.PAR", 11U, 100.0f); /* ten steps of 100 m north */

    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/DIST.PAR"));

    parcours_info_t info;

    TEST_ASSERT_EQUAL(APP_OK, parcours_get_info(&info));
    TEST_ASSERT_FLOAT_WITHIN(30.0f, 1000.0f, info.total_distance);
    /* one metre of climb per point, ten in all */
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 10.0f, info.total_climb);
}

static void test_the_rider_walks_the_route_and_can_leave_it(void)
{
    add_route("/SD:/RIDE.PAR", 11U, 100.0f);
    TEST_ASSERT_EQUAL(APP_OK, parcours_load("/SD:/RIDE.PAR"));
    TEST_ASSERT_EQUAL(APP_OK, parcours_start());
    TEST_ASSERT_TRUE(parcours_is_active());

    /* on the route, at the third point */
    parcours_update(BASE_LAT + DEG_LAT(300.0f), BASE_LON, 203.0f);

    nav_info_t nav;

    TEST_ASSERT_EQUAL(APP_OK, parcours_get_nav_info(&nav));
    TEST_ASSERT_TRUE(nav.on_route);

    /* two hundred metres to the side: out of the route */
    parcours_update(BASE_LAT + DEG_LAT(300.0f), BASE_LON + 0.0027f, 203.0f);
    TEST_ASSERT_EQUAL(APP_OK, parcours_get_nav_info(&nav));
    TEST_ASSERT_FALSE(nav.on_route);

    /* and back on it, which the legacy also allows */
    parcours_update(BASE_LAT + DEG_LAT(400.0f), BASE_LON, 204.0f);
    TEST_ASSERT_EQUAL(APP_OK, parcours_get_nav_info(&nav));
    TEST_ASSERT_TRUE(nav.on_route);
}

static void test_the_real_routes_of_the_legacy_load(void)
{
    static const char *const names[] = {"MJ_40.PAR", "ROTT.PAR"};
    static char body[65536];

    for (unsigned int i = 0U; i < (sizeof(names) / sizeof(names[0])); i++) {
        char host_path[256];
        char card_path[32];
        FILE *f;

        (void)snprintf(host_path, sizeof(host_path), "%s/%s", PARCOURS_DB_DIR, names[i]);
        f = fopen(host_path, "rb");
        if (f == NULL) {
            TEST_FAIL_MESSAGE("route of tools/TDD/DB not found");
        }

        size_t n = fread(body, 1U, sizeof(body) - 1U, f);

        (void)fclose(f);
        body[n] = '\0';

        host_fs_reset();
        (void)snprintf(card_path, sizeof(card_path), "/SD:/%s", names[i]);
        TEST_ASSERT_TRUE(host_fs_add_file(card_path, body));

        parcours_unload();
        TEST_ASSERT_EQUAL(APP_OK, parcours_load(card_path));
        TEST_ASSERT_TRUE(parcours_get_num_points() > 100U);

        /* real positions in Europe, with an altitude that makes sense */
        const point_t *p = parcours_get_point(0U);
        const point_t *last = parcours_get_point((uint16_t)(parcours_get_num_points() - 1U));

        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_NOT_NULL(last);
        TEST_ASSERT_TRUE((p->lat > 40.0f) && (p->lat < 60.0f));
        TEST_ASSERT_TRUE((p->lon > -5.0f) && (p->lon < 15.0f));
        /* ROTT.PAR is Rotterdam: the route runs below sea level */
        TEST_ASSERT_TRUE((p->alt > -50.0f) && (p->alt < 3000.0f));
        TEST_ASSERT_TRUE((last->lat > 40.0f) && (last->lat < 60.0f));

        /* and a length that is a real ride, not a couple of points */
        parcours_info_t info;

        TEST_ASSERT_EQUAL(APP_OK, parcours_get_info(&info));
        TEST_ASSERT_TRUE(info.total_distance > 1000.0f);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_route_of_the_legacy_loads);
    RUN_TEST(test_the_end_of_line_of_the_legacy_does_not_stop_the_loader);
    RUN_TEST(test_a_point_without_altitude_still_counts);
    RUN_TEST(test_the_last_line_without_an_end_of_line_counts);
    RUN_TEST(test_a_route_with_one_point_is_refused);
    RUN_TEST(test_a_file_that_is_not_there_gives_an_error);
    RUN_TEST(test_loading_again_replaces_the_route);
    RUN_TEST(test_the_route_knows_its_length_and_climb);
    RUN_TEST(test_the_rider_walks_the_route_and_can_leave_it);
    RUN_TEST(test_the_real_routes_of_the_legacy_load);

    return UNITY_END();
}
