/**
 * @file power_ant.h
 * @brief The rider's power meter over ANT+, with the profile left outside
 *
 * Most power meters on a bicycle speak **both** Bluetooth and ANT+, and
 * plenty of riders keep the Bluetooth link for their phone and the ANT+ one
 * for the head unit, because ANT+ lets several devices listen to the same
 * meter at once while a Bluetooth meter usually talks to one. So the device
 * wants both, and this is the ANT+ half.
 *
 * The ANT+ **Bicycle Power** profile (device type 11) is the specified way
 * to read one, and the one this port cannot carry: the profile documents
 * and code derived from them are under the ANT+ Shared Source License, the
 * ANT+ Adopter Agreement forbids redistributing them, and this repository
 * is public ([07](../../docs/07-radio-ant-ble.md#decisão-ant-e-ble)). The
 * arrangement is the same as the rear radar's (`rf/radar_ant.h`):
 *
 * 1. the channel parameters come from Kconfig (`GNSS_ANT_POWER_DEV_TYPE`,
 *    `_PERIOD`, `_RF`), which are zero here and which the owner sets from
 *    the profile document;
 * 2. the page decoding is `power_ant_page()`, **declared** here and
 *    **defined** in `src/rf/ant/power_pages.c`, a file `.gitignore` keeps
 *    out of the repository. Without it the weak definition below returns
 *    false, the meter never reports over ANT+, and the Bluetooth client
 *    (`rf/ble_cps_client.h`) goes on working.
 *
 * What the owner has to write is one function: take the eight bytes of a
 * broadcast and fill a `struct power_ant_reading`. Everything else — the
 * choice between meter and estimate, the zones, the normalised power, the
 * screens — is already here and covered by the host tests, because none of
 * it is profile material.
 */

#ifndef RF_POWER_ANT_H
#define RF_POWER_ANT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bytes of an ANT broadcast */
#define POWER_ANT_PAYLOAD   8U

/** What one broadcast of a power meter carried */
struct power_ant_reading {
    uint16_t power_w;           /**< instantaneous or averaged, as the page gives */
    uint8_t cadence_rpm;        /**< 0 when the page does not carry one */
    uint8_t balance_pct;        /**< 0 when the page does not carry one */
    bool balance_is_left;
    bool have_power;
    bool have_cadence;
    bool have_balance;
};

/**
 * Turn one ANT broadcast into a reading.
 *
 * Defined in `src/rf/ant/power_pages.c`, which is not in this repository
 * (see the note at the top of this file). The weak definition that ships
 * here returns false.
 *
 * @param page first byte of the broadcast, the page number
 * @param data the eight bytes
 * @param out reading to fill
 * @return true when the page carried a power and @p out was filled
 */
bool power_ant_page(uint8_t page, const uint8_t *data, struct power_ant_reading *out);

/** Open the power channel; does nothing while the Kconfig values are zero */
int power_ant_start(void);

/** Close it */
void power_ant_stop(void);

/** true while the channel is open and a meter is answering */
bool power_ant_is_linked(void);

/** One broadcast arrived: decode it and publish */
void power_ant_on_broadcast(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* RF_POWER_ANT_H */
