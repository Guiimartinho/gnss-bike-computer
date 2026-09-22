/**
 * @file ble_cps_client.c
 * @brief GATT client for the Cycling Power Service (0x1818)
 *
 * What it is for, and where the reading of the notification lives, in
 * rf/ble_cps_client.h.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "model/cps_parse.h"
#include "model/csc_calc.h"
#include "rf/ble_cps_client.h"

LOG_MODULE_REGISTER(ble_cps_client, CONFIG_LOG_DEFAULT_LEVEL);

#ifndef BT_UUID_CPS_VAL
#define BT_UUID_CPS_VAL             0x1818
#endif
#ifndef BT_UUID_CPS_MEASUREMENT_VAL
#define BT_UUID_CPS_MEASUREMENT_VAL 0x2A63
#endif

static struct bt_uuid_16 uuid_cps = BT_UUID_INIT_16(BT_UUID_CPS_VAL);
static struct bt_uuid_16 uuid_cps_measurement = BT_UUID_INIT_16(BT_UUID_CPS_MEASUREMENT_VAL);

static struct bt_conn *cps_conn;
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

/*
 * The host finds the CCC descriptor by itself, but only when it is given a
 * place to do it and where the service ends; without them
 * bt_gatt_subscribe() writes through a null pointer. The same trap the
 * other three clients fell into.
 */
static struct bt_gatt_discover_params ccc_disc_params;
static uint16_t service_end_handle;
static uint16_t measurement_handle;

static cps_info_t cps_data;
static cps_data_callback_t data_callback;
static cps_conn_callback_t conn_callback;
static bool is_initialized;
static bool subscribed;

/** The previous notification, which the cadence and the speed need */
static struct cps_measurement prev;
static bool have_prev;

/** Circumference the rider set, kept so a reconnection does not lose it */
static uint16_t wheel_mm = CSC_WHEEL_MM;

static K_MUTEX_DEFINE(cps_mutex);

/**
 * @brief Read one notification and hand it on
 *
 * Runs in the Bluetooth receive thread: it parses, copies and calls back,
 * and touches nothing of the model (docs/05, threads).
 */
static void handle_measurement(const uint8_t *data, uint16_t len)
{
    struct cps_measurement now;

    if (!cps_parse_measurement(data, len, &now)) {
        LOG_WRN("power meter sent %u bytes that do not match its flags", len);
        return;
    }

    uint8_t cadence = 0U;
    uint16_t speed = 0U;

    if (have_prev) {
        cadence = cps_cadence_rpm(&prev, &now);
        speed = cps_speed_kmh100(&prev, &now, wheel_mm);
    }

    /*
     * Only a notification that carried the counters becomes the reference
     * for the next one. A meter that sends power alone between two
     * notifications with crank data must not break the pair.
     */
    if (now.have_crank || now.have_wheel) {
        prev = now;
        have_prev = true;
    }

    cps_info_t info = {
        .power_w = now.power_w,
        .cadence_rpm = cadence,
        .speed_kmh100 = speed,
        .balance_pct = now.balance_pct,
        .balance_is_left = now.balance_is_left,
        .have_balance = now.have_balance,
        .offset_needed = now.offset_needed,
        .timestamp = k_uptime_get_32(),
        .connected = true,
    };

    k_mutex_lock(&cps_mutex, K_FOREVER);
    cps_data = info;
    k_mutex_unlock(&cps_mutex);

    LOG_DBG("power %d W, cadence %u rpm", (int)info.power_w, info.cadence_rpm);

    if (data_callback != NULL) {
        data_callback(&info);
    }
}

static uint8_t notify_func(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                           const void *data, uint16_t length)
{
    ARG_UNUSED(conn);

    if (data == NULL) {
        LOG_INF("power notifications ended");
        params->value_handle = 0U;
        subscribed = false;

        return BT_GATT_ITER_STOP;
    }

    handle_measurement(data, length);

    return BT_GATT_ITER_CONTINUE;
}

static app_err_t subscribe_to_measurement(struct bt_conn *conn)
{
    subscribe_params.notify = notify_func;
    subscribe_params.value_handle = measurement_handle;
    subscribe_params.ccc_handle = BT_GATT_AUTO_DISCOVER_CCC_HANDLE;
    subscribe_params.end_handle = service_end_handle;
    subscribe_params.disc_params = &ccc_disc_params;
    subscribe_params.value = BT_GATT_CCC_NOTIFY;

    int err = bt_gatt_subscribe(conn, &subscribe_params);

    if (err != 0) {
        LOG_ERR("power subscribe failed (%d)", err);

        return APP_ERR_IO;
    }

    subscribed = true;
    LOG_INF("subscribed to the power meter");

    return APP_OK;
}

static uint8_t discover_func(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    if (attr == NULL) {
        (void)memset(params, 0, sizeof(*params));
        if (measurement_handle != 0U) {
            (void)subscribe_to_measurement(conn);
        } else {
            LOG_WRN("no Cycling Power Measurement on this peer");
        }

        return BT_GATT_ITER_STOP;
    }

    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        const struct bt_gatt_service_val *svc = attr->user_data;

        params->uuid = &uuid_cps_measurement.uuid;
        params->start_handle = attr->handle + 1U;
        service_end_handle = svc->end_handle;
        params->end_handle = svc->end_handle;
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;

        int err = bt_gatt_discover(conn, params);

        if (err != 0) {
            LOG_ERR("power characteristic discovery failed (%d)", err);
        }

        return BT_GATT_ITER_STOP;
    }

    if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        const struct bt_gatt_chrc *chrc = attr->user_data;

        if (bt_uuid_cmp(chrc->uuid, &uuid_cps_measurement.uuid) == 0) {
            measurement_handle = chrc->value_handle;
            LOG_INF("power meter measurement at handle %u", measurement_handle);
        }
    }

    return BT_GATT_ITER_CONTINUE;
}

static app_err_t start_discovery(struct bt_conn *conn)
{
    measurement_handle = 0U;

    discover_params.uuid = &uuid_cps.uuid;
    discover_params.func = discover_func;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    int err = bt_gatt_discover(conn, &discover_params);

    if (err != 0) {
        LOG_ERR("power discovery failed to start (%d)", err);

        return APP_ERR_IO;
    }

    return APP_OK;
}

void ble_cps_client_on_connect(struct bt_conn *conn)
{
    if (!is_initialized || (cps_conn != NULL)) {
        return;
    }

    cps_conn = bt_conn_ref(conn);
    have_prev = false;
    (void)memset(&prev, 0, sizeof(prev));

    LOG_INF("power meter connected");
    (void)start_discovery(conn);

    if (conn_callback != NULL) {
        conn_callback(true);
    }
}

void ble_cps_client_on_disconnect(struct bt_conn *conn)
{
    if (!is_initialized || (cps_conn != conn)) {
        return;
    }

    bt_conn_unref(cps_conn);
    cps_conn = NULL;
    measurement_handle = 0U;
    subscribed = false;
    have_prev = false;

    k_mutex_lock(&cps_mutex, K_FOREVER);
    (void)memset(&cps_data, 0, sizeof(cps_data));
    k_mutex_unlock(&cps_mutex);

    LOG_INF("power meter gone");

    if (conn_callback != NULL) {
        conn_callback(false);
    }
}

app_err_t ble_cps_client_init(void)
{
    if (is_initialized) {
        return APP_ERR_ALREADY_INIT;
    }

    (void)memset(&cps_data, 0, sizeof(cps_data));
    (void)memset(&prev, 0, sizeof(prev));
    cps_conn = NULL;
    measurement_handle = 0U;
    subscribed = false;
    have_prev = false;
    is_initialized = true;

    LOG_INF("power meter client ready");

    return APP_OK;
}

void ble_cps_client_register_callback(cps_data_callback_t callback)
{
    data_callback = callback;
}

void ble_cps_client_register_conn_callback(cps_conn_callback_t callback)
{
    conn_callback = callback;
}

bool ble_cps_client_is_connected(void)
{
    return subscribed && (cps_conn != NULL);
}

app_err_t ble_cps_client_get_info(cps_info_t *info)
{
    if (info == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    k_mutex_lock(&cps_mutex, K_FOREVER);
    *info = cps_data;
    k_mutex_unlock(&cps_mutex);

    return APP_OK;
}

void ble_cps_client_set_wheel_circumference(uint16_t circumference_mm)
{
    if (circumference_mm != 0U) {
        wheel_mm = circumference_mm;
    }
}
