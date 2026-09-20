/**
 * @file test_route_file.c
 * @brief The route file of this project (src/model/route_file.c)
 *
 * The format is in `include/model/route_file.h`. The tests write the bytes
 * by hand, the way `tools/route_convert.py` writes them, and check that the
 * reader takes what is right and refuses what is not: another magic,
 * another version, counts that do not fit the file, a box that is not a
 * box, a cue sheet without its flag.
 *
 * The CRC is the IEEE one of zlib; `test_the_crc_is_the_one_of_zlib` pins
 * it against values computed with `zlib.crc32` so the tool and the firmware
 * cannot drift apart.
 */

#include <string.h>

#include "unity.h"

#include "model/route_file.h"

static uint8_t file[4096];
static size_t file_len;

static void put_u16(size_t off, uint16_t v)
{
    file[off] = (uint8_t)(v & 0xFFU);
    file[off + 1U] = (uint8_t)(v >> 8);
}

static void put_u32(size_t off, uint32_t v)
{
    file[off] = (uint8_t)(v & 0xFFU);
    file[off + 1U] = (uint8_t)((v >> 8) & 0xFFU);
    file[off + 2U] = (uint8_t)((v >> 16) & 0xFFU);
    file[off + 3U] = (uint8_t)((v >> 24) & 0xFFU);
}

/** A file with @p points points going north and @p cues turns */
static void build(uint32_t points, uint32_t cues)
{
    (void)memset(file, 0, sizeof(file));
    (void)memcpy(file, "RTE1", 4U);
    put_u16(4U, ROUTE_FILE_VERSION);
    put_u16(6U, (cues > 0U) ? ROUTE_FILE_FLAG_CUES : 0U);
    put_u32(8U, points);
    put_u32(12U, cues);
    put_u32(16U, 12345U);
    put_u32(20U, 678U);
    put_u32(24U, (uint32_t)486921000);      /* lat min */
    put_u32(28U, (uint32_t)487921000);      /* lat max */
    put_u32(32U, (uint32_t)61844000);       /* lon min */
    put_u32(36U, (uint32_t)61944000);       /* lon max */
    (void)memcpy(&file[40], "SERRA DO MAR", 12U);

    size_t off = ROUTE_FILE_HEADER_SIZE;

    for (uint32_t i = 0U; i < points; i++) {
        put_u32(off, (uint32_t)(486921000 + (int32_t)(i * 100U)));
        put_u32(off + 4U, (uint32_t)61844000);
        put_u16(off + 8U, (uint16_t)(int16_t)(300 + (int16_t)i));
        off += ROUTE_FILE_POINT_SIZE;
    }

    for (uint32_t i = 0U; i < cues; i++) {
        put_u32(off, i * 10U);
        file[off + 4U] = (uint8_t)((i % 2U) ? ROUTE_TURN_LEFT : ROUTE_TURN_RIGHT);
        (void)memcpy(&file[off + 6U], "RUA DAS FLORES", 14U);
        off += ROUTE_FILE_CUE_SIZE;
    }

    file_len = off;
    put_u32(60U, route_file_crc32(0U, &file[ROUTE_FILE_HEADER_SIZE],
                                  file_len - ROUTE_FILE_HEADER_SIZE));
}

void setUp(void)
{
    build(20U, 0U);
}

void tearDown(void) {}

static void test_the_magic_tells_a_route_file_from_the_text_of_the_legacy(void)
{
    TEST_ASSERT_TRUE(route_file_is_rte(file, file_len));

    /* the first line of a `.PAR` of the legacy */
    static const uint8_t par[] = "48.68770900 6.128876000 341.9\r\n";

    TEST_ASSERT_FALSE(route_file_is_rte(par, sizeof(par)));
    TEST_ASSERT_FALSE(route_file_is_rte(file, 3U));
    TEST_ASSERT_FALSE(route_file_is_rte(NULL, 10U));
}

static void test_the_header_says_what_the_menu_needs(void)
{
    struct route_header h;

    TEST_ASSERT_TRUE(route_file_header(&h, file, file_len, file_len));
    TEST_ASSERT_EQUAL_UINT16(ROUTE_FILE_VERSION, h.version);
    TEST_ASSERT_EQUAL_UINT32(20U, h.points);
    TEST_ASSERT_EQUAL_UINT32(0U, h.cues);
    TEST_ASSERT_EQUAL_UINT32(12345U, h.distance_m);
    TEST_ASSERT_EQUAL_UINT32(678U, h.climb_m);
    TEST_ASSERT_EQUAL_STRING("SERRA DO MAR", h.name);
    TEST_ASSERT_EQUAL_INT32(486921000, h.lat_min_e7);
    TEST_ASSERT_EQUAL_INT32(487921000, h.lat_max_e7);
}

static void test_the_points_come_out_as_they_went_in(void)
{
    struct route_point p;

    TEST_ASSERT_TRUE(route_file_point(&p, &file[ROUTE_FILE_HEADER_SIZE],
                                      file_len - ROUTE_FILE_HEADER_SIZE));
    TEST_ASSERT_EQUAL_INT32(486921000, p.lat_e7);
    TEST_ASSERT_EQUAL_INT32(61844000, p.lon_e7);
    TEST_ASSERT_EQUAL_INT16(300, p.alt_m);

    TEST_ASSERT_TRUE(route_file_point(&p, &file[ROUTE_FILE_HEADER_SIZE + (5U * ROUTE_FILE_POINT_SIZE)],
                                      ROUTE_FILE_POINT_SIZE));
    TEST_ASSERT_EQUAL_INT32(486921500, p.lat_e7);
    TEST_ASSERT_EQUAL_INT16(305, p.alt_m);

    /* half a point is no point */
    TEST_ASSERT_FALSE(route_file_point(&p, file, 9U));
}

static void test_a_point_below_sea_level_and_south_of_the_equator(void)
{
    uint8_t bytes[ROUTE_FILE_POINT_SIZE];
    struct route_point p;

    (void)memset(bytes, 0, sizeof(bytes));
    /* -23.55 deg, -46.63 deg, -13 m: Rotterdam is below the sea, Brazil is south */
    bytes[0] = 0x60; bytes[1] = 0x1B; bytes[2] = 0x7C; bytes[3] = 0xF1;
    bytes[8] = 0xF3; bytes[9] = 0xFF;

    TEST_ASSERT_TRUE(route_file_point(&p, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(p.lat_e7 < 0);
    TEST_ASSERT_EQUAL_INT16(-13, p.alt_m);
}

static void test_a_cue_sheet_carries_the_turn_and_the_street(void)
{
    struct route_cue c;

    build(20U, 3U);

    struct route_header h;

    TEST_ASSERT_TRUE(route_file_header(&h, file, file_len, file_len));
    TEST_ASSERT_EQUAL_UINT32(3U, h.cues);
    TEST_ASSERT_TRUE((h.flags & ROUTE_FILE_FLAG_CUES) != 0U);

    size_t off = ROUTE_FILE_HEADER_SIZE + (h.points * ROUTE_FILE_POINT_SIZE);

    TEST_ASSERT_TRUE(route_file_cue(&c, &file[off], ROUTE_FILE_CUE_SIZE));
    TEST_ASSERT_EQUAL_UINT32(0U, c.point);
    TEST_ASSERT_EQUAL_UINT8(ROUTE_TURN_RIGHT, c.turn);
    TEST_ASSERT_EQUAL_STRING("RUA DAS FLORES", c.street);

    TEST_ASSERT_TRUE(route_file_cue(&c, &file[off + ROUTE_FILE_CUE_SIZE], ROUTE_FILE_CUE_SIZE));
    TEST_ASSERT_EQUAL_UINT32(10U, c.point);
    TEST_ASSERT_EQUAL_UINT8(ROUTE_TURN_LEFT, c.turn);
}

static void test_a_turn_this_firmware_does_not_know_becomes_straight(void)
{
    struct route_cue c;
    uint8_t bytes[ROUTE_FILE_CUE_SIZE];

    (void)memset(bytes, 0, sizeof(bytes));
    bytes[4] = 200U; /* a kind from a newer tool */

    TEST_ASSERT_TRUE(route_file_cue(&c, bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT8(ROUTE_TURN_STRAIGHT, c.turn);
}

static void test_another_version_is_refused(void)
{
    struct route_header h;

    put_u16(4U, ROUTE_FILE_VERSION + 1U);
    TEST_ASSERT_FALSE(route_file_header(&h, file, file_len, file_len));
}

static void test_a_file_cut_in_half_is_refused(void)
{
    struct route_header h;

    /* the header says twenty points but only half of them arrived */
    TEST_ASSERT_FALSE(route_file_header(&h, file, file_len, file_len - 100U));
}

static void test_a_header_that_does_not_make_sense_is_refused(void)
{
    struct route_header h;

    build(1U, 0U); /* a route of one point is not a route */
    TEST_ASSERT_FALSE(route_file_header(&h, file, file_len, file_len));

    build(20U, 0U);
    put_u32(24U, (uint32_t)488000000); /* lat min above lat max */
    TEST_ASSERT_FALSE(route_file_header(&h, file, file_len, file_len));

    build(20U, 0U);
    put_u32(28U, (uint32_t)1900000000); /* a latitude that is not on the Earth */
    TEST_ASSERT_FALSE(route_file_header(&h, file, file_len, file_len));

    build(20U, 2U);
    put_u16(6U, 0U); /* cues without the flag that says so */
    TEST_ASSERT_FALSE(route_file_header(&h, file, file_len, file_len));

    TEST_ASSERT_FALSE(route_file_header(NULL, file, file_len, file_len));
}

static void test_the_crc_is_the_one_of_zlib(void)
{
    /* values from python: zlib.crc32(b"...") */
    TEST_ASSERT_EQUAL_HEX32(0x00000000U, route_file_crc32(0U, (const uint8_t *)"", 0U));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926U, route_file_crc32(0U, (const uint8_t *)"123456789", 9U));
    TEST_ASSERT_EQUAL_HEX32(0x414FA339U,
                            route_file_crc32(0U, (const uint8_t *)"The quick brown fox jumps over the lazy dog", 43U));

    /* and it chains, which is how a big file is checked in chunks */
    uint32_t crc = route_file_crc32(0U, (const uint8_t *)"1234", 4U);

    crc = route_file_crc32(crc, (const uint8_t *)"56789", 5U);
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926U, crc);
}

static void test_the_crc_of_the_body_is_the_one_in_the_header(void)
{
    struct route_header h;

    TEST_ASSERT_TRUE(route_file_header(&h, file, file_len, file_len));
    TEST_ASSERT_EQUAL_HEX32(h.crc32, route_file_crc32(0U, &file[ROUTE_FILE_HEADER_SIZE],
                                                      file_len - ROUTE_FILE_HEADER_SIZE));

    /* one byte of the body changed and the CRC no longer matches */
    file[ROUTE_FILE_HEADER_SIZE + 3U] ^= 0x01U;
    TEST_ASSERT_NOT_EQUAL(h.crc32, route_file_crc32(0U, &file[ROUTE_FILE_HEADER_SIZE],
                                                    file_len - ROUTE_FILE_HEADER_SIZE));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_magic_tells_a_route_file_from_the_text_of_the_legacy);
    RUN_TEST(test_the_header_says_what_the_menu_needs);
    RUN_TEST(test_the_points_come_out_as_they_went_in);
    RUN_TEST(test_a_point_below_sea_level_and_south_of_the_equator);
    RUN_TEST(test_a_cue_sheet_carries_the_turn_and_the_street);
    RUN_TEST(test_a_turn_this_firmware_does_not_know_becomes_straight);
    RUN_TEST(test_another_version_is_refused);
    RUN_TEST(test_a_file_cut_in_half_is_refused);
    RUN_TEST(test_a_header_that_does_not_make_sense_is_refused);
    RUN_TEST(test_the_crc_is_the_one_of_zlib);
    RUN_TEST(test_the_crc_of_the_body_is_the_one_in_the_header);

    return UNITY_END();
}
