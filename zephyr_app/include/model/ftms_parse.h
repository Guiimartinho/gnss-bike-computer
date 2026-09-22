/**
 * @file ftms_parse.h
 * @brief The Indoor Bike Data of a trainer, read field by field
 *
 * A trainer that speaks the Fitness Machine Service sends one
 * characteristic with everything in it, and a bitfield at the front says
 * which fields are there. Every field is optional, they come in a fixed
 * order, and they have different widths, so reading the one you want means
 * walking past the ones you do not. Get a bit wrong and every field after
 * it comes out of the wrong bytes.
 *
 * That is why this is its own module: the walk is the whole of it, and the
 * host tests can build a payload with any combination of flags and check
 * what comes out.
 *
 * | Bit | Field | Bytes | Note |
 * |---|---|---|---|
 * | 0 | More Data | — | **inverted**: instantaneous speed is there when it is **0** |
 * | 1 | Average speed | 2 | |
 * | 2 | Instantaneous cadence | 2 | in halves of a turn per minute |
 * | 3 | Average cadence | 2 | |
 * | 4 | Total distance | 3 | |
 * | 5 | Resistance level | 2 | |
 * | 6 | Instantaneous power | 2 | signed watts |
 * | 7 | Average power | 2 | |
 * | 8 | Expended energy | 4 | total, per hour, per minute |
 * | 9 | Heart rate | 1 | |
 * | 10 | Metabolic equivalent | 1 | |
 * | 11 | Elapsed time | 2 | seconds |
 * | 12 | Remaining time | 2 | seconds |
 *
 * The order above is the order on the wire, and it is the thing to keep
 * right: a reader walks it from the front, and the position of each field
 * depends on every flag before it.
 */

#ifndef MODEL_FTMS_PARSE_H
#define MODEL_FTMS_PARSE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flags of the Indoor Bike Data characteristic, in wire order */
#define FTMS_F_MORE_DATA        0x0001U     /**< 0 means the speed is present */
#define FTMS_F_AVG_SPEED        0x0002U
#define FTMS_F_CADENCE          0x0004U
#define FTMS_F_AVG_CADENCE      0x0008U
#define FTMS_F_TOTAL_DISTANCE   0x0010U
#define FTMS_F_RESISTANCE       0x0020U
#define FTMS_F_POWER            0x0040U
#define FTMS_F_AVG_POWER        0x0080U
#define FTMS_F_ENERGY           0x0100U
#define FTMS_F_HEART_RATE       0x0200U
#define FTMS_F_MET              0x0400U
#define FTMS_F_ELAPSED_TIME     0x0800U
#define FTMS_F_REMAINING_TIME   0x1000U

/** What one notification carried */
struct ftms_bike_data {
    uint16_t speed_kmh100;      /**< 0,01 km/h */
    uint16_t cadence_rpm;       /**< whole turns per minute */
    int16_t power_w;            /**< signed: a trainer can report a negative */
    uint16_t elapsed_s;
    uint8_t hr_bpm;
    bool have_speed;
    bool have_cadence;
    bool have_power;
    bool have_elapsed;
    bool have_hr;
};

/**
 * Read one Indoor Bike Data notification.
 *
 * @param data payload, flags first
 * @param len its length
 * @param out filled; everything it does not find stays zero, with its
 * `have_` flag false
 * @return false when the payload is too short to hold the flags, or when a
 * field the flags promise runs past the end — a notification that lies
 * about itself is dropped whole rather than read from the wrong bytes
 */
bool ftms_parse_bike_data(const uint8_t *data, uint16_t len, struct ftms_bike_data *out);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_FTMS_PARSE_H */
