/**
 * @file ble_cps_client.h
 * @brief The rider's power meter, over the Bluetooth Cycling Power Service
 *
 * A crank, a pedal pair, a spider or a hub that measures power announces
 * the Cycling Power Service (0x1818) and notifies its Cycling Power
 * Measurement (0x2A63). This is the GATT client for it, in the same shape
 * as the heart rate and the cadence clients: it finds the service, watches
 * the characteristic, and hands the numbers on.
 *
 * The reading of the notification itself lives apart in
 * `model/cps_parse.c`, with the host tests, because the walk over its
 * optional fields is where the mistakes are. What is left here is the
 * plumbing.
 *
 * Until now the port had no power meter client at all: outdoors the power
 * on the screen was **estimated** from speed, slope and weight
 * (`model/power_estimate.c`, the formula of the legacy). A rider with a
 * meter now gets what the meter says, and the estimate stays for the rider
 * without one.
 *
 * Nothing here has been tested with a power meter.
 */

#ifndef RF_BLE_CPS_CLIENT_H
#define RF_BLE_CPS_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** What the meter last said */
typedef struct {
    int16_t power_w;            /**< instantaneous, signed */
    uint8_t cadence_rpm;        /**< 0 when the meter has no crank counter */
    uint16_t speed_kmh100;      /**< 0 when it has no wheel counter */
    uint8_t balance_pct;        /**< of the leg named by `balance_is_left` */
    bool balance_is_left;
    bool have_balance;
    bool offset_needed;         /**< the meter is asking for a zero offset */
    uint32_t timestamp;         /**< uptime of the notification */
    bool connected;
} cps_info_t;

/** One notification arrived */
typedef void (*cps_data_callback_t)(const cps_info_t *info);

/** The meter connected or went away */
typedef void (*cps_conn_callback_t)(bool connected);

app_err_t ble_cps_client_init(void);
void ble_cps_client_register_callback(cps_data_callback_t callback);
void ble_cps_client_register_conn_callback(cps_conn_callback_t callback);

/** Called by the manager when a peer connects or drops */
void ble_cps_client_on_connect(struct bt_conn *conn);
void ble_cps_client_on_disconnect(struct bt_conn *conn);

bool ble_cps_client_is_connected(void);
app_err_t ble_cps_client_get_info(cps_info_t *info);

/** The wheel the rider set, for a meter that counts wheel turns */
void ble_cps_client_set_wheel_circumference(uint16_t circumference_mm);

#ifdef __cplusplus
}
#endif

#endif /* RF_BLE_CPS_CLIENT_H */
