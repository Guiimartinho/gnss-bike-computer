/**
 * @file power_estimate.h
 * @brief Power the rider puts out, estimated as the legacy does
 *
 * `legacy/source/model/Attitude.cpp:556-575` (`Attitude::computePower`):
 *
 *     P = 1,025 x [9,81 x W x vit_asc + 0,004 x 9,81 x W x v + 0,204 x v^3]
 *
 * with W the weight of the rider alone (`USER_WEIGHT`, 79 kg by default,
 * `legacy/source/parameters.h:62`), `vit_asc` the vertical speed of the
 * Kalman filter in m/s and v the speed in m/s. The three terms are gravity,
 * rolling plus mechanical losses and air; the factor of 1,025 is the
 * transmission. The result may be negative going downhill: the legacy keeps
 * it in an `int16_t` (`SAtt.pwr`) and prints it as it is.
 *
 * The 0,204 of the air term is a CdA of about 0,333 m2 with rho = 1,225.
 */

#ifndef MODEL_POWER_ESTIMATE_H
#define MODEL_POWER_ESTIMATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Transmission efficiency of the legacy */
#define POWER_TRANSMISSION      1.025f
/** Rolling and mechanical coefficient */
#define POWER_ROLLING           0.004f
/** Air term, watts per (m/s)^3 */
#define POWER_AIR               0.204f
/** Gravity of the legacy */
#define POWER_GRAVITY           9.81f

/**
 * Estimate the power in watts.
 *
 * @param weight_kg weight of the rider, without the bike
 * @param speed_ms speed over the ground, in m/s
 * @param vit_asc_ms vertical speed, in m/s, from the altitude filter
 * @return power in watts, negative going down, saturated to int16_t
 */
int16_t power_estimate_w(float weight_kg, float speed_ms, float vit_asc_ms);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_POWER_ESTIMATE_H */
