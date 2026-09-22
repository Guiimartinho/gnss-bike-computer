/**
 * @file csc_calc.h
 * @brief Speed and cadence from the counters a CSC sensor sends
 *
 * A Cycling Speed and Cadence sensor does not send a speed. It sends how
 * many times the wheel has turned since it was switched on and the time of
 * the last turn, and the head unit works out the rest from the difference
 * between two notifications. Same for the crank.
 *
 * This is the arithmetic of that, on its own, because it is where the
 * mistakes live: the units run from millimetres and 1/1024 of a second to
 * hundredths of a kilometre per hour, both counters wrap around, and the
 * first notification has nothing to compare against.
 *
 * | Field | Unit the sensor uses | Where |
 * |---|---|---|
 * | Cumulative wheel revolutions | uint32, wraps | CSC Measurement |
 * | Last wheel event time | uint16, 1/1024 s, wraps | CSC Measurement |
 * | Cumulative crank revolutions | uint16, wraps | CSC Measurement |
 * | Last crank event time | uint16, 1/1024 s, wraps | CSC Measurement |
 *
 * The legacy reads speed and cadence from an ANT+ BSC sensor and does the
 * same sum (`legacy/source/ant/ant_bsc.c`); the numbers on the screen are
 * the same either way.
 */

#ifndef MODEL_CSC_CALC_H
#define MODEL_CSC_CALC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Ticks of the event time in one second, as the profile fixes it */
#define CSC_TICKS_PER_S     1024U

/** Wheel of a 700x25c, in millimetres, which is what the port assumes */
#define CSC_WHEEL_MM        2105U

/** Speed above which a reading is thrown away as impossible, 0,01 km/h */
#define CSC_SPEED_MAX       15000U   /* 150 km/h */

/** Cadence above which a reading is thrown away, rpm */
#define CSC_CADENCE_MAX     250U

/** What the sum needs to remember between two notifications */
struct csc_calc {
    uint32_t wheel_revs;
    uint16_t wheel_time;
    uint16_t crank_revs;
    uint16_t crank_time;
    uint16_t wheel_mm;      /**< circumference in use */
    bool have_wheel;        /**< a first wheel reading arrived */
    bool have_crank;
};

/** Start over; @p wheel_mm of 0 takes CSC_WHEEL_MM */
void csc_calc_init(struct csc_calc *c, uint16_t wheel_mm);

/** Change the wheel without losing the counters */
void csc_calc_set_wheel(struct csc_calc *c, uint16_t wheel_mm);

/**
 * Speed from a wheel reading, in hundredths of a kilometre per hour.
 *
 * The first reading has nothing to compare against and gives zero. A
 * reading with the same counters as the last one also gives zero, which is
 * a bike that is standing still: the sensor keeps sending.
 */
uint16_t csc_calc_speed(struct csc_calc *c, uint32_t wheel_revs, uint16_t wheel_time);

/** Cadence from a crank reading, in revolutions per minute */
uint8_t csc_calc_cadence(struct csc_calc *c, uint16_t crank_revs, uint16_t crank_time);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_CSC_CALC_H */
