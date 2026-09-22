/**
 * @file ble_ancs_client.h
 * @brief The phone's notifications on the bicycle's screen
 *
 * A call, a message or a calendar reminder shown on the bars, so the rider
 * can decide whether it is worth taking a hand off them. Every other head
 * unit does it and this port did not; the `$ANCS` command it carried was a
 * way of pushing a notification in over the serial line, not a link to a
 * phone.
 *
 * The transport is the **Apple Notification Center Service**, and the
 * client for it is the one the nRF Connect SDK ships
 * (`bt_ancs_client`): nothing of it is written here, as the project's rule
 * about using the SDK's own subsystems asks.
 *
 * Which notifications actually reach the screen is a separate question and
 * a separate module, `model/notif_filter.c`, with its own host tests. That
 * is where the work is: a phone will hand over everything it has, and a
 * rider who is shown all of it turns the feature off before the end of the
 * first ride.
 *
 * ## iPhone only
 *
 * This service is Apple's, and it is what an iPhone offers. Android has no
 * equivalent a device can simply connect to and read; a phone running it
 * needs an application of its own to push notifications over, which this
 * project does not have. The device advertises the service as *solicited*
 * so that iOS offers it on pairing.
 *
 * Nothing here has run against a phone.
 */

#ifndef RF_BLE_ANCS_CLIENT_H
#define RF_BLE_ANCS_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"
#include "model/notif_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the connection is only passed through; the Bluetooth headers stay out */
struct bt_conn;

/** Letters kept of a title and of a message */
#define ANCS_TITLE_LEN      24U
#define ANCS_MESSAGE_LEN    48U

/** One notification that got past the filter */
struct ancs_notification {
    char title[ANCS_TITLE_LEN];
    char message[ANCS_MESSAGE_LEN];
    uint8_t category;       /**< enum notif_category */
    bool is_call;
};

/** A notification is ready to be shown */
typedef void (*ancs_notif_callback_t)(const struct ancs_notification *n);

/** The phone connected or went away */
typedef void (*ancs_conn_callback_t)(bool connected);

app_err_t ble_ancs_client_init(void);
void ble_ancs_client_register_callback(ancs_notif_callback_t cb);
void ble_ancs_client_register_conn_callback(ancs_conn_callback_t cb);

/** Called by the manager when a peer connects or drops */
void ble_ancs_client_on_connect(struct bt_conn *conn);
void ble_ancs_client_on_disconnect(struct bt_conn *conn);

/** Whether a phone is connected and its notifications are being watched */
bool ble_ancs_client_is_connected(void);

/** The filter the rider set, so the settings screen can reach it */
struct notif_filter *ble_ancs_client_filter(void);

#ifdef __cplusplus
}
#endif

#endif /* RF_BLE_ANCS_CLIENT_H */
