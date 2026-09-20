/**
 * @file test_ubx_m10.c
 * @brief UBX frames and messages of the u-blox M10 (modules/gnss_drivers)
 *
 * The arrays below come from an independent implementation of the documents
 * (ubx_vectors.py of the tools) following "u-blox M10 SPG 5.30 Interface
 * description" (UBXDOC-304424225-20395, R01):
 *
 * | Message | Section |
 * |---|---|
 * | Frame and 8-bit Fletcher checksum | 3.2 and 3.4 |
 * | UBX-CFG-VALSET (0x06 0x8A) and UBX-CFG-VALGET (0x06 0x8B) | 3.10.5 and 3.10.4 |
 * | UBX-CFG-RST (0x06 0x04) | 3.10.2 |
 * | UBX-RXM-PMREQ (0x02 0x41) | 3.16.7 |
 * | UBX-NAV-PVT (0x01 0x07) | 3.15.11 |
 * | UBX-NAV-SAT (0x01 0x35) | 3.15.13 |
 *
 * The position is the one of the other tests of the project: Nancy,
 * 48.6921 N, 6.1844 E, where the simulation traces of the legacy come from.
 */

#include <string.h>

#include "unity.h"

#include "drivers/gnss/ubx_m10.h"

static const uint8_t valset_rate_meas[] = {
    0xB5U, 0x62U, 0x06U, 0x8AU, 0x0AU, 0x00U, 0x00U, 0x03U, 0x00U, 0x00U, 0x01U, 0x00U,
    0x21U, 0x30U, 0xE8U, 0x03U, 0xDAU, 0xD6U,
};

static const uint8_t valset_leap[] = {
    0xB5U, 0x62U, 0x06U, 0x8AU, 0x09U, 0x00U, 0x00U, 0x03U, 0x00U, 0x00U, 0x01U, 0x00U,
    0xD0U, 0x20U, 0x02U, 0x8FU, 0xF8U,
};

static const uint8_t valset_baudrate_flash[] = {
    0xB5U, 0x62U, 0x06U, 0x8AU, 0x0CU, 0x00U, 0x00U, 0x04U, 0x00U, 0x00U, 0x01U, 0x00U,
    0x52U, 0x40U, 0x00U, 0x96U, 0x00U, 0x00U, 0xC9U, 0x40U,
};

static const uint8_t valget_dynmodel[] = {
    0xB5U, 0x62U, 0x06U, 0x8BU, 0x08U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x21U, 0x00U,
    0x11U, 0x20U, 0xEBU, 0x57U,
};

static const uint8_t pmreq_backup[] = {
    0xB5U, 0x62U, 0x02U, 0x41U, 0x10U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x06U, 0x00U, 0x00U, 0x00U, 0x08U, 0x00U, 0x00U, 0x00U, 0x61U, 0x6BU,
};

static const uint8_t cfg_rst_cold[] = {
    0xB5U, 0x62U, 0x06U, 0x04U, 0x04U, 0x00U, 0xFFU, 0xFFU, 0x00U, 0x00U, 0x0CU, 0x5DU,
};

static const uint8_t nav_pvt_nancy[] = {
    0x00U, 0x00U, 0x00U, 0x00U, 0xEAU, 0x07U, 0x09U, 0x13U, 0x0EU, 0x23U, 0x07U, 0x07U,
    0x1EU, 0x00U, 0x00U, 0x00U, 0x80U, 0xB2U, 0xE6U, 0x0EU, 0x03U, 0x01U, 0x00U, 0x0BU,
    0x20U, 0xAAU, 0xAFU, 0x03U, 0x28U, 0xD3U, 0x05U, 0x1DU, 0x90U, 0xD0U, 0x03U, 0x00U,
    0x10U, 0x15U, 0x03U, 0x00U, 0xACU, 0x0DU, 0x00U, 0x00U, 0x88U, 0x13U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x8DU, 0x20U, 0x00U, 0x00U, 0x4EU, 0x61U, 0xBCU, 0x00U, 0x78U, 0x00U, 0x00U, 0x00U,
    0x90U, 0xD0U, 0x03U, 0x00U, 0x84U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
};

static const uint8_t nav_pvt_no_fix[] = {
    0x00U, 0x00U, 0x00U, 0x00U, 0xEAU, 0x07U, 0x09U, 0x13U, 0x0EU, 0x23U, 0x07U, 0x00U,
    0x1EU, 0x00U, 0x00U, 0x00U, 0x80U, 0xB2U, 0xE6U, 0x0EU, 0x00U, 0x00U, 0x00U, 0x03U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x90U, 0xD0U, 0x03U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0xACU, 0x0DU, 0x00U, 0x00U, 0x88U, 0x13U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x78U, 0x00U, 0x00U, 0x00U,
    0x90U, 0xD0U, 0x03U, 0x00U, 0x0FU, 0x27U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
};

static const uint8_t nav_pvt_date_only[] = {
    0x00U, 0x00U, 0x00U, 0x00U, 0xEAU, 0x07U, 0x09U, 0x13U, 0x0EU, 0x23U, 0x07U, 0x05U,
    0x1EU, 0x00U, 0x00U, 0x00U, 0x80U, 0xB2U, 0xE6U, 0x0EU, 0x02U, 0x01U, 0x00U, 0x0BU,
    0x20U, 0xAAU, 0xAFU, 0x03U, 0x28U, 0xD3U, 0x05U, 0x1DU, 0x90U, 0xD0U, 0x03U, 0x00U,
    0x10U, 0x15U, 0x03U, 0x00U, 0xACU, 0x0DU, 0x00U, 0x00U, 0x88U, 0x13U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x8DU, 0x20U, 0x00U, 0x00U, 0x4EU, 0x61U, 0xBCU, 0x00U, 0x78U, 0x00U, 0x00U, 0x00U,
    0x90U, 0xD0U, 0x03U, 0x00U, 0x84U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
};

static const uint8_t nav_pvt_pot[] = {
    0x00U, 0x00U, 0x00U, 0x00U, 0xEAU, 0x07U, 0x09U, 0x13U, 0x0EU, 0x23U, 0x07U, 0x07U,
    0x1EU, 0x00U, 0x00U, 0x00U, 0x80U, 0xB2U, 0xE6U, 0x0EU, 0x03U, 0x11U, 0x00U, 0x0BU,
    0x20U, 0xAAU, 0xAFU, 0x03U, 0x28U, 0xD3U, 0x05U, 0x1DU, 0x90U, 0xD0U, 0x03U, 0x00U,
    0x10U, 0x15U, 0x03U, 0x00U, 0xACU, 0x0DU, 0x00U, 0x00U, 0x88U, 0x13U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x8DU, 0x20U, 0x00U, 0x00U, 0xE0U, 0x55U, 0xBBU, 0xFFU, 0x78U, 0x00U, 0x00U, 0x00U,
    0x90U, 0xD0U, 0x03U, 0x00U, 0x84U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
};

static const uint8_t nav_sat_three[] = {
    0x40U, 0xE2U, 0x01U, 0x00U, 0x01U, 0x03U, 0x00U, 0x00U, 0x00U, 0x07U, 0x2AU, 0x3FU,
    0x76U, 0x00U, 0x00U, 0x00U, 0x18U, 0x00U, 0x00U, 0x00U, 0x02U, 0x15U, 0x25U, 0x18U,
    0x2CU, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x03U, 0x0CU, 0x13U, 0xFBU,
    0xE2U, 0xFFU, 0x00U, 0x00U, 0x08U, 0x00U, 0x00U, 0x00U,
};

/** The pseudo-code of section 3.4, transcribed, as an oracle for the frames */
static void ref_checksum(const uint8_t *data, size_t len, uint8_t *ck_a, uint8_t *ck_b)
{
    uint8_t a = 0U;
    uint8_t b = 0U;

    for (size_t i = 0U; i < len; i++) {
        a = (uint8_t)((a + data[i]) & 0xFFU);
        b = (uint8_t)((b + a) & 0xFFU);
    }
    *ck_a = a;
    *ck_b = b;
}

void setUp(void) {}
void tearDown(void) {}

static void test_key_size_comes_from_bits_30_to_28_of_the_key(void)
{
    TEST_ASSERT_EQUAL_size_t(1U, ubx_m10_key_size(UBX_M10_KEY_UART1INPROT_UBX));  /* L */
    TEST_ASSERT_EQUAL_size_t(1U, ubx_m10_key_size(UBX_M10_KEY_NAVSPG_DYNMODEL));  /* E1 */
    TEST_ASSERT_EQUAL_size_t(2U, ubx_m10_key_size(UBX_M10_KEY_RATE_MEAS));        /* U2 */
    TEST_ASSERT_EQUAL_size_t(4U, ubx_m10_key_size(UBX_M10_KEY_UART1_BAUDRATE));   /* U4 */
    TEST_ASSERT_EQUAL_size_t(8U, ubx_m10_key_size(0x50123456U));                  /* U8 */
    TEST_ASSERT_EQUAL_size_t(0U, ubx_m10_key_size(0x00123456U));
    TEST_ASSERT_EQUAL_size_t(0U, ubx_m10_key_size(0x60123456U));
}

static void test_frame_carries_the_header_the_length_and_the_checksum(void)
{
    const uint8_t payload[] = {0x11U, 0x22U, 0x33U};
    uint8_t buf[32];
    uint8_t ck_a;
    uint8_t ck_b;
    size_t len = ubx_m10_frame(buf, sizeof(buf), UBX_M10_CLASS_MON, UBX_M10_MON_VER, payload,
                               sizeof(payload));

    TEST_ASSERT_EQUAL_size_t(UBX_M10_FRAME_LEN(sizeof(payload)), len);
    TEST_ASSERT_EQUAL_HEX8(UBX_M10_SYNC1, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(UBX_M10_SYNC2, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(UBX_M10_CLASS_MON, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(UBX_M10_MON_VER, buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x03U, buf[4]); /* length, little-endian */
    TEST_ASSERT_EQUAL_HEX8(0x00U, buf[5]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, &buf[6], sizeof(payload));

    /* the checksum starts at the class and stops before the two bytes */
    ref_checksum(&buf[2], len - 4U, &ck_a, &ck_b);
    TEST_ASSERT_EQUAL_HEX8(ck_a, buf[len - 2U]);
    TEST_ASSERT_EQUAL_HEX8(ck_b, buf[len - 1U]);
}

static void test_frame_without_payload_is_a_poll(void)
{
    uint8_t buf[16];
    size_t len = ubx_m10_frame(buf, sizeof(buf), UBX_M10_CLASS_MON, UBX_M10_MON_GNSS, NULL, 0U);

    TEST_ASSERT_EQUAL_size_t(8U, len);
    TEST_ASSERT_EQUAL_HEX8(0x00U, buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00U, buf[5]);
}

static void test_frame_refuses_a_buffer_that_is_one_byte_short(void)
{
    const uint8_t payload[] = {0x01U, 0x02U};
    uint8_t buf[9]; /* 8 of overhead plus 2 of payload would need 10 */

    TEST_ASSERT_EQUAL_size_t(0U, ubx_m10_frame(buf, sizeof(buf), 0x06U, 0x8AU, payload,
                                               sizeof(payload)));
}

static void test_valset_of_the_rate_matches_the_document(void)
{
    uint8_t buf[32];
    size_t len = ubx_m10_valset(buf, sizeof(buf), UBX_M10_KEY_RATE_MEAS, 1000U,
                                UBX_M10_LAYER_RAM | UBX_M10_LAYER_BBR);

    TEST_ASSERT_EQUAL_size_t(sizeof(valset_rate_meas), len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(valset_rate_meas, buf, len);
}

static void test_valset_of_a_one_byte_key_carries_one_byte(void)
{
    uint8_t buf[32];
    size_t len = ubx_m10_valset(buf, sizeof(buf), UBX_M10_KEY_PM_OPERATEMODE, UBX_M10_OPERATE_LEAP,
                                UBX_M10_LAYER_RAM | UBX_M10_LAYER_BBR);

    TEST_ASSERT_EQUAL_size_t(sizeof(valset_leap), len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(valset_leap, buf, len);
}

static void test_valset_of_the_baud_rate_goes_to_the_layer_asked(void)
{
    uint8_t buf[32];
    size_t len = ubx_m10_valset(buf, sizeof(buf), UBX_M10_KEY_UART1_BAUDRATE, 38400U,
                                UBX_M10_LAYER_FLASH);

    TEST_ASSERT_EQUAL_size_t(sizeof(valset_baudrate_flash), len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(valset_baudrate_flash, buf, len);
}

static void test_valset_refuses_an_unknown_key_or_layer(void)
{
    uint8_t buf[32];

    TEST_ASSERT_EQUAL_size_t(0U, ubx_m10_valset(buf, sizeof(buf), 0x00210001U, 1U,
                                                UBX_M10_LAYER_RAM));
    TEST_ASSERT_EQUAL_size_t(0U, ubx_m10_valset(buf, sizeof(buf), UBX_M10_KEY_RATE_MEAS, 1U, 0U));
    TEST_ASSERT_EQUAL_size_t(0U, ubx_m10_valset(buf, sizeof(buf), UBX_M10_KEY_RATE_MEAS, 1U,
                                                0x08U));
}

static void test_valget_polls_the_layer_in_use(void)
{
    uint8_t buf[32];
    size_t len = ubx_m10_valget(buf, sizeof(buf), UBX_M10_KEY_NAVSPG_DYNMODEL);

    TEST_ASSERT_EQUAL_size_t(sizeof(valget_dynmodel), len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(valget_dynmodel, buf, len);
}

static void test_valget_answer_gives_the_value_of_the_key_asked(void)
{
    const uint8_t answer[] = {0x01U, 0x01U, 0x00U, 0x00U, 0x01U, 0x00U, 0x21U, 0x30U,
                              0xE8U, 0x03U};
    uint64_t value = 0U;

    TEST_ASSERT_TRUE(ubx_m10_valget_parse(answer, sizeof(answer), UBX_M10_KEY_RATE_MEAS, &value));
    TEST_ASSERT_EQUAL_UINT64(1000U, value);

    /* another key, or a payload without the whole value, is not an answer */
    TEST_ASSERT_FALSE(ubx_m10_valget_parse(answer, sizeof(answer), UBX_M10_KEY_RATE_NAV, &value));
    TEST_ASSERT_FALSE(ubx_m10_valget_parse(answer, sizeof(answer) - 1U, UBX_M10_KEY_RATE_MEAS,
                                           &value));
}

static void test_pmreq_asks_for_backup_with_the_force_flag(void)
{
    uint8_t buf[32];
    size_t len = ubx_m10_pmreq(buf, sizeof(buf), 0U, true, true, UBX_M10_PMREQ_WAKE_UARTRX);

    TEST_ASSERT_EQUAL_size_t(sizeof(pmreq_backup), len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(pmreq_backup, buf, len);
    /* the integration manual requires force (3.7.4.2): flags bit 2 */
    TEST_ASSERT_EQUAL_HEX8(0x06U, buf[14]);
}

static void test_cfg_rst_cold_start_drops_everything(void)
{
    uint8_t buf[16];
    size_t len = ubx_m10_cfg_rst(buf, sizeof(buf), UBX_M10_RST_COLD, 0x00U);

    TEST_ASSERT_EQUAL_size_t(sizeof(cfg_rst_cold), len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(cfg_rst_cold, buf, len);
}

static void test_nav_pvt_of_nancy_gives_position_speed_and_time(void)
{
    struct ubx_m10_pvt pvt;

    TEST_ASSERT_TRUE(ubx_m10_parse_pvt(nav_pvt_nancy, sizeof(nav_pvt_nancy), &pvt));
    TEST_ASSERT_EQUAL_INT32(486921000, pvt.lat_e7);
    TEST_ASSERT_EQUAL_INT32(61844000, pvt.lon_e7);
    TEST_ASSERT_EQUAL_INT32(202000, pvt.hmsl_mm);
    TEST_ASSERT_EQUAL_INT32(250000, pvt.height_mm);
    TEST_ASSERT_EQUAL_INT32(8333, pvt.ground_speed_mms); /* 30 km/h */
    TEST_ASSERT_EQUAL_INT32(12345678, pvt.head_motion_e5);
    TEST_ASSERT_EQUAL_UINT16(132U, pvt.pdop_e2);         /* 1.32 */
    TEST_ASSERT_EQUAL_UINT8(11U, pvt.num_sv);
    TEST_ASSERT_EQUAL_UINT8(UBX_M10_FIX_3D, pvt.fix_type);
    TEST_ASSERT_TRUE(pvt.fix_ok);
    TEST_ASSERT_FALSE(pvt.llh_invalid);
    TEST_ASSERT_EQUAL_UINT8(UBX_M10_PSM_OFF, pvt.psm_state);
    TEST_ASSERT_EQUAL_UINT16(2026U, pvt.year);
    TEST_ASSERT_EQUAL_UINT8(9U, pvt.month);
    TEST_ASSERT_EQUAL_UINT8(19U, pvt.day);
    TEST_ASSERT_EQUAL_UINT8(14U, pvt.hour);
    TEST_ASSERT_EQUAL_UINT8(35U, pvt.minute);
    TEST_ASSERT_EQUAL_UINT8(7U, pvt.second);
    TEST_ASSERT_EQUAL_INT32(250000000, pvt.nano);
    TEST_ASSERT_TRUE(pvt.date_valid);
    TEST_ASSERT_TRUE(pvt.time_valid);
    TEST_ASSERT_TRUE(pvt.fully_resolved);
    TEST_ASSERT_EQUAL_UINT32(3500U, pvt.hacc_mm);
}

static void test_nav_pvt_without_fix_says_so(void)
{
    struct ubx_m10_pvt pvt;

    TEST_ASSERT_TRUE(ubx_m10_parse_pvt(nav_pvt_no_fix, sizeof(nav_pvt_no_fix), &pvt));
    TEST_ASSERT_EQUAL_UINT8(UBX_M10_FIX_NONE, pvt.fix_type);
    TEST_ASSERT_FALSE(pvt.fix_ok);
    TEST_ASSERT_FALSE(pvt.date_valid);
    TEST_ASSERT_FALSE(pvt.time_valid);
    TEST_ASSERT_EQUAL_UINT8(3U, pvt.num_sv);
    TEST_ASSERT_EQUAL_UINT16(9999U, pvt.pdop_e2);
}

static void test_nav_pvt_tells_the_bits_of_the_valid_field_apart(void)
{
    struct ubx_m10_pvt pvt;

    /* valid = 0x05: date and fully resolved, without a valid time */
    TEST_ASSERT_TRUE(ubx_m10_parse_pvt(nav_pvt_date_only, sizeof(nav_pvt_date_only), &pvt));
    TEST_ASSERT_TRUE(pvt.date_valid);
    TEST_ASSERT_FALSE(pvt.time_valid);
    TEST_ASSERT_TRUE(pvt.fully_resolved);
    TEST_ASSERT_EQUAL_UINT8(UBX_M10_FIX_2D, pvt.fix_type);
}

static void test_nav_pvt_carries_the_power_state_and_a_negative_course(void)
{
    struct ubx_m10_pvt pvt;

    TEST_ASSERT_TRUE(ubx_m10_parse_pvt(nav_pvt_pot, sizeof(nav_pvt_pot), &pvt));
    /* flags bits 4 to 2: power optimized tracking, the LEAP state */
    TEST_ASSERT_EQUAL_UINT8(UBX_M10_PSM_POT, pvt.psm_state);
    TEST_ASSERT_TRUE(pvt.fix_ok);
    TEST_ASSERT_EQUAL_INT32(-4500000, pvt.head_motion_e5); /* -45 degrees */
}

static void test_nav_pvt_shorter_than_92_bytes_is_refused(void)
{
    struct ubx_m10_pvt pvt;

    TEST_ASSERT_FALSE(ubx_m10_parse_pvt(nav_pvt_nancy, sizeof(nav_pvt_nancy) - 1U, &pvt));
}

static void test_nav_sat_gives_every_satellite_of_the_message(void)
{
    struct ubx_m10_sat sats[8];
    int n = ubx_m10_parse_sat(nav_sat_three, sizeof(nav_sat_three), sats, 8);

    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_UINT8(UBX_M10_GNSS_GPS, sats[0].gnss_id);
    TEST_ASSERT_EQUAL_UINT8(7U, sats[0].sv_id);
    TEST_ASSERT_EQUAL_UINT8(42U, sats[0].cno_dbhz);
    TEST_ASSERT_EQUAL_INT8(63, sats[0].elev_deg);
    TEST_ASSERT_EQUAL_INT16(118, sats[0].azim_deg);
    TEST_ASSERT_TRUE(sats[0].used);

    TEST_ASSERT_EQUAL_UINT8(UBX_M10_GNSS_GALILEO, sats[1].gnss_id);
    TEST_ASSERT_FALSE(sats[1].used); /* flags bit 3 off */

    TEST_ASSERT_EQUAL_UINT8(UBX_M10_GNSS_BEIDOU, sats[2].gnss_id);
    TEST_ASSERT_EQUAL_INT8(-5, sats[2].elev_deg);
    TEST_ASSERT_EQUAL_INT16(-30, sats[2].azim_deg);
    TEST_ASSERT_TRUE(sats[2].used);
}

static void test_nav_sat_stops_at_the_room_it_is_given(void)
{
    struct ubx_m10_sat sats[2];

    TEST_ASSERT_EQUAL_INT(2, ubx_m10_parse_sat(nav_sat_three, sizeof(nav_sat_three), sats, 2));
    TEST_ASSERT_EQUAL_UINT8(21U, sats[1].sv_id);
}

static void test_nav_sat_truncated_gives_only_what_arrived(void)
{
    struct ubx_m10_sat sats[8];

    /* the header says three satellites, the payload carries two */
    TEST_ASSERT_EQUAL_INT(2, ubx_m10_parse_sat(nav_sat_three, sizeof(nav_sat_three) - 12U, sats,
                                               8));
    TEST_ASSERT_EQUAL_INT(-1, ubx_m10_parse_sat(nav_sat_three, 4U, sats, 8));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_key_size_comes_from_bits_30_to_28_of_the_key);
    RUN_TEST(test_frame_carries_the_header_the_length_and_the_checksum);
    RUN_TEST(test_frame_without_payload_is_a_poll);
    RUN_TEST(test_frame_refuses_a_buffer_that_is_one_byte_short);
    RUN_TEST(test_valset_of_the_rate_matches_the_document);
    RUN_TEST(test_valset_of_a_one_byte_key_carries_one_byte);
    RUN_TEST(test_valset_of_the_baud_rate_goes_to_the_layer_asked);
    RUN_TEST(test_valset_refuses_an_unknown_key_or_layer);
    RUN_TEST(test_valget_polls_the_layer_in_use);
    RUN_TEST(test_valget_answer_gives_the_value_of_the_key_asked);
    RUN_TEST(test_pmreq_asks_for_backup_with_the_force_flag);
    RUN_TEST(test_cfg_rst_cold_start_drops_everything);
    RUN_TEST(test_nav_pvt_of_nancy_gives_position_speed_and_time);
    RUN_TEST(test_nav_pvt_without_fix_says_so);
    RUN_TEST(test_nav_pvt_tells_the_bits_of_the_valid_field_apart);
    RUN_TEST(test_nav_pvt_carries_the_power_state_and_a_negative_course);
    RUN_TEST(test_nav_pvt_shorter_than_92_bytes_is_refused);
    RUN_TEST(test_nav_sat_gives_every_satellite_of_the_message);
    RUN_TEST(test_nav_sat_stops_at_the_room_it_is_given);
    RUN_TEST(test_nav_sat_truncated_gives_only_what_arrived);

    return UNITY_END();
}
