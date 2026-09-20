/**
 * @file test_segment_file.c
 * @brief Segment files of the legacy (src/model/segment_file.c)
 *
 * The oracle is the real database of the project: the 140 files of
 * `tools/TDD/DB/`, recorded by the legacy. For each one the test decodes
 * the position from the name and compares it with the first point inside,
 * which is what `calculePos()` promises (`libraries/utils/utils.c:201-227`):
 * the name carries the start of the segment with a resolution of 1e-5
 * degrees, about 1,1 m.
 */

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "model/segment_file.h"

#ifndef SEGMENT_DB_DIR
#define SEGMENT_DB_DIR "../../../tools/TDD/DB"
#endif

void setUp(void) {}
void tearDown(void) {}

static void test_a_name_of_the_legacy_is_recognized(void)
{
    TEST_ASSERT_TRUE(segment_file_name_is_valid("4NWKZ#KN.OAI"));
    TEST_ASSERT_TRUE(segment_file_name_is_valid("0Z9AB#12.345"));

    TEST_ASSERT_FALSE(segment_file_name_is_valid("4NWKZ#KN.OA"));    /* short */
    TEST_ASSERT_FALSE(segment_file_name_is_valid("4NWKZ-KN.OAI"));   /* no # */
    TEST_ASSERT_FALSE(segment_file_name_is_valid("4NWKZ#KNXOAI"));   /* no dot */
    TEST_ASSERT_FALSE(segment_file_name_is_valid("4nwkz#kn.oai"));   /* lower case */
    TEST_ASSERT_FALSE(segment_file_name_is_valid("MJ_40.PAR"));      /* a course */
    TEST_ASSERT_FALSE(segment_file_name_is_valid(NULL));
}

static void test_the_name_carries_the_position(void)
{
    float lat = 0.0f;
    float lon = 0.0f;

    /* the first file of the database, whose first point is -11.662205, 166.968906 */
    TEST_ASSERT_TRUE(segment_file_position("4NWKZ#KN.OAI", &lat, &lon));
    TEST_ASSERT_FLOAT_WITHIN(0.00002f, -11.66221f, lat);
    TEST_ASSERT_FLOAT_WITHIN(0.00002f, 166.96890f, lon);
}

static void test_a_line_of_points_is_read(void)
{
    struct segment_file_point p;

    TEST_ASSERT_TRUE(segment_file_parse_line("-11.662205 ; 166.968906 ; 518.0 ; 8.0", &p));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -11.662205f, p.lat);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 166.968906f, p.lon);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 518.0f, p.rtime);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.0f, p.alt);
}

static void test_the_name_line_and_rubbish_are_not_points(void)
{
    struct segment_file_point p;

    TEST_ASSERT_FALSE(segment_file_parse_line("<Name>MOUNT ZWIFT</Name>", &p));
    TEST_ASSERT_FALSE(segment_file_parse_line("", &p));
    TEST_ASSERT_FALSE(segment_file_parse_line("48.69 ; 6.18", &p));       /* missing fields */
    TEST_ASSERT_FALSE(segment_file_parse_line("48.69 6.18 1.0 200", &p)); /* no separator */
}

static void test_a_name_comes_back_from_a_position(void)
{
    char name[SEGMENT_FILE_NAME_LEN + 1U];
    float lat = 0.0f;
    float lon = 0.0f;

    TEST_ASSERT_TRUE(segment_file_name_of(name, sizeof(name), 48.6921f, 6.1844f));
    TEST_ASSERT_EQUAL_UINT(SEGMENT_FILE_NAME_LEN, strlen(name));
    TEST_ASSERT_TRUE(segment_file_name_is_valid(name));

    TEST_ASSERT_TRUE(segment_file_position(name, &lat, &lon));
    TEST_ASSERT_FLOAT_WITHIN(0.00002f, 48.6921f, lat);
    TEST_ASSERT_FLOAT_WITHIN(0.00002f, 6.1844f, lon);

    /* the name of the database comes back the same */
    TEST_ASSERT_TRUE(segment_file_position("4NWKZ#KN.OAI", &lat, &lon));
    TEST_ASSERT_TRUE(segment_file_name_of(name, sizeof(name), lat, lon));
    TEST_ASSERT_EQUAL_STRING("4NWKZ#KN.OAI", name);
}

/**
 * Every file of the database: the name has to agree with the first point
 * inside, within the resolution of the name.
 */
static void test_every_file_of_the_database_agrees_with_its_name(void)
{
    DIR *dir = opendir(SEGMENT_DB_DIR);

    if (dir == NULL) {
        TEST_IGNORE_MESSAGE("tools/TDD/DB not reachable from here");
        return;
    }

    unsigned int checked = 0U;
    unsigned int points = 0U;
    const struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (!segment_file_name_is_valid(entry->d_name)) {
            continue;
        }

        float name_lat = 0.0f;
        float name_lon = 0.0f;

        TEST_ASSERT_TRUE(segment_file_position(entry->d_name, &name_lat, &name_lon));

        char path[512];

        (void)snprintf(path, sizeof(path), "%s/%s", SEGMENT_DB_DIR, entry->d_name);

        FILE *f = fopen(path, "r");

        TEST_ASSERT_NOT_NULL(f);

        char line[256];
        struct segment_file_point first = {0};
        bool has_first = false;

        while (fgets(line, sizeof(line), f) != NULL) {
            struct segment_file_point p;

            if (!segment_file_parse_line(line, &p)) {
                continue;
            }
            points++;
            if (!has_first) {
                first = p;
                has_first = true;
            }
        }
        (void)fclose(f);

        TEST_ASSERT_TRUE_MESSAGE(has_first, entry->d_name);
        /* 1e-5 degree of resolution, one unit of rounding on each axis */
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.00002f, first.lat, name_lat, entry->d_name);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.00002f, first.lon, name_lon, entry->d_name);
        checked++;
    }
    (void)closedir(dir);

    TEST_ASSERT_GREATER_THAN_UINT(100U, checked);   /* the database has 138 segments */
    TEST_ASSERT_GREATER_THAN_UINT(5000U, points);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_name_of_the_legacy_is_recognized);
    RUN_TEST(test_the_name_carries_the_position);
    RUN_TEST(test_a_line_of_points_is_read);
    RUN_TEST(test_the_name_line_and_rubbish_are_not_points);
    RUN_TEST(test_a_name_comes_back_from_a_position);
    RUN_TEST(test_every_file_of_the_database_agrees_with_its_name);

    return UNITY_END();
}
