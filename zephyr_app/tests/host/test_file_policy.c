/**
 * @file test_file_policy.c
 * @brief What the phone may read and write (src/model/file_policy.c)
 *
 * The rules are of this port: the legacy took any file from anyone that
 * called itself stravaAP. Here a transfer only touches the root of the
 * storage, only writes what the firmware knows how to read (a route or a
 * segment) and never takes an activity from outside.
 */

#include "unity.h"

#include "model/file_policy.h"

void setUp(void) {}
void tearDown(void) {}

static void test_a_route_is_a_route(void)
{
    /* the course of this project */
    TEST_ASSERT_EQUAL_INT(FILE_KIND_ROUTE, file_policy_kind("/SD:/SERRA.RTE"));
    /* a GPX as Strava or Komoot exports it, with no conversion */
    TEST_ASSERT_EQUAL_INT(FILE_KIND_ROUTE, file_policy_kind("/SD:/serra.gpx"));
    TEST_ASSERT_EQUAL_INT(FILE_KIND_ROUTE, file_policy_kind("/SD:/VOLTA.TCX"));
    /* and the text of the legacy */
    TEST_ASSERT_EQUAL_INT(FILE_KIND_ROUTE, file_policy_kind("/SD:/MJ_40.PAR"));
    TEST_ASSERT_EQUAL_INT(FILE_KIND_ROUTE, file_policy_kind("/SD:/rott.par"));
    TEST_ASSERT_EQUAL_INT(FILE_KIND_ROUTE, file_policy_kind("/SD:/SERRA.CRS"));

    TEST_ASSERT_TRUE(file_policy_allows("/SD:/SERRA.RTE", FILE_ACCESS_WRITE));
    TEST_ASSERT_TRUE(file_policy_allows("/SD:/serra.gpx", FILE_ACCESS_WRITE));
}

static void test_a_segment_of_the_legacy_is_a_segment(void)
{
    /* the base-36 name with the position of the start */
    TEST_ASSERT_EQUAL_INT(FILE_KIND_SEGMENT, file_policy_kind("/SD:/4NWKZ#KN.OAI"));
    TEST_ASSERT_EQUAL_INT(FILE_KIND_SEGMENT, file_policy_kind("/SD:/8BIFA#B2.WN9"));
}

static void test_an_activity_is_a_log(void)
{
    TEST_ASSERT_EQUAL_INT(FILE_KIND_LOG, file_policy_kind("/SD:/@50925.txt"));
    TEST_ASSERT_EQUAL_INT(FILE_KIND_LOG, file_policy_kind("/SD:/@201226.TXT"));
}

static void test_anything_else_is_unknown(void)
{
    TEST_ASSERT_EQUAL_INT(FILE_KIND_UNKNOWN, file_policy_kind("/SD:/FOTO.JPG"));
    TEST_ASSERT_EQUAL_INT(FILE_KIND_UNKNOWN, file_policy_kind("/SD:/readme"));
    TEST_ASSERT_EQUAL_INT(FILE_KIND_UNKNOWN, file_policy_kind(NULL));
}

static void test_the_transfer_stays_on_the_root(void)
{
    /* no directories, and nothing that climbs out of the storage */
    TEST_ASSERT_FALSE(file_policy_allows("/SD:/sub/ROTA.PAR", FILE_ACCESS_WRITE));
    TEST_ASSERT_FALSE(file_policy_allows("/SD:/../etc/ROTA.PAR", FILE_ACCESS_WRITE));
    TEST_ASSERT_FALSE(file_policy_allows("/lfs/ROTA.PAR", FILE_ACCESS_WRITE));
    TEST_ASSERT_FALSE(file_policy_allows("/SD:/", FILE_ACCESS_READ));
    TEST_ASSERT_FALSE(file_policy_allows("ROTA.PAR", FILE_ACCESS_WRITE));
}

static void test_the_phone_sends_routes_and_segments(void)
{
    TEST_ASSERT_TRUE(file_policy_allows("/SD:/SERRA.PAR", FILE_ACCESS_WRITE));
    TEST_ASSERT_TRUE(file_policy_allows("/SD:/4NWKZ#KN.OAI", FILE_ACCESS_WRITE));
}

static void test_the_phone_does_not_send_activities(void)
{
    /* the device is what writes a ride; taking one from outside would let
     * anyone in radio range forge it */
    TEST_ASSERT_FALSE(file_policy_allows("/SD:/@50925.txt", FILE_ACCESS_WRITE));
    /* but the rider downloads it */
    TEST_ASSERT_TRUE(file_policy_allows("/SD:/@50925.txt", FILE_ACCESS_READ));
    TEST_ASSERT_TRUE(file_policy_allows("/SD:/@50925.txt", FILE_ACCESS_STATUS));
    TEST_ASSERT_TRUE(file_policy_allows("/SD:/@50925.txt", FILE_ACCESS_HASH));
}

static void test_a_file_the_firmware_does_not_read_takes_no_room(void)
{
    TEST_ASSERT_FALSE(file_policy_allows("/SD:/FILME.MP4", FILE_ACCESS_WRITE));
    TEST_ASSERT_FALSE(file_policy_allows("/SD:/FILME.MP4", FILE_ACCESS_READ));
}

static void test_a_name_longer_than_the_card_takes_is_refused(void)
{
    TEST_ASSERT_FALSE(file_policy_allows("/SD:/UM_NOME_MUITO_COMPRIDO_DEMAIS.PAR",
                                         FILE_ACCESS_WRITE));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_route_is_a_route);
    RUN_TEST(test_a_segment_of_the_legacy_is_a_segment);
    RUN_TEST(test_an_activity_is_a_log);
    RUN_TEST(test_anything_else_is_unknown);
    RUN_TEST(test_the_transfer_stays_on_the_root);
    RUN_TEST(test_the_phone_sends_routes_and_segments);
    RUN_TEST(test_the_phone_does_not_send_activities);
    RUN_TEST(test_a_file_the_firmware_does_not_read_takes_no_room);
    RUN_TEST(test_a_name_longer_than_the_card_takes_is_refused);

    return UNITY_END();
}
