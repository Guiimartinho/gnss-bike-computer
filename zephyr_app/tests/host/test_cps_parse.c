/**
 * @file test_cps_parse.c
 * @brief The Cycling Power Measurement of a power meter (src/model/cps_parse.c)
 *
 * The walk over the optional fields is the whole of the module, so the
 * tests build payloads with every combination that matters and check what
 * comes out of the far end. The field that proves the walk is the
 * **accumulated energy**: it is the last one in the table, so it only lands
 * on the right bytes if every field before it was measured correctly.
 *
 * Two things get cases of their own because they are how a reader of this
 * characteristic goes wrong:
 *
 * - the **wheel clock ticks at 1/2048 s** while the crank clock and both
 *   clocks of the Cycling Speed and Cadence service tick at 1/1024 s, so a
 *   reader that assumes one rate reports twice or half the speed;
 * - the **pedal analysis fields** (extreme forces, torques and angles)
 *   carry nothing this device draws, but skipping them by the wrong number
 *   of bytes moves everything after them.
 *
 * The field order itself is not verified against hardware: no power meter
 * has been on a bench here (see the note in `model/cps_parse.h`).
 */

#include <string.h>

#include "unity.h"

#include "model/cps_parse.h"

static uint8_t buf[64];
static uint16_t n;
static struct cps_measurement m;

void setUp(void)
{
    (void)memset(buf, 0, sizeof(buf));
    (void)memset(&m, 0, sizeof(m));
    n = 0U;
}

void tearDown(void) {}

/* ---- building a notification ---- */

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

static void put32(uint32_t v)
{
    put16((uint16_t)(v & 0xFFFFU));
    put16((uint16_t)(v >> 16));
}

/* ==========================================================================
 * The one field that is always there
 * ========================================================================== */

static void test_a_meter_that_sends_only_the_power(void)
{
    put16(0x0000U);
    put16(210U);

    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_EQUAL_INT16(210, m.power_w);
    TEST_ASSERT_FALSE(m.have_balance);
    TEST_ASSERT_FALSE(m.have_crank);
    TEST_ASSERT_FALSE(m.have_wheel);
    TEST_ASSERT_FALSE(m.have_energy);
}

static void test_the_power_is_signed(void)
{
    /* a meter on a freewheel going downhill reports below zero */
    put16(0x0000U);
    put16((uint16_t)(int16_t)-35);

    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_EQUAL_INT16(-35, m.power_w);
}

static void test_a_sprint_of_fifteen_hundred_watts(void)
{
    put16(0x0000U);
    put16(1500U);

    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_EQUAL_INT16(1500, m.power_w);
}

/* ==========================================================================
 * Payloads that cannot be read
 * ========================================================================== */

static void test_a_payload_too_short_for_the_power_is_refused(void)
{
    put16(0x0000U);
    put8(210U);     /* half a power */

    TEST_ASSERT_FALSE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_FALSE(cps_parse_measurement(buf, 0U, &m));
    TEST_ASSERT_FALSE(cps_parse_measurement(buf, 2U, &m));
}

static void test_a_notification_that_lies_about_itself_is_dropped_whole(void)
{
    /*
     * The flags promise crank data and the payload stops before it. Reading
     * what is there and leaving the rest at zero would put a cadence on the
     * screen that the meter never sent, so nothing at all comes back.
     */
    put16(CPS_F_CRANK_REV);
    put16(210U);
    put16(100U);    /* the turns, but not the time */

    TEST_ASSERT_FALSE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_EQUAL_INT16(0, m.power_w);      /* and nothing was kept */
}

/* ==========================================================================
 * Pedal balance
 * ========================================================================== */

static void test_the_balance_is_in_half_percent_steps(void)
{
    /* 101 on the wire is 50,5 %, which the model keeps as 50 */
    put16(CPS_F_BALANCE);
    put16(210U);
    put8(101U);

    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_TRUE(m.have_balance);
    TEST_ASSERT_EQUAL_UINT8(50U, m.balance_pct);
    TEST_ASSERT_FALSE(m.balance_is_left);   /* the meter did not say whose */
}

static void test_the_balance_can_name_the_left_leg(void)
{
    put16(CPS_F_BALANCE | CPS_F_BALANCE_LEFT);
    put16(210U);
    put8(96U);      /* 48 % on the left, so 52 % on the right */

    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_EQUAL_UINT8(48U, m.balance_pct);
    TEST_ASSERT_TRUE(m.balance_is_left);
}

/* ==========================================================================
 * The two clocks
 * ========================================================================== */

/** One notification with crank data */
static void crank_at(uint16_t revs, uint16_t time)
{
    n = 0U;
    put16(CPS_F_CRANK_REV);
    put16(210U);
    put16(revs);
    put16(time);
}

/** One notification with wheel data */
static void wheel_at(uint32_t revs, uint16_t time)
{
    n = 0U;
    put16(CPS_F_WHEEL_REV);
    put16(210U);
    put32(revs);
    put16(time);
}

static void test_one_crank_turn_a_second_is_sixty_revolutions_a_minute(void)
{
    struct cps_measurement a;
    struct cps_measurement b;

    crank_at(100U, 1000U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));

    crank_at(101U, 1000U + 1024U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    TEST_ASSERT_EQUAL_UINT8(60U, cps_cadence_rpm(&a, &b));
}

static void test_a_normal_cadence(void)
{
    struct cps_measurement a;
    struct cps_measurement b;

    /* 90 rpm: three turns in two seconds */
    crank_at(100U, 0U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));

    crank_at(103U, 2U * CPS_CRANK_TICKS_PER_S);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    TEST_ASSERT_EQUAL_UINT8(90U, cps_cadence_rpm(&a, &b));
}

static void test_the_wheel_clock_runs_at_twice_the_crank_clock(void)
{
    /*
     * The trap this module exists to avoid. One turn of a 2105 mm wheel in
     * one second is 7,57 km/h. One second is 2048 ticks here, not 1024: a
     * reader that used the rate of the Cycling Speed and Cadence service
     * would report 3,78 km/h, exactly half.
     */
    struct cps_measurement a;
    struct cps_measurement b;

    wheel_at(1000U, 0U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));

    /*
     * 2048 is written out and not taken from the constant on purpose: a
     * test that used the constant on both sides would follow it wherever
     * it was changed to, and this is the one number that must not move.
     */
    wheel_at(1001U, 2048U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    TEST_ASSERT_EQUAL_UINT16(757U, cps_speed_kmh100(&a, &b, 2105U));
    TEST_ASSERT_EQUAL_UINT16(2048U, CPS_WHEEL_TICKS_PER_S);
    TEST_ASSERT_EQUAL_UINT16(1024U, CPS_CRANK_TICKS_PER_S);
}

static void test_a_ride_at_thirty_kilometres_an_hour(void)
{
    struct cps_measurement a;
    struct cps_measurement b;

    /* 30 km/h on a 2105 mm wheel is 3,96 turns a second; take four */
    wheel_at(0U, 0U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));

    wheel_at(4U, CPS_WHEEL_TICKS_PER_S);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    TEST_ASSERT_UINT16_WITHIN(20U, 3031U, cps_speed_kmh100(&a, &b, 2105U));
}

static void test_the_counters_wrapping_does_not_break_the_sum(void)
{
    struct cps_measurement a;
    struct cps_measurement b;

    /* the crank counter is a uint16 and its clock too */
    crank_at(65534U, 65000U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));

    crank_at(1U, (uint16_t)(65000U + CPS_CRANK_TICKS_PER_S));
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    TEST_ASSERT_EQUAL_UINT8(180U, cps_cadence_rpm(&a, &b));   /* three turns */

    /* and the wheel counter is a uint32 with its own clock */
    wheel_at(0xFFFFFFFFUL, 65535U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));

    wheel_at(0U, (uint16_t)(65535U + CPS_WHEEL_TICKS_PER_S));
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    TEST_ASSERT_EQUAL_UINT16(757U, cps_speed_kmh100(&a, &b, 2105U));
}

static void test_a_rider_who_stopped_pedalling(void)
{
    struct cps_measurement a;
    struct cps_measurement b;

    /* the meter keeps sending, with the same counters */
    crank_at(100U, 5000U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));
    crank_at(100U, 5000U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    TEST_ASSERT_EQUAL_UINT8(0U, cps_cadence_rpm(&a, &b));

    wheel_at(100U, 5000U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));
    wheel_at(100U, 5000U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    TEST_ASSERT_EQUAL_UINT16(0U, cps_speed_kmh100(&a, &b, 2105U));
}

static void test_a_cadence_no_rider_produces_is_thrown_away(void)
{
    struct cps_measurement a;
    struct cps_measurement b;

    /* a counter wrap of more than one full turn looks like 4000 rpm */
    crank_at(0U, 0U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));
    crank_at(5000U, 1024U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    TEST_ASSERT_EQUAL_UINT8(0U, cps_cadence_rpm(&a, &b));
}

static void test_a_speed_no_bicycle_reaches_is_thrown_away(void)
{
    struct cps_measurement a;
    struct cps_measurement b;

    wheel_at(0U, 0U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));
    wheel_at(1000U, 2048U);     /* 1000 turns in a second */
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    TEST_ASSERT_EQUAL_UINT16(0U, cps_speed_kmh100(&a, &b, 2105U));
}

/* ==========================================================================
 * The walk itself
 * ========================================================================== */

static void test_every_field_at_once_and_the_last_one_lands_right(void)
{
    /*
     * Accumulated energy is the last field in the table, at offset 32 of a
     * 34 byte payload. It only reads back as 4321 if all ten fields before
     * it were walked with the right widths — including the three that this
     * device keeps nothing from.
     */
    put16(CPS_F_BALANCE | CPS_F_BALANCE_LEFT | CPS_F_TORQUE | CPS_F_TORQUE_CRANK |
          CPS_F_WHEEL_REV | CPS_F_CRANK_REV | CPS_F_EXTREME_FORCE | CPS_F_EXTREME_TORQUE |
          CPS_F_EXTREME_ANGLES | CPS_F_TOP_DEAD_SPOT | CPS_F_BOTTOM_DEAD_SPOT | CPS_F_ENERGY);
    put16(275U);            /* power */
    put8(102U);             /* balance, 51 % */
    put16(9999U);           /* accumulated torque */
    put32(123456UL);        /* wheel turns */
    put16(40000U);          /* wheel time */
    put16(7777U);           /* crank turns */
    put16(20000U);          /* crank time */
    put16(500U); put16(100U);   /* extreme force, max and min */
    put16(600U); put16(200U);   /* extreme torque */
    put8(0x11U); put8(0x22U); put8(0x33U);   /* extreme angles */
    put16(70U);             /* top dead spot */
    put16(250U);            /* bottom dead spot */
    put16(4321U);           /* accumulated energy */

    TEST_ASSERT_EQUAL_UINT16(34U, n);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &m));

    TEST_ASSERT_EQUAL_INT16(275, m.power_w);
    TEST_ASSERT_EQUAL_UINT8(51U, m.balance_pct);
    TEST_ASSERT_TRUE(m.balance_is_left);
    TEST_ASSERT_EQUAL_UINT16(9999U, m.torque_32nm);
    TEST_ASSERT_TRUE(m.torque_from_crank);
    TEST_ASSERT_EQUAL_UINT32(123456UL, m.wheel_revs);
    TEST_ASSERT_EQUAL_UINT16(40000U, m.wheel_time);
    TEST_ASSERT_EQUAL_UINT16(7777U, m.crank_revs);
    TEST_ASSERT_EQUAL_UINT16(20000U, m.crank_time);
    TEST_ASSERT_TRUE(m.have_energy);
    TEST_ASSERT_EQUAL_UINT16(4321U, m.energy_kj);
}

static void test_the_pedal_analysis_fields_are_skipped_by_the_right_width(void)
{
    /*
     * The same idea with only the three fields this device keeps nothing
     * from between the power and the energy. Nineteen bytes: two of flags,
     * two of power, four of force, four of torque, three of angles, two of
     * the top dead spot and two of energy.
     */
    put16(CPS_F_EXTREME_FORCE | CPS_F_EXTREME_TORQUE | CPS_F_EXTREME_ANGLES |
          CPS_F_TOP_DEAD_SPOT | CPS_F_ENERGY);
    put16(180U);
    put16(0U); put16(0U);
    put16(0U); put16(0U);
    put8(0U); put8(0U); put8(0U);
    put16(0U);
    put16(1234U);

    TEST_ASSERT_EQUAL_UINT16(19U, n);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_EQUAL_INT16(180, m.power_w);
    TEST_ASSERT_EQUAL_UINT16(1234U, m.energy_kj);
}

static void test_a_gap_in_the_middle_still_lands_right(void)
{
    /* torque and crank present, wheel absent: the crank must not slide */
    put16(CPS_F_TORQUE | CPS_F_CRANK_REV);
    put16(200U);
    put16(1111U);
    put16(2222U);
    put16(3333U);

    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_EQUAL_UINT16(1111U, m.torque_32nm);
    TEST_ASSERT_FALSE(m.have_wheel);
    TEST_ASSERT_TRUE(m.have_crank);
    TEST_ASSERT_EQUAL_UINT16(2222U, m.crank_revs);
    TEST_ASSERT_EQUAL_UINT16(3333U, m.crank_time);
}

/* ==========================================================================
 * The bits that carry no field
 * ========================================================================== */

static void test_the_meter_asking_for_a_zero_offset(void)
{
    put16(CPS_F_OFFSET_NEEDED);
    put16(0U);

    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_TRUE(m.offset_needed);

    n = 0U;
    put16(0x0000U);
    put16(200U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_FALSE(m.offset_needed);
}

static void test_the_bits_without_a_field_do_not_move_anything(void)
{
    /*
     * Balance reference, torque source and offset compensation are flags
     * with no bytes of their own. If the walk gave them a width, the
     * energy below would come out wrong.
     */
    put16(CPS_F_BALANCE_LEFT | CPS_F_TORQUE_CRANK | CPS_F_OFFSET_NEEDED | CPS_F_ENERGY);
    put16(200U);
    put16(4444U);

    TEST_ASSERT_EQUAL_UINT16(6U, n);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &m));
    TEST_ASSERT_EQUAL_UINT16(4444U, m.energy_kj);
}

/* ==========================================================================
 * The edges
 * ========================================================================== */

static void test_the_first_notification_has_nothing_to_compare_against(void)
{
    struct cps_measurement a;

    crank_at(100U, 1000U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));

    TEST_ASSERT_EQUAL_UINT8(0U, cps_cadence_rpm(NULL, &a));
    TEST_ASSERT_EQUAL_UINT16(0U, cps_speed_kmh100(NULL, &a, 2105U));
}

static void test_a_meter_that_stopped_sending_the_counters(void)
{
    struct cps_measurement a;
    struct cps_measurement b;

    crank_at(100U, 1000U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));

    n = 0U;
    put16(0x0000U);
    put16(210U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    TEST_ASSERT_EQUAL_UINT8(0U, cps_cadence_rpm(&a, &b));
}

static void test_nothing_blows_up_without_a_payload(void)
{
    TEST_ASSERT_FALSE(cps_parse_measurement(NULL, 10U, &m));
    TEST_ASSERT_FALSE(cps_parse_measurement(buf, 10U, NULL));
    TEST_ASSERT_EQUAL_UINT8(0U, cps_cadence_rpm(NULL, NULL));
    TEST_ASSERT_EQUAL_UINT16(0U, cps_speed_kmh100(NULL, NULL, 2105U));

    struct cps_measurement a;
    struct cps_measurement b;

    wheel_at(0U, 0U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &a));
    wheel_at(1U, 2048U);
    TEST_ASSERT_TRUE(cps_parse_measurement(buf, n, &b));

    /* a wheel of no size gives no speed instead of dividing by nothing */
    TEST_ASSERT_EQUAL_UINT16(0U, cps_speed_kmh100(&a, &b, 0U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_meter_that_sends_only_the_power);
    RUN_TEST(test_the_power_is_signed);
    RUN_TEST(test_a_sprint_of_fifteen_hundred_watts);
    RUN_TEST(test_a_payload_too_short_for_the_power_is_refused);
    RUN_TEST(test_a_notification_that_lies_about_itself_is_dropped_whole);
    RUN_TEST(test_the_balance_is_in_half_percent_steps);
    RUN_TEST(test_the_balance_can_name_the_left_leg);
    RUN_TEST(test_one_crank_turn_a_second_is_sixty_revolutions_a_minute);
    RUN_TEST(test_a_normal_cadence);
    RUN_TEST(test_the_wheel_clock_runs_at_twice_the_crank_clock);
    RUN_TEST(test_a_ride_at_thirty_kilometres_an_hour);
    RUN_TEST(test_the_counters_wrapping_does_not_break_the_sum);
    RUN_TEST(test_a_rider_who_stopped_pedalling);
    RUN_TEST(test_a_cadence_no_rider_produces_is_thrown_away);
    RUN_TEST(test_a_speed_no_bicycle_reaches_is_thrown_away);
    RUN_TEST(test_every_field_at_once_and_the_last_one_lands_right);
    RUN_TEST(test_the_pedal_analysis_fields_are_skipped_by_the_right_width);
    RUN_TEST(test_a_gap_in_the_middle_still_lands_right);
    RUN_TEST(test_the_meter_asking_for_a_zero_offset);
    RUN_TEST(test_the_bits_without_a_field_do_not_move_anything);
    RUN_TEST(test_the_first_notification_has_nothing_to_compare_against);
    RUN_TEST(test_a_meter_that_stopped_sending_the_counters);
    RUN_TEST(test_nothing_blows_up_without_a_payload);

    return UNITY_END();
}
