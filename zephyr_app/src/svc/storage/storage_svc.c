/**
 * @file storage_svc.c
 * @brief Storage service: the card, the activity log, segments and routes
 *
 * docs/16-arquitetura-firmware.md (Serviços, Armazenamento). The thread owns
 * the FatFs volume: it mounts the card (disk SD of the devicetree), loads
 * the segments, lists the routes and writes the activity log from the
 * points the model publishes, so no other thread waits on the card. The
 * log keeps the legacy rhythm through sd_logger: one entry each 15 m, five
 * entries per write.
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#if defined(CONFIG_FAT_FILESYSTEM_ELM)
#include <ff.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#endif

#include "app/app_channels.h"
#include "app/app_svc.h"
#include "model/sd_logger.h"
#include "model/segment.h"

LOG_MODULE_REGISTER(storage_svc, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * Deepest chain: segment_load_all() through FatFs to the SD SPI, about
 * 2.2 KB with a log call (CONFIG_STACK_USAGE, docs/05); 1 KB margin.
 */
#define STORAGE_STACK_SIZE  3584
#define STORAGE_INBOX_LEN   16

#define DISK_NAME           "SD"
#define MOUNT_POINT         "/" DISK_NAME ":"

struct storage_msg {
    const struct zbus_channel *chan;
    union {
        struct app_system_state sys;
        struct app_system_cmd cmd;
        struct app_log_point point;
    } u;
};

K_MSGQ_DEFINE(storage_inbox, sizeof(struct storage_msg), STORAGE_INBOX_LEN, 4);

static void storage_listener(const struct zbus_channel *chan)
{
    struct storage_msg msg = {.chan = chan};

    if (chan == &chan_system_cmd) {
        const struct app_system_cmd *cmd = zbus_chan_const_msg(chan);

        if ((cmd->id != APP_CMD_FORMAT) && (cmd->id != APP_CMD_MSC) &&
            (cmd->id != APP_CMD_STORAGE_RESCAN)) {
            return;
        }
    }
    if (zbus_chan_msg_size(chan) > sizeof(msg.u)) {
        return;
    }
    (void)memcpy(&msg.u, zbus_chan_const_msg(chan), zbus_chan_msg_size(chan));
    app_inbox_put(&storage_inbox, &msg, "storage");
}

ZBUS_LISTENER_DEFINE(storage_lis, storage_listener);
ZBUS_CHAN_ADD_OBS(chan_system_state, storage_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_system_cmd, storage_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_log_point, storage_lis, 3);

static sd_logger_t logger;
static struct app_storage_info info;

#if defined(CONFIG_FAT_FILESYSTEM_ELM)

static FATFS fat_fs;
static struct fs_mount_t fat_mount = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
    .mnt_point = MOUNT_POINT,
};

static bool card_mount(void)
{
    if (disk_access_init(DISK_NAME) != 0) {
        LOG_WRN("no card");
        return false;
    }
    int err = fs_mount(&fat_mount);

    if (err != 0) {
        LOG_WRN("card not mounted: %d", err);
        return false;
    }
    LOG_INF("card mounted on %s", MOUNT_POINT);
    return true;
}

static void card_unmount(void)
{
    (void)fs_unmount(&fat_mount);
}

/** Routes on the root of the card: *.PAR of the legacy and *.CRS of the port */
static void list_routes(void)
{
    struct fs_dir_t dir;
    struct fs_dirent entry;

    info.nroutes = 0U;
    fs_dir_t_init(&dir);
    if (fs_opendir(&dir, MOUNT_POINT "/") != 0) {
        return;
    }
    while ((info.nroutes < APP_ROUTE_LIST_MAX) && (fs_readdir(&dir, &entry) == 0) &&
           (entry.name[0] != '\0')) {
        size_t len = strlen(entry.name);

        if ((entry.type != FS_DIR_ENTRY_FILE) || (len < 5U)) {
            continue;
        }
        const char *ext = &entry.name[len - 4U];

        if ((strcmp(ext, ".PAR") == 0) || (strcmp(ext, ".par") == 0) ||
            (strcmp(ext, ".CRS") == 0) || (strcmp(ext, ".crs") == 0)) {
            /* the whole name, extension and all: it is what opens the file */
            (void)snprintf(info.route[info.nroutes], sizeof(info.route[0]), "%.*s",
                           (int)(sizeof(info.route[0]) - 1U), entry.name);
            info.nroutes++;
        }
    }
    (void)fs_closedir(&dir);
}

#else

static bool card_mount(void)
{
    LOG_WRN("built without the FAT file system");
    return false;
}

static void card_unmount(void)
{
}

static void list_routes(void)
{
}

#endif /* CONFIG_FAT_FILESYSTEM_ELM */

static void on_point(const struct app_log_point *p)
{
    sd_log_entry_t entry;

    if (!info.mounted) {
        return;
    }
    if (!sd_logger_is_active(&logger)) {
        (void)sd_logger_start(&logger, &p->date);
    }
    sd_log_altitude_t alti = {
        .baro_alt = p->baro_alt,
        .baro_corr = p->baro_corr,
        .filt_alt = p->filt_alt,
        .gps_alt = p->loc.alt,
        .alpha_bar = p->alpha_bar,
        .alpha_zero = p->alpha_zero,
        .vit_asc = p->vit_asc,
        .b_rough = p->b_rough,
        .slope = p->slope_pct,
    };

    (void)memcpy(alti.rough, p->rough, sizeof(alti.rough));

    sd_logger_build_entry(&entry, &p->loc, &p->date, p->power_w, p->hr_bpm, p->cadence_rpm,
                          (uint16_t)(p->loc.speed * 100.0f), &alti, p->dist_m, p->climb_m);
    (void)sd_logger_add_entry(&logger, &entry, p->dist_m);
}

static void storage_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct storage_msg msg;
    bool done = false;

    (void)sd_logger_init(&logger);
    info.mounted = card_mount();
    if (info.mounted) {
        int n = segment_load_all();

        info.segments = (n > 0) ? (uint16_t)n : 0U;
        list_routes();
    }
    (void)app_publish(&chan_storage_info, &info);

    int wdt = app_wdt_add("storage");

    for (;;) {
        if (app_inbox_get(&storage_inbox, &msg, wdt, APP_SVC_TICK_MS) != 0) {
            continue;
        }
        if (msg.chan == &chan_log_point) {
            if (!done) {
                on_point(&msg.u.point);
            }
        } else if (msg.chan == &chan_system_state) {
            if ((msg.u.sys.state == APP_SYS_SHUTDOWN) && !done) {
                /* the last batch and the directory entry reach the card */
                done = true;
                if (sd_logger_is_active(&logger)) {
                    (void)sd_logger_stop(&logger);
                }
                if (info.mounted) {
                    card_unmount();
                }

                struct app_shutdown_ack ack = {.svc = APP_SVC_STORAGE};

                (void)app_publish(&chan_shutdown_ack, &ack);
            }
        } else if (msg.chan == &chan_system_cmd) {
            if (msg.u.cmd.id == APP_CMD_STORAGE_RESCAN) {
                /*
                 * A route or a segment arrived over Bluetooth
                 * (`rf/file_xfer.c`): list the storage again so the rider
                 * finds it in the menu without a reset.
                 */
                if (info.mounted) {
                    int n = segment_load_all();

                    info.segments = (n > 0) ? (uint16_t)n : 0U;
                    list_routes();
                    (void)app_publish(&chan_storage_info, &info);
                    LOG_INF("storage listed again: %u segments, %u routes",
                            (unsigned int)info.segments, (unsigned int)info.nroutes);
                }
            } else {
                /* format and USB mass storage: the USB step */
                LOG_INF("storage command %u", msg.u.cmd.id);
            }
        } else {
            /* nothing else reaches this inbox */
        }
    }
}

K_THREAD_DEFINE(storage_tid, STORAGE_STACK_SIZE, storage_thread, NULL, NULL, NULL,
                APP_PRIO_STORAGE, 0, SYS_FOREVER_MS);

void storage_svc_start(void)
{
    k_thread_name_set(storage_tid, "storage");
    k_thread_start(storage_tid);
}
