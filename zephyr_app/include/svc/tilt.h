/**
 * @file tilt.h
 * @brief Pitch, roll, roughness and tilt-compensated heading
 *
 * Port of the attitude part of legacy/source/sensors/fxos.cpp: the
 * accelerometer is sampled at 50 Hz (fxos.cpp:600) and averaged over 50
 * samples, one second (MAX_ACCEL_AVG_COUNT, fxos.cpp:72); the roughness of
 * each axis is the mean absolute deviation over the same window
 * (fxos.cpp:761-766), in the legacy unit: counts of the FXOS8700 at +-4 g,
 * 2048 per g (fxos.cpp:484).
 *
 * Frame: the board frame is X forward, Y left, Z up. Pitch is nose up
 * positive, roll is right side down positive, and the heading runs
 * clockwise from magnetic north, with the equations of Freescale AN4248
 * (eCompass), fed in its north-east-down convention. How the sensors sit on
 * the board is the job of the caller (to confirm on the board).
 */

#ifndef SVC_TILT_H
#define SVC_TILT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Samples averaged per attitude, one second at 50 Hz (legacy MAX_ACCEL_AVG_COUNT) */
#define TILT_WINDOW         50U

/** Legacy roughness unit: FXOS8700 counts at +-4 g */
#define TILT_COUNTS_PER_G   2048.0f

/** Standard gravity, m/s2 */
#define TILT_G              9.80665f

struct tilt_window {
    float ax[TILT_WINDOW];      /**< m/s2, board frame */
    float ay[TILT_WINDOW];
    float az[TILT_WINDOW];
    uint8_t n;                  /**< samples held */
    uint8_t next;               /**< slot of the next sample */
};

struct tilt_out {
    float pitch_deg;
    float roll_deg;
    float rough[3];             /**< X, Y, Z, legacy counts */
};

/** Empty the window */
void tilt_window_reset(struct tilt_window *w);

/**
 * @brief Add one accelerometer sample (m/s2, board frame)
 * @return true when the window holds TILT_WINDOW samples
 */
bool tilt_window_add(struct tilt_window *w, float ax, float ay, float az);

/**
 * @brief Pitch and roll of the window mean, and the roughness
 * @return false if the window is empty
 */
bool tilt_compute(const struct tilt_window *w, struct tilt_out *out);

/**
 * @brief Tilt-compensated magnetic heading
 * @param pitch_deg Pitch from tilt_compute()
 * @param roll_deg Roll from tilt_compute()
 * @param mx Magnetic field, board frame, any unit
 * @return Heading in [0, 360)
 */
float tilt_heading_deg(float pitch_deg, float roll_deg, float mx, float my, float mz);

#ifdef __cplusplus
}
#endif

#endif /* SVC_TILT_H */
