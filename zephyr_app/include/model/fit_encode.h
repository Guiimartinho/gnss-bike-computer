/**
 * @file fit_encode.h
 * @brief Writes an activity in the Garmin FIT format, a block at a time
 *
 * The legacy writes `@DDMMYY.txt`, a semicolon file of its own that only
 * its own tools read (`legacy/source/sd/sd_functions.cpp:605`). Strava,
 * Garmin Connect, Komoot and everything else take **FIT**, so the device
 * writes one FIT per ride beside the text log.
 *
 * Everything here is plain C on a caller buffer: no Zephyr, no file, no
 * allocation, so the host tests check the bytes against the format. The
 * caller asks for one message at a time, writes the bytes it gets to the
 * file and calls again; a four-hour ride at 1 Hz is about 380 KB and never
 * fits in RAM.
 *
 * | Item | Where in the format |
 * |---|---|
 * | File header of 14 bytes, `.FIT` at offset 8 | FIT Protocol, file header |
 * | Record header: bit 6 tells a definition from a data message | normal header |
 * | Definition: reserved, architecture, global number, fields | definition message |
 * | Base types: `0x84` uint16, `0x85` sint32, `0x86` uint32 | base type field |
 * | CRC-16 with the 16-entry nibble table, over header and data | CRC |
 * | A message followed by its own CRC leaves the CRC at zero | CRC |
 * | `date_time` counts seconds from 1989-12-31 00:00 UTC | date_time type |
 * | Position in semicircles: degree x (2^31 / 180) | semicircles |
 *
 * The device announces itself as manufacturer 255 (development), which is
 * what the format reserves for hardware that is not a product.
 */

#ifndef MODEL_FIT_ENCODE_H
#define MODEL_FIT_ENCODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bytes of the file header the encoder writes */
#define FIT_HEADER_LEN          14U

/** Longest message the encoder builds, definition and data together */
#define FIT_BLOCK_MAX           160U

/** Seconds between the Unix epoch and the FIT epoch, 1989-12-31 00:00 UTC */
#define FIT_EPOCH_OFFSET        631065600UL

/** Manufacturer 255: "development", for hardware that is not a product */
#define FIT_MANUFACTURER_DEV    255U

/** What a field carries when there is no value (the format calls it invalid) */
#define FIT_INVALID_U8          0xFFU
#define FIT_INVALID_U16         0xFFFFU
#define FIT_INVALID_U32         0xFFFFFFFFUL
#define FIT_INVALID_S32         0x7FFFFFFFL

/** One point of the ride, a `record` message */
struct fit_record {
    uint32_t time;          /**< FIT date_time, seconds */
    int32_t lat_semi;       /**< semicircles, FIT_INVALID_S32 without a fix */
    int32_t lon_semi;       /**< semicircles */
    float alt_m;            /**< altitude above sea level */
    float dist_m;           /**< distance since the start of the ride */
    float speed_kmh;
    int16_t power_w;        /**< negative going down; the format takes no sign */
    uint8_t hr_bpm;         /**< 0 without a strap */
    uint8_t cadence_rpm;    /**< 0 without a sensor */
    int8_t temp_c;          /**< INT8_MIN without a sensor */
};

/** Totals of a lap or of the whole ride */
struct fit_totals {
    uint32_t start_time;    /**< FIT date_time of the first point */
    uint32_t end_time;      /**< FIT date_time of the last point */
    uint32_t elapsed_ms;    /**< wall time, pauses included */
    uint32_t timer_ms;      /**< moving time, pauses taken out */
    float dist_m;
    float ascent_m;
    float descent_m;
    float avg_speed_kmh;
    float max_speed_kmh;
    uint16_t avg_power_w;
    uint16_t max_power_w;
    uint16_t calories_kcal;
    uint8_t avg_hr_bpm;
    uint8_t max_hr_bpm;
    uint8_t avg_cadence_rpm;
};

/** State of one file being written */
struct fit_enc {
    uint16_t crc;           /**< over what went out so far, from byte 0 */
    uint32_t data_size;     /**< bytes after the header */
    uint16_t laps;          /**< laps already written */
    uint8_t defined;        /**< bitmap of the local types already defined */
    bool started;
};

/**
 * Start a file.
 *
 * Writes the 14-byte header with the size still zero: the size is only
 * known at the end, and fit_enc_header() rebuilds it then.
 *
 * @param buf output, at least FIT_HEADER_LEN bytes
 * @return bytes written, or 0 on a bad argument
 */
size_t fit_enc_begin(struct fit_enc *e, uint8_t *buf, size_t cap);

/**
 * The `file_id` message, which every FIT file opens with.
 *
 * @param serial serial number of the device, 0 for none
 * @param time_created FIT date_time when the ride started
 */
size_t fit_enc_file_id(struct fit_enc *e, uint8_t *buf, size_t cap, uint32_t serial,
                       uint32_t time_created);

/** A `event` message that starts or stops the timer */
size_t fit_enc_timer(struct fit_enc *e, uint8_t *buf, size_t cap, uint32_t time, bool start);

/** One point of the ride */
size_t fit_enc_record(struct fit_enc *e, uint8_t *buf, size_t cap, const struct fit_record *r);

/** One lap; the index comes from how many were written before */
size_t fit_enc_lap(struct fit_enc *e, uint8_t *buf, size_t cap, const struct fit_totals *t);

/** The `session` message, with the totals of the whole ride */
size_t fit_enc_session(struct fit_enc *e, uint8_t *buf, size_t cap, const struct fit_totals *t);

/**
 * The `activity` message and the two bytes of the file CRC, which close
 * the file. Nothing else may be written after this.
 */
size_t fit_enc_end(struct fit_enc *e, uint8_t *buf, size_t cap, const struct fit_totals *t);

/**
 * Rebuild the file header with the size the file ended up with.
 *
 * The caller seeks back to offset 0 and writes these FIT_HEADER_LEN bytes
 * over the ones fit_enc_begin() gave. Call it **after** fit_enc_end(),
 * which is what knows the size.
 *
 * Writing the header again does not invalidate the file CRC that
 * fit_enc_end() already produced. The last two bytes of a header are the
 * CRC of the twelve before them, and feeding a message followed by its own
 * CRC leaves this CRC at zero, so the state after **any** valid header is
 * zero and the size written there never reaches the file CRC. That is what
 * lets the encoder close a file of hundreds of kilobytes without reading a
 * byte of it back.
 *
 * @return FIT_HEADER_LEN, or 0 on a bad argument
 */
size_t fit_enc_header(const struct fit_enc *e, uint8_t *buf, size_t cap);

/** Seconds of the FIT epoch from a date `DDMMYY` and the seconds of the day */
uint32_t fit_time_from_date(uint32_t ddmmyy, uint32_t secj);

/** Degrees to semicircles, the position unit of the format */
int32_t fit_semicircles(float degrees);

/** CRC-16 of the format over @p len bytes, continuing from @p crc */
uint16_t fit_crc(uint16_t crc, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_FIT_ENCODE_H */
