/**
 * @file ble_lns_client.c
 * @brief Where the phone says it is, over Bluetooth
 *
 * The device already had `rf/ble_lns.c`, which is the other direction: it
 * tells a phone where *this* is. What was missing is the side that
 * listens, and without it the branch of `model/loc_arbiter.c` that picks a
 * position from the phone — written, and covered by its own tests — could
 * never fire, because nothing ever fed it.
 *
 * The reading of the characteristic lives apart in `model/lns_parse.c`,
 * with the host tests, for the same reason as the power meter's and the
 * trainer's: the walk over the optional fields is where the mistakes are.
 * What is left here is the plumbing.
 *
 * A position from the phone is published as an ordinary GNSS epoch with
 * its `phone` flag set, so it goes through the same path as the receiver's
 * own and the arbiter decides between them by the rule of the legacy. It
 * is the last source it will take, and only while the receiver has no fix.
 *
 * Nothing here has run against a phone.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "app/app_channels.h"
#include "model/lns_parse.h"
#include "rf/ble_lns_client.h"

LOG_MODULE_REGISTER(ble_lns_client, CONFIG_LOG_DEFAULT_LEVEL);

#ifndef BT_UUID_LNS_VAL
#define BT_UUID_LNS_VAL             0x1819
#endif
#ifndef BT_UUID_LN_LOCATION_SPEED_VAL
#define BT_UUID_LN_LOCATION_SPEED_VAL   0x2A67
#endif

static struct bt_uuid_16 uuid_lns = BT_UUID_INIT_16(BT_UUID_LNS_VAL);
static struct bt_uuid_16 uuid_loc_speed = BT_UUID_INIT_16(BT_UUID_LN_LOCATION_SPEED_VAL);

static struct bt_conn *lns_conn;
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

/*
 * The host finds the CCC descriptor itself, but only when it is given a
 * place to do it and where the service ends; without them
 * bt_gatt_subscribe() writes through a null pointer. The same trap the
 * other clients fell into.
 */
static struct bt_gatt_discover_params ccc_disc_params;
static uint16_t service_end_handle;
static uint16_t loc_speed_handle;

static bool is_initialized;
static bool subscribed;

/**
 * @brief One notification from the phone
 *
 * Runs in the Bluetooth receive thread: it reads, fills an epoch and
 * publishes, and touches nothing of the model (docs/05, threads).
 */
static void handle_location(const uint8_t *data, uint16_t len)
{
    struct lns_location loc;

    if (!lns_parse_location(data, len, &loc)) {
        LOG_WRN("the phone sent %u bytes that do not match its flags", len);
        return;
    }

    if (!lns_position_is_usable(&loc)) {
        /*
         * The phone has no position of its own, or only a guess. Taking it
         * would put the rider where the phone last thought it was, which
         * looks like an answer and is worse than nothing.
         */
        return;
    }

    struct app_gnss_fix f = {
        .uptime_ms = k_uptime_get_32(),
        .fix = true,
        .phone = true,
        .lat_e7 = loc.lat_e7,
        .lon_e7 = loc.lon_e7,
        .alt_mm = loc.have_elevation ? (loc.elevation_cm * 10) : 0,
        /* the service counts in hundredths of a metre a second */
        .speed_mms = loc.have_speed ? ((uint32_t)loc.speed_cms * 10U) : 0U,
        .course_mdeg = loc.have_heading ? ((uint32_t)loc.heading_cdeg * 10U) : 0U,
    };

    (void)app_publish(&chan_gnss_fix, &f);
}

static uint8_t notify_func(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                           const void *data, uint16_t length)
{
    ARG_UNUSED(conn);

    if (data == NULL) {
        LOG_INF("the phone stopped sending its position");
        params->value_handle = 0U;
        subscribed = false;

        return BT_GATT_ITER_STOP;
    }

    handle_location(data, length);

    return BT_GATT_ITER_CONTINUE;
}

static app_err_t subscribe_to_location(struct bt_conn *conn)
{
    subscribe_params.notify = notify_func;
    subscribe_params.value_handle = loc_speed_handle;
    subscribe_params.ccc_handle = BT_GATT_AUTO_DISCOVER_CCC_HANDLE;
    subscribe_params.end_handle = service_end_handle;
    subscribe_params.disc_params = &ccc_disc_params;
    subscribe_params.value = BT_GATT_CCC_NOTIFY;

    int err = bt_gatt_subscribe(conn, &subscribe_params);

    if (err != 0) {
        LOG_ERR("position subscribe failed (%d)", err);

        return APP_ERR_IO;
    }

    subscribed = true;
    LOG_INF("listening to the phone's position");

    return APP_OK;
}

static uint8_t discover_func(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    if (attr == NULL) {
        (void)memset(params, 0, sizeof(*params));
        if (loc_speed_handle != 0U) {
            (void)subscribe_to_location(conn);
        } else {
            LOG_INF("this phone does not offer a position");
        }

        return BT_GATT_ITER_STOP;
    }

    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        const struct bt_gatt_service_val *svc = attr->user_data;

        params->uuid = &uuid_loc_speed.uuid;
        params->start_handle = attr->handle + 1U;
        service_end_handle = svc->end_handle;
        params->end_handle = svc->end_handle;
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;

        int err = bt_gatt_discover(conn, params);

        if (err != 0) {
            LOG_ERR("position characteristic discovery failed (%d)", err);
        }

        return BT_GATT_ITER_STOP;
    }

    if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        const struct bt_gatt_chrc *chrc = attr->user_data;

        if (bt_uuid_cmp(chrc->uuid, &uuid_loc_speed.uuid) == 0) {
            loc_speed_handle = chrc->value_handle;
            LOG_INF("the phone's position is at handle %u", loc_speed_handle);
        }
    }

    return BT_GATT_ITER_CONTINUE;
}

void ble_lns_client_on_connect(struct bt_conn *conn)
{
    if (!is_initialized || (lns_conn != NULL)) {
        return;
    }

    lns_conn = bt_conn_ref(conn);
    loc_speed_handle = 0U;

    discover_params.uuid = &uuid_lns.uuid;
    discover_params.func = discover_func;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    int err = bt_gatt_discover(conn, &discover_params);

    if (err != 0) {
        LOG_WRN("position discovery did not start (%d)", err);
    }
}

void ble_lns_client_on_disconnect(struct bt_conn *conn)
{
    if (!is_initialized || (lns_conn != conn)) {
        return;
    }

    bt_conn_unref(lns_conn);
    lns_conn = NULL;
    loc_speed_handle = 0U;
    subscribed = false;

    LOG_INF("the phone's position is gone");
}

app_err_t ble_lns_client_init(void)
{
    if (is_initialized) {
        return APP_ERR_ALREADY_INIT;
    }

    lns_conn = NULL;
    loc_speed_handle = 0U;
    subscribed = false;
    is_initialized = true;

    LOG_INF("phone position client ready");

    return APP_OK;
}

bool ble_lns_client_is_connected(void)
{
    return subscribed && (lns_conn != NULL);
}
