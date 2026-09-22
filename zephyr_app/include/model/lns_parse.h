/**
 * @file lns_parse.h
 * @brief Where the phone says it is, read field by field
 *
 * A phone running a navigation application offers the Bluetooth **Location
 * and Navigation Service** (0x1819) and notifies its Location and Speed
 * characteristic (0x2A67). That is a position the device can use when its
 * own receiver has nothing: indoors at the start of a ride, under trees,
 * in a city of tall buildings.
 *
 * The port had the machinery for it and no way to fill it:
 * `model/loc_arbiter.c` has a whole branch for a position from the phone,
 * with its own tests, and **nothing ever fed it**. What existed was
 * `rf/ble_lns.c`, which is the other direction — the device telling a
 * phone where *it* is.
 *
 * The characteristic has the same shape as the trainer's Indoor Bike Data
 * and the power meter's measurement: a bitfield at the front, then the
 * fields that are present, in a fixed order and of different widths. The
 * walk is the whole of it, so it lives here with the host tests.
 *
 * | Bit | Field | Bytes | Unit |
 * |---|---|---|---|
 * | 0 | Instantaneous speed | 2 | 1/100 m/s |
 * | 1 | Total distance | 3 | 1/10 m |
 * | 2 | Location | 8 | two sint32, 1e-7 degrees |
 * | 3 | Elevation | 3 | sint24, 1/100 m |
 * | 4 | Heading | 2 | 1/100 degrees |
 * | 5 | Rolling time | 1 | seconds |
 * | 6 | UTC time | 7 | year, month, day, hour, minute, second |
 * | 7-8 | Position status | — | 0 no position, 1 position ok, 2 estimated, 3 last known |
 * | 9 | Speed and distance format | — | 0 in two dimensions, 1 in three |
 * | 10-11 | Elevation source | — | not used here |
 * | 12 | Heading source | — | not used here |
 *
 * ## The one that matters
 *
 * **Position status.** The two bits say whether the coordinates mean
 * anything: a phone that has not got a fix still sends the field, with the
 * status at "no position". A reader that takes the coordinates without
 * looking puts the rider wherever the phone last thought it was, or at
 * zero. Only `LNS_POS_OK` is accepted here, and there is a test for each
 * of the other three.
 *
 * ## Not verified
 *
 * The order and the widths are the specification as this port understands
 * it. **No phone has been on a bench here.**
 */

#ifndef MODEL_LNS_PARSE_H
#define MODEL_LNS_PARSE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flags of the Location and Speed characteristic, in wire order */
#define LNS_F_SPEED             0x0001U
#define LNS_F_DISTANCE          0x0002U
#define LNS_F_LOCATION          0x0004U
#define LNS_F_ELEVATION         0x0008U
#define LNS_F_HEADING           0x0010U
#define LNS_F_ROLLING_TIME      0x0020U
#define LNS_F_UTC_TIME          0x0040U
#define LNS_F_POS_STATUS_MASK   0x0180U     /**< bits 7 and 8 */
#define LNS_F_POS_STATUS_SHIFT  7U
#define LNS_F_3D_FORMAT         0x0200U

/** What the two status bits mean */
enum lns_pos_status {
    LNS_POS_NONE = 0,       /**< the phone has no position */
    LNS_POS_OK,             /**< it has one */
    LNS_POS_ESTIMATED,      /**< dead reckoning, not a fix */
    LNS_POS_LAST_KNOWN      /**< where it was, not where it is */
};

/** What one notification carried */
struct lns_location {
    int32_t lat_e7;
    int32_t lon_e7;
    int32_t elevation_cm;
    uint16_t speed_cms;         /**< 1/100 m/s */
    uint16_t heading_cdeg;      /**< 1/100 degrees */
    uint8_t status;             /**< enum lns_pos_status */
    bool have_location;
    bool have_speed;
    bool have_elevation;
    bool have_heading;
};

/**
 * @brief Read one Location and Speed notification.
 *
 * @return false when the payload is too short for the flags, or when a
 * field the flags promise runs past the end — a notification that lies
 * about itself is dropped whole rather than read from the wrong bytes
 */
bool lns_parse_location(const uint8_t *data, uint16_t len, struct lns_location *out);

/**
 * @brief Whether the position of a reading may be used
 *
 * True only when the coordinates are there **and** the phone says it has a
 * real position. An estimated or a last-known one is refused: putting a
 * rider where the phone last thought it was is worse than showing nothing.
 */
bool lns_position_is_usable(const struct lns_location *loc);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_LNS_PARSE_H */
