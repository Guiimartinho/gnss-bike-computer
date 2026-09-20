/**
 * @file dfu.c
 * @brief Firmware update over Bluetooth (mcumgr SMP)
 *
 * mcumgr does the transfer and MCUboot does the swap; what is here is the
 * part that belongs to this device: confirm the image that is running so
 * MCUboot stops reverting it, publish how the upload is going so the screen
 * can show it, and refuse an upload that would cost the rider a ride or run
 * the battery out in the middle.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_MCUMGR)
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h>
#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt_callbacks.h>
#endif

#include "app/app_channels.h"
#include "model/dfu_state.h"
#include "rf/dfu.h"

LOG_MODULE_REGISTER(rf_dfu, CONFIG_LOG_DEFAULT_LEVEL);

#if defined(CONFIG_MCUMGR)

static struct dfu_state state;
static struct dfu_conditions conditions = {
    .ride_active = false,
    .usb_present = false,
    .battery_pct = 100U,
};

/** Last percentage published, so the screen is not woken for nothing */
static uint8_t published_pct = UINT8_MAX;

static void publish(void)
{
    struct app_dfu msg = {
        .phase = (uint8_t)state.phase,
        .percent = dfu_state_percent(&state),
    };

    published_pct = msg.percent;
    (void)app_publish(&chan_dfu, &msg);
}

/* the signature is the mgmt_cb typedef of Zephyr: `data` cannot be const */
static enum mgmt_cb_return dfu_callback(uint32_t event, enum mgmt_cb_return prev_status,
                                        int32_t *rc, uint16_t *group, bool *abort_more,
                                        /* cppcheck-suppress constParameterCallback */
                                        void *data, size_t data_size)
{
    ARG_UNUSED(prev_status);
    ARG_UNUSED(group);
    ARG_UNUSED(abort_more);

    switch (event) {
    case MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK: {
        const struct img_mgmt_upload_check *check = data;

        if ((check == NULL) || (data_size < sizeof(*check)) || (check->req == NULL)) {
            break;
        }

        if (!dfu_state_allow(&conditions)) {
            LOG_WRN("update refused: riding %d, usb %d, battery %u%%",
                    (int)conditions.ride_active, (int)conditions.usb_present,
                    conditions.battery_pct);
            dfu_state_stopped(&state, false);
            publish();
            app_notify("DFU", "Atualizacao recusada", NULL, false, 0U);
            *rc = MGMT_ERR_EACCESSDENIED;

            return MGMT_CB_ERROR_RC;
        }

        size_t off = check->req->off;
        size_t size = check->req->size;

        dfu_state_progress(&state, (off == SIZE_MAX) ? 0U : (uint32_t)off,
                           (size == SIZE_MAX) ? 0U : (uint32_t)size);

        /* one message per whole percent: the screen redraws at its own pace */
        if (dfu_state_percent(&state) != published_pct) {
            publish();
        }
        break;
    }

    case MGMT_EVT_OP_IMG_MGMT_DFU_STARTED:
        LOG_INF("update started");
        dfu_state_started(&state);
        publish();
        app_notify("DFU", "Atualizando", NULL, true, 0U);
        break;

    case MGMT_EVT_OP_IMG_MGMT_DFU_PENDING:
        LOG_INF("update image in place");
        dfu_state_pending(&state);
        publish();
        app_notify("DFU", "Reinicie para aplicar", NULL, true, 0U);
        break;

    case MGMT_EVT_OP_IMG_MGMT_DFU_STOPPED:
        if (state.phase != DFU_PHASE_DONE) {
            LOG_WRN("update stopped");
            dfu_state_stopped(&state, false);
            publish();
        }
        break;

    default:
        break;
    }

    return MGMT_CB_OK;
}

static struct mgmt_callback dfu_cb = {
    .callback = dfu_callback,
    .event_id = (MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK | MGMT_EVT_OP_IMG_MGMT_DFU_STARTED |
                 MGMT_EVT_OP_IMG_MGMT_DFU_PENDING | MGMT_EVT_OP_IMG_MGMT_DFU_STOPPED),
};

app_err_t rf_dfu_init(void)
{
    dfu_state_init(&state);
    mgmt_callback_register(&dfu_cb);

    /*
     * MCUboot boots a new image once and takes it back at the next reset
     * unless it is confirmed. Getting this far means the services are up,
     * so the image works: keep it (`boot_write_img_confirmed()`).
     */
    if (!boot_is_img_confirmed()) {
        int err = boot_write_img_confirmed();

        if (err != 0) {
            LOG_ERR("cannot confirm the running image: %d", err);
            return APP_ERR_IO;
        }
        LOG_INF("running image confirmed");
        app_notify("DFU", "Firmware atualizado", NULL, true, 0U);
    }

    return APP_OK;
}

void rf_dfu_set_conditions(bool ride_active, bool usb_present, uint8_t battery_pct)
{
    conditions.ride_active = ride_active;
    conditions.usb_present = usb_present;
    conditions.battery_pct = battery_pct;
}

bool rf_dfu_is_busy(void)
{
    return dfu_state_is_busy(&state);
}

#else /* CONFIG_MCUMGR */

app_err_t rf_dfu_init(void)
{
    return APP_OK;
}

void rf_dfu_set_conditions(bool ride_active, bool usb_present, uint8_t battery_pct)
{
    ARG_UNUSED(ride_active);
    ARG_UNUSED(usb_present);
    ARG_UNUSED(battery_pct);
}

bool rf_dfu_is_busy(void)
{
    return false;
}

#endif /* CONFIG_MCUMGR */
