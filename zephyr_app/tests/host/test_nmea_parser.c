/**
 * @file test_nmea_parser.c
 * @brief Host tests for drivers/gps/nmea_parser.c.
 *
 * Sentences are real NMEA 0183 lines with valid checksums (computed with the
 * XOR of every character between '$' and '*'). Positions are in Nancy, where
 * the legacy simulation traces were recorded.
 */

#include <string.h>

#include "unity.h"

#include "drivers/nmea_parser.h"

#define GGA_FIX   "$GPGGA,123519.000,4841.5260,N,00611.0640,E,1,08,0.9,212.5,M,47.9,M,,*52"
#define GGA_NOFIX "$GPGGA,123519.000,,,,,0,00,99.9,,M,,M,,*62"
#define RMC_FIX   "$GPRMC,123519.200,A,4841.5260,N,00611.0640,E,12.5,054.7,180926,,,A*59"
#define RMC_FIX_1 "$GPRMC,123519.2,A,4841.5260,N,00611.0640,E,12.5,054.7,180926,,,A*59"
#define RMC_FIX_2 "$GPRMC,123519.25,A,4841.5260,N,00611.0640,E,12.5,054.7,180926,,,A*6C"
#define RMC_VOID  "$GPRMC,123519.000,V,,,,,,,180926,,,N*44"
#define RMC_SW    "$GPRMC,081836.000,A,3751.6500,S,14507.3600,W,0.0,360.0,130998,011.3,E,A*03"
#define GSA_3D    "$GPGSA,A,3,04,05,09,12,,,,,,,,,2.5,1.3,2.1*3F"
#define GSV_1     "$GPGSV,2,1,08,04,77,040,46,05,33,070,41,09,20,210,35,12,45,300,40*7C"
#define GSV_2     "$GPGSV,2,2,08,17,10,160,,20,05,020,22,25,60,250,38,29,15,330,*76"

/* 48 + 41.5260 / 60 and 6 + 11.0640 / 60 */
#define NANCY_LAT 48.692100f
#define NANCY_LON 6.184400f

/* A float degree near 48 has a resolution of about 4e-6 (0.4 m). */
#define DEG_TOL   2e-5f

/* Feeds a whole line, CR LF included, and returns true if a sentence was parsed. */
static bool feed(const char *line)
{
    bool complete = false;

    for (const char *c = line; *c != '\0'; c++) {
        complete = nmea_parser_char(*c) || complete;
    }
    complete = nmea_parser_char('\r') || complete;
    complete = nmea_parser_char('\n') || complete;
    return complete;
}

/*
 * Parses a whole line through the sentence API, the way gps_mgmt.c calls it:
 * with the leading '$' and the "*CS" checksum still in place.
 */
static app_err_t parse(const char *line, nmea_data_t *data)
{
    return nmea_parser_sentence(line, data);
}

void setUp(void)
{
    nmea_parser_init();
    nmea_parser_reset_satellites();
}

void tearDown(void)
{
}

static void test_the_checksum_helpers_accept_good_lines_and_reject_bad_ones(void)
{
    TEST_ASSERT_TRUE(nmea_verify_checksum(GGA_FIX));
    TEST_ASSERT_TRUE(nmea_verify_checksum(RMC_SW));
    TEST_ASSERT_FALSE(nmea_verify_checksum("$GPGGA,123519.000,4841.5260,N,00611.0640,E,1,08,0.9,212.5,M,47.9,M,,*53"));
    TEST_ASSERT_FALSE(nmea_verify_checksum("GPGGA,no,dollar*00"));
    TEST_ASSERT_EQUAL_HEX8(0x52, nmea_calculate_checksum(
        "GPGGA,123519.000,4841.5260,N,00611.0640,E,1,08,0.9,212.5,M,47.9,M,,"));
}

static void test_a_gga_sentence_gives_position_altitude_and_quality(void)
{
    nmea_data_t data;

    TEST_ASSERT_EQUAL(APP_OK, parse(GGA_FIX, &data));
    TEST_ASSERT_EQUAL(NMEA_GGA, data.type);
    TEST_ASSERT_TRUE(data.fix_valid);
    TEST_ASSERT_FLOAT_WITHIN(DEG_TOL, NANCY_LAT, data.latitude);
    TEST_ASSERT_FLOAT_WITHIN(DEG_TOL, NANCY_LON, data.longitude);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 212.5f, data.altitude);
    TEST_ASSERT_EQUAL_UINT8(1U, data.fix_quality);
    TEST_ASSERT_EQUAL_UINT8(8U, data.satellites);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.9f, data.hdop);
    TEST_ASSERT_EQUAL_UINT8(12U, data.hour);
    TEST_ASSERT_EQUAL_UINT8(35U, data.minute);
    TEST_ASSERT_EQUAL_UINT8(19U, data.second);
}

static void test_a_gga_without_fix_is_not_valid(void)
{
    nmea_data_t data;

    TEST_ASSERT_EQUAL(APP_OK, parse(GGA_NOFIX, &data));
    TEST_ASSERT_FALSE(data.fix_valid);
    TEST_ASSERT_EQUAL_UINT8(0U, data.satellites);
}

static void test_an_rmc_sentence_gives_speed_in_kmh_course_and_date(void)
{
    nmea_data_t data;

    TEST_ASSERT_EQUAL(APP_OK, parse(RMC_FIX, &data));
    TEST_ASSERT_EQUAL(NMEA_RMC, data.type);
    TEST_ASSERT_TRUE(data.fix_valid);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 12.5f, data.speed_knots);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 12.5f * 1.852f, data.speed_kmh);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 54.7f, data.course);
    TEST_ASSERT_EQUAL_UINT8(18U, data.day);
    TEST_ASSERT_EQUAL_UINT8(9U, data.month);
    TEST_ASSERT_EQUAL_UINT16(2026U, data.year);
}

static void test_the_fraction_of_a_second_gives_milliseconds(void)
{
    nmea_data_t data;

    /* One to three decimals: ".2", ".20" and ".200" are all 200 ms */
    TEST_ASSERT_EQUAL(APP_OK, parse(RMC_FIX, &data));
    TEST_ASSERT_EQUAL_UINT8(19U, data.second);
    TEST_ASSERT_EQUAL_UINT16(200U, data.millisecond);
    TEST_ASSERT_EQUAL(APP_OK, parse(RMC_FIX_1, &data));
    TEST_ASSERT_EQUAL_UINT16(200U, data.millisecond);
    TEST_ASSERT_EQUAL(APP_OK, parse(RMC_FIX_2, &data));
    TEST_ASSERT_EQUAL_UINT16(250U, data.millisecond);
}

static void test_a_void_rmc_is_not_a_fix(void)
{
    nmea_data_t data;

    TEST_ASSERT_EQUAL(APP_OK, parse(RMC_VOID, &data));
    TEST_ASSERT_FALSE(data.fix_valid);
}

static void test_southern_and_western_coordinates_are_negative(void)
{
    nmea_data_t data;

    TEST_ASSERT_EQUAL(APP_OK, parse(RMC_SW, &data));
    TEST_ASSERT_FLOAT_WITHIN(DEG_TOL, -(37.0f + 51.65f / 60.0f), data.latitude);
    TEST_ASSERT_FLOAT_WITHIN(DEG_TOL, -(145.0f + 7.36f / 60.0f), data.longitude);
}

static void test_a_line_fed_char_by_char_is_parsed_and_gives_a_position(void)
{
    nmea_data_t data;

    TEST_ASSERT_FALSE(nmea_parser_has_position());
    TEST_ASSERT_TRUE(feed(GGA_FIX));
    TEST_ASSERT_TRUE(nmea_parser_has_position());
    TEST_ASSERT_TRUE(nmea_parser_has_time());
    TEST_ASSERT_EQUAL(APP_OK, nmea_parser_get_data(&data));
    TEST_ASSERT_FLOAT_WITHIN(DEG_TOL, NANCY_LAT, data.latitude);
}

static void test_a_void_rmc_clears_the_position_and_a_gsa_keeps_it(void)
{
    TEST_ASSERT_TRUE(feed(GGA_FIX));
    TEST_ASSERT_TRUE(nmea_parser_has_position());
    TEST_ASSERT_TRUE(feed(GSA_3D));     /* no position in a GSA: nothing changes */
    TEST_ASSERT_TRUE(nmea_parser_has_position());
    TEST_ASSERT_TRUE(feed(RMC_VOID));
    TEST_ASSERT_FALSE(nmea_parser_has_position());
}

static void test_a_line_with_a_wrong_checksum_is_dropped(void)
{
    TEST_ASSERT_FALSE(feed("$GPGGA,123519.000,4841.5260,N,00611.0640,E,1,08,0.9,212.5,M,47.9,M,,*53"));
    TEST_ASSERT_FALSE(nmea_parser_has_position());
}

static void test_an_overlong_line_is_dropped_and_the_next_one_is_parsed(void)
{
    char longline[200];

    memset(longline, 'A', sizeof(longline) - 1U);
    longline[0] = '$';
    longline[sizeof(longline) - 1U] = '\0';

    TEST_ASSERT_FALSE(feed(longline));
    TEST_ASSERT_TRUE(feed(GGA_FIX));
}

static void test_gsa_and_gsv_give_the_satellites_in_view_and_in_use(void)
{
    nmea_satellites_t sats;

    TEST_ASSERT_TRUE(feed(GSA_3D));
    TEST_ASSERT_TRUE(feed(GSV_1));
    TEST_ASSERT_TRUE(feed(GSV_2));
    TEST_ASSERT_EQUAL(APP_OK, nmea_parser_get_satellites(&sats));

    TEST_ASSERT_EQUAL_UINT8(8U, sats.sats_in_view);
    TEST_ASSERT_EQUAL_UINT8(8U, sats.count);
    TEST_ASSERT_EQUAL_UINT8(4U, sats.in_use_count);
    TEST_ASSERT_EQUAL_UINT8(4U, sats.sats[0].prn);
    TEST_ASSERT_TRUE(sats.sats[0].in_use);
    TEST_ASSERT_EQUAL_UINT8(46U, sats.sats[0].snr);
    TEST_ASSERT_EQUAL_UINT8(17U, sats.sats[4].prn);
    TEST_ASSERT_FALSE(sats.sats[4].in_use);
    TEST_ASSERT_EQUAL_UINT8(0U, sats.sats[4].snr);   /* empty SNR: not tracking */
    /* Mean SNR of the satellites in use: (46 + 41 + 35 + 40) / 4 */
    TEST_ASSERT_EQUAL_UINT8(40U, nmea_parser_get_avg_snr());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_checksum_helpers_accept_good_lines_and_reject_bad_ones);
    RUN_TEST(test_a_gga_sentence_gives_position_altitude_and_quality);
    RUN_TEST(test_a_gga_without_fix_is_not_valid);
    RUN_TEST(test_an_rmc_sentence_gives_speed_in_kmh_course_and_date);
    RUN_TEST(test_the_fraction_of_a_second_gives_milliseconds);
    RUN_TEST(test_a_void_rmc_is_not_a_fix);
    RUN_TEST(test_southern_and_western_coordinates_are_negative);
    RUN_TEST(test_a_line_fed_char_by_char_is_parsed_and_gives_a_position);
    RUN_TEST(test_a_void_rmc_clears_the_position_and_a_gsa_keeps_it);
    RUN_TEST(test_a_line_with_a_wrong_checksum_is_dropped);
    RUN_TEST(test_an_overlong_line_is_dropped_and_the_next_one_is_parsed);
    RUN_TEST(test_gsa_and_gsv_give_the_satellites_in_view_and_in_use);
    return UNITY_END();
}
