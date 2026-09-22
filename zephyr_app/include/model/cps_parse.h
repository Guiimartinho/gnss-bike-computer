/**
 * @file cps_parse.h
 * @brief The Cycling Power Measurement of a power meter, read field by field
 *
 * A power meter — a crank, a pedal pair, a hub, a spider — speaks the
 * Bluetooth Cycling Power Service (0x1818) and sends everything in one
 * notification, Cycling Power Measurement (0x2A63), with a bitfield at the
 * front saying which fields are there. The instantaneous power is the only
 * one always present; every other field is optional, they come in a fixed
 * order and they have different widths, so reading the one you want means
 * walking past the ones you do not.
 *
 * It is the same shape of problem as the trainer's Indoor Bike Data
 * (`model/ftms_parse.h`), and the same reason for a module of its own: the
 * walk is the whole of it, and the host tests can build a payload with any
 * combination of flags and check what comes out.
 *
 * | Bit | Field | Bytes | Unit |
 * |---|---|---|---|
 * | — | Instantaneous power | 2 | watts, signed, **always there** |
 * | 0 | Pedal power balance | 1 | half a percent |
 * | 1 | Balance reference | — | 0 unknown, 1 the value is the left leg |
 * | 2 | Accumulated torque | 2 | 1/32 N·m |
 * | 3 | Torque source | — | 0 from the wheel, 1 from the crank |
 * | 4 | Wheel revolution data | 6 | uint32 turns + uint16 time, **1/2048 s** |
 * | 5 | Crank revolution data | 4 | uint16 turns + uint16 time, 1/1024 s |
 * | 6 | Extreme force magnitudes | 4 | newtons, max then min |
 * | 7 | Extreme torque magnitudes | 4 | 1/32 N·m, max then min |
 * | 8 | Extreme angles | 3 | two 12-bit degrees packed together |
 * | 9 | Top dead spot angle | 2 | degrees |
 * | 10 | Bottom dead spot angle | 2 | degrees |
 * | 11 | Accumulated energy | 2 | kilojoules |
 * | 12 | Offset compensation needed | — | the meter wants a zero offset |
 *
 * ## The trap
 *
 * The wheel event time here ticks at **1/2048 s**, while the crank event
 * time and both times of the Cycling Speed and Cadence service tick at
 * 1/1024 s. A reader that assumes one rate for both reports twice or half
 * the speed it should. `CPS_WHEEL_TICKS_PER_S` and `CPS_CRANK_TICKS_PER_S`
 * are separate constants for that reason, and there is a test for it.
 *
 * ## What is not verified
 *
 * The order and the widths above are from the Cycling Power Service
 * specification as this port understands it. **No power meter has been on
 * a bench here**, so a first session with a real one should confirm the
 * walk, most easily by checking that a meter which sends crank data
 * reports a plausible cadence: a wrong offset earlier in the table shows
 * up there immediately.
 */

#ifndef MODEL_CPS_PARSE_H
#define MODEL_CPS_PARSE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flags of the Cycling Power Measurement, in wire order */
#define CPS_F_BALANCE           0x0001U
#define CPS_F_BALANCE_LEFT      0x0002U /**< the balance is the left leg */
#define CPS_F_TORQUE            0x0004U
#define CPS_F_TORQUE_CRANK      0x0008U /**< the torque comes from the crank */
#define CPS_F_WHEEL_REV         0x0010U
#define CPS_F_CRANK_REV         0x0020U
#define CPS_F_EXTREME_FORCE     0x0040U
#define CPS_F_EXTREME_TORQUE    0x0080U
#define CPS_F_EXTREME_ANGLES    0x0100U
#define CPS_F_TOP_DEAD_SPOT     0x0200U
#define CPS_F_BOTTOM_DEAD_SPOT  0x0400U
#define CPS_F_ENERGY            0x0800U
#define CPS_F_OFFSET_NEEDED     0x1000U

/** The wheel event time of this service ticks twice as fast as the crank's */
#define CPS_WHEEL_TICKS_PER_S   2048U
#define CPS_CRANK_TICKS_PER_S   1024U

/** What one notification carried */
struct cps_measurement {
    int16_t power_w;            /**< signed: coasting downhill can be negative */
    uint8_t balance_pct;        /**< whole percent of the leg named below */
    bool balance_is_left;       /**< false means the meter did not say */
    uint16_t torque_32nm;       /**< accumulated, 1/32 N·m, wraps */
    uint32_t wheel_revs;        /**< cumulative, wraps */
    uint16_t wheel_time;        /**< 1/2048 s, wraps */
    uint16_t crank_revs;        /**< cumulative, wraps */
    uint16_t crank_time;        /**< 1/1024 s, wraps */
    uint16_t energy_kj;         /**< accumulated since the meter woke up */
    bool have_balance;
    bool have_torque;
    bool torque_from_crank;
    bool have_wheel;
    bool have_crank;
    bool have_energy;
    bool offset_needed;         /**< the meter is asking for a zero offset */
};

/**
 * @brief Read one Cycling Power Measurement notification.
 *
 * @param data payload, flags first
 * @param len its length
 * @param out filled; everything it does not find stays zero, with its
 * `have_` flag false
 * @return false when the payload cannot hold the flags and the power, or
 * when a field the flags promise runs past the end — a notification that
 * lies about itself is dropped whole rather than read from the wrong bytes
 */
bool cps_parse_measurement(const uint8_t *data, uint16_t len, struct cps_measurement *out);

/**
 * @brief Cadence in turns per minute from two notifications.
 *
 * The crank counter and its clock both wrap; unsigned arithmetic gives the
 * right difference as long as less than one full turn of the counter went
 * by. The first call has nothing to compare against and answers 0.
 *
 * @param prev the previous notification, or NULL for the first one
 * @return turns per minute, or 0 while coasting or on the first sample
 */
uint8_t cps_cadence_rpm(const struct cps_measurement *prev,
                        const struct cps_measurement *now);

/**
 * @brief Speed in hundredths of a km/h from two notifications.
 *
 * @param wheel_mm circumference of the wheel
 * @return 0,01 km/h, or 0 while stopped or on the first sample
 */
uint16_t cps_speed_kmh100(const struct cps_measurement *prev,
                          const struct cps_measurement *now,
                          uint16_t wheel_mm);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_CPS_PARSE_H */
