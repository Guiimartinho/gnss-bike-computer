/**
 * @file route_file.c
 * @brief The route file of this project (`.RTE`), read and checked
 */

#include <string.h>

#include "model/route_file.h"

/** Little endian, byte by byte: no casting of pointers to wider types */
static uint16_t u16(const uint8_t *b)
{
    return (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}

static uint32_t u32(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

static int32_t i32(const uint8_t *b)
{
    return (int32_t)u32(b);
}

static int16_t i16(const uint8_t *b)
{
    return (int16_t)u16(b);
}

bool route_file_is_rte(const uint8_t *bytes, size_t len)
{
    return (bytes != NULL) && (len >= 4U) && (bytes[0] == 'R') && (bytes[1] == 'T') &&
           (bytes[2] == 'E') && (bytes[3] == '1');
}

bool route_file_header(struct route_header *out, const uint8_t *bytes, size_t len,
                       size_t file_size)
{
    if ((out == NULL) || !route_file_is_rte(bytes, len) || (len < ROUTE_FILE_HEADER_SIZE)) {
        return false;
    }

    (void)memset(out, 0, sizeof(*out));

    out->version = u16(&bytes[4]);
    if (out->version != ROUTE_FILE_VERSION) {
        return false;
    }

    out->flags = u16(&bytes[6]);
    out->points = u32(&bytes[8]);
    out->cues = u32(&bytes[12]);
    out->distance_m = u32(&bytes[16]);
    out->climb_m = u32(&bytes[20]);
    out->lat_min_e7 = i32(&bytes[24]);
    out->lat_max_e7 = i32(&bytes[28]);
    out->lon_min_e7 = i32(&bytes[32]);
    out->lon_max_e7 = i32(&bytes[36]);
    (void)memcpy(out->name, &bytes[40], ROUTE_FILE_NAME_MAX);
    out->name[ROUTE_FILE_NAME_MAX] = '\0';
    out->crc32 = u32(&bytes[60]);

    /* a route is at least two points, and the file has to hold what it says */
    if (out->points < 2U) {
        return false;
    }

    uint64_t body = ((uint64_t)out->points * ROUTE_FILE_POINT_SIZE) +
                    ((uint64_t)out->cues * ROUTE_FILE_CUE_SIZE);

    if ((body + ROUTE_FILE_HEADER_SIZE) > (uint64_t)file_size) {
        return false;
    }

    /* a cue sheet only exists when the flag says so */
    if ((out->cues > 0U) && ((out->flags & ROUTE_FILE_FLAG_CUES) == 0U)) {
        return false;
    }

    /* the box of the route has to be a box, and on the Earth */
    if ((out->lat_min_e7 > out->lat_max_e7) || (out->lon_min_e7 > out->lon_max_e7) ||
        (out->lat_min_e7 < -900000000) || (out->lat_max_e7 > 900000000) ||
        (out->lon_min_e7 < -1800000000) || (out->lon_max_e7 > 1800000000)) {
        return false;
    }

    return true;
}

bool route_file_point(struct route_point *out, const uint8_t *bytes, size_t len)
{
    if ((out == NULL) || (bytes == NULL) || (len < ROUTE_FILE_POINT_SIZE)) {
        return false;
    }

    out->lat_e7 = i32(&bytes[0]);
    out->lon_e7 = i32(&bytes[4]);
    out->alt_m = i16(&bytes[8]);

    return true;
}

bool route_file_cue(struct route_cue *out, const uint8_t *bytes, size_t len)
{
    if ((out == NULL) || (bytes == NULL) || (len < ROUTE_FILE_CUE_SIZE)) {
        return false;
    }

    (void)memset(out, 0, sizeof(*out));
    out->point = u32(&bytes[0]);
    out->turn = bytes[4];
    if (out->turn >= (uint8_t)ROUTE_TURN_KINDS) {
        out->turn = (uint8_t)ROUTE_TURN_STRAIGHT;
    }
    (void)memcpy(out->street, &bytes[6], ROUTE_FILE_STREET_MAX);
    out->street[ROUTE_FILE_STREET_MAX] = '\0';

    return true;
}

uint32_t route_file_crc32(uint32_t crc, const uint8_t *bytes, size_t len)
{
    /*
     * CRC-32 IEEE, the one of zlib and of `crc32_ieee()` of Zephyr, bit by
     * bit so that no table takes flash: a route of a hundred kilometres is
     * 100 KB, and this runs once, when the file is opened.
     */
    if (bytes == NULL) {
        return crc;
    }

    crc = ~crc;
    for (size_t i = 0U; i < len; i++) {
        crc ^= bytes[i];
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
        }
    }

    return ~crc;
}
