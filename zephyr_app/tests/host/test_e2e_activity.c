/**
 * @file test_e2e_activity.c
 * @brief A whole ride, from the model to the FIT file on the card
 *
 * The two modules of the ride together, the way the firmware runs them:
 * `model/activity.c` takes one sample per second from the model and
 * `model/fit_encode.c` turns what it produces into the bytes the storage
 * service writes. Each has its own tests; this one checks that the chain
 * makes an activity a reader would accept, with the numbers a rider would
 * recognise.
 *
 * The ride is the one the legacy was measured on: out of Nancy, a climb, a
 * traffic light, a descent and a sprint, an hour and a bit in all.
 */

#include <math.h>
#include <string.h>

#include "unity.h"

#include "model/activity.h"
#include "model/fit_encode.h"

/* an hour at one point a second, with room to spare */
#define FILE_MAX    (140U * 1024U)

static struct activity act;
static struct fit_enc enc;
static uint8_t *file;
static size_t len;
static uint8_t block[FIT_BLOCK_MAX];

/* what the ride looked like, for the assertions at the end */
static unsigned int seconds;
static unsigned int records;
static unsigned int laps_written;
static unsigned int timer_events;

static uint8_t backing[FILE_MAX];

static void push(size_t n)
{
    TEST_ASSERT_TRUE_MESSAGE(n > 0U, "the encoder refused a block");
    TEST_ASSERT_TRUE_MESSAGE((len + n) <= FILE_MAX, "the ride did not fit the buffer");
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
    file = backing;
    len = 0U;
    seconds = 0U;
    records = 0U;
    laps_written = 0U;
    timer_events = 0U;
    (void)memset(backing, 0, sizeof(backing));
}

void tearDown(void) {}

/** The compact totals the channel carries, as the encoder wants them */
static void totals_of(struct fit_totals *out, const struct activity_totals *t)
{
    (void)memset(out, 0, sizeof(*out));
    out->start_time = t->start_time;
    out->end_time = t->end_time;
    out->elapsed_ms = t->elapsed_ms;
    out->timer_ms = t->timer_ms;
    out->dist_m = t->dist_m;
    out->ascent_m = t->ascent_m;
    out->descent_m = t->descent_m;
    out->avg_speed_kmh = activity_avg_speed(t);
    out->max_speed_kmh = t->max_speed_kmh;
    out->avg_power_w = activity_avg_power(t);
    out->max_power_w = t->max_power_w;
    out->calories_kcal = activity_calories(t);
    out->avg_hr_bpm = activity_avg_hr(t);
    out->max_hr_bpm = t->max_hr_bpm;
    out->avg_cadence_rpm = activity_avg_cadence(t);
}

/* the ride as the model would publish it, second by second */
static uint32_t now;
static float dist_m;
static float climb_m;
static float alt_m;
static bool running_before;

/** One second of riding: the model, then the activity, then the file */
static void tick(float kmh, float dalt, int16_t power, uint8_t hr, uint8_t cad)
{
    now++;
    seconds++;
    dist_m += kmh / 3.6f;
    alt_m += dalt;
    if (dalt > 0.0f) {
        climb_m += dalt;
    }

    struct activity_sample s = {
        .time = now,
        .speed_kmh = kmh,
        .dist_m = dist_m,
        .climb_m = climb_m,
        .alt_m = alt_m,
        .power_w = power,
        .hr_bpm = hr,
        .cadence_rpm = cad,
    };

    /* the storage writes the record of the epoch before the activity
     * event, so a lap lands after the records that belong to it */
    struct fit_record r = {
        .time = now,
        .lat_semi = fit_semicircles(48.6921f + (dist_m * 0.0000090f)),
        .lon_semi = fit_semicircles(6.1844f),
        .alt_m = alt_m,
        .dist_m = dist_m,
        .speed_kmh = kmh,
        .power_w = power,
        .hr_bpm = hr,
        .cadence_rpm = cad,
        .temp_c = 18,
    };

    push(fit_enc_record(&enc, block, sizeof(block), &r));
    records++;

    enum activity_event ev = activity_update(&act, &s, 1000U);

    if (act.running != running_before) {
        running_before = act.running;
        push(fit_enc_timer(&enc, block, sizeof(block), now, act.running));
        timer_events++;
    }
    if (ev == ACTIVITY_EVENT_LAP) {
        struct fit_totals lap;

        totals_of(&lap, activity_lap(&act));
        push(fit_enc_lap(&enc, block, sizeof(block), &lap));
        laps_written++;
    }
}

/** Open the ride: the file header, the file_id and the timer start */
static void ride_begin(uint32_t autolap_m)
{
    now = fit_time_from_date(210926U, 32400U);  /* 21/09/2026, 09:00 UTC */
    dist_m = 0.0f;
    climb_m = 0.0f;
    alt_m = 212.0f;

    activity_init(&act, autolap_m, true);
    running_before = act.running;

    push(fit_enc_begin(&enc, block, sizeof(block)));
    push(fit_enc_file_id(&enc, block, sizeof(block), 0U, now));
    push(fit_enc_timer(&enc, block, sizeof(block), now, true));
    timer_events++;

    /* the first sample only sets the starting point */
    struct activity_sample s = {.time = now, .alt_m = alt_m};

    (void)activity_update(&act, &s, 1000U);
}

/** Close it as the storage service does, header last */
static void ride_end(void)
{
    activity_finish(&act);

    struct fit_totals ride;
    struct fit_totals lap;

    totals_of(&ride, activity_ride(&act));
    totals_of(&lap, activity_lap(&act));

    push(fit_enc_timer(&enc, block, sizeof(block), ride.end_time, false));
    timer_events++;
    push(fit_enc_lap(&enc, block, sizeof(block), &lap));
    laps_written++;
    push(fit_enc_session(&enc, block, sizeof(block), &ride));
    push(fit_enc_end(&enc, block, sizeof(block), &ride));

    /* the storage seeks back to zero and writes this over the first bytes */
    TEST_ASSERT_EQUAL_size_t(FIT_HEADER_LEN, fit_enc_header(&enc, block, sizeof(block)));
    (void)memcpy(file, block, FIT_HEADER_LEN);
}

/** Walk the finished file as a reader does */
static void decode(unsigned int *recs, unsigned int *lapmsgs, unsigned int *sessions,
                   unsigned int *events)
{
    uint8_t sizes[16] = {0};
    size_t at = FIT_HEADER_LEN;
    size_t end = len - 2U;

    *recs = 0U;
    *lapmsgs = 0U;
    *sessions = 0U;
    *events = 0U;

    while (at < end) {
        uint8_t hdr = file[at];
        uint8_t local = (uint8_t)(hdr & 0x0FU);

        TEST_ASSERT_EQUAL_UINT8(0U, hdr & 0xB0U);   /* normal header, no dev data */
        if ((hdr & 0x40U) != 0U) {
            uint8_t nf = file[at + 5U];
            size_t bytes = 0U;

            for (uint8_t i = 0U; i < nf; i++) {
                bytes += file[at + 6U + (i * 3U) + 1U];
            }
            sizes[local] = (uint8_t)bytes;
            at += 6U + ((size_t)nf * 3U);
            continue;
        }
        TEST_ASSERT_TRUE_MESSAGE(sizes[local] != 0U, "data before its definition");
        switch (local) {
        case 1U:
            (*events)++;
            break;
        case 2U:
            (*recs)++;
            break;
        case 3U:
            (*lapmsgs)++;
            break;
        case 4U:
            (*sessions)++;
            break;
        default:
            break;
        }
        at += 1U + sizes[local];
    }
    TEST_ASSERT_EQUAL_size_t(end, at);
}

/** The whole ride, once, so several tests read the same file */
static void a_ride_out_of_nancy(uint32_t autolap_m)
{
    ride_begin(autolap_m);

    /* ten minutes of flat, 30 km/h */
    for (unsigned int i = 0U; i < 600U; i++) {
        tick(30.0f, 0.0f, 180, 145U, 88U);
    }
    /* twelve minutes of climb, 12 km/h, 400 m of it */
    for (unsigned int i = 0U; i < 720U; i++) {
        tick(12.0f, 400.0f / 720.0f, 260, 168U, 72U);
    }
    /* three minutes at a traffic light */
    for (unsigned int i = 0U; i < 180U; i++) {
        tick(0.0f, 0.0f, 0, 110U, 0U);
    }
    /* eight minutes of descent, 55 km/h, and the 400 m back */
    for (unsigned int i = 0U; i < 480U; i++) {
        tick(55.0f, -400.0f / 480.0f, 0, 132U, 0U);
    }
    /* a two-minute sprint home */
    for (unsigned int i = 0U; i < 120U; i++) {
        tick(42.0f, 0.0f, 420, 178U, 102U);
    }

    ride_end();
}

static void test_the_file_a_reader_gets_is_whole(void)
{
    a_ride_out_of_nancy(0U);

    unsigned int recs;
    unsigned int lapmsgs;
    unsigned int sessions;
    unsigned int events;

    decode(&recs, &lapmsgs, &sessions, &events);

    /* one record per second of the ride, whatever the timer did */
    TEST_ASSERT_EQUAL_UINT(seconds, recs);
    TEST_ASSERT_EQUAL_UINT(2100U, recs);
    /* one lap, because no automatic lap was asked for */
    TEST_ASSERT_EQUAL_UINT(1U, lapmsgs);
    TEST_ASSERT_EQUAL_UINT(1U, sessions);
    /* start, the pause at the light, the resume, and the stop at the end */
    TEST_ASSERT_EQUAL_UINT(4U, events);
    TEST_ASSERT_EQUAL_UINT(timer_events, events);

    /* the header and the CRC are what a reader checks first */
    TEST_ASSERT_EQUAL_UINT8(FIT_HEADER_LEN, file[0]);
    TEST_ASSERT_EQUAL_UINT8('.', file[8]);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(len - FIT_HEADER_LEN - 2U), rd32(4U));
    TEST_ASSERT_EQUAL_UINT16(fit_crc(0U, file, 12U), rd16(12U));
    TEST_ASSERT_EQUAL_UINT16(fit_crc(0U, file, len - 2U), rd16(len - 2U));
}

static void test_the_numbers_of_the_ride_are_the_ones_a_rider_would_see(void)
{
    a_ride_out_of_nancy(0U);

    const struct activity_totals *t = activity_ride(&act);

    /* 5 km flat + 2,4 km of climb + 7,33 km of descent + 1,4 km of sprint */
    TEST_ASSERT_FLOAT_WITHIN(60.0f, 16133.0f, t->dist_m);
    /* the three minutes at the light are out of the moving time, less the
     * three seconds it takes the timer to notice */
    TEST_ASSERT_EQUAL_UINT32(2100U * 1000U, t->elapsed_ms);
    TEST_ASSERT_UINT32_WITHIN(2000U, (2100U - 180U + 3U) * 1000U, t->timer_ms);
    /* 400 m up and 400 m down, less the dead band of the last step */
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 400.0f, t->ascent_m);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 400.0f, t->descent_m);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 55.0f, t->max_speed_kmh);
    TEST_ASSERT_EQUAL_UINT16(420U, t->max_power_w);
    TEST_ASSERT_EQUAL_UINT8(178U, t->max_hr_bpm);

    /* the average is of the ride, not of the wait: over the wall clock it
     * would be 27,7 km/h, and over the moving time it is 30,3 */
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 30.3f, activity_avg_speed(t));
}

static void test_the_session_in_the_file_carries_those_same_numbers(void)
{
    a_ride_out_of_nancy(0U);

    /* find the session message: local type 4, the last data message but one */
    uint8_t sizes[16] = {0};
    size_t at = FIT_HEADER_LEN;
    size_t session = 0U;

    while (at < (len - 2U)) {
        uint8_t hdr = file[at];
        uint8_t local = (uint8_t)(hdr & 0x0FU);

        if ((hdr & 0x40U) != 0U) {
            uint8_t nf = file[at + 5U];
            size_t bytes = 0U;

            for (uint8_t i = 0U; i < nf; i++) {
                bytes += file[at + 6U + (i * 3U) + 1U];
            }
            sizes[local] = (uint8_t)bytes;
            at += 6U + ((size_t)nf * 3U);
            continue;
        }
        if (local == 4U) {
            session = at;
        }
        at += 1U + sizes[local];
    }
    TEST_ASSERT_TRUE_MESSAGE(session != 0U, "no session in the file");

    /* header, message_index, timestamp, start_time, elapsed, timer, dist */
    size_t d = session + 1U + 2U;

    TEST_ASSERT_EQUAL_UINT32(2100U * 1000U, rd32(d + 8U));      /* elapsed, ms */
    TEST_ASSERT_UINT32_WITHIN(2000U, 1923U * 1000U, rd32(d + 12U));  /* timer, ms */
    /* the distance in centimetres, as the format keeps it */
    TEST_ASSERT_UINT32_WITHIN(6000U, 1613300UL, rd32(d + 16U));
    /*
     * ascent and descent, after calories, the two speeds, the three
     * one-byte averages and the two powers: 20 + 2 + 2 + 2 + 3 + 2 + 2
     */
    TEST_ASSERT_UINT16_WITHIN(5U, 400U, rd16(d + 33U));
    TEST_ASSERT_UINT16_WITHIN(5U, 400U, rd16(d + 35U));
    /* and right after them, the lap count of a ride with no automatic lap */
    TEST_ASSERT_EQUAL_UINT16(0U, rd16(d + 37U));    /* first_lap_index */
    TEST_ASSERT_EQUAL_UINT16(1U, rd16(d + 39U));    /* num_laps */
}

static void test_a_ride_with_automatic_laps_splits_the_file(void)
{
    a_ride_out_of_nancy(5000U);     /* a lap every five kilometres */

    unsigned int recs;
    unsigned int lapmsgs;
    unsigned int sessions;
    unsigned int events;

    decode(&recs, &lapmsgs, &sessions, &events);

    /* 16,1 km makes three full laps and the piece that was left */
    TEST_ASSERT_EQUAL_UINT(4U, lapmsgs);
    TEST_ASSERT_EQUAL_UINT(4U, laps_written);
    TEST_ASSERT_EQUAL_UINT(1U, sessions);
    TEST_ASSERT_EQUAL_UINT(seconds, recs);
    TEST_ASSERT_EQUAL_UINT16(fit_crc(0U, file, len - 2U), rd16(len - 2U));
}

static void test_the_file_of_an_hour_fits_the_storage(void)
{
    a_ride_out_of_nancy(5000U);

    /* 35 minutes of riding, 2100 points: about 55 KB, which is 26 bytes a
     * point plus the definitions. A four-hour ride is about 380 KB, and the
     * 8 MB of the new board hold around twenty of them beside the segments */
    TEST_ASSERT_TRUE_MESSAGE(len > 54000U, "smaller than a FIT of this ride can be");
    TEST_ASSERT_TRUE_MESSAGE(len < 57000U, "bigger than a FIT of this ride should be");
    TEST_ASSERT_UINT32_WITHIN(2U, 26U, (uint32_t)((len - 500U) / records));
}

static void test_a_ride_that_never_gets_a_date_writes_no_file(void)
{
    /*
     * The storage does not open a FIT until a point carries a date: a file
     * with no time base is not an activity. The encoder itself refuses
     * nothing, so this is a rule of the service, checked here on the piece
     * of it that is pure: the time of a point without a date is zero.
     */
    TEST_ASSERT_EQUAL_UINT32(0U, fit_time_from_date(0U, 0U));
    TEST_ASSERT_EQUAL_UINT32(0U, fit_time_from_date(0U, 43200U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_file_a_reader_gets_is_whole);
    RUN_TEST(test_the_numbers_of_the_ride_are_the_ones_a_rider_would_see);
    RUN_TEST(test_the_session_in_the_file_carries_those_same_numbers);
    RUN_TEST(test_a_ride_with_automatic_laps_splits_the_file);
    RUN_TEST(test_the_file_of_an_hour_fits_the_storage);
    RUN_TEST(test_a_ride_that_never_gets_a_date_writes_no_file);

    return UNITY_END();
}
