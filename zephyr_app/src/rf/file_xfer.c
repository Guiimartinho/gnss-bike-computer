/**
 * @file file_xfer.c
 * @brief Routes and segments arriving over Bluetooth
 *
 * The phone sends a course exported from Strava or Komoot with the file
 * group of mcumgr, over the same SMP link the firmware update uses
 * (`docs/09-armazenamento-usb.md`, seção Arquivos pelo telefone). Nothing
 * of the protocol is written here: what this file adds is the rule about
 * what a transfer may touch (`model/file_policy.c`) and the word to the
 * rest of the device when a file lands, so the new route shows up in the
 * menu without a reset.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_MCUMGR_GRP_FS)
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/mgmt/mcumgr/grp/fs_mgmt/fs_mgmt_callbacks.h>
#endif

#include "app/app_channels.h"
#include "model/file_policy.h"
#include "rf/file_xfer.h"

LOG_MODULE_REGISTER(rf_file_xfer, CONFIG_LOG_DEFAULT_LEVEL);

#if defined(CONFIG_MCUMGR_GRP_FS)

/** Name of the last file written, to tell the rider what arrived */
static char last_name[24];
static bool wrote_something;

static enum file_access access_of(enum fs_mgmt_file_access_types type)
{
    switch (type) {
    case FS_MGMT_FILE_ACCESS_WRITE:
        return FILE_ACCESS_WRITE;
    case FS_MGMT_FILE_ACCESS_STATUS:
        return FILE_ACCESS_STATUS;
    case FS_MGMT_FILE_ACCESS_HASH_CHECKSUM:
        return FILE_ACCESS_HASH;
    case FS_MGMT_FILE_ACCESS_READ:
    default:
        return FILE_ACCESS_READ;
    }
}

/** The name inside the root, for the notification */
static const char *short_name(const char *path)
{
    const char *slash = strrchr(path, '/');

    return (slash != NULL) ? (slash + 1) : path;
}

/* the signature is the mgmt_cb typedef of Zephyr: `data` cannot be const */
static enum mgmt_cb_return file_callback(uint32_t event, enum mgmt_cb_return prev_status,
                                         int32_t *rc, uint16_t *group, bool *abort_more,
                                         /* cppcheck-suppress constParameterCallback */
                                         void *data, size_t data_size)
{
    ARG_UNUSED(prev_status);
    ARG_UNUSED(group);
    ARG_UNUSED(abort_more);

    const struct fs_mgmt_file_access *fa = data;

    if ((fa == NULL) || (data_size < sizeof(*fa)) || (fa->filename == NULL)) {
        return MGMT_CB_OK;
    }

    if (event == MGMT_EVT_OP_FS_MGMT_FILE_ACCESS) {
        enum file_access access = access_of(fa->access);

        if (!file_policy_allows(fa->filename, access)) {
            LOG_WRN("refused %s on %s", (access == FILE_ACCESS_WRITE) ? "write" : "read",
                    fa->filename);
            *rc = MGMT_ERR_EACCESSDENIED;

            return MGMT_CB_ERROR_RC;
        }

        if (access == FILE_ACCESS_WRITE) {
            (void)snprintf(last_name, sizeof(last_name), "%s", short_name(fa->filename));
            wrote_something = true;
        }

        return MGMT_CB_OK;
    }

    /* the transfer ended: let the storage see what arrived */
    if ((event == MGMT_EVT_OP_FS_MGMT_FILE_ACCESS_DONE) && wrote_something) {
        enum file_kind kind = file_policy_kind(fa->filename);
        struct app_system_cmd cmd = {.id = APP_CMD_STORAGE_RESCAN, .arg = 0};

        wrote_something = false;
        LOG_INF("file %s arrived", last_name);
        (void)app_publish(&chan_system_cmd, &cmd);
        app_notify((kind == FILE_KIND_ROUTE) ? "PRC" : "SEG", "Recebido", last_name, true, 0U);
    }

    return MGMT_CB_OK;
}

static struct mgmt_callback file_cb = {
    .callback = file_callback,
    .event_id = (MGMT_EVT_OP_FS_MGMT_FILE_ACCESS | MGMT_EVT_OP_FS_MGMT_FILE_ACCESS_DONE),
};

app_err_t rf_file_xfer_init(void)
{
    mgmt_callback_register(&file_cb);
    LOG_INF("file transfer ready");

    return APP_OK;
}

#else /* CONFIG_MCUMGR_GRP_FS */

app_err_t rf_file_xfer_init(void)
{
    return APP_OK;
}

#endif /* CONFIG_MCUMGR_GRP_FS */
