/**
 * @file fit_encode.c
 * @brief Writes an activity in the Garmin FIT format, a block at a time
 *
 * Rules and sources in model/fit_encode.h.
 */

#include <math.h>
#include <string.h>

#include "model/fit_encode.h"

/* Base types of the format, with the endian bit set on everything wider
 * than a byte */
#define BT_ENUM      0x00U
#define BT_SINT8     0x01U
#define BT_UINT8     0x02U
#define BT_SINT32    0x85U
#define BT_UINT16    0x84U
#define BT_UINT32    0x86U
#define BT_UINT32Z   0x8CU

/* Global message numbers */
#define MSG_FILE_ID  0U
#define MSG_SESSION  18U
#define MSG_LAP      19U
#define MSG_RECORD   20U
#define MSG_EVENT    21U
#define MSG_ACTIVITY 34U

/* Local message types: one slot of the sixteen for each message we write */
#define LOC_FILE_ID  0U
#define LOC_EVENT    1U
#define LOC_RECORD   2U
#define LOC_LAP      3U
#define LOC_SESSION  4U
#define LOC_ACTIVITY 5U

/** Bit 6 of a record header tells a definition message from a data one */
#define HDR_DEFINITION  0x40U

/* Cursor over the caller buffer */
struct wr {
    uint8_t *buf;
    size_t cap;
    size_t at;
    bool over;
};

static void w8(struct wr *w, uint8_t v)
{
    if (w->at >= w->cap) {
        w->over = true;
        return;
    }
    w->buf[w->at] = v;
    w->at++;
}

static void w16(struct wr *w, uint16_t v)
{
    w8(w, (uint8_t)(v & 0xFFU));
    w8(w, (uint8_t)((v >> 8) & 0xFFU));
}

static void w32(struct wr *w, uint32_t v)
{
    w16(w, (uint16_t)(v & 0xFFFFU));
    w16(w, (uint16_t)((v >> 16) & 0xFFFFU));
}

/** One field of a definition message: number, size in bytes, base type */
static void wfield(struct wr *w, uint8_t num, uint8_t size, uint8_t type)
{
    w8(w, num);
    w8(w, size);
    w8(w, type);
}

/** Head of a definition message, with the count of the fields that follow */
static void wdef(struct wr *w, uint8_t local, uint16_t global, uint8_t fields)
{
    w8(w, (uint8_t)(HDR_DEFINITION | local));
    w8(w, 0U);              /* reserved */
    w8(w, 0U);              /* architecture 0: little endian */
    w16(w, global);
    w8(w, fields);
}

/*
 * CRC-16 of the format. The table holds the value of each nibble, and a
 * byte goes in as its low nibble then its high one.
 */
static const uint16_t crc_table[16] = {
    0x0000U, 0xCC01U, 0xD801U, 0x1400U, 0xF001U, 0x3C00U, 0x2800U, 0xE401U,
    0xA001U, 0x6C00U, 0x7800U, 0xB401U, 0x5000U, 0x9C01U, 0x8801U, 0x4400U,
};

/** One nibble step; with a zero nibble it is the whole state update */
static uint16_t crc_nibble(uint16_t crc, uint8_t nib)
{
    uint16_t tmp = crc_table[crc & 0x0FU];

    crc = (uint16_t)((crc >> 4) & 0x0FFFU);

    return (uint16_t)(crc ^ tmp ^ crc_table[nib & 0x0FU]);
}

uint16_t fit_crc(uint16_t crc, const uint8_t *data, size_t len)
{
    if (data == NULL) {
        return crc;
    }

    for (size_t i = 0U; i < len; i++) {
        crc = crc_nibble(crc, (uint8_t)(data[i] & 0x0FU));
        crc = crc_nibble(crc, (uint8_t)((data[i] >> 4) & 0x0FU));
    }

    return crc;
}

/** The 14 bytes of the file header for a given data size */
static void build_header(uint8_t *hdr, uint32_t data_size)
{
    struct wr w = {.buf = hdr, .cap = FIT_HEADER_LEN, .at = 0U, .over = false};

    w8(&w, (uint8_t)FIT_HEADER_LEN);
    w8(&w, 0x20U);          /* protocol version 2.0 */
    w16(&w, 2172U);         /* profile version, 21.72 */
    w32(&w, data_size);
    w8(&w, (uint8_t)'.');
    w8(&w, (uint8_t)'F');
    w8(&w, (uint8_t)'I');
    w8(&w, (uint8_t)'T');
    w16(&w, fit_crc(0U, hdr, 12U));
}

/** Close a block: add it to the running CRC and count it as data */
static size_t done(struct fit_enc *e, const struct wr *w, bool is_data)
{
    if (w->over) {
        return 0U;
    }

    e->crc = fit_crc(e->crc, w->buf, w->at);
    if (is_data) {
        e->data_size += (uint32_t)w->at;
    }

    return w->at;
}

uint32_t fit_time_from_date(uint32_t ddmmyy, uint32_t secj)
{
    /* the legacy keeps the date as DDMMYY, with the year in this century */
    uint32_t day = ddmmyy / 10000U;
    uint32_t month = (ddmmyy / 100U) % 100U;
    uint32_t year = 2000U + (ddmmyy % 100U);

    if ((day < 1U) || (day > 31U) || (month < 1U) || (month > 12U)) {
        return 0U;
    }

    /* days from 1970-01-01, by the civil calendar algorithm */
    uint32_t y = year;
    uint32_t m = month;

    if (m <= 2U) {
        y--;
        m += 12U;
    }

    int64_t era = (int64_t)y / 400;
    uint32_t yoe = (uint32_t)(y - (uint32_t)(era * 400));
    uint32_t doy = ((153U * (m - 3U)) + 2U) / 5U + (day - 1U);
    uint32_t doe = (yoe * 365U) + (yoe / 4U) - (yoe / 100U) + doy;
    int64_t days = (era * 146097) + (int64_t)doe - 719468;
    int64_t unix_s = (days * 86400) + (int64_t)secj;

    if (unix_s < (int64_t)FIT_EPOCH_OFFSET) {
        return 0U;
    }

    return (uint32_t)(unix_s - (int64_t)FIT_EPOCH_OFFSET);
}

int32_t fit_semicircles(float degrees)
{
    if (!isfinite(degrees) || (degrees < -180.0f) || (degrees > 180.0f)) {
        return (int32_t)FIT_INVALID_S32;
    }

    /* 2^31 / 180 */
    double semi = (double)degrees * 11930464.711111111;

    if (semi > 2147483646.0) {
        semi = 2147483646.0;
    }
    if (semi < -2147483647.0) {
        semi = -2147483647.0;
    }

    return (int32_t)semi;
}

/** A float to an unsigned raw field, with the scale and offset of the format */
static uint32_t scaled(float v, float scale, float offset, uint32_t max, uint32_t invalid)
{
    if (!isfinite(v)) {
        return invalid;
    }

    double raw = ((double)v + (double)offset) * (double)scale;

    if (raw < 0.0) {
        return 0U;
    }
    if (raw > (double)max) {
        return max;
    }

    return (uint32_t)(raw + 0.5);
}

size_t fit_enc_begin(struct fit_enc *e, uint8_t *buf, size_t cap)
{
    if ((e == NULL) || (buf == NULL) || (cap < FIT_HEADER_LEN)) {
        return 0U;
    }

    (void)memset(e, 0, sizeof(*e));
    /* the size is still unknown: fit_enc_header() rebuilds this at the end */
    build_header(buf, 0U);
    e->crc = fit_crc(0U, buf, FIT_HEADER_LEN);
    e->started = true;

    return FIT_HEADER_LEN;
}

size_t fit_enc_file_id(struct fit_enc *e, uint8_t *buf, size_t cap, uint32_t serial,
                       uint32_t time_created)
{
    if ((e == NULL) || !e->started || (buf == NULL)) {
        return 0U;
    }

    struct wr w = {.buf = buf, .cap = cap, .at = 0U, .over = false};

    wdef(&w, LOC_FILE_ID, MSG_FILE_ID, 5U);
    wfield(&w, 0U, 1U, BT_ENUM);        /* type */
    wfield(&w, 1U, 2U, BT_UINT16);      /* manufacturer */
    wfield(&w, 2U, 2U, BT_UINT16);      /* product */
    wfield(&w, 3U, 4U, BT_UINT32Z);     /* serial_number */
    wfield(&w, 4U, 4U, BT_UINT32);      /* time_created */

    w8(&w, LOC_FILE_ID);
    w8(&w, 4U);                         /* file type 4: activity */
    w16(&w, FIT_MANUFACTURER_DEV);
    w16(&w, 1U);                        /* product 1 of this manufacturer */
    w32(&w, serial);
    w32(&w, time_created);

    e->defined |= (uint8_t)(1U << LOC_FILE_ID);

    return done(e, &w, true);
}

size_t fit_enc_timer(struct fit_enc *e, uint8_t *buf, size_t cap, uint32_t time, bool start)
{
    if ((e == NULL) || !e->started || (buf == NULL)) {
        return 0U;
    }

    struct wr w = {.buf = buf, .cap = cap, .at = 0U, .over = false};

    if ((e->defined & (uint8_t)(1U << LOC_EVENT)) == 0U) {
        wdef(&w, LOC_EVENT, MSG_EVENT, 3U);
        wfield(&w, 253U, 4U, BT_UINT32);    /* timestamp */
        wfield(&w, 0U, 1U, BT_ENUM);        /* event */
        wfield(&w, 1U, 1U, BT_ENUM);        /* event_type */
        e->defined |= (uint8_t)(1U << LOC_EVENT);
    }

    w8(&w, LOC_EVENT);
    w32(&w, time);
    w8(&w, 0U);                             /* event 0: timer */
    w8(&w, start ? 0U : 4U);                /* 0 start, 4 stop_all */

    return done(e, &w, true);
}

size_t fit_enc_record(struct fit_enc *e, uint8_t *buf, size_t cap, const struct fit_record *r)
{
    if ((e == NULL) || !e->started || (buf == NULL) || (r == NULL)) {
        return 0U;
    }

    struct wr w = {.buf = buf, .cap = cap, .at = 0U, .over = false};

    if ((e->defined & (uint8_t)(1U << LOC_RECORD)) == 0U) {
        wdef(&w, LOC_RECORD, MSG_RECORD, 10U);
        wfield(&w, 253U, 4U, BT_UINT32);    /* timestamp */
        wfield(&w, 0U, 4U, BT_SINT32);      /* position_lat */
        wfield(&w, 1U, 4U, BT_SINT32);      /* position_long */
        wfield(&w, 2U, 2U, BT_UINT16);      /* altitude */
        wfield(&w, 3U, 1U, BT_UINT8);       /* heart_rate */
        wfield(&w, 4U, 1U, BT_UINT8);       /* cadence */
        wfield(&w, 5U, 4U, BT_UINT32);      /* distance */
        wfield(&w, 6U, 2U, BT_UINT16);      /* speed */
        wfield(&w, 7U, 2U, BT_UINT16);      /* power */
        wfield(&w, 13U, 1U, BT_SINT8);      /* temperature */
        e->defined |= (uint8_t)(1U << LOC_RECORD);
    }

    /* altitude: scale 5, offset 500, so -500 m is raw zero */
    uint16_t alt = (uint16_t)scaled(r->alt_m, 5.0f, 500.0f, 65534U, FIT_INVALID_U16);
    /* speed in mm/s; the format keeps m/s with scale 1000 */
    uint16_t speed = (uint16_t)scaled(r->speed_kmh / 3.6f, 1000.0f, 0.0f, 65534U, FIT_INVALID_U16);
    uint32_t dist = scaled(r->dist_m, 100.0f, 0.0f, 0xFFFFFFFEUL, FIT_INVALID_U32);
    uint16_t power = (r->power_w > 0) ? (uint16_t)r->power_w : 0U;

    w8(&w, LOC_RECORD);
    w32(&w, r->time);
    w32(&w, (uint32_t)r->lat_semi);
    w32(&w, (uint32_t)r->lon_semi);
    w16(&w, alt);
    w8(&w, (r->hr_bpm != 0U) ? r->hr_bpm : FIT_INVALID_U8);
    w8(&w, (r->cadence_rpm != 0U) ? r->cadence_rpm : FIT_INVALID_U8);
    w32(&w, dist);
    w16(&w, speed);
    w16(&w, power);
    w8(&w, (uint8_t)r->temp_c);

    return done(e, &w, true);
}

/** Fields that the lap and the session share, in the order of the format */
static void totals_common(struct wr *w, const struct fit_totals *t)
{
    w32(w, t->end_time);                /* timestamp */
    w32(w, t->start_time);
    w32(w, t->elapsed_ms);
    w32(w, t->timer_ms);
    w32(w, scaled(t->dist_m, 100.0f, 0.0f, 0xFFFFFFFEUL, FIT_INVALID_U32));
    w16(w, t->calories_kcal);
    w16(w, (uint16_t)scaled(t->avg_speed_kmh / 3.6f, 1000.0f, 0.0f, 65534U, FIT_INVALID_U16));
    w16(w, (uint16_t)scaled(t->max_speed_kmh / 3.6f, 1000.0f, 0.0f, 65534U, FIT_INVALID_U16));
    w8(w, (t->avg_hr_bpm != 0U) ? t->avg_hr_bpm : FIT_INVALID_U8);
    w8(w, (t->max_hr_bpm != 0U) ? t->max_hr_bpm : FIT_INVALID_U8);
    w8(w, (t->avg_cadence_rpm != 0U) ? t->avg_cadence_rpm : FIT_INVALID_U8);
    w16(w, t->avg_power_w);
    w16(w, t->max_power_w);
    w16(w, (uint16_t)scaled(t->ascent_m, 1.0f, 0.0f, 65534U, FIT_INVALID_U16));
    w16(w, (uint16_t)scaled(t->descent_m, 1.0f, 0.0f, 65534U, FIT_INVALID_U16));
}

size_t fit_enc_lap(struct fit_enc *e, uint8_t *buf, size_t cap, const struct fit_totals *t)
{
    if ((e == NULL) || !e->started || (buf == NULL) || (t == NULL)) {
        return 0U;
    }

    struct wr w = {.buf = buf, .cap = cap, .at = 0U, .over = false};

    if ((e->defined & (uint8_t)(1U << LOC_LAP)) == 0U) {
        wdef(&w, LOC_LAP, MSG_LAP, 20U);
        wfield(&w, 254U, 2U, BT_UINT16);    /* message_index */
        wfield(&w, 253U, 4U, BT_UINT32);    /* timestamp */
        wfield(&w, 2U, 4U, BT_UINT32);      /* start_time */
        wfield(&w, 7U, 4U, BT_UINT32);      /* total_elapsed_time */
        wfield(&w, 8U, 4U, BT_UINT32);      /* total_timer_time */
        wfield(&w, 9U, 4U, BT_UINT32);      /* total_distance */
        wfield(&w, 11U, 2U, BT_UINT16);     /* total_calories */
        wfield(&w, 13U, 2U, BT_UINT16);     /* avg_speed */
        wfield(&w, 14U, 2U, BT_UINT16);     /* max_speed */
        wfield(&w, 15U, 1U, BT_UINT8);      /* avg_heart_rate */
        wfield(&w, 16U, 1U, BT_UINT8);      /* max_heart_rate */
        wfield(&w, 17U, 1U, BT_UINT8);      /* avg_cadence */
        wfield(&w, 19U, 2U, BT_UINT16);     /* avg_power */
        wfield(&w, 20U, 2U, BT_UINT16);     /* max_power */
        wfield(&w, 21U, 2U, BT_UINT16);     /* total_ascent */
        wfield(&w, 22U, 2U, BT_UINT16);     /* total_descent */
        wfield(&w, 0U, 1U, BT_ENUM);        /* event */
        wfield(&w, 1U, 1U, BT_ENUM);        /* event_type */
        wfield(&w, 24U, 1U, BT_ENUM);       /* lap_trigger */
        wfield(&w, 25U, 1U, BT_ENUM);       /* sport */
        e->defined |= (uint8_t)(1U << LOC_LAP);
    }

    w8(&w, LOC_LAP);
    w16(&w, e->laps);
    totals_common(&w, t);
    w8(&w, 9U);                             /* event 9: lap */
    w8(&w, 1U);                             /* event_type 1: stop */
    w8(&w, 0U);                             /* lap_trigger 0: manual */
    w8(&w, 2U);                             /* sport 2: cycling */

    size_t n = done(e, &w, true);

    if (n > 0U) {
        e->laps++;
    }

    return n;
}

size_t fit_enc_session(struct fit_enc *e, uint8_t *buf, size_t cap, const struct fit_totals *t)
{
    if ((e == NULL) || !e->started || (buf == NULL) || (t == NULL)) {
        return 0U;
    }

    struct wr w = {.buf = buf, .cap = cap, .at = 0U, .over = false};

    if ((e->defined & (uint8_t)(1U << LOC_SESSION)) == 0U) {
        wdef(&w, LOC_SESSION, MSG_SESSION, 21U);
        wfield(&w, 254U, 2U, BT_UINT16);    /* message_index */
        wfield(&w, 253U, 4U, BT_UINT32);    /* timestamp */
        wfield(&w, 2U, 4U, BT_UINT32);      /* start_time */
        wfield(&w, 7U, 4U, BT_UINT32);      /* total_elapsed_time */
        wfield(&w, 8U, 4U, BT_UINT32);      /* total_timer_time */
        wfield(&w, 9U, 4U, BT_UINT32);      /* total_distance */
        wfield(&w, 11U, 2U, BT_UINT16);     /* total_calories */
        wfield(&w, 14U, 2U, BT_UINT16);     /* avg_speed */
        wfield(&w, 15U, 2U, BT_UINT16);     /* max_speed */
        wfield(&w, 16U, 1U, BT_UINT8);      /* avg_heart_rate */
        wfield(&w, 17U, 1U, BT_UINT8);      /* max_heart_rate */
        wfield(&w, 18U, 1U, BT_UINT8);      /* avg_cadence */
        wfield(&w, 20U, 2U, BT_UINT16);     /* avg_power */
        wfield(&w, 21U, 2U, BT_UINT16);     /* max_power */
        wfield(&w, 22U, 2U, BT_UINT16);     /* total_ascent */
        wfield(&w, 23U, 2U, BT_UINT16);     /* total_descent */
        wfield(&w, 25U, 2U, BT_UINT16);     /* first_lap_index */
        wfield(&w, 26U, 2U, BT_UINT16);     /* num_laps */
        wfield(&w, 0U, 1U, BT_ENUM);        /* event */
        wfield(&w, 1U, 1U, BT_ENUM);        /* event_type */
        wfield(&w, 5U, 1U, BT_ENUM);        /* sport */
        e->defined |= (uint8_t)(1U << LOC_SESSION);
    }

    w8(&w, LOC_SESSION);
    w16(&w, 0U);                            /* the ride is one session */
    totals_common(&w, t);
    w16(&w, 0U);                            /* first_lap_index */
    w16(&w, (e->laps != 0U) ? e->laps : 1U);
    w8(&w, 8U);                             /* event 8: session */
    w8(&w, 1U);                             /* event_type 1: stop */
    w8(&w, 2U);                             /* sport 2: cycling */

    return done(e, &w, true);
}

size_t fit_enc_end(struct fit_enc *e, uint8_t *buf, size_t cap, const struct fit_totals *t)
{
    if ((e == NULL) || !e->started || (buf == NULL) || (t == NULL)) {
        return 0U;
    }

    struct wr w = {.buf = buf, .cap = cap, .at = 0U, .over = false};

    if ((e->defined & (uint8_t)(1U << LOC_ACTIVITY)) == 0U) {
        wdef(&w, LOC_ACTIVITY, MSG_ACTIVITY, 6U);
        wfield(&w, 253U, 4U, BT_UINT32);    /* timestamp */
        wfield(&w, 0U, 4U, BT_UINT32);      /* total_timer_time */
        wfield(&w, 1U, 2U, BT_UINT16);      /* num_sessions */
        wfield(&w, 2U, 1U, BT_ENUM);        /* type */
        wfield(&w, 3U, 1U, BT_ENUM);        /* event */
        wfield(&w, 4U, 1U, BT_ENUM);        /* event_type */
        e->defined |= (uint8_t)(1U << LOC_ACTIVITY);
    }

    w8(&w, LOC_ACTIVITY);
    w32(&w, t->end_time);
    w32(&w, t->timer_ms);
    w16(&w, 1U);                            /* one session per file */
    w8(&w, 0U);                             /* type 0: manual */
    w8(&w, 26U);                            /* event 26: activity */
    w8(&w, 1U);                             /* event_type 1: stop */

    size_t n = done(e, &w, true);

    if (n == 0U) {
        return 0U;
    }

    /*
     * The file CRC covers the header, and the header the caller wrote at
     * the start carried a zero size instead of the real one. It does not
     * matter: the last two bytes of a header are the CRC of the twelve
     * before them, and feeding a message followed by its own CRC leaves
     * this CRC at zero. So the running state after **any** valid header is
     * zero, and the size written there never reaches the file CRC.
     * `test_fit_encode` checks the whole thing against the plain
     * calculation over the finished bytes, for thirty-three file lengths.
     */
    uint16_t file_crc = e->crc;

    if ((w.at + 2U) > cap) {
        return 0U;
    }
    buf[w.at] = (uint8_t)(file_crc & 0xFFU);
    buf[w.at + 1U] = (uint8_t)((file_crc >> 8) & 0xFFU);
    e->started = false;

    return w.at + 2U;
}

size_t fit_enc_header(const struct fit_enc *e, uint8_t *buf, size_t cap)
{
    if ((e == NULL) || (buf == NULL) || (cap < FIT_HEADER_LEN)) {
        return 0U;
    }

    build_header(buf, e->data_size);

    return FIT_HEADER_LEN;
}
