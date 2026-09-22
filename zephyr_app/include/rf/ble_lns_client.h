/**
 * @file ble_lns_client.h
 * @brief Where the phone says it is, over Bluetooth
 *
 * The device already told a phone where it was (`rf/ble_lns.c`); this is
 * the side that listens, over the Location and Navigation Service
 * (0x1819). Without it the branch of `model/loc_arbiter.c` that picks a
 * position from the phone was unreachable: written, tested, and never fed.
 *
 * A position from the phone is published as an ordinary GNSS epoch with
 * its `phone` flag set, so it takes the same path as the receiver's own
 * and the arbiter decides between them by the rule of the legacy: the
 * phone is the last source it will take, and only while the receiver has
 * no fix of its own.
 *
 * The reading of the characteristic is in `model/lns_parse.h`, with the
 * host tests. Nothing here has run against a phone.
 */

#ifndef RF_BLE_LNS_CLIENT_H
#define RF_BLE_LNS_CLIENT_H

#include <stdbool.h>

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the connection is only passed through; the Bluetooth headers stay out */
struct bt_conn;

app_err_t ble_lns_client_init(void);

/** Called by the manager when a peer connects or drops */
void ble_lns_client_on_connect(struct bt_conn *conn);
void ble_lns_client_on_disconnect(struct bt_conn *conn);

/** Whether a phone is sending its position */
bool ble_lns_client_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* RF_BLE_LNS_CLIENT_H */
