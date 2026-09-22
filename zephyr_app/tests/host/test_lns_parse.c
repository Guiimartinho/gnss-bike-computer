/**
 * @file test_lns_parse.c
 * @brief Where the phone says it is (src/model/lns_parse.c)
 *
 * The third characteristic in this firmware with a bitfield at the front
 * and optional fields behind it, after the trainer's and the power
 * meter's, and it fails the same way: one wrong width and every field
 * after it comes out of the wrong bytes. The test that proves the walk is
 * the one with everything present, because the last field only lands right
 * if all of them were measured correctly.
 *
 * The rule that matters more than the walk is the **position status**. A
 * phone with no fix still sends coordinates, with two bits saying they mean
 * nothing. A reader that takes them anyway puts the rider at the last place
 * the phone thought it was, or at zero off the coast of Africa — and it
 * looks like an answer, which is what makes it worse than showing nothing.
 * Each of the four statuses has a case.
 *
 * The field order is the specification as this port understands it; no
 * phone has been on a bench here.
 */

#include <string.h>

#include "unity.h"

#include "model/lns_parse.h"

static uint8_t buf[64];
static uint16_t n;
static struct lns_location m;

void setUp(void)
{
    (void)memset(buf, 0, sizeof(buf));
    (void)memset(&m, 0, sizeof(m));
    n = 0U;
}

void tearDown(void) {}

static void put8(uint8_t v)
{
    buf[n] = v;
    n++;
}

static void put16(uint16_t v)
{
    put8((uint8_t)(v & 0xFFU));
    put8((uint8_t)(v >> 8));
}

static void put24(uint32_t v)
{
    put8((uint8_t)(v & 0xFFU));
    put8((uint8_t)((v >> 8) & 0xFFU));
    put8((uint8_t)((v >> 16) & 0xFFU));
}

static void put32(uint32_t v)
{
    put16((uint16_t)(v & 0xFFFFU));
    put16((uint16_t)(v >> 16));
}

/** Flags with a position status in the two bits that carry it */
static uint16_t with_status(uint16_t flags, enum lns_pos_status st)
{
    return (uint16_t)(flags | ((uint16_t)st << LNS_F_POS_STATUS_SHIFT));
}

/* Nancy, where the simulation traces of the legacy were recorded */
#define LAT_E7  486921000
#define LON_E7  61844000

/* ==========================================================================
 * The position status
 * ========================================================================== */

/** A payload with a location and the status asked for */
static void location_with(enum lns_pos_status st)
{
    n = 0U;
    put16(with_status(LNS_F_LOCATION, st));
    put32((uint32_t)LAT_E7);
    put32((uint32_t)LON_E7);
}

static void test_a_phone_with_a_real_position(void)
{
    location_with(LNS_POS_OK);

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_TRUE(m.have_location);
    TEST_ASSERT_EQUAL_INT32(LAT_E7, m.lat_e7);
    TEST_ASSERT_EQUAL_INT32(LON_E7, m.lon_e7);
    TEST_ASSERT_TRUE(lns_position_is_usable(&m));
}

static void test_a_phone_with_no_position_at_all(void)
{
    /*
     * It still sends the coordinates. Taking them would put the rider
     * wherever the phone happened to have in the field.
     */
    location_with(LNS_POS_NONE);

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_TRUE(m.have_location);
    TEST_ASSERT_FALSE(lns_position_is_usable(&m));
}

static void test_a_position_the_phone_only_guessed(void)
{
    location_with(LNS_POS_ESTIMATED);

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_EQUAL_UINT8(LNS_POS_ESTIMATED, m.status);
    TEST_ASSERT_FALSE(lns_position_is_usable(&m));
}

static void test_where_the_phone_was_and_not_where_it_is(void)
{
    location_with(LNS_POS_LAST_KNOWN);

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_EQUAL_UINT8(LNS_POS_LAST_KNOWN, m.status);
    TEST_ASSERT_FALSE(lns_position_is_usable(&m));
}

static void test_a_good_status_with_no_coordinates_is_still_nothing(void)
{
    /* the status says yes and the flags carry no location */
    n = 0U;
    put16(with_status(LNS_F_SPEED, LNS_POS_OK));
    put16(500U);

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_FALSE(m.have_location);
    TEST_ASSERT_FALSE(lns_position_is_usable(&m));
}

/* ==========================================================================
 * The fields
 * ========================================================================== */

static void test_a_notification_with_nothing_in_it(void)
{
    put16(0x0000U);

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_FALSE(m.have_location);
    TEST_ASSERT_FALSE(m.have_speed);
    TEST_ASSERT_FALSE(m.have_elevation);
    TEST_ASSERT_FALSE(m.have_heading);
}

static void test_the_speed_in_hundredths_of_a_metre_a_second(void)
{
    /* 8,33 m/s is 30 km/h */
    put16(LNS_F_SPEED);
    put16(833U);

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_TRUE(m.have_speed);
    TEST_ASSERT_EQUAL_UINT16(833U, m.speed_cms);
}

static void test_a_position_south_and_west_of_nothing(void)
{
    /* the coordinates are signed: the southern and western hemispheres */
    n = 0U;
    put16(with_status(LNS_F_LOCATION, LNS_POS_OK));
    put32((uint32_t)(int32_t)-238000000);   /* Rio de Janeiro */
    put32((uint32_t)(int32_t)-432000000);

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_EQUAL_INT32(-238000000, m.lat_e7);
    TEST_ASSERT_EQUAL_INT32(-432000000, m.lon_e7);
    TEST_ASSERT_TRUE(lns_position_is_usable(&m));
}

static void test_an_elevation_below_the_sea(void)
{
    /*
     * Three bytes with a sign in the top bit. The Dead Sea road is 400 m
     * down, and a reader that forgot the sign would put it at 167 km up.
     */
    put16(LNS_F_ELEVATION);
    put24(0xFF623CUL);      /* -40 388 hundredths, about -403,88 m */

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_TRUE(m.have_elevation);
    TEST_ASSERT_EQUAL_INT32(-40388, m.elevation_cm);
}

static void test_an_elevation_above_it(void)
{
    put16(LNS_F_ELEVATION);
    put24(20000UL);         /* 200,00 m */

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_EQUAL_INT32(20000, m.elevation_cm);
}

static void test_the_heading(void)
{
    put16(LNS_F_HEADING);
    put16(18000U);          /* due south */

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_TRUE(m.have_heading);
    TEST_ASSERT_EQUAL_UINT16(18000U, m.heading_cdeg);
}

/* ==========================================================================
 * The walk
 * ========================================================================== */

static void test_every_field_at_once_and_the_last_one_lands_right(void)
{
    /*
     * The heading is the last field this device keeps, and it sits behind
     * the distance, the location and the elevation. It only reads back as
     * 9000 if all of them were walked with the right widths — including
     * the three-byte distance, which nothing here keeps.
     */
    n = 0U;
    put16(with_status(LNS_F_SPEED | LNS_F_DISTANCE | LNS_F_LOCATION | LNS_F_ELEVATION |
                      LNS_F_HEADING | LNS_F_ROLLING_TIME | LNS_F_UTC_TIME,
                      LNS_POS_OK));
    put16(833U);                        /* speed */
    put24(123456UL);                    /* total distance, not kept */
    put32((uint32_t)LAT_E7);
    put32((uint32_t)LON_E7);
    put24(20000UL);                     /* elevation */
    put16(9000U);                       /* heading, due east */
    put8(42U);                          /* rolling time, not kept */
    put16(2026U); put8(9U); put8(22U); put8(7U); put8(42U); put8(0U);  /* clock */

    TEST_ASSERT_EQUAL_UINT16(28U, n);
    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));

    TEST_ASSERT_EQUAL_UINT16(833U, m.speed_cms);
    TEST_ASSERT_EQUAL_INT32(LAT_E7, m.lat_e7);
    TEST_ASSERT_EQUAL_INT32(LON_E7, m.lon_e7);
    TEST_ASSERT_EQUAL_INT32(20000, m.elevation_cm);
    TEST_ASSERT_EQUAL_UINT16(9000U, m.heading_cdeg);
    TEST_ASSERT_TRUE(lns_position_is_usable(&m));
}

static void test_a_gap_in_the_middle_does_not_shift_what_follows(void)
{
    /* distance present, location absent: the elevation must not slide */
    put16(LNS_F_DISTANCE | LNS_F_ELEVATION);
    put24(999UL);
    put24(12345UL);

    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_FALSE(m.have_location);
    TEST_ASSERT_TRUE(m.have_elevation);
    TEST_ASSERT_EQUAL_INT32(12345, m.elevation_cm);
}

static void test_the_bits_without_a_field_do_not_move_anything(void)
{
    /*
     * The position status and the two-or-three-dimension bit carry no
     * bytes. If the walk gave them a width, the heading below would come
     * out of the wrong place.
     */
    n = 0U;
    put16(with_status(LNS_F_3D_FORMAT | LNS_F_HEADING, LNS_POS_OK));
    put16(4500U);

    TEST_ASSERT_EQUAL_UINT16(4U, n);
    TEST_ASSERT_TRUE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_EQUAL_UINT16(4500U, m.heading_cdeg);
}

/* ==========================================================================
 * Payloads that cannot be read
 * ========================================================================== */

static void test_a_payload_too_short_for_the_flags(void)
{
    put8(0x04U);

    TEST_ASSERT_FALSE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_FALSE(lns_parse_location(buf, 0U, &m));
}

static void test_a_notification_that_lies_about_itself_is_dropped_whole(void)
{
    /*
     * The flags promise a location and the payload stops in the middle of
     * the longitude. Half a coordinate is a place the rider has never
     * been.
     */
    n = 0U;
    put16(with_status(LNS_F_LOCATION, LNS_POS_OK));
    put32((uint32_t)LAT_E7);
    put16(0x1234U);     /* half the longitude */

    TEST_ASSERT_FALSE(lns_parse_location(buf, n, &m));
    TEST_ASSERT_FALSE(m.have_location);
    TEST_ASSERT_EQUAL_INT32(0, m.lat_e7);
}

static void test_nothing_blows_up_without_a_payload(void)
{
    TEST_ASSERT_FALSE(lns_parse_location(NULL, 10U, &m));
    TEST_ASSERT_FALSE(lns_parse_location(buf, 10U, NULL));
    TEST_ASSERT_FALSE(lns_position_is_usable(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_phone_with_a_real_position);
    RUN_TEST(test_a_phone_with_no_position_at_all);
    RUN_TEST(test_a_position_the_phone_only_guessed);
    RUN_TEST(test_where_the_phone_was_and_not_where_it_is);
    RUN_TEST(test_a_good_status_with_no_coordinates_is_still_nothing);
    RUN_TEST(test_a_notification_with_nothing_in_it);
    RUN_TEST(test_the_speed_in_hundredths_of_a_metre_a_second);
    RUN_TEST(test_a_position_south_and_west_of_nothing);
    RUN_TEST(test_an_elevation_below_the_sea);
    RUN_TEST(test_an_elevation_above_it);
    RUN_TEST(test_the_heading);
    RUN_TEST(test_every_field_at_once_and_the_last_one_lands_right);
    RUN_TEST(test_a_gap_in_the_middle_does_not_shift_what_follows);
    RUN_TEST(test_the_bits_without_a_field_do_not_move_anything);
    RUN_TEST(test_a_payload_too_short_for_the_flags);
    RUN_TEST(test_a_notification_that_lies_about_itself_is_dropped_whole);
    RUN_TEST(test_nothing_blows_up_without_a_payload);

    return UNITY_END();
}
