/**
 * @file ble_radar_client.c
 * @brief GATT client of the rear radar service of a Garmin Varia
 *
 * The legacy has no radar. Of the two radios that can carry one, this is
 * the half that the repository can hold: the ANT+ Bike Radar profile is
 * under the ANT+ Shared Source License, and this repository is public
 * ([07](../../docs/07-radio-ant-ble.md)), so the ANT+ side only has the
 * plumbing and the profile stays on the owner's machine
 * (`src/rf/ant/radar_ant.c`).
 *
 * > [!IMPORTANT]
 * > Garmin publishes no specification for this service. The UUIDs and the
 * > shape of the notification follow the open projects that talk to a
 * > Varia, and **nothing here was checked against a radar**. What is most
 * > likely to need a fix is the unit of the closing speed, which lives in
 * > one place (`RADAR_VARIA_SPEED_KMH` of `model/radar_wire.h`).
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "model/radar_wire.h"
#include "rf/ble_radar_client.h"

LOG_MODULE_REGISTER(ble_radar, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * The service of the Varia, 6A4E3200-667B-11E3-949A-0800200C9A66, and the
 * characteristic that notifies what is behind, ...3203. Reverse
 * engineered, not published by Garmin.
 */
#define RADAR_SVC_UUID                                                                             \
    BT_UUID_128_ENCODE(0x6a4e3200, 0x667b, 0x11e3, 0x949a, 0x0800200c9a66)
#define RADAR_MEAS_UUID                                                                            \
    BT_UUID_128_ENCODE(0x6a4e3203, 0x667b, 0x11e3, 0x949a, 0x0800200c9a66)

static const struct bt_uuid_128 uuid_radar_svc = BT_UUID_INIT_128(RADAR_SVC_UUID);
static const struct bt_uuid_128 uuid_radar_meas = BT_UUID_INIT_128(RADAR_MEAS_UUID);

static struct bt_conn *radar_conn;
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

/*
 * The host finds the CCC descriptor by itself, but only when it is given a
 * place to look and where the service ends; without them bt_gatt_subscribe()
 * writes through a null pointer (the defect fixed in the other clients on
 * 2026-09-20).
 */
static struct bt_gatt_discover_params ccc_disc_params;
static uint16_t service_end_handle;
static uint16_t meas_handle;

static radar_frame_cb_t frame_cb;
static radar_link_cb_t link_cb;

const struct bt_uuid *ble_radar_service_uuid(void)
{
    return &uuid_radar_svc.uuid;
}

static uint8_t on_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length)
{
    ARG_UNUSED(conn);

    if (data == NULL) {
        /* the peer removed the subscription */
        params->value_handle = 0U;
        if (link_cb != NULL) {
            link_cb(false);
        }

        return BT_GATT_ITER_STOP;
    }

    struct radar_frame f;

    /* the callback runs on the BT RX thread: parse, hand over, return */
    if (radar_wire_varia(data, length, &f) && (frame_cb != NULL)) {
        frame_cb(&f);
    }

    return BT_GATT_ITER_CONTINUE;
}

static uint8_t on_discover(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *params)
{
    if (attr == NULL) {
        (void)memset(params, 0, sizeof(*params));

        return BT_GATT_ITER_STOP;
    }

    if (bt_uuid_cmp(params->uuid, &uuid_radar_svc.uuid) == 0) {
        const struct bt_gatt_service_val *svc = attr->user_data;

        service_end_handle = svc->end_handle;
        discover_params.uuid = &uuid_radar_meas.uuid;
        discover_params.start_handle = attr->handle + 1U;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
        if (bt_gatt_discover(conn, &discover_params) != 0) {
            LOG_ERR("radar: characteristic discovery did not start");
        }

        return BT_GATT_ITER_STOP;
    }

    if (bt_uuid_cmp(params->uuid, &uuid_radar_meas.uuid) == 0) {
        meas_handle = bt_gatt_attr_value_handle(attr);

        subscribe_params.value_handle = meas_handle;
        subscribe_params.ccc_handle = 0U;   /* the host finds it */
        subscribe_params.end_handle = service_end_handle;
        subscribe_params.disc_params = &ccc_disc_params;
        subscribe_params.notify = on_notify;
        subscribe_params.value = BT_GATT_CCC_NOTIFY;

        int err = bt_gatt_subscribe(conn, &subscribe_params);

        if ((err != 0) && (err != -EALREADY)) {
            LOG_ERR("radar: subscribe failed (%d)", err);
        } else {
            LOG_INF("radar: listening");
            if (link_cb != NULL) {
                link_cb(true);
            }
        }

        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_CONTINUE;
}

int ble_radar_client_init(radar_frame_cb_t on_frame, radar_link_cb_t on_link)
{
    frame_cb = on_frame;
    link_cb = on_link;

    return 0;
}

int ble_radar_client_start(struct bt_conn *conn)
{
    if (conn == NULL) {
        return -EINVAL;
    }

    radar_conn = conn;
    meas_handle = 0U;
    service_end_handle = 0U;

    discover_params.uuid = &uuid_radar_svc.uuid;
    discover_params.func = on_discover;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    return bt_gatt_discover(conn, &discover_params);
}

void ble_radar_client_stop(void)
{
    if ((radar_conn != NULL) && (subscribe_params.value_handle != 0U)) {
        (void)bt_gatt_unsubscribe(radar_conn, &subscribe_params);
    }
    radar_conn = NULL;
    meas_handle = 0U;
    if (link_cb != NULL) {
        link_cb(false);
    }
}

bool ble_radar_client_is_linked(void)
{
    return (radar_conn != NULL) && (meas_handle != 0U);
}
