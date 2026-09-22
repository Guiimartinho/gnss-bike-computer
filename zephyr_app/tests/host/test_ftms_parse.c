/**
 * @file test_ftms_parse.c
 * @brief Indoor Bike Data of a trainer (src/model/ftms_parse.c)
 *
 * Two flags of this characteristic were wrong in the port until
 * 2026-09-21: instantaneous cadence carried the bit of the average speed
 * and elapsed time carried the bit of the average power. The walk was
 * otherwise in ascending order, which is how the mistake shows: the
 * sequence had two gaps, 0x0004 and 0x0800, and two duplicated constants.
 *
 * With a trainer that sends instantaneous cadence and power and nothing
 * else — which is what most of them send — the cadence was lost and every
 * field after it came out two bytes early, so the power was read from the
 * wrong place. `test_the_payload_most_trainers_send` is that case.
 *
 * The payloads here are built by hand from the table in
 * `model/ftms_parse.h`, so the test says what the wire looks like and not
 * what the code does.
 */

#include <string.h>

#include "unity.h"

#include "model/ftms_parse.h"

static struct ftms_bike_data d;
static uint8_t pkt[64];
static uint16_t len;

void setUp(void)
{
    (void)memset(&d, 0, sizeof(d));
    (void)memset(pkt, 0, sizeof(pkt));
    len = 0U;
}

void tearDown(void) {}

/** Start a payload with its flags */
static void begin(uint16_t flags)
{
    pkt[0] = (uint8_t)(flags & 0xFFU);
    pkt[1] = (uint8_t)(flags >> 8);
    len = 2U;
}

static void put16(uint16_t v)
{
    pkt[len] = (uint8_t)(v & 0xFFU);
    pkt[len + 1U] = (uint8_t)(v >> 8);
    len = (uint16_t)(len + 2U);
}

static void put8(uint8_t v)
{
    pkt[len] = v;
    len = (uint16_t)(len + 1U);
}

static void put24(uint32_t v)
{
    pkt[len] = (uint8_t)(v & 0xFFU);
    pkt[len + 1U] = (uint8_t)((v >> 8) & 0xFFU);
    pkt[len + 2U] = (uint8_t)((v >> 16) & 0xFFU);
    len = (uint16_t)(len + 3U);
}

static void test_the_payload_most_trainers_send(void)
{
    /*
     * Instantaneous speed (More Data zero), instantaneous cadence and
     * instantaneous power, and nothing else. This is the case the wrong
     * flags broke: the cadence was lost and the power came out of the two
     * bytes that belong to the cadence.
     */
    begin(FTMS_F_CADENCE | FTMS_F_POWER);
    put16(2850U);       /* 28,50 km/h */
    put16(180U);        /* 90 rpm, in halves */
    put16(245U);        /* 245 W */

    TEST_ASSERT_TRUE(ftms_parse_bike_data(pkt, len, &d));
    TEST_ASSERT_TRUE(d.have_speed);
    TEST_ASSERT_EQUAL_UINT16(2850U, d.speed_kmh100);
    TEST_ASSERT_TRUE(d.have_cadence);
    TEST_ASSERT_EQUAL_UINT16(90U, d.cadence_rpm);
    TEST_ASSERT_TRUE(d.have_power);
    TEST_ASSERT_EQUAL_INT16(245, d.power_w);
}

static void test_more_data_set_means_there_is_no_speed(void)
{
    /* the one inverted bit of the characteristic */
    begin(FTMS_F_MORE_DATA | FTMS_F_POWER);
    put16(300U);

    TEST_ASSERT_TRUE(ftms_parse_bike_data(pkt, len, &d));
    TEST_ASSERT_FALSE(d.have_speed);
    TEST_ASSERT_EQUAL_UINT16(0U, d.speed_kmh100);
    /* and the power is the first field, not the second */
    TEST_ASSERT_TRUE(d.have_power);
    TEST_ASSERT_EQUAL_INT16(300, d.power_w);
}

static void test_the_average_fields_are_walked_past_not_read(void)
{
    /*
     * Average speed and average cadence come before the instantaneous
     * cadence and power. Their bytes must be skipped, not read: this is
     * exactly what went wrong before.
     */
    begin(FTMS_F_AVG_SPEED | FTMS_F_CADENCE | FTMS_F_AVG_CADENCE | FTMS_F_POWER |
          FTMS_F_AVG_POWER);
    put16(2500U);       /* instantaneous speed */
    put16(2400U);       /* average speed, skipped */
    put16(190U);        /* instantaneous cadence: 95 rpm */
    put16(180U);        /* average cadence, skipped */
    put16(260U);        /* instantaneous power */
    put16(240U);        /* average power, skipped */

    TEST_ASSERT_TRUE(ftms_parse_bike_data(pkt, len, &d));
    TEST_ASSERT_EQUAL_UINT16(2500U, d.speed_kmh100);
    TEST_ASSERT_EQUAL_UINT16(95U, d.cadence_rpm);
    TEST_ASSERT_EQUAL_INT16(260, d.power_w);
}

static void test_every_field_at_once(void)
{
    /* the widest notification the characteristic allows */
    begin(FTMS_F_AVG_SPEED | FTMS_F_CADENCE | FTMS_F_AVG_CADENCE | FTMS_F_TOTAL_DISTANCE |
          FTMS_F_RESISTANCE | FTMS_F_POWER | FTMS_F_AVG_POWER | FTMS_F_ENERGY |
          FTMS_F_HEART_RATE | FTMS_F_MET | FTMS_F_ELAPSED_TIME | FTMS_F_REMAINING_TIME);
    put16(3010U);       /* speed */
    put16(2900U);       /* average speed */
    put16(176U);        /* cadence: 88 rpm */
    put16(170U);        /* average cadence */
    put24(12345U);      /* total distance, three bytes */
    put16(7U);          /* resistance */
    put16(312U);        /* power */
    put16(280U);        /* average power */
    put16(150U);        /* energy total */
    put16(600U);        /* energy per hour and per minute, two more bytes */
    put8(161U);         /* heart rate */
    put8(9U);           /* metabolic equivalent */
    put16(1834U);       /* elapsed time */
    put16(766U);        /* remaining time */

    TEST_ASSERT_TRUE(ftms_parse_bike_data(pkt, len, &d));
    TEST_ASSERT_EQUAL_UINT16(3010U, d.speed_kmh100);
    TEST_ASSERT_EQUAL_UINT16(88U, d.cadence_rpm);
    TEST_ASSERT_EQUAL_INT16(312, d.power_w);
    TEST_ASSERT_EQUAL_UINT8(161U, d.hr_bpm);
    TEST_ASSERT_EQUAL_UINT16(1834U, d.elapsed_s);
    TEST_ASSERT_TRUE(d.have_hr);
    TEST_ASSERT_TRUE(d.have_elapsed);
}

static void test_the_energy_field_takes_four_bytes(void)
{
    /*
     * Expended energy is three values in four bytes: total in two, per
     * hour in two more... and per minute in one. The profile packs them as
     * four bytes in all, and a reader that counts two loses the heart rate.
     */
    begin(FTMS_F_ENERGY | FTMS_F_HEART_RATE);
    put16(2000U);       /* speed */
    put16(500U);        /* energy, first two bytes */
    put16(250U);        /* energy, last two bytes */
    put8(142U);         /* heart rate */

    TEST_ASSERT_TRUE(ftms_parse_bike_data(pkt, len, &d));
    TEST_ASSERT_EQUAL_UINT8(142U, d.hr_bpm);
}

static void test_a_power_the_trainer_reports_as_negative(void)
{
    begin(FTMS_F_POWER);
    put16(2000U);
    put16((uint16_t)(int16_t)-35);

    TEST_ASSERT_TRUE(ftms_parse_bike_data(pkt, len, &d));
    TEST_ASSERT_EQUAL_INT16(-35, d.power_w);
}

static void test_a_notification_that_lies_about_itself_is_dropped(void)
{
    /* the flags promise a power that the payload does not hold */
    begin(FTMS_F_CADENCE | FTMS_F_POWER);
    put16(2500U);
    put16(180U);
    put8(0U);           /* one byte where two are needed */

    TEST_ASSERT_FALSE(ftms_parse_bike_data(pkt, len, &d));
    /* and nothing half-read is left behind */
    TEST_ASSERT_FALSE(d.have_speed);
    TEST_ASSERT_FALSE(d.have_cadence);
    TEST_ASSERT_EQUAL_UINT16(0U, d.cadence_rpm);
}

static void test_a_payload_with_only_flags_carries_nothing(void)
{
    /* every bit set means nothing is present except the speed, which the
     * inverted bit turns off too */
    begin(FTMS_F_MORE_DATA);

    TEST_ASSERT_TRUE(ftms_parse_bike_data(pkt, len, &d));
    TEST_ASSERT_FALSE(d.have_speed);
    TEST_ASSERT_FALSE(d.have_cadence);
    TEST_ASSERT_FALSE(d.have_power);
}

static void test_what_is_too_short_to_read_is_refused(void)
{
    pkt[0] = 0x00U;

    TEST_ASSERT_FALSE(ftms_parse_bike_data(pkt, 1U, &d));
    TEST_ASSERT_FALSE(ftms_parse_bike_data(pkt, 0U, &d));
    TEST_ASSERT_FALSE(ftms_parse_bike_data(NULL, 8U, &d));
    TEST_ASSERT_FALSE(ftms_parse_bike_data(pkt, 8U, NULL));
}

static void test_the_flags_are_the_ones_the_wire_order_needs(void)
{
    /*
     * The walk is in ascending bit order, so the constants have to be a
     * run with no gap and no repeat. This is the shape of the defect that
     * was fixed: two gaps and two duplicates.
     */
    static const uint16_t all[] = {
        FTMS_F_MORE_DATA, FTMS_F_AVG_SPEED, FTMS_F_CADENCE, FTMS_F_AVG_CADENCE,
        FTMS_F_TOTAL_DISTANCE, FTMS_F_RESISTANCE, FTMS_F_POWER, FTMS_F_AVG_POWER,
        FTMS_F_ENERGY, FTMS_F_HEART_RATE, FTMS_F_MET, FTMS_F_ELAPSED_TIME,
        FTMS_F_REMAINING_TIME,
    };

    for (size_t i = 0U; i < (sizeof(all) / sizeof(all[0])); i++) {
        TEST_ASSERT_EQUAL_UINT16_MESSAGE((uint16_t)(1U << i), all[i],
                                         "a flag is out of the wire order");
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_payload_most_trainers_send);
    RUN_TEST(test_more_data_set_means_there_is_no_speed);
    RUN_TEST(test_the_average_fields_are_walked_past_not_read);
    RUN_TEST(test_every_field_at_once);
    RUN_TEST(test_the_energy_field_takes_four_bytes);
    RUN_TEST(test_a_power_the_trainer_reports_as_negative);
    RUN_TEST(test_a_notification_that_lies_about_itself_is_dropped);
    RUN_TEST(test_a_payload_with_only_flags_carries_nothing);
    RUN_TEST(test_what_is_too_short_to_read_is_refused);
    RUN_TEST(test_the_flags_are_the_ones_the_wire_order_needs);

    return UNITY_END();
}
