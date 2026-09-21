/**
 * @file test_fit_encode.c
 * @brief The activity in the Garmin FIT format (src/model/fit_encode.c)
 *
 * The bytes are checked against the format itself, not against another
 * implementation: the file header, the record headers, the definition
 * messages, the base types, the scales and the CRC-16 with the nibble
 * table. The last test builds a whole small ride and decodes it back,
 * which is what a reader like Strava does.
 *
 * The one thing that deserves its own test is how the encoder closes the
 * file: the file CRC covers the header, and the header carries a size that
 * only the end of the ride knows. The encoder never reads the file back,
 * and it does not have to, because the last two bytes of a header are the
 * CRC of the twelve before them and feeding a message followed by its own
 * CRC leaves this CRC at zero. The state after any valid header is zero,
 * whatever size it says, so writing the real header at the end changes
 * nothing. Two tests hold that up: one on the property itself, and one
 * that compares the file CRC with the plain calculation over the finished
 * bytes for thirty-three file lengths.
 */

#include <math.h>
#include <string.h>

#include "unity.h"

#include "model/fit_encode.h"

#define FILE_MAX    8192U

static struct fit_enc enc;
static uint8_t file[FILE_MAX];
static size_t len;
static uint8_t block[FIT_BLOCK_MAX];

/** Append what the encoder just built to the file being assembled */
static void append(size_t n)
{
    TEST_ASSERT_TRUE_MESSAGE(n > 0U, "the encoder refused to build the block");
    TEST_ASSERT_TRUE((len + n) <= FILE_MAX);
    (void)memcpy(&file[len], block, n);
    len += n;
}

static uint16_t rd16(size_t at)
{
    return (uint16_t)((uint16_t)file[at] | ((uint16_t)file[at + 1U] << 8));
}

static uint32_t rd32(size_t at)
{
    return (uint32_t)file[at] | ((uint32_t)file[at + 1U] << 8) |
           ((uint32_t)file[at + 2U] << 16) | ((uint32_t)file[at + 3U] << 24);
}

void setUp(void)
{
    len = 0U;
    (void)memset(file, 0, sizeof(file));
    append(fit_enc_begin(&enc, block, sizeof(block)));
}

void tearDown(void) {}

/** Close the file the way the storage service does, header last */
static void finish(const struct fit_totals *t)
{
    append(fit_enc_end(&enc, block, sizeof(block), t));
    TEST_ASSERT_EQUAL_size_t(FIT_HEADER_LEN, fit_enc_header(&enc, block, sizeof(block)));
    (void)memcpy(file, block, FIT_HEADER_LEN);
}

static void test_the_file_header_says_fit(void)
{
    TEST_ASSERT_EQUAL_UINT8(FIT_HEADER_LEN, file[0]);
    TEST_ASSERT_EQUAL_UINT8(0x20U, file[1]);    /* protocol 2.0 */
    TEST_ASSERT_EQUAL_UINT8('.', file[8]);
    TEST_ASSERT_EQUAL_UINT8('F', file[9]);
    TEST_ASSERT_EQUAL_UINT8('I', file[10]);
    TEST_ASSERT_EQUAL_UINT8('T', file[11]);
    /* the header carries the CRC of its own first twelve bytes */
    TEST_ASSERT_EQUAL_UINT16(fit_crc(0U, file, 12U), rd16(12U));
}

static void test_the_size_in_the_header_is_only_the_data(void)
{
    struct fit_totals t = {.start_time = 1000U, .end_time = 1100U};

    append(fit_enc_file_id(&enc, block, sizeof(block), 42U, 1000U));
    finish(&t);

    /* header and the two bytes of the file CRC stay out of the count */
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(len - FIT_HEADER_LEN - 2U), rd32(4U));
}

static void test_the_first_message_is_the_file_id_of_an_activity(void)
{
    append(fit_enc_file_id(&enc, block, sizeof(block), 0x11223344UL, 800000000UL));

    size_t at = FIT_HEADER_LEN;

    /* definition: bit 6 set, local type 0, global message 0 */
    TEST_ASSERT_EQUAL_UINT8(0x40U, file[at]);
    TEST_ASSERT_EQUAL_UINT8(0U, file[at + 1U]);     /* reserved */
    TEST_ASSERT_EQUAL_UINT8(0U, file[at + 2U]);     /* little endian */
    TEST_ASSERT_EQUAL_UINT16(0U, rd16(at + 3U));    /* global file_id */
    TEST_ASSERT_EQUAL_UINT8(5U, file[at + 5U]);     /* five fields */

    size_t data = at + 6U + (5U * 3U);

    TEST_ASSERT_EQUAL_UINT8(0U, file[data]);        /* data message, local 0 */
    TEST_ASSERT_EQUAL_UINT8(4U, file[data + 1U]);   /* file type 4: activity */
    TEST_ASSERT_EQUAL_UINT16(255U, rd16(data + 2U));/* manufacturer: development */
    TEST_ASSERT_EQUAL_UINT32(0x11223344UL, rd32(data + 6U));
    TEST_ASSERT_EQUAL_UINT32(800000000UL, rd32(data + 10U));
}

static void test_a_record_carries_the_point_in_the_units_of_the_format(void)
{
    struct fit_record r = {
        .time = 900000000UL,
        .lat_semi = fit_semicircles(48.6921f),
        .lon_semi = fit_semicircles(6.1844f),
        .alt_m = 240.0f,
        .dist_m = 1234.5f,
        .speed_kmh = 36.0f,
        .power_w = 180,
        .hr_bpm = 152U,
        .cadence_rpm = 88U,
        .temp_c = 21,
    };

    append(fit_enc_record(&enc, block, sizeof(block), &r));

    /* the definition comes first, with its ten fields */
    size_t at = FIT_HEADER_LEN;

    TEST_ASSERT_EQUAL_UINT8(0x42U, file[at]);       /* definition, local 2 */
    TEST_ASSERT_EQUAL_UINT16(20U, rd16(at + 3U));   /* global record */
    TEST_ASSERT_EQUAL_UINT8(10U, file[at + 5U]);

    size_t d = at + 6U + (10U * 3U);

    TEST_ASSERT_EQUAL_UINT8(2U, file[d]);           /* data, local 2 */
    TEST_ASSERT_EQUAL_UINT32(900000000UL, rd32(d + 1U));
    TEST_ASSERT_EQUAL_INT32(r.lat_semi, (int32_t)rd32(d + 5U));
    TEST_ASSERT_EQUAL_INT32(r.lon_semi, (int32_t)rd32(d + 9U));
    /* altitude: scale 5 and offset 500, so 240 m is (240 + 500) x 5 */
    TEST_ASSERT_EQUAL_UINT16(3700U, rd16(d + 13U));
    TEST_ASSERT_EQUAL_UINT8(152U, file[d + 15U]);
    TEST_ASSERT_EQUAL_UINT8(88U, file[d + 16U]);
    /* distance in centimetres */
    TEST_ASSERT_EQUAL_UINT32(123450UL, rd32(d + 17U));
    /* 36 km/h is 10 m/s, and the format keeps millimetres per second */
    TEST_ASSERT_EQUAL_UINT16(10000U, rd16(d + 21U));
    TEST_ASSERT_EQUAL_UINT16(180U, rd16(d + 23U));
    TEST_ASSERT_EQUAL_UINT8(21U, file[d + 25U]);
}

static void test_the_definition_of_a_message_goes_out_once(void)
{
    struct fit_record r = {.time = 1U, .lat_semi = 0, .lon_semi = 0, .alt_m = 0.0f};

    size_t first = fit_enc_record(&enc, block, sizeof(block), &r);

    append(first);

    size_t second = fit_enc_record(&enc, block, sizeof(block), &r);

    append(second);

    /* the second one is only the data message: 26 bytes */
    TEST_ASSERT_EQUAL_size_t(26U, second);
    TEST_ASSERT_EQUAL_size_t(26U + 36U, first);
    TEST_ASSERT_EQUAL_UINT8(2U, block[0]);          /* data header, not 0x42 */
}

static void test_what_is_missing_goes_out_as_invalid(void)
{
    struct fit_record r = {
        .time = 1U,
        .lat_semi = (int32_t)FIT_INVALID_S32,
        .lon_semi = (int32_t)FIT_INVALID_S32,
        .alt_m = 100.0f,
        .power_w = -40,     /* going down; the format has no negative power */
        .hr_bpm = 0U,       /* no strap */
        .cadence_rpm = 0U,  /* no sensor */
    };

    append(fit_enc_record(&enc, block, sizeof(block), &r));

    size_t d = FIT_HEADER_LEN + 6U + (10U * 3U);

    /* position_lat is signed: its invalid value is 0x7FFFFFFF, not 0xFFFFFFFF */
    TEST_ASSERT_EQUAL_INT32((int32_t)FIT_INVALID_S32, (int32_t)rd32(d + 5U));
    TEST_ASSERT_EQUAL_UINT8(FIT_INVALID_U8, file[d + 15U]);
    TEST_ASSERT_EQUAL_UINT8(FIT_INVALID_U8, file[d + 16U]);
    TEST_ASSERT_EQUAL_UINT16(0U, rd16(d + 23U));
}

static void test_semicircles_follow_the_two_to_the_thirty_first_over_180(void)
{
    /* the unit of the format: a degree is 2^31 / 180 semicircles */
    TEST_ASSERT_EQUAL_INT32(0, fit_semicircles(0.0f));
    TEST_ASSERT_INT32_WITHIN(2, 2147483646L / 2L, fit_semicircles(90.0f));
    TEST_ASSERT_INT32_WITHIN(2, -(2147483646L / 2L), fit_semicircles(-90.0f));
    /* Nancy, where the traces of the legacy were recorded */
    TEST_ASSERT_EQUAL_INT32(580919387L, fit_semicircles(48.6921f));
    /* out of the globe there is no position */
    TEST_ASSERT_EQUAL_INT32((int32_t)FIT_INVALID_S32, fit_semicircles(181.0f));
    TEST_ASSERT_EQUAL_INT32((int32_t)FIT_INVALID_S32, fit_semicircles(NAN));
}

static void test_the_date_of_the_legacy_becomes_the_time_of_the_format(void)
{
    /* the legacy keeps DDMMYY without a leading zero (`@50925.txt`) */
    uint32_t t = fit_time_from_date(210926U, 3600U);   /* 21/09/2026, 01:00 UTC */

    /* 20717 days from 1970-01-01 to 2026-09-21 */
    uint32_t unix_s = (20717UL * 86400UL) + 3600UL;

    TEST_ASSERT_EQUAL_UINT32(unix_s - FIT_EPOCH_OFFSET, t);

    /* the two digits of the year are of this century: the earliest date
     * the device can name is 01/01/2000, well after the FIT epoch */
    TEST_ASSERT_EQUAL_UINT32(315619200UL, fit_time_from_date(10100U, 0U));

    /* a date that cannot exist gives no time at all */
    TEST_ASSERT_EQUAL_UINT32(0U, fit_time_from_date(0U, 0U));          /* day 0 */
    TEST_ASSERT_EQUAL_UINT32(0U, fit_time_from_date(321226U, 0U));     /* day 32 */
    TEST_ASSERT_EQUAL_UINT32(0U, fit_time_from_date(011326U, 0U));     /* month 13 */
}

static void test_a_lap_counts_up_and_the_session_says_how_many(void)
{
    struct fit_totals t = {
        .start_time = 1000U, .end_time = 2000U, .elapsed_ms = 1000000U,
        .timer_ms = 900000U, .dist_m = 5000.0f, .ascent_m = 120.0f,
    };

    append(fit_enc_lap(&enc, block, sizeof(block), &t));
    size_t second = fit_enc_lap(&enc, block, sizeof(block), &t);

    append(second);
    /* the index of the second lap is one */
    TEST_ASSERT_EQUAL_UINT16(1U, (uint16_t)(block[1] | ((uint16_t)block[2] << 8)));

    size_t at = len;

    append(fit_enc_session(&enc, block, sizeof(block), &t));

    size_t d = at + 6U + (21U * 3U);

    /* num_laps sits after the sixteen common fields and first_lap_index */
    TEST_ASSERT_EQUAL_UINT16(0U, rd16(d + 1U));     /* message_index */
    TEST_ASSERT_EQUAL_UINT32(2000U, rd32(d + 3U));  /* timestamp */
    TEST_ASSERT_EQUAL_UINT32(1000U, rd32(d + 7U));  /* start_time */
    TEST_ASSERT_EQUAL_UINT32(1000000U, rd32(d + 11U));  /* elapsed, ms */
    TEST_ASSERT_EQUAL_UINT32(900000U, rd32(d + 15U));   /* timer, ms */
    TEST_ASSERT_EQUAL_UINT32(500000U, rd32(d + 19U));   /* 5000 m in cm */
    TEST_ASSERT_EQUAL_UINT16(120U, rd16(d + 36U));      /* total_ascent */
    TEST_ASSERT_EQUAL_UINT16(0U, rd16(d + 40U));        /* first_lap_index */
    TEST_ASSERT_EQUAL_UINT16(2U, rd16(d + 42U));        /* num_laps */
}

static void test_a_ride_without_laps_still_declares_one(void)
{
    struct fit_totals t = {.start_time = 10U, .end_time = 20U};

    append(fit_enc_session(&enc, block, sizeof(block), &t));

    size_t d = FIT_HEADER_LEN + 6U + (21U * 3U);

    TEST_ASSERT_EQUAL_UINT16(1U, rd16(d + 42U));
}

static void test_a_header_of_any_size_leaves_the_crc_at_zero(void)
{
    /*
     * This is what lets the encoder close a file without reading it back.
     * A header ends with the CRC of its own first twelve bytes, and this
     * CRC has residue zero, so the running state after the header is zero
     * whatever size the header declares. If it ever stopped being true,
     * every file the device writes would carry the wrong CRC.
     */
    static const uint32_t sizes[] = {0U, 1U, 26U, 55000U, 380000U, 0xFFFFFFFFUL};

    for (size_t i = 0U; i < (sizeof(sizes) / sizeof(sizes[0])); i++) {
        uint8_t hdr[FIT_HEADER_LEN];
        struct fit_enc e = {0};

        (void)fit_enc_begin(&e, hdr, sizeof(hdr));
        e.data_size = sizes[i];
        TEST_ASSERT_EQUAL_size_t(FIT_HEADER_LEN, fit_enc_header(&e, hdr, sizeof(hdr)));
        TEST_ASSERT_EQUAL_UINT16(0U, fit_crc(0U, hdr, FIT_HEADER_LEN));
    }
}

static void test_the_crc_of_the_whole_file_matches_the_direct_calculation(void)
{
    struct fit_record r = {.time = 900000000UL, .alt_m = 300.0f, .dist_m = 10.0f};
    struct fit_totals t = {.start_time = 1U, .end_time = 2U, .timer_ms = 1000U};

    append(fit_enc_file_id(&enc, block, sizeof(block), 7U, 900000000UL));
    append(fit_enc_timer(&enc, block, sizeof(block), 900000000UL, true));
    for (unsigned int i = 0U; i < 40U; i++) {
        r.time = 900000000UL + i;
        r.dist_m = 10.0f * (float)i;
        append(fit_enc_record(&enc, block, sizeof(block), &r));
    }
    append(fit_enc_timer(&enc, block, sizeof(block), 900000040UL, false));
    append(fit_enc_lap(&enc, block, sizeof(block), &t));
    append(fit_enc_session(&enc, block, sizeof(block), &t));
    finish(&t);

    uint16_t written = rd16(len - 2U);
    uint16_t direct = fit_crc(0U, file, len - 2U);

    TEST_ASSERT_EQUAL_UINT16(direct, written);
    /* and the header CRC still matches the header that ends up on disk */
    TEST_ASSERT_EQUAL_UINT16(fit_crc(0U, file, 12U), rd16(12U));
}

static void test_the_crc_holds_for_files_of_every_length(void)
{
    /* the linearity trick has to hold for any data size, not just one */
    for (unsigned int n = 0U; n < 33U; n++) {
        struct fit_record r = {.time = 1U, .alt_m = 0.0f};
        struct fit_totals t = {.start_time = 1U, .end_time = 2U};

        len = 0U;
        append(fit_enc_begin(&enc, block, sizeof(block)));
        for (unsigned int i = 0U; i < n; i++) {
            r.time = i;
            append(fit_enc_record(&enc, block, sizeof(block), &r));
        }
        finish(&t);

        TEST_ASSERT_EQUAL_UINT16(fit_crc(0U, file, len - 2U), rd16(len - 2U));
        TEST_ASSERT_EQUAL_UINT32((uint32_t)(len - FIT_HEADER_LEN - 2U), rd32(4U));
    }
}

static void test_a_buffer_that_does_not_hold_the_block_builds_nothing(void)
{
    struct fit_record r = {.time = 1U};
    uint8_t small[20];

    TEST_ASSERT_EQUAL_size_t(0U, fit_enc_begin(&enc, small, 4U));
    /* the encoder is still the one from setUp: the record does not fit */
    TEST_ASSERT_EQUAL_size_t(0U, fit_enc_record(&enc, small, sizeof(small), &r));
    TEST_ASSERT_EQUAL_size_t(0U, fit_enc_record(NULL, block, sizeof(block), &r));
    TEST_ASSERT_EQUAL_size_t(0U, fit_enc_record(&enc, block, sizeof(block), NULL));
}

/** Walk the file as a reader does, message by message */
static void decode(unsigned int *records, unsigned int *laps, unsigned int *sessions)
{
    uint8_t sizes[16] = {0};
    size_t at = FIT_HEADER_LEN;
    size_t end = len - 2U;

    *records = 0U;
    *laps = 0U;
    *sessions = 0U;

    while (at < end) {
        uint8_t hdr = file[at];

        TEST_ASSERT_EQUAL_UINT8(0U, hdr & 0x80U);   /* only normal headers */
        TEST_ASSERT_EQUAL_UINT8(0U, hdr & 0x20U);   /* no developer data */

        uint8_t local = (uint8_t)(hdr & 0x0FU);

        if ((hdr & 0x40U) != 0U) {
            uint8_t nf = file[at + 5U];
            size_t bytes = 0U;

            for (uint8_t i = 0U; i < nf; i++) {
                bytes += file[at + 6U + (i * 3U) + 1U];
            }
            TEST_ASSERT_TRUE(bytes < 256U);
            sizes[local] = (uint8_t)bytes;
            at += 6U + ((size_t)nf * 3U);
            continue;
        }

        TEST_ASSERT_TRUE_MESSAGE(sizes[local] != 0U, "data before its definition");
        if (local == 2U) {
            (*records)++;
        } else if (local == 3U) {
            (*laps)++;
        } else if (local == 4U) {
            (*sessions)++;
        }
        at += 1U + sizes[local];
    }

    TEST_ASSERT_EQUAL_size_t(end, at);
}

static void test_a_whole_ride_reads_back_message_by_message(void)
{
    struct fit_totals t = {
        .start_time = 900000000UL, .end_time = 900003600UL,
        .elapsed_ms = 3600000U, .timer_ms = 3500000U,
        .dist_m = 25000.0f, .ascent_m = 300.0f, .descent_m = 290.0f,
        .avg_speed_kmh = 25.7f, .max_speed_kmh = 58.2f,
        .avg_power_w = 165U, .max_power_w = 640U, .calories_kcal = 580U,
        .avg_hr_bpm = 148U, .max_hr_bpm = 181U, .avg_cadence_rpm = 84U,
    };

    append(fit_enc_file_id(&enc, block, sizeof(block), 1U, t.start_time));
    append(fit_enc_timer(&enc, block, sizeof(block), t.start_time, true));
    for (unsigned int i = 0U; i < 100U; i++) {
        struct fit_record r = {
            .time = t.start_time + i,
            .lat_semi = fit_semicircles(48.6921f + ((float)i * 0.0001f)),
            .lon_semi = fit_semicircles(6.1844f),
            .alt_m = 240.0f + (float)i,
            .dist_m = 7.0f * (float)i,
            .speed_kmh = 25.2f,
            .power_w = (int16_t)(150 + (int)i),
            .hr_bpm = (uint8_t)(140U + (i % 20U)),
            .cadence_rpm = 85U,
            .temp_c = 22,
        };

        append(fit_enc_record(&enc, block, sizeof(block), &r));
    }
    append(fit_enc_timer(&enc, block, sizeof(block), t.end_time, false));
    append(fit_enc_lap(&enc, block, sizeof(block), &t));
    append(fit_enc_lap(&enc, block, sizeof(block), &t));
    append(fit_enc_session(&enc, block, sizeof(block), &t));
    finish(&t);

    unsigned int records;
    unsigned int laps;
    unsigned int sessions;

    decode(&records, &laps, &sessions);
    TEST_ASSERT_EQUAL_UINT(100U, records);
    TEST_ASSERT_EQUAL_UINT(2U, laps);
    TEST_ASSERT_EQUAL_UINT(1U, sessions);
    TEST_ASSERT_EQUAL_UINT16(fit_crc(0U, file, len - 2U), rd16(len - 2U));

    /* a ride of one hundred points takes about 2,8 KB */
    TEST_ASSERT_TRUE(len > 2600U);
    TEST_ASSERT_TRUE(len < 3100U);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_file_header_says_fit);
    RUN_TEST(test_the_size_in_the_header_is_only_the_data);
    RUN_TEST(test_the_first_message_is_the_file_id_of_an_activity);
    RUN_TEST(test_a_record_carries_the_point_in_the_units_of_the_format);
    RUN_TEST(test_the_definition_of_a_message_goes_out_once);
    RUN_TEST(test_what_is_missing_goes_out_as_invalid);
    RUN_TEST(test_semicircles_follow_the_two_to_the_thirty_first_over_180);
    RUN_TEST(test_the_date_of_the_legacy_becomes_the_time_of_the_format);
    RUN_TEST(test_a_lap_counts_up_and_the_session_says_how_many);
    RUN_TEST(test_a_ride_without_laps_still_declares_one);
    RUN_TEST(test_a_header_of_any_size_leaves_the_crc_at_zero);
    RUN_TEST(test_the_crc_of_the_whole_file_matches_the_direct_calculation);
    RUN_TEST(test_the_crc_holds_for_files_of_every_length);
    RUN_TEST(test_a_buffer_that_does_not_hold_the_block_builds_nothing);
    RUN_TEST(test_a_whole_ride_reads_back_message_by_message);

    return UNITY_END();
}
