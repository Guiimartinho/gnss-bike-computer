/**
 * @file ubx_m10.c
 * @brief UBX frames and messages of the u-blox M10, in plain C
 *
 * The bytes go in and out one at a time, little-endian, without casting the
 * buffer to a structure: the payloads of u-blox are not aligned, and this way
 * the host tests compile anywhere. Sources in ubx_m10.h.
 */

#include "drivers/gnss/ubx_m10.h"

#include <string.h>

/* Little-endian readers */

static uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int16_t rd_i16(const uint8_t *p)
{
    return (int16_t)rd_u16(p);
}

static int32_t rd_i32(const uint8_t *p)
{
    return (int32_t)rd_u32(p);
}

/** 8-bit Fletcher checksum over class, id, length and payload (3.4) */
static void checksum(const uint8_t *data, size_t len, uint8_t *ck_a, uint8_t *ck_b)
{
    uint8_t a = 0U;
    uint8_t b = 0U;

    for (size_t i = 0U; i < len; i++) {
        a = (uint8_t)(a + data[i]);
        b = (uint8_t)(b + a);
    }
    *ck_a = a;
    *ck_b = b;
}

size_t ubx_m10_key_size(uint32_t key)
{
    switch ((key >> 28) & 0x07U) {
    case 0x01U: /* one bit, stored in one byte */
    case 0x02U:
        return 1U;
    case 0x03U:
        return 2U;
    case 0x04U:
        return 4U;
    case 0x05U:
        return 8U;
    default:
        return 0U;
    }
}

size_t ubx_m10_frame(uint8_t *buf, size_t cap, uint8_t cls, uint8_t id, const uint8_t *payload,
                     size_t len)
{
    size_t total = UBX_M10_FRAME_LEN(len);

    if ((buf == NULL) || (cap < total) || (len > 0xFFFFU) || ((payload == NULL) && (len > 0U))) {
        return 0U;
    }

    buf[0] = UBX_M10_SYNC1;
    buf[1] = UBX_M10_SYNC2;
    buf[2] = cls;
    buf[3] = id;
    buf[4] = (uint8_t)(len & 0xFFU);
    buf[5] = (uint8_t)((len >> 8) & 0xFFU);
    if (len > 0U) {
        (void)memcpy(&buf[UBX_M10_HEADER_LEN], payload, len);
    }
    checksum(&buf[2], len + 4U, &buf[UBX_M10_HEADER_LEN + len], &buf[UBX_M10_HEADER_LEN + len + 1U]);

    return total;
}

size_t ubx_m10_valset(uint8_t *buf, size_t cap, uint32_t key, uint64_t value, uint8_t layers)
{
    uint8_t payload[UBX_M10_VALSET_MAX];
    size_t size = ubx_m10_key_size(key);

    if ((size == 0U) || (layers == 0U) ||
        ((layers & ~(UBX_M10_LAYER_RAM | UBX_M10_LAYER_BBR | UBX_M10_LAYER_FLASH)) != 0U)) {
        return 0U;
    }

    payload[0] = 0x00U; /* version 0: no transaction */
    payload[1] = layers;
    payload[2] = 0x00U;
    payload[3] = 0x00U;
    payload[4] = (uint8_t)(key & 0xFFU);
    payload[5] = (uint8_t)((key >> 8) & 0xFFU);
    payload[6] = (uint8_t)((key >> 16) & 0xFFU);
    payload[7] = (uint8_t)((key >> 24) & 0xFFU);
    for (size_t i = 0U; i < size; i++) {
        payload[UBX_M10_VALSET_HDR + 4U + i] = (uint8_t)((value >> (8U * i)) & 0xFFU);
    }

    return ubx_m10_frame(buf, cap, UBX_M10_CLASS_CFG, UBX_M10_CFG_VALSET, payload,
                         UBX_M10_VALSET_HDR + 4U + size);
}

size_t ubx_m10_valget(uint8_t *buf, size_t cap, uint32_t key)
{
    uint8_t payload[UBX_M10_VALSET_HDR + 4U];

    if (ubx_m10_key_size(key) == 0U) {
        return 0U;
    }

    payload[0] = 0x00U; /* version 0: request */
    payload[1] = 0x00U; /* layer 0: the RAM layer, which is what runs */
    payload[2] = 0x00U;
    payload[3] = 0x00U;
    payload[4] = (uint8_t)(key & 0xFFU);
    payload[5] = (uint8_t)((key >> 8) & 0xFFU);
    payload[6] = (uint8_t)((key >> 16) & 0xFFU);
    payload[7] = (uint8_t)((key >> 24) & 0xFFU);

    return ubx_m10_frame(buf, cap, UBX_M10_CLASS_CFG, UBX_M10_CFG_VALGET, payload,
                         sizeof(payload));
}

bool ubx_m10_valget_parse(const uint8_t *payload, size_t len, uint32_t key, uint64_t *value)
{
    size_t size = ubx_m10_key_size(key);

    if ((payload == NULL) || (value == NULL) || (size == 0U) ||
        (len < (UBX_M10_VALSET_HDR + 4U + size))) {
        return false;
    }
    if (rd_u32(&payload[UBX_M10_VALSET_HDR]) != key) {
        return false;
    }

    uint64_t v = 0U;

    for (size_t i = 0U; i < size; i++) {
        v |= (uint64_t)payload[UBX_M10_VALSET_HDR + 4U + i] << (8U * i);
    }
    *value = v;

    return true;
}

size_t ubx_m10_pmreq(uint8_t *buf, size_t cap, uint32_t duration_ms, bool backup, bool force,
                     uint32_t wakeup_sources)
{
    uint8_t payload[UBX_M10_PMREQ_LEN];
    uint32_t flags = 0U;

    if (backup) {
        flags |= 0x02U; /* bit 1 */
    }
    if (force) {
        flags |= 0x04U; /* bit 2 */
    }

    (void)memset(payload, 0, sizeof(payload));
    payload[0] = 0x00U; /* version */
    payload[4] = (uint8_t)(duration_ms & 0xFFU);
    payload[5] = (uint8_t)((duration_ms >> 8) & 0xFFU);
    payload[6] = (uint8_t)((duration_ms >> 16) & 0xFFU);
    payload[7] = (uint8_t)((duration_ms >> 24) & 0xFFU);
    payload[8] = (uint8_t)(flags & 0xFFU);
    payload[9] = (uint8_t)((flags >> 8) & 0xFFU);
    payload[10] = (uint8_t)((flags >> 16) & 0xFFU);
    payload[11] = (uint8_t)((flags >> 24) & 0xFFU);
    payload[12] = (uint8_t)(wakeup_sources & 0xFFU);
    payload[13] = (uint8_t)((wakeup_sources >> 8) & 0xFFU);
    payload[14] = (uint8_t)((wakeup_sources >> 16) & 0xFFU);
    payload[15] = (uint8_t)((wakeup_sources >> 24) & 0xFFU);

    return ubx_m10_frame(buf, cap, UBX_M10_CLASS_RXM, UBX_M10_RXM_PMREQ, payload,
                         sizeof(payload));
}

size_t ubx_m10_cfg_rst(uint8_t *buf, size_t cap, uint16_t nav_bbr_mask, uint8_t reset_mode)
{
    uint8_t payload[4];

    payload[0] = (uint8_t)(nav_bbr_mask & 0xFFU);
    payload[1] = (uint8_t)((nav_bbr_mask >> 8) & 0xFFU);
    payload[2] = reset_mode;
    payload[3] = 0x00U;

    return ubx_m10_frame(buf, cap, UBX_M10_CLASS_CFG, UBX_M10_CFG_RST, payload, sizeof(payload));
}

bool ubx_m10_parse_pvt(const uint8_t *payload, size_t len, struct ubx_m10_pvt *out)
{
    if ((payload == NULL) || (out == NULL) || (len < UBX_M10_NAV_PVT_LEN)) {
        return false;
    }

    uint8_t valid = payload[11];
    uint8_t flags = payload[21];
    uint16_t flags3 = rd_u16(&payload[78]);

    out->itow_ms = rd_u32(&payload[0]);
    out->year = rd_u16(&payload[4]);
    out->month = payload[6];
    out->day = payload[7];
    out->hour = payload[8];
    out->minute = payload[9];
    out->second = payload[10];
    out->date_valid = (valid & 0x01U) != 0U;
    out->time_valid = (valid & 0x02U) != 0U;
    out->fully_resolved = (valid & 0x04U) != 0U;
    out->nano = rd_i32(&payload[16]);
    out->fix_type = payload[20];
    out->fix_ok = (flags & 0x01U) != 0U;
    out->psm_state = (uint8_t)((flags >> 2) & 0x07U);
    out->llh_invalid = (flags3 & 0x01U) != 0U;
    out->num_sv = payload[23];
    out->lon_e7 = rd_i32(&payload[24]);
    out->lat_e7 = rd_i32(&payload[28]);
    out->height_mm = rd_i32(&payload[32]);
    out->hmsl_mm = rd_i32(&payload[36]);
    out->hacc_mm = rd_u32(&payload[40]);
    out->vacc_mm = rd_u32(&payload[44]);
    out->ground_speed_mms = rd_i32(&payload[60]);
    out->head_motion_e5 = rd_i32(&payload[64]);
    out->speed_acc_mms = rd_u32(&payload[68]);
    out->head_acc_e5 = rd_u32(&payload[72]);
    out->pdop_e2 = rd_u16(&payload[76]);

    return true;
}

int ubx_m10_parse_sat(const uint8_t *payload, size_t len, struct ubx_m10_sat *out, int max)
{
    if ((payload == NULL) || (out == NULL) || (max < 0) || (len < UBX_M10_NAV_SAT_HDR)) {
        return -1;
    }

    size_t room = (len - UBX_M10_NAV_SAT_HDR) / UBX_M10_NAV_SAT_ITEM;
    size_t count = payload[5]; /* numSvs */

    if (count > room) {
        count = room; /* a truncated message gives what arrived */
    }
    if (count > (size_t)max) {
        count = (size_t)max;
    }

    for (size_t i = 0U; i < count; i++) {
        const uint8_t *p = &payload[UBX_M10_NAV_SAT_HDR + (i * UBX_M10_NAV_SAT_ITEM)];

        out[i].gnss_id = p[0];
        out[i].sv_id = p[1];
        out[i].cno_dbhz = p[2];
        out[i].elev_deg = (int8_t)p[3];
        out[i].azim_deg = rd_i16(&p[4]);
        out[i].used = (rd_u32(&p[8]) & 0x08U) != 0U; /* flags bit 3, svUsed */
    }

    return (int)count;
}
