/**
 * @file ble_ancs_client.c
 * @brief The phone's notifications, over the Apple Notification Center Service
 *
 * Why this service and not another, and where the decision about which
 * notifications reach the screen lives, in rf/ble_ancs_client.h. The
 * client itself is the one the nRF Connect SDK ships; this file gives it
 * somewhere to report and runs each notification past the filter.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>

#include <bluetooth/gatt_dm.h>
#include <bluetooth/services/ancs_client.h>

#include "model/notif_filter.h"
#include "rf/ble_ancs_client.h"

LOG_MODULE_REGISTER(ble_ancs_client, CONFIG_LOG_DEFAULT_LEVEL);

static struct bt_ancs_client ancs_c;
static struct bt_conn *phone_conn;
static bool is_initialized;
static bool watching;

static ancs_notif_callback_t notif_callback;
static ancs_conn_callback_t conn_callback;

/** What the rider chose to be told about (`model/notif_filter.h`) */
static struct notif_filter filter;

/*
 * Where the client puts the attributes it fetches. They are static because
 * the client keeps the pointers, and sized to what a bicycle screen can
 * show: a title and a couple of lines. Anything longer is cut by the
 * client itself.
 */
static uint8_t attr_title[ANCS_TITLE_LEN];
static uint8_t attr_message[ANCS_MESSAGE_LEN];

/** The one being fetched, kept between the two halves of the exchange */
static struct ancs_notification pending;
static bool pending_valid;

/**
 * @brief One notification announced by the phone
 *
 * Runs in the Bluetooth receive thread. It decides and, when the answer is
 * yes, asks for the title and the message; the text arrives later, in
 * `data_source_cb()`.
 */
static void notification_source_cb(struct bt_ancs_client *c, int err,
                                   const struct bt_ancs_evt_notif *notif)
{
    ARG_UNUSED(c);

    if ((err != 0) || (notif == NULL)) {
        return;
    }

    struct notif_event e = {
        .uid = notif->notif_uid,
        .category = (uint8_t)notif->category_id,
        .event_id = (uint8_t)notif->evt_id,
        .silent = (notif->evt_flags.silent != 0U),
        .important = (notif->evt_flags.important != 0U),
        .preexisting = (notif->evt_flags.pre_existing != 0U),
    };

    enum notif_action act = notif_filter_decide(&filter, &e, k_uptime_get_32());

    if (act != NOTIF_SHOW) {
        LOG_DBG("notification %u dropped (%d)", (unsigned int)e.uid, (int)act);
        return;
    }

    /*
     * Only one is fetched at a time. A second arriving while the first is
     * still being answered would overwrite the buffers the client holds,
     * so it is let go: the filter's own gap means this is rare, and a
     * notification lost is better than two shown as one.
     */
    if (pending_valid) {
        LOG_DBG("notification %u dropped: one is already being read",
                (unsigned int)e.uid);
        return;
    }

    (void)memset(&pending, 0, sizeof(pending));
    pending.category = e.category;
    pending.is_call = (e.category == (uint8_t)NOTIF_CAT_INCOMING_CALL);
    pending_valid = true;

    (void)memset(attr_title, 0, sizeof(attr_title));
    (void)memset(attr_message, 0, sizeof(attr_message));

    int rc = bt_ancs_request_attrs(&ancs_c, notif, NULL);

    if (rc != 0) {
        LOG_WRN("could not ask for the text of %u (%d)", (unsigned int)e.uid, rc);
        pending_valid = false;
    }
}

/** The text of the notification, one attribute at a time */
static void data_source_cb(struct bt_ancs_client *c, const struct bt_ancs_attr_response *response)
{
    ARG_UNUSED(c);

    if ((response == NULL) || !pending_valid) {
        return;
    }

    if (response->command_id != BT_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES) {
        return;
    }

    switch (response->attr.attr_id) {
    case BT_ANCS_NOTIF_ATTR_ID_TITLE:
        (void)snprintk(pending.title, sizeof(pending.title), "%.*s",
                       (int)response->attr.attr_len, (const char *)response->attr.attr_data);
        break;
    case BT_ANCS_NOTIF_ATTR_ID_MESSAGE:
        (void)snprintk(pending.message, sizeof(pending.message), "%.*s",
                       (int)response->attr.attr_len, (const char *)response->attr.attr_data);
        /*
         * The message is the last attribute asked for, so its arrival is
         * what says the notification is whole.
         */
        pending_valid = false;
        if (notif_callback != NULL) {
            notif_callback(&pending);
        }
        break;
    default:
        break;
    }
}

static void discovery_done(struct bt_gatt_dm *dm, void *ctx)
{
    ARG_UNUSED(ctx);

    int err = bt_ancs_handles_assign(dm, &ancs_c);

    if (err != 0) {
        LOG_ERR("ANCS handles could not be assigned (%d)", err);
    } else {
        err = bt_ancs_subscribe_notification_source(&ancs_c, notification_source_cb);
        if (err != 0) {
            LOG_ERR("ANCS notification source (%d)", err);
        }
        err = bt_ancs_subscribe_data_source(&ancs_c, data_source_cb);
        if (err != 0) {
            LOG_ERR("ANCS data source (%d)", err);
        }
        watching = (err == 0);
        LOG_INF("watching the phone's notifications");
    }

    (void)bt_gatt_dm_data_release(dm);
}

static void discovery_not_found(struct bt_conn *conn, void *ctx)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(ctx);

    /* an Android phone, or an iPhone that has not been paired yet */
    LOG_INF("this peer offers no notification service");
}

static void discovery_error(struct bt_conn *conn, int err, void *ctx)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(ctx);

    LOG_WRN("notification service discovery failed (%d)", err);
}

static const struct bt_gatt_dm_cb discovery_cb = {
    .completed = discovery_done,
    .service_not_found = discovery_not_found,
    .error_found = discovery_error,
};

void ble_ancs_client_on_connect(struct bt_conn *conn)
{
    if (!is_initialized || (phone_conn != NULL)) {
        return;
    }

    phone_conn = bt_conn_ref(conn);
    pending_valid = false;
    notif_filter_reset(&filter);

    int err = bt_gatt_dm_start(conn, BT_UUID_ANCS, &discovery_cb, NULL);

    if (err != 0) {
        LOG_WRN("notification discovery did not start (%d)", err);
    }

    if (conn_callback != NULL) {
        conn_callback(true);
    }
}

void ble_ancs_client_on_disconnect(struct bt_conn *conn)
{
    if (!is_initialized || (phone_conn != conn)) {
        return;
    }

    bt_conn_unref(phone_conn);
    phone_conn = NULL;
    watching = false;
    pending_valid = false;
    notif_filter_reset(&filter);

    LOG_INF("the phone went away");

    if (conn_callback != NULL) {
        conn_callback(false);
    }
}

app_err_t ble_ancs_client_init(void)
{
    if (is_initialized) {
        return APP_ERR_ALREADY_INIT;
    }

    notif_filter_init(&filter);

    int err = bt_ancs_client_init(&ancs_c);

    if (err != 0) {
        LOG_ERR("ANCS client init failed (%d)", err);

        return APP_ERR_IO;
    }

    /*
     * Only the two attributes a bicycle can show. Asking for more would
     * lengthen every exchange for text nobody reads on the bars.
     */
    err = bt_ancs_register_attr(&ancs_c, BT_ANCS_NOTIF_ATTR_ID_TITLE, attr_title,
                                sizeof(attr_title));
    if (err == 0) {
        err = bt_ancs_register_attr(&ancs_c, BT_ANCS_NOTIF_ATTR_ID_MESSAGE, attr_message,
                                    sizeof(attr_message));
    }
    if (err != 0) {
        LOG_ERR("ANCS attributes could not be registered (%d)", err);

        return APP_ERR_IO;
    }

    is_initialized = true;
    LOG_INF("phone notifications ready");

    return APP_OK;
}

void ble_ancs_client_register_callback(ancs_notif_callback_t cb)
{
    notif_callback = cb;
}

void ble_ancs_client_register_conn_callback(ancs_conn_callback_t cb)
{
    conn_callback = cb;
}

bool ble_ancs_client_is_connected(void)
{
    return watching && (phone_conn != NULL);
}

struct notif_filter *ble_ancs_client_filter(void)
{
    return &filter;
}
