/**
 * @file usb_msc.c
 * @brief USB Mass Storage Class implementation
 *
 * Provides USB Mass Storage for exposing SD card to host PC.
 * Implements switching between CDC and MSC modes.
 *
 * Follows MISRA C:2012 guidelines.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/fs/fs.h>

#ifdef CONFIG_USB_DEVICE_STACK
#include <zephyr/usb/usb_device.h>
#endif

#ifdef CONFIG_USB_DEVICE_MSC
#include <zephyr/usb/class/usb_msc.h>
#endif

#include "usb/usb_msc.h"

LOG_MODULE_REGISTER(usb_msc, CONFIG_LOG_DEFAULT_LEVEL);

/* ==========================================================================
 * Private Definitions
 * ========================================================================== */

/** SD card disk name */
#define SD_DISK_NAME    "SD"

/** Work buffer size for MSC operations */
#define MSC_WORK_BUF_SIZE   512U

/* ==========================================================================
 * Private Variables
 * ========================================================================== */

/** Current state */
static usb_msc_state_t state = USB_MSC_STATE_DISABLED;

/** Initialization flag */
static bool is_initialized;

/** Filesystem mounted flag */
static bool fs_was_mounted;

/** Disk status */
static bool disk_available;

/** Disk info */
static struct {
    uint32_t sector_count;
    uint32_t sector_size;
} disk_info;

/* ==========================================================================
 * Private Functions
 * ========================================================================== */

/**
 * @brief Check if SD card disk is available
 * @return true if available
 */
static bool check_disk_available(void)
{
#ifdef CONFIG_DISK_ACCESS
    int err;
    uint32_t sector_count = 0U;
    uint32_t sector_size = 0U;

    /* Try to initialize disk */
    err = disk_access_init(SD_DISK_NAME);
    if (err != 0) {
        LOG_WRN("SD disk not available: %d", err);
        return false;
    }

    /* Get disk properties */
    err = disk_access_ioctl(SD_DISK_NAME, DISK_IOCTL_GET_SECTOR_COUNT, &sector_count);
    if (err != 0) {
        LOG_WRN("Failed to get sector count: %d", err);
        return false;
    }

    err = disk_access_ioctl(SD_DISK_NAME, DISK_IOCTL_GET_SECTOR_SIZE, &sector_size);
    if (err != 0) {
        LOG_WRN("Failed to get sector size: %d", err);
        sector_size = 512U;  /* Default */
    }

    disk_info.sector_count = sector_count;
    disk_info.sector_size = sector_size;

    LOG_INF("SD card: %u sectors, %u bytes/sector, total %u MB",
            sector_count, sector_size,
            (sector_count * sector_size) / (1024U * 1024U));

    return true;
#else
    return false;
#endif
}

/**
 * @brief Unmount filesystem to allow MSC access
 * @return APP_OK on success
 * @note Actual unmount requires fs_mount_t structure from SD card module
 */
__attribute__((unused))
static app_err_t unmount_filesystem(void)
{
#ifdef CONFIG_FILE_SYSTEM
    /* Note: fs_unmount requires struct fs_mount_t* from SD card module.
     * When SD card module is fully integrated, get mount point from there.
     * For now, we just track that FS should be unmounted. */
    fs_was_mounted = true;
    LOG_INF("Filesystem should be unmounted for MSC access");
#endif
    return APP_OK;
}

/**
 * @brief Remount filesystem after MSC disconnect
 * @return APP_OK on success
 */
__attribute__((unused))
static app_err_t remount_filesystem(void)
{
#ifdef CONFIG_FILE_SYSTEM
    if (fs_was_mounted) {
        /* Filesystem should be remounted by the SD card handler */
        LOG_INF("Filesystem can be remounted");
        fs_was_mounted = false;
    }
#endif
    return APP_OK;
}

/* ==========================================================================
 * USB MSC Callbacks (when CONFIG_USB_DEVICE_MSC is enabled)
 * ========================================================================== */

#ifdef CONFIG_USB_DEVICE_MSC

/**
 * @brief MSC interface configuration callback
 * @note Will be used when MSC is properly configured via devicetree
 */
__attribute__((unused))
static void msc_status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
    (void)param;

    switch (status) {
    case USB_DC_CONFIGURED:
        LOG_INF("MSC configured");
        state = USB_MSC_STATE_ACTIVE;
        break;

    case USB_DC_DISCONNECTED:
        LOG_INF("MSC disconnected");
        state = USB_MSC_STATE_READY;
        (void)remount_filesystem();
        break;

    case USB_DC_SUSPEND:
        LOG_DBG("MSC suspended");
        break;

    case USB_DC_RESUME:
        LOG_DBG("MSC resumed");
        break;

    default:
        break;
    }
}

#endif /* CONFIG_USB_DEVICE_MSC */

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

app_err_t usb_msc_init(void)
{
    if (is_initialized) {
        return APP_ERR_ALREADY_INIT;
    }

    disk_available = check_disk_available();

    if (!disk_available) {
        LOG_WRN("No SD card detected - MSC will be unavailable");
        state = USB_MSC_STATE_ERROR;
    } else {
        state = USB_MSC_STATE_DISABLED;
    }

#ifdef CONFIG_USB_DEVICE_MSC
    LOG_INF("USB MSC initialized (MSC support enabled)");
    if (disk_available) {
        state = USB_MSC_STATE_READY;
    }
#else
    LOG_INF("USB MSC initialized (MSC support disabled in Kconfig)");
    LOG_INF("To enable: CONFIG_USB_DEVICE_MSC=y + devicetree config");
#endif

    is_initialized = true;
    return APP_OK;
}

app_err_t usb_msc_enable(void)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (!disk_available) {
        LOG_ERR("Cannot enable MSC - no disk available");
        return APP_ERR_NOT_FOUND;
    }

#ifdef CONFIG_USB_DEVICE_MSC
    if (state == USB_MSC_STATE_ACTIVE) {
        return APP_OK;  /* Already active */
    }

    /* Unmount filesystem first */
    (void)unmount_filesystem();

    /* MSC class is registered at compile time via Kconfig/devicetree.
     * The USB device needs to be enabled and configured.
     *
     * In a typical setup:
     * 1. USB device is initialized at boot
     * 2. MSC class is auto-registered if CONFIG_USB_DEVICE_MSC=y
     * 3. Host PC will see the device when USB cable is connected
     */

    state = USB_MSC_STATE_ACTIVE;
    LOG_INF("USB MSC mode enabled - SD card exposed to host");

    return APP_OK;
#else
    LOG_WRN("USB MSC not available in this build");
    LOG_WRN("Rebuild with CONFIG_USB_DEVICE_MSC=y");
    return APP_ERR_NOT_INIT;
#endif
}

app_err_t usb_msc_disable(void)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

#ifdef CONFIG_USB_DEVICE_MSC
    if ((state == USB_MSC_STATE_READY) || (state == USB_MSC_STATE_DISABLED)) {
        return APP_OK;  /* Already disabled */
    }

    /* Note: Proper disable would require USB reconfiguration.
     * For now, we just update state and allow remount. */
    (void)remount_filesystem();

    state = USB_MSC_STATE_READY;
    LOG_INF("USB MSC mode disabled");

    return APP_OK;
#else
    return APP_OK;
#endif
}

usb_msc_state_t usb_msc_get_state(void)
{
    return state;
}

bool usb_msc_is_active(void)
{
    return (state == USB_MSC_STATE_ACTIVE);
}

bool usb_msc_is_available(void)
{
    return disk_available;
}

app_err_t usb_msc_get_disk_info(uint32_t *sector_count, uint32_t *sector_size)
{
    if (!is_initialized || !disk_available) {
        return APP_ERR_NOT_INIT;
    }

    if (sector_count != NULL) {
        *sector_count = disk_info.sector_count;
    }

    if (sector_size != NULL) {
        *sector_size = disk_info.sector_size;
    }

    return APP_OK;
}

void usb_msc_process(void)
{
    /* MSC processing is handled automatically by USB stack */

#ifdef CONFIG_USB_DEVICE_MSC
    /* Could add periodic status checks here if needed */
#endif
}

const char *usb_msc_state_str(usb_msc_state_t msc_state)
{
    switch (msc_state) {
    case USB_MSC_STATE_DISABLED:
        return "Disabled";
    case USB_MSC_STATE_READY:
        return "Ready";
    case USB_MSC_STATE_ACTIVE:
        return "Active";
    case USB_MSC_STATE_ERROR:
        return "Error";
    default:
        return "Unknown";
    }
}
