/**
 * @file ublox_m10.h
 * @brief What the u-blox M10 driver offers beyond the Zephyr GNSS API
 *
 * The receiver of the new board is a u-blox MAX-M10N-10B on a UART
 * (docs/14-hardware-placa-nova.md, GNSS). The Zephyr GNSS API gives the fix,
 * the satellites, the fix rate and the dynamic model; the power states of the
 * GNSS service (docs/16-arquitetura-firmware.md, GNSS) need what is here.
 */

#ifndef DRIVERS_GNSS_UBLOX_M10_H
#define DRIVERS_GNSS_UBLOX_M10_H

#include <stdint.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Power mode of the receiver, CFG-PM-OPERATEMODE */
enum ublox_m10_power_mode {
    UBLOX_M10_POWER_FULL = 0,   /**< full power: acquisition and tracking */
    UBLOX_M10_POWER_LEAP = 2    /**< low energy accurate positioning */
};

/** Restart of the receiver, UBX-CFG-RST */
enum ublox_m10_restart {
    UBLOX_M10_RESTART_HOT = 0,  /**< keeps everything */
    UBLOX_M10_RESTART_WARM,     /**< drops the ephemerides */
    UBLOX_M10_RESTART_COLD      /**< drops everything, as PMTK_COLD in the legacy */
};

/**
 * Apply the whole configuration of the devicetree: protocols of the UART,
 * NMEA off, UBX-NAV-PVT (and UBX-NAV-SAT) on, rate, dynamic model, time pulse
 * and power mode. The values go to the RAM and BBR layers.
 *
 * Idempotent: the GNSS service calls it at start-up and after every wake-up,
 * because the software standby clears the RAM of the receiver.
 *
 * @retval 0 on success
 * @retval -EIO if the receiver did not acknowledge a message
 */
int ublox_m10_configure(const struct device *dev);

/** Change the power mode (LEAP or full power) */
int ublox_m10_set_power_mode(const struct device *dev, enum ublox_m10_power_mode mode);

/** Power mode the driver last applied */
enum ublox_m10_power_mode ublox_m10_get_power_mode(const struct device *dev);

/**
 * psmState of the last UBX-NAV-PVT: 0 power save off, 2 acquisition,
 * 3 tracking, 4 power optimized tracking, 5 inactive (interface description
 * 3.15.11). Without a message yet, 0.
 */
uint8_t ublox_m10_psm_state(const struct device *dev);

/**
 * Put the receiver in software standby (UBX-RXM-PMREQ with backup and force),
 * waking on the UART RX line. If the node has vcc-supply, the rail goes down
 * after the message, which leaves the receiver in hardware backup on V_BCKP.
 *
 * The receiver stops answering: ublox_m10_wake() brings it back.
 */
int ublox_m10_standby(const struct device *dev);

/**
 * Wake the receiver from the software standby: turns the rail back on when
 * there is one, sends a byte that the receiver consumes as the wake-up edge
 * and waits for it to boot. The caller configures it again.
 */
int ublox_m10_wake(const struct device *dev);

/** true between ublox_m10_standby() and ublox_m10_wake() */
bool ublox_m10_is_standby(const struct device *dev);

/** Restart the receiver (UBX-CFG-RST); the receiver does not acknowledge it */
int ublox_m10_restart(const struct device *dev, enum ublox_m10_restart kind);

/**
 * Pull RESET_N for the 1 ms the receiver asks for (integration manual 4.2)
 * and wait for it to boot. This clears the backup memory, so the next fix
 * comes from a cold start: only for a receiver that stopped answering.
 *
 * @retval -ENOTSUP when the node has no reset-gpios
 */
int ublox_m10_hw_reset(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_GNSS_UBLOX_M10_H */
