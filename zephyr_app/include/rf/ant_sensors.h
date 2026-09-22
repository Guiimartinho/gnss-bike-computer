/**
 * @file ant_sensors.h
 * @brief The heart rate strap and the cadence sensor over ANT+
 *
 * The two the legacy actually rode with: `ant_hrm` and `ant_bsc` of the
 * stravaV10 (`legacy/rf/ant.c`). The port had neither — the ANT stack
 * started and no channel was ever opened — so a rider whose sensors are
 * ANT+, which is most riders with older kit, had nothing.
 *
 * The arrangement is the one the radar and the power meter already use
 * (`rf/radar_ant.h`, `rf/power_ant.h`): the channel is opened by
 * `rf/ant_channel.h`, the parameters come from Kconfig and are zero as
 * shipped, and the **page decoding** is a weak function defined in a file
 * `.gitignore` keeps out of the repository. The ANT+ device profiles are
 * under the ANT+ Shared Source License, the Adopter Agreement forbids
 * redistributing them, and this repository is public
 * ([07](../../docs/07-radio-ant-ble.md#decisão-ant-e-ble)).
 *
 * What the owner writes is two functions that turn eight bytes into a
 * reading. Everything else — the channel, the pairing, the publishing, the
 * screens — is here.
 *
 * A rider whose sensors speak Bluetooth needs none of this: those clients
 * are complete and work as shipped.
 */

#ifndef RF_ANT_SENSORS_H
#define RF_ANT_SENSORS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bytes of an ANT broadcast */
#define ANT_SENSOR_PAYLOAD  8U

/** What a heart rate page carried */
struct ant_hrm_reading {
    uint8_t bpm;
    uint16_t rr_ms;     /**< 0 when the page does not carry one */
    bool have_bpm;
    bool have_rr;
};

/** What a speed and cadence page carried */
struct ant_bsc_reading {
    uint16_t speed_kmh100;  /**< 0,01 km/h; 0 when the page has no wheel */
    uint8_t cadence_rpm;    /**< 0 when it has no crank */
    bool have_speed;
    bool have_cadence;
};

/**
 * Turn one broadcast of a heart rate strap into a reading.
 *
 * Defined in `src/rf/ant/sensor_pages.c`, which is not in this repository.
 * The weak definition that ships here returns false.
 */
bool ant_hrm_page(uint8_t page, const uint8_t *data, struct ant_hrm_reading *out);

/**
 * Turn one broadcast of a speed and cadence sensor into a reading.
 *
 * Same arrangement. The wheel circumference the rider set is passed in,
 * because a sensor counts turns and only the head unit knows the wheel.
 */
bool ant_bsc_page(uint8_t page, const uint8_t *data, uint16_t wheel_mm,
                  struct ant_bsc_reading *out);

/** Open the two channels; each answers -ENOTSUP while its Kconfig is zero */
int ant_sensors_start(void);

/** Close them */
void ant_sensors_stop(void);

/** The wheel the rider set, for the speed of a cadence sensor */
void ant_sensors_set_wheel(uint16_t circumference_mm);

#ifdef __cplusplus
}
#endif

#endif /* RF_ANT_SENSORS_H */
