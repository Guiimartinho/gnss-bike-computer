/**
 * @file test_sd_logger.c
 * @brief Host tests for model/sd_logger.c.
 *
 * Legacy behaviour (legacy/source/model/Attitude.cpp:430-487): one snapshot
 * every 15 m, written in batches of 5 (ATT_BUFFER_NB_ELEM).
 */

#include <string.h>

#include "unity.h"

#include "host_fs.h"
#include "model/sd_logger.h"

#define CANARY_BYTE 0xA5U

/* The logger with guard bytes right after it: a write past buffer[] lands there. */
static struct {
    sd_logger_t logger;
    uint8_t canary[512];
} box;

static sd_log_entry_t make_entry(float distance)
{
    sd_log_entry_t entry;
    loc_data_t loc = { .lat = 48.6921f, .lon = 6.1844f, .alt = 210.0f,
                       .speed = 25.0f, .course = 90.0f, .timestamp = 1000U };
    date_data_t date = { .date = 180926U, .secj = 43200U, .timestamp = 1000U };

    sd_log_altitude_t alti = {
        .baro_alt = 212.0f,
        .baro_corr = -1.5f,
        .filt_alt = 211.0f,
        .gps_alt = 210.0f,
        .alpha_bar = 0.0123f,
        .alpha_zero = -0.0456f,
        .vit_asc = 0.75f,
        .rough = {12.0f, 34.0f, 56.0f},
        .b_rough = 7.25f,
        .slope = 3,
    };

    sd_logger_build_entry(&entry, &loc, &date, 180, 140U, 85U, 2500U, &alti, distance, 12.0f);
    return entry;
}

static unsigned int count_lines(const char *text)
{
    unsigned int lines = 0U;

    for (const char *c = text; *c != '\0'; c++) {
        if (*c == '\n') {
            lines++;
        }
    }
    return lines;
}

void setUp(void)
{
    date_data_t date = { .date = 180926U, .secj = 43200U, .timestamp = 0U };

    host_fs_reset();
    memset(&box, 0, sizeof(box));
    memset(box.canary, CANARY_BYTE, sizeof(box.canary));
    TEST_ASSERT_EQUAL(APP_OK, sd_logger_init(&box.logger));
    TEST_ASSERT_EQUAL(APP_OK, sd_logger_start(&box.logger, &date));
}

void tearDown(void)
{
}

static void test_entries_closer_than_15_m_are_skipped(void)
{
    sd_log_entry_t entry = make_entry(10.0f);

    TEST_ASSERT_EQUAL(APP_OK, sd_logger_add_entry(&box.logger, &entry, 10.0f));
    TEST_ASSERT_EQUAL_UINT8(0U, box.logger.buffer_count);
}

static void test_five_entries_15_m_apart_are_written_in_the_format_of_the_legacy(void)
{
    for (int i = 1; i <= 5; i++) {
        float d = 15.0f * (float)i;
        sd_log_entry_t entry = make_entry(d);

        TEST_ASSERT_EQUAL(APP_OK, sd_logger_add_entry(&box.logger, &entry, d));
    }

    TEST_ASSERT_EQUAL_UINT8(0U, box.logger.buffer_count);
    TEST_ASSERT_EQUAL_UINT32(5U, sd_logger_get_count(&box.logger));
    /* the legacy writes only points, without a header line */
    TEST_ASSERT_EQUAL_STRING_LEN("48.692101;6.184400", host_fs_content(), 18);
    TEST_ASSERT_EQUAL_UINT(5U, count_lines(host_fs_content()));
}

static void test_a_missing_card_never_writes_past_the_buffer(void)
{
    host_fs_set_available(false);

    for (int i = 1; i <= 30; i++) {
        float d = 15.0f * (float)i;
        sd_log_entry_t entry = make_entry(d);

        (void)sd_logger_add_entry(&box.logger, &entry, d);
        TEST_ASSERT_TRUE(box.logger.buffer_count <= SD_LOG_BUFFER_SIZE);
    }

    for (size_t i = 0U; i < sizeof(box.canary); i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(CANARY_BYTE, box.canary[i], "write past sd_logger_t");
    }
}

static void test_logging_resumes_when_the_card_comes_back(void)
{
    host_fs_set_available(false);
    for (int i = 1; i <= 7; i++) {
        float d = 15.0f * (float)i;
        sd_log_entry_t entry = make_entry(d);

        (void)sd_logger_add_entry(&box.logger, &entry, d);
    }

    host_fs_set_available(true);
    TEST_ASSERT_EQUAL(APP_OK, sd_logger_flush(&box.logger));
    TEST_ASSERT_TRUE(sd_logger_get_count(&box.logger) > 0U);
    TEST_ASSERT_EQUAL_UINT8(0U, box.logger.buffer_count);
}

static void test_the_line_has_the_nineteen_fields_of_the_legacy(void)
{
    sd_log_entry_t entry = make_entry(100.0f);
    char line[256];
    int len = sd_logger_format_line(line, sizeof(line), &entry);

    TEST_ASSERT_GREATER_THAN_INT(0, len);

    unsigned int fields = 0U;

    for (int i = 0; i < len; i++) {
        if (line[i] == ';') {
            fields++;
        }
    }
    TEST_ASSERT_EQUAL_UINT(19U, fields);

    /* ends with CRLF, as the legacy writes */
    TEST_ASSERT_EQUAL_CHAR(13, line[len - 2]); /* CR */
    TEST_ASSERT_EQUAL_CHAR(10, line[len - 1]); /* LF */

    /* position with six decimals, altitude with two, seconds of the day */
    TEST_ASSERT_EQUAL_STRING_LEN("48.692101;6.184400;210.00;43200;", line, 32);
}

static void test_the_line_carries_the_angles_and_the_roughness(void)
{
    sd_log_entry_t entry = make_entry(100.0f);
    char line[256];

    (void)sd_logger_format_line(line, sizeof(line), &entry);

    TEST_ASSERT_NOT_NULL(strstr(line, ";0.012;-0.046;212.000;-1.50;"));
    TEST_ASSERT_NOT_NULL(strstr(line, ";12;34;56;7.2;"));
}

static void test_the_name_is_the_one_of_the_legacy(void)
{
    char name[64];

    /* 5 September 2025 is 50925, without a leading zero on the day */
    sd_logger_filename(name, sizeof(name), 50925U);
    TEST_ASSERT_EQUAL_STRING("/SD:/@50925.txt", name);

    sd_logger_filename(name, sizeof(name), 180926U);
    TEST_ASSERT_EQUAL_STRING("/SD:/@180926.txt", name);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_line_has_the_nineteen_fields_of_the_legacy);
    RUN_TEST(test_the_line_carries_the_angles_and_the_roughness);
    RUN_TEST(test_the_name_is_the_one_of_the_legacy);
    RUN_TEST(test_entries_closer_than_15_m_are_skipped);
    RUN_TEST(test_five_entries_15_m_apart_are_written_in_the_format_of_the_legacy);
    RUN_TEST(test_a_missing_card_never_writes_past_the_buffer);
    RUN_TEST(test_logging_resumes_when_the_card_comes_back);
    return UNITY_END();
}
