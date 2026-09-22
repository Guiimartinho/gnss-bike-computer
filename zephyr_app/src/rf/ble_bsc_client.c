/**
 * @file ble_bsc_client.c
 * @brief BLE Cycling Speed and Cadence Service Client implementation
 *
 * GATT client for CSC Service (0x1816)
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "model/csc_calc.h"
#include "rf/ble_bsc_client.h"

LOG_MODULE_REGISTER(ble_bsc_client, CONFIG_LOG_DEFAULT_LEVEL);

/* ==========================================================================
 * Private Definitions
 * ========================================================================== */

/* Note: BT_UUID_CSC_VAL, BT_UUID_CSC_MEASUREMENT_VAL, BT_UUID_CSC_FEATURE_VAL
 * are defined in <zephyr/bluetooth/uuid.h> */

/** Sensor Location Characteristic UUID (not in standard uuid.h) */
#ifndef BT_UUID_SENSOR_LOC_VAL
#define BT_UUID_SENSOR_LOC_VAL      0x2A5D
#endif

/* CSC Measurement flags */
#define CSC_FLAG_WHEEL_REV_PRESENT  0x01U
#define CSC_FLAG_CRANK_REV_PRESENT  0x02U

/* UUID declarations */
static struct bt_uuid_16 uuid_csc = BT_UUID_INIT_16(BT_UUID_CSC_VAL);
static struct bt_uuid_16 uuid_csc_measurement = BT_UUID_INIT_16(BT_UUID_CSC_MEASUREMENT_VAL);

/* ==========================================================================
 * Private Variables
 * ========================================================================== */

/** Client state */
static bsc_client_state_t client_state = BSC_CLIENT_STATE_IDLE;

/** Current connection */
static struct bt_conn *bsc_conn;

/** Discovery parameters */
static struct bt_gatt_discover_params discover_params;

/** Subscribe parameters */
static struct bt_gatt_subscribe_params subscribe_params;

/**
 * The host finds the CCC descriptor of the characteristic by itself
 * (CONFIG_BT_GATT_AUTO_DISCOVER_CCC), but only when it is given a place to
 * do it and where the service ends; without them bt_gatt_subscribe()
 * dereferences a null pointer.
 */
static struct bt_gatt_discover_params ccc_disc_params;
static uint16_t service_end_handle;

/** CSC Measurement handle */
static uint16_t csc_measurement_handle;

/** Current BSC data */
static bsc_info_t bsc_data;

/** Data callback */
static bsc_data_callback_t data_callback;

/** Connection callback */
static bsc_conn_callback_t conn_callback;

/** Initialization flag */
static bool is_initialized;

/*
 * Speed and cadence are worked out in `model/csc_calc.c`, which the host
 * tests cover: the sum used to live here and gave a speed 3600 times too
 * small, so a wheel turning once a second read as zero.
 */
static struct csc_calc calc;

/** Circumference the rider set, kept so a reset does not lose it */
static uint16_t wheel_mm = CSC_WHEEL_MM;

/** Mutex for data protection */
static K_MUTEX_DEFINE(bsc_mutex);

/**
 * @brief Parse CSC Measurement characteristic value
 */
static void parse_csc_value(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len < 1U)) {
        return;
    }

    uint8_t flags = data[0];
    uint8_t offset = 1U;
    uint16_t speed = 0U;
    uint8_t cadence = 0U;

    /* Wheel Revolution Data Present */
    if ((flags & CSC_FLAG_WHEEL_REV_PRESENT) != 0U) {
        if (len >= (offset + 6U)) {
            uint32_t wheel_revs = data[offset] |
                                  ((uint32_t)data[offset + 1U] << 8U) |
                                  ((uint32_t)data[offset + 2U] << 16U) |
                                  ((uint32_t)data[offset + 3U] << 24U);
            uint16_t wheel_time = data[offset + 4U] | ((uint16_t)data[offset + 5U] << 8U);

            speed = csc_calc_speed(&calc, wheel_revs, wheel_time);
            offset += 6U;
        }
    }

    /* Crank Revolution Data Present */
    if ((flags & CSC_FLAG_CRANK_REV_PRESENT) != 0U) {
        if (len >= (offset + 4U)) {
            uint16_t crank_revs = data[offset] | ((uint16_t)data[offset + 1U] << 8U);
            uint16_t crank_time = data[offset + 2U] | ((uint16_t)data[offset + 3U] << 8U);

            cadence = csc_calc_cadence(&calc, crank_revs, crank_time);
        }
    }

    k_mutex_lock(&bsc_mutex, K_FOREVER);
    bsc_data.speed = speed;
    bsc_data.cadence = cadence;
    bsc_data.timestamp = k_uptime_get_32();
    bsc_data.connected = true;
    k_mutex_unlock(&bsc_mutex);

    LOG_DBG("Speed: %d.%02d km/h, Cadence: %d rpm",
            speed / 100, speed % 100, cadence);

    /* Notify callback */
    if (data_callback != NULL) {
        data_callback(speed, cadence);
    }
}

/**
 * @brief Notification callback for CSC Measurement
 */
static uint8_t notify_func(struct bt_conn *conn,
                           struct bt_gatt_subscribe_params *params,
                           const void *data, uint16_t length)
{
    if (data == NULL) {
        /* Subscription ended */
        LOG_INF("CSC notifications disabled");
        params->value_handle = 0U;
        client_state = BSC_CLIENT_STATE_CONNECTED;
        return BT_GATT_ITER_STOP;
    }

    parse_csc_value(data, length);

    return BT_GATT_ITER_CONTINUE;
}

/**
 * @brief Subscribe to CSC Measurement notifications
 */
static app_err_t subscribe_to_csc(struct bt_conn *conn)
{
    int err;

    subscribe_params.notify = notify_func;
    subscribe_params.value_handle = csc_measurement_handle;
    subscribe_params.ccc_handle = BT_GATT_AUTO_DISCOVER_CCC_HANDLE;
    subscribe_params.end_handle = service_end_handle;
    subscribe_params.disc_params = &ccc_disc_params;
    subscribe_params.value = BT_GATT_CCC_NOTIFY;

    err = bt_gatt_subscribe(conn, &subscribe_params);
    if (err != 0) {
        LOG_ERR("Subscribe failed (err %d)", err);
        return APP_ERR_IO;
    }

    client_state = BSC_CLIENT_STATE_SUBSCRIBED;
    LOG_INF("Subscribed to CSC Measurement notifications");

    return APP_OK;
}

/**
 * @brief GATT discovery callback
 */
static uint8_t discover_func(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    if (attr == NULL) {
        LOG_INF("CSC Discovery complete");
        (void)memset(params, 0, sizeof(*params));

        if (csc_measurement_handle != 0U) {
            /* Found the characteristic, subscribe */
            (void)subscribe_to_csc(conn);
        } else {
            LOG_WRN("CSC Measurement characteristic not found");
        }

        return BT_GATT_ITER_STOP;
    }

    LOG_DBG("Discovered attr handle %u", attr->handle);

    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        /* Found CSC service, now discover characteristics */
        const struct bt_gatt_service_val *svc = attr->user_data;

        params->uuid = &uuid_csc_measurement.uuid;
        params->start_handle = attr->handle + 1U;
        service_end_handle = svc->end_handle;
        params->end_handle = svc->end_handle;
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;

        int err = bt_gatt_discover(conn, params);
        if (err != 0) {
            LOG_ERR("Characteristic discovery failed (err %d)", err);
        }

        return BT_GATT_ITER_STOP;
    }

    if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        struct bt_gatt_chrc *chrc = attr->user_data;

        if (bt_uuid_cmp(chrc->uuid, &uuid_csc_measurement.uuid) == 0) {
            csc_measurement_handle = chrc->value_handle;
            LOG_INF("Found CSC Measurement handle: %u", csc_measurement_handle);
        }
    }

    return BT_GATT_ITER_CONTINUE;
}

/**
 * @brief Start CSC service discovery
 */
static app_err_t start_discovery(struct bt_conn *conn)
{
    int err;

    csc_measurement_handle = 0U;

    discover_params.uuid = &uuid_csc.uuid;
    discover_params.func = discover_func;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    err = bt_gatt_discover(conn, &discover_params);
    if (err != 0) {
        LOG_ERR("Discovery start failed (err %d)", err);
        return APP_ERR_IO;
    }

    client_state = BSC_CLIENT_STATE_DISCOVERING;
    LOG_INF("CSC discovery started");

    return APP_OK;
}

/* ==========================================================================
 * Connection Callbacks (called by ble_manager)
 * ========================================================================== */

void ble_bsc_client_on_connect(struct bt_conn *conn)
{
    if (!is_initialized) {
        return;
    }

    if (bsc_conn != NULL) {
        LOG_WRN("Already connected to a BSC device");
        return;
    }

    bsc_conn = bt_conn_ref(conn);
    client_state = BSC_CLIENT_STATE_CONNECTED;

    /* Reset measurement state */
    csc_calc_init(&calc, wheel_mm);

    LOG_INF("BSC device connected");

    /* Start discovery */
    (void)start_discovery(conn);

    /* Notify connection callback */
    if (conn_callback != NULL) {
        conn_callback(true);
    }
}

void ble_bsc_client_on_disconnect(struct bt_conn *conn)
{
    if (!is_initialized) {
        return;
    }

    if (bsc_conn != conn) {
        return;
    }

    bt_conn_unref(bsc_conn);
    bsc_conn = NULL;

    csc_measurement_handle = 0U;
    client_state = BSC_CLIENT_STATE_IDLE;

    k_mutex_lock(&bsc_mutex, K_FOREVER);
    bsc_data.connected = false;
    bsc_data.speed = 0U;
    bsc_data.cadence = 0U;
    k_mutex_unlock(&bsc_mutex);

    LOG_INF("BSC device disconnected");

    /* Notify connection callback */
    if (conn_callback != NULL) {
        conn_callback(false);
    }
}

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

app_err_t ble_bsc_client_init(void)
{
    if (is_initialized) {
        return APP_ERR_ALREADY_INIT;
    }

    /* Initialize data */
    (void)memset(&bsc_data, 0, sizeof(bsc_data));
    bsc_conn = NULL;
    csc_measurement_handle = 0U;
    client_state = BSC_CLIENT_STATE_IDLE;

    csc_calc_init(&calc, wheel_mm);

    is_initialized = true;
    LOG_INF("BSC client initialized");

    return APP_OK;
}

void ble_bsc_client_register_callback(bsc_data_callback_t callback)
{
    data_callback = callback;
}

void ble_bsc_client_register_conn_callback(bsc_conn_callback_t callback)
{
    conn_callback = callback;
}

bool ble_bsc_client_is_connected(void)
{
    return (client_state == BSC_CLIENT_STATE_SUBSCRIBED) && (bsc_conn != NULL);
}

uint16_t ble_bsc_client_get_speed(void)
{
    uint16_t speed;

    k_mutex_lock(&bsc_mutex, K_FOREVER);
    speed = bsc_data.speed;
    k_mutex_unlock(&bsc_mutex);

    return speed;
}

uint8_t ble_bsc_client_get_cadence(void)
{
    uint8_t cadence;

    k_mutex_lock(&bsc_mutex, K_FOREVER);
    cadence = bsc_data.cadence;
    k_mutex_unlock(&bsc_mutex);

    return cadence;
}

app_err_t ble_bsc_client_get_info(bsc_info_t *info)
{
    if (info == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    k_mutex_lock(&bsc_mutex, K_FOREVER);
    *info = bsc_data;
    k_mutex_unlock(&bsc_mutex);

    return APP_OK;
}

void ble_bsc_client_set_wheel_circumference(uint16_t circumference_mm)
{
    wheel_mm = circumference_mm;
    csc_calc_set_wheel(&calc, circumference_mm);
    LOG_INF("Wheel circumference set to %u mm", circumference_mm);
}
