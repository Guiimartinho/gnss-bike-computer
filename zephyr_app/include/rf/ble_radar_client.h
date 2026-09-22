/**
 * @file ble_radar_client.h
 * @brief GATT client of the rear radar service of a Garmin Varia
 *
 * The model of what is behind is in `model/radar.h`; this only turns the
 * notifications of one radio into frames for it. Caveats about the format,
 * which Garmin does not publish, in `model/radar_wire.h`.
 */

#ifndef RF_BLE_RADAR_CLIENT_H
#define RF_BLE_RADAR_CLIENT_H

#include <stdbool.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>

#include "model/radar.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One frame of the radar; runs on the BT RX thread, so it only copies */
typedef void (*radar_frame_cb_t)(const struct radar_frame *frame);

/** The radar came or went */
typedef void (*radar_link_cb_t)(bool linked);

/** Remember the callbacks; call before the first connection */
int ble_radar_client_init(radar_frame_cb_t on_frame, radar_link_cb_t on_link);

/** Find the service on a connection and subscribe to what it notifies */
int ble_radar_client_start(struct bt_conn *conn);

/** Stop listening */
void ble_radar_client_stop(void);

/** true while a radar is connected and subscribed */
bool ble_radar_client_is_linked(void);

/** The UUID of the service, for the scan filter of the manager */
const struct bt_uuid *ble_radar_service_uuid(void);

#ifdef __cplusplus
}
#endif

#endif /* RF_BLE_RADAR_CLIENT_H */
