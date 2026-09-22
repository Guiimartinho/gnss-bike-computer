/**
 * @file gnss_power.h
 * @brief Power state machine of the GNSS receiver
 *
 * The machine of docs/16-arquitetura-firmware.md (GNSS): backup while the
 * mode does not use the receiver, acquiring until the first fix, LEAP while
 * tracking and full power when the signal is weak. Pure logic, without
 * Zephyr, so the host tests cover it; the service applies the actions.
 *
 * The legacy did the same with what the MT3333 had: it woke the GPS in CRS
 * and PRC (`legacy/source/model/BoucleCRS.cpp:38`) and put it in standby in
 * FEC (`legacy/source/model/BoucleFEC.cpp:45`), by the GPS_S pin
 * (`legacy/source/sensors/GPSMGMT.cpp:195`). The u-blox has no standby pin:
 * the receiver goes down with UBX-RXM-PMREQ and the rail, and the power
 * modes come from CFG-PM-OPERATEMODE.
 *
 * The MAX-F10S of the new board has no CFG-PM group at all (u-blox F10 SPG
 * 6.00 interface description UBX-23002975 R02): with has_leap false the
 * machine keeps the same shape, never asks for a power mode, and the energy
 * comes only from the standby between modes.
 */

#ifndef SVC_GNSS_POWER_H
#define SVC_GNSS_POWER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Epochs without a fix before asking for full power and showing acquiring */
#define GNSS_POWER_WEAK_EPOCHS      10U

/** Time with a fix at full power before going back to LEAP */
#define GNSS_POWER_GOOD_MS          60000U

/** Silence of the receiver before configuring it again (docs/16, GNSS) */
#define GNSS_POWER_SILENCE_MS       10000U

/** Silence of the receiver before pulling its reset pin */
#define GNSS_POWER_RESET_MS         30000U

/** What the service has to do with the receiver */
enum gnss_power_action {
    GNSS_POWER_ACTION_NONE = 0,
    GNSS_POWER_ACTION_WAKE,     /**< wake it up and configure it again */
    GNSS_POWER_ACTION_STANDBY,  /**< UBX-RXM-PMREQ and, if there is one, the rail off */
    GNSS_POWER_ACTION_FULL,     /**< CFG-PM-OPERATEMODE = full power */
    GNSS_POWER_ACTION_LEAP,     /**< CFG-PM-OPERATEMODE = LEAP */
    GNSS_POWER_ACTION_CONFIGURE,/**< the receiver went quiet: configure it again */
    GNSS_POWER_ACTION_RESET     /**< still quiet: reset pin and configure again */
};

/** State of the machine; the service keeps one */
struct gnss_power {
    uint8_t mode;       /**< enum app_gnss_mode, what the interface shows */
    bool started;       /**< a mode already reached the machine */
    bool had_fix;       /**< there was a fix since the receiver woke up */
    bool full_power;    /**< the driver was asked for full power */
    bool has_leap;      /**< the receiver has a low power tracking mode */
    uint8_t no_fix;     /**< epochs in a row without a fix, capped */
    uint32_t good_ms;   /**< time with a fix while at full power */
    uint32_t silence_ms;/**< time since the last epoch, while awake */
};

/** Start in backup: the receiver only wakes up when the mode asks for it */
/**
 * Start the machine.
 *
 * @param has_leap true for a receiver with a low power tracking mode (the
 * MAX-M10N and its LEAP). The MAX-F10S has no CFG-PM group at all: with
 * false, the machine never asks for a power mode and shows tracking as full
 * power, and what saves energy is the standby between modes.
 */
void gnss_power_init(struct gnss_power *p, bool has_leap);

/**
 * The mode of the device changed. The first call always gives an action,
 * because at boot the receiver is on, out of the reset configuration and not
 * where this machine believes it is.
 *
 * @param uses_gnss the new mode uses the receiver (CRS, PRC, DBG)
 * @return what to do with the receiver
 */
enum gnss_power_action gnss_power_mode_change(struct gnss_power *p, bool uses_gnss);

/**
 * One navigation epoch arrived.
 *
 * @param fix the epoch has a valid position
 * @param interval_ms time between epochs, 1000 ms on this board
 * @return what to do with the receiver
 */
enum gnss_power_action gnss_power_epoch(struct gnss_power *p, bool fix, uint32_t interval_ms);

/**
 * Time passed without an epoch arriving, called by the service on every turn
 * of its loop. A receiver that stops answering gets its configuration again
 * after GNSS_POWER_SILENCE_MS and its reset pin after GNSS_POWER_RESET_MS.
 *
 * @param elapsed_ms time since the last call
 * @return what to do with the receiver
 */
enum gnss_power_action gnss_power_tick(struct gnss_power *p, uint32_t elapsed_ms);

/** Mode to publish, enum app_gnss_mode */
uint8_t gnss_power_mode(const struct gnss_power *p);

#ifdef __cplusplus
}
#endif

#endif /* SVC_GNSS_POWER_H */
