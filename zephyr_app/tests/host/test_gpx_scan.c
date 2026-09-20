/**
 * @file test_gpx_scan.c
 * @brief Points out of a GPX (src/model/gpx_scan.c)
 *
 * The device takes a course as it comes from Strava, Komoot or
 * RideWithGPS, so the reader has to survive what those write: namespaces,
 * extensions it knows nothing about, attributes in either order, single or
 * double quotes, elements closed on themselves, and a file that arrives in
 * pieces because it is read in chunks.
 */

#include <math.h>
#include <string.h>

#include "unity.h"

#include "model/gpx_scan.h"

#define MAX_POINTS  16U

static struct gpx_scan scan;
static float lats[MAX_POINTS];
static float lons[MAX_POINTS];
static float alts[MAX_POINTS];
static unsigned int count;

static void collect(float lat, float lon, float alt, void *user)
{
    (void)user;
    if (count < MAX_POINTS) {
        lats[count] = lat;
        lons[count] = lon;
        alts[count] = alt;
    }
    count++;
}

/** Feed the whole text at once */
static void feed(const char *text)
{
    gpx_scan_feed(&scan, text, strlen(text), collect, NULL);
    gpx_scan_end(&scan, collect, NULL);
}

void setUp(void)
{
    gpx_scan_init(&scan);
    count = 0U;
    (void)memset(lats, 0, sizeof(lats));
    (void)memset(lons, 0, sizeof(lons));
    (void)memset(alts, 0, sizeof(alts));
}

void tearDown(void) {}

static void test_a_track_of_strava(void)
{
    feed("<?xml version=\"1.0\"?>\n"
         "<gpx creator=\"StravaGPX\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n"
         " <trk><name>Serra do Mar</name><trkseg>\n"
         "  <trkpt lat=\"-23.5505200\" lon=\"-46.6333090\"><ele>760.4</ele></trkpt>\n"
         "  <trkpt lat=\"-23.5506100\" lon=\"-46.6334000\"><ele>761.0</ele></trkpt>\n"
         " </trkseg></trk>\n</gpx>\n");

    TEST_ASSERT_EQUAL_UINT(2U, count);
    TEST_ASSERT_FLOAT_WITHIN(0.0000005f, -23.55052f, lats[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.0000005f, -46.633309f, lons[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 760.4f, alts[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 761.0f, alts[1]);
}

static void test_a_track_with_namespaces_and_extensions(void)
{
    /* what Komoot and Garmin write, with prefixes and things we ignore */
    feed("<gpx:gpx xmlns:gpx=\"http://www.topografix.com/GPX/1/1\">"
         "<gpx:trk><gpx:trkseg>"
         "<gpx:trkpt lat='48.6921000' lon='6.1844000'>"
         "<gpx:ele>240.0</gpx:ele>"
         "<gpx:time>2026-09-20T07:42:00Z</gpx:time>"
         "<extensions><gpxtpx:TrackPointExtension><gpxtpx:hr>152</gpxtpx:hr>"
         "</gpxtpx:TrackPointExtension></extensions>"
         "</gpx:trkpt>"
         "<gpx:trkpt lat='48.6922000' lon='6.1845000'><gpx:ele>241.5</gpx:ele></gpx:trkpt>"
         "</gpx:trkseg></gpx:trk></gpx:gpx>");

    TEST_ASSERT_EQUAL_UINT(2U, count);
    TEST_ASSERT_FLOAT_WITHIN(0.0000005f, 48.6921f, lats[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 240.0f, alts[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 241.5f, alts[1]);
}

static void test_the_attributes_come_in_any_order(void)
{
    feed("<gpx><trkpt lon=\"6.1844000\" lat=\"48.6921000\"><ele>240</ele></trkpt></gpx>");

    TEST_ASSERT_EQUAL_UINT(1U, count);
    TEST_ASSERT_FLOAT_WITHIN(0.0000005f, 48.6921f, lats[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.0000005f, 6.1844f, lons[0]);
}

static void test_a_point_without_altitude_is_taken_with_zero(void)
{
    feed("<gpx><trkpt lat='48.6921' lon='6.1844'/><trkpt lat='48.6922' lon='6.1845'/></gpx>");

    TEST_ASSERT_EQUAL_UINT(2U, count);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, alts[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.0000005f, 48.6922f, lats[1]);
}

static void test_a_route_of_waypoints_counts_as_a_course(void)
{
    /* some services export the course as `rte`, not as `trk` */
    feed("<gpx><rte><name>Volta</name>"
         "<rtept lat='48.6921' lon='6.1844'><ele>240</ele><name>Start</name></rtept>"
         "<rtept lat='48.6931' lon='6.1854'><ele>250</ele><name>Turn right</name></rtept>"
         "</rte></gpx>");

    TEST_ASSERT_EQUAL_UINT(2U, count);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 250.0f, alts[1]);
}

static void test_the_file_arrives_in_pieces(void)
{
    /* the loader reads the card in chunks: a point may straddle two of them */
    static const char whole[] =
        "<gpx><trkpt lat=\"48.6921000\" lon=\"6.1844000\"><ele>240.5</ele></trkpt>"
        "<trkpt lat=\"48.6922000\" lon=\"6.1845000\"><ele>241.5</ele></trkpt></gpx>";

    for (size_t cut = 1U; cut < (sizeof(whole) - 1U); cut += 7U) {
        gpx_scan_init(&scan);
        count = 0U;
        gpx_scan_feed(&scan, whole, cut, collect, NULL);
        gpx_scan_feed(&scan, &whole[cut], (sizeof(whole) - 1U) - cut, collect, NULL);
        gpx_scan_end(&scan, collect, NULL);

        TEST_ASSERT_EQUAL_UINT(2U, count);
        TEST_ASSERT_FLOAT_WITHIN(0.0000005f, 48.6921f, lats[0]);
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 241.5f, alts[1]);
    }
}

static void test_the_last_point_of_the_file_still_comes(void)
{
    /* a file cut before the closing tag still gives what it had */
    gpx_scan_feed(&scan, "<gpx><trkpt lat='1.5' lon='2.5'><ele>10</ele>", 44U, collect, NULL);
    TEST_ASSERT_EQUAL_UINT(0U, count);

    gpx_scan_end(&scan, collect, NULL);
    TEST_ASSERT_EQUAL_UINT(1U, count);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, lats[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 10.0f, alts[0]);
}

static void test_what_is_not_a_point_is_ignored(void)
{
    feed("<gpx><metadata><name>Nada</name><author><link href='x'><text>y</text></link>"
         "</author></metadata><wpt lat='1' lon='2'><name>casa</name></wpt></gpx>");

    /* `wpt` is a waypoint of the map, not a point of the course */
    TEST_ASSERT_EQUAL_UINT(0U, count);
}

static void test_a_file_that_is_not_a_gpx_is_recognised(void)
{
    static const char par[] = "48.68770900 6.128876000 341.9\r\n";
    static const char gpx[] = "<?xml version=\"1.0\"?><gpx>";
    static const char gpx_no_decl[] = "<gpx creator='x'>";

    TEST_ASSERT_TRUE(gpx_scan_looks_like_gpx(gpx, sizeof(gpx) - 1U));
    TEST_ASSERT_TRUE(gpx_scan_looks_like_gpx(gpx_no_decl, sizeof(gpx_no_decl) - 1U));
    TEST_ASSERT_FALSE(gpx_scan_looks_like_gpx(par, sizeof(par) - 1U));
    TEST_ASSERT_FALSE(gpx_scan_looks_like_gpx(NULL, 10U));
    TEST_ASSERT_FALSE(gpx_scan_looks_like_gpx("a", 1U));
}

static void test_an_element_longer_than_the_buffer_does_not_run_over(void)
{
    static char big[GPX_SCAN_BUF + 200U];

    (void)memset(big, 'a', sizeof(big));
    big[0] = '<';
    big[sizeof(big) - 1U] = '\0';

    gpx_scan_feed(&scan, big, strlen(big), collect, NULL);
    gpx_scan_end(&scan, collect, NULL);

    TEST_ASSERT_EQUAL_UINT(0U, count);
}

static void test_a_thousand_points_come_out_in_order(void)
{
    char one[96];

    gpx_scan_feed(&scan, "<gpx><trkseg>", 13U, collect, NULL);
    for (unsigned int i = 0U; i < 1000U; i++) {
        int n = snprintf(one, sizeof(one), "<trkpt lat='%.7f' lon='6.0'><ele>%u</ele></trkpt>",
                         48.0 + ((double)i * 0.0001), 200U + i);

        gpx_scan_feed(&scan, one, (size_t)n, collect, NULL);
    }
    gpx_scan_end(&scan, collect, NULL);

    TEST_ASSERT_EQUAL_UINT(1000U, count);
    TEST_ASSERT_EQUAL_UINT32(1000U, scan.points);
    /* only the first sixteen were kept, which is enough to see the order */
    TEST_ASSERT_FLOAT_WITHIN(0.0000005f, 48.0f, lats[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.0000005f, 48.0001f, lats[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 201.0f, alts[1]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_track_of_strava);
    RUN_TEST(test_a_track_with_namespaces_and_extensions);
    RUN_TEST(test_the_attributes_come_in_any_order);
    RUN_TEST(test_a_point_without_altitude_is_taken_with_zero);
    RUN_TEST(test_a_route_of_waypoints_counts_as_a_course);
    RUN_TEST(test_the_file_arrives_in_pieces);
    RUN_TEST(test_the_last_point_of_the_file_still_comes);
    RUN_TEST(test_what_is_not_a_point_is_ignored);
    RUN_TEST(test_a_file_that_is_not_a_gpx_is_recognised);
    RUN_TEST(test_an_element_longer_than_the_buffer_does_not_run_over);
    RUN_TEST(test_a_thousand_points_come_out_in_order);

    return UNITY_END();
}
