/**
 * @file route_file.h
 * @brief The route file of this project (`.RTE`), read and checked
 *
 * The legacy keeps a route as text, `lat lon alt` with a line per point
 * (the `.PAR` files of `tools/TDD/DB`). That costs 31 bytes a point, says nothing about
 * the route itself and cannot tell a file cut in half from a whole one —
 * and a file that arrives over Bluetooth can be cut in half.
 *
 * The `.RTE` of this project is binary, little endian, with a header that
 * carries what the menu needs before anything else is read, a CRC-32 over
 * the body, and room for the turns of a cue sheet. A point takes ten bytes
 * instead of thirty-one, so a hundred kilometres with a point every ten
 * metres is 100 KB instead of 300 KB.
 *
 * ```text
 * header, 64 bytes
 *   0  char     magic[4]      "RTE1"
 *   4  uint16   version       1
 *   6  uint16   flags         bit 0: the file carries a cue sheet
 *   8  uint32   points
 *  12  uint32   cues
 *  16  uint32   distance_m    of the whole route
 *  20  uint32   climb_m       only the rises, as the legacy counts
 *  24  int32    lat_min_e7    the box of the route, in 1e-7 degree
 *  28  int32    lat_max_e7
 *  32  int32    lon_min_e7
 *  36  int32    lon_max_e7
 *  40  char     name[20]      UTF-8, padded with zeros
 *  60  uint32   crc32         IEEE, over everything after the header
 *
 * point, 10 bytes: int32 lat_e7, int32 lon_e7, int16 alt_m
 * cue, 28 bytes:   uint32 point, uint8 turn, uint8 reserved, char street[22]
 * ```
 *
 * Pure C: this module only reads and checks bytes. Whoever has them, from
 * a file or from the radio, is the caller.
 */

#ifndef MODEL_ROUTE_FILE_H
#define MODEL_ROUTE_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bytes of the header, fixed */
#define ROUTE_FILE_HEADER_SIZE  64U
/** Bytes of one point */
#define ROUTE_FILE_POINT_SIZE   10U
/** Bytes of one cue */
#define ROUTE_FILE_CUE_SIZE     28U
/** Longest name kept in the header */
#define ROUTE_FILE_NAME_MAX     20U
/** Longest street name of a cue */
#define ROUTE_FILE_STREET_MAX   22U
/** Version this firmware writes and reads */
#define ROUTE_FILE_VERSION      1U

/** The file carries a cue sheet after the points */
#define ROUTE_FILE_FLAG_CUES    0x0001U

/** Turns of a cue sheet, the ones the screen can draw (`ui_turn_t`) */
enum route_turn {
    ROUTE_TURN_STRAIGHT = 0,
    ROUTE_TURN_LEFT,
    ROUTE_TURN_RIGHT,
    ROUTE_TURN_SHARP_LEFT,
    ROUTE_TURN_SHARP_RIGHT,
    ROUTE_TURN_EASY_LEFT,
    ROUTE_TURN_EASY_RIGHT,
    ROUTE_TURN_ROUNDABOUT,
    ROUTE_TURN_UTURN,
    ROUTE_TURN_ARRIVE,
    ROUTE_TURN_KINDS,
};

/** What the header of a route file says */
struct route_header {
    uint16_t version;
    uint16_t flags;
    uint32_t points;
    uint32_t cues;
    uint32_t distance_m;
    uint32_t climb_m;
    int32_t lat_min_e7;
    int32_t lat_max_e7;
    int32_t lon_min_e7;
    int32_t lon_max_e7;
    char name[ROUTE_FILE_NAME_MAX + 1U];
    uint32_t crc32;
};

/** One point of the route, as the file keeps it */
struct route_point {
    int32_t lat_e7;
    int32_t lon_e7;
    int16_t alt_m;
};

/** One turn of the cue sheet */
struct route_cue {
    uint32_t point;                         /**< which point of the route */
    uint8_t turn;                           /**< enum route_turn */
    char street[ROUTE_FILE_STREET_MAX + 1U];
};

/**
 * @brief Whether these bytes begin a route file of this project
 *
 * Cheap enough to run on the first bytes of any file, which is how the
 * loader tells a `.RTE` from the text of the legacy.
 */
bool route_file_is_rte(const uint8_t *bytes, size_t len);

/**
 * @brief Read the header
 *
 * Checks the magic, the version and that the sizes fit in @p file_size.
 *
 * @param out Where to write it
 * @param bytes At least ROUTE_FILE_HEADER_SIZE bytes
 * @param len Bytes available
 * @param file_size Whole size of the file, to check the counts against
 * @return true when the header makes sense
 */
bool route_file_header(struct route_header *out, const uint8_t *bytes, size_t len,
                       size_t file_size);

/** @brief Read one point out of ROUTE_FILE_POINT_SIZE bytes */
bool route_file_point(struct route_point *out, const uint8_t *bytes, size_t len);

/** @brief Read one cue out of ROUTE_FILE_CUE_SIZE bytes */
bool route_file_cue(struct route_cue *out, const uint8_t *bytes, size_t len);

/**
 * @brief CRC-32 (IEEE) of a piece of the body, chained
 *
 * The caller starts with 0 and feeds the body in chunks, in order; the
 * answer of the last chunk is what the header must carry. It is the same
 * CRC of zlib and of `crc32_ieee()` of Zephyr, so the tool that writes the
 * file and the firmware that reads it agree.
 */
uint32_t route_file_crc32(uint32_t crc, const uint8_t *bytes, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_ROUTE_FILE_H */
