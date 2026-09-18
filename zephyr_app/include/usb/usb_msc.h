/**
 * @file usb_msc.h
 * @brief USB Mass Storage Class interface
 *
 * Provides USB Mass Storage for exposing SD card over USB.
 * Follows MISRA C:2012 guidelines.
 */

#ifndef USB_MSC_H
#define USB_MSC_H

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Type Definitions
 * ========================================================================== */

/** USB MSC state */
typedef enum {
    USB_MSC_STATE_DISABLED = 0,     /**< MSC functionality disabled */
    USB_MSC_STATE_READY,            /**< MSC ready but not connected */
    USB_MSC_STATE_ACTIVE,           /**< MSC active and connected to host */
    USB_MSC_STATE_ERROR             /**< Error (no disk, etc.) */
} usb_msc_state_t;

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

/**
 * @brief Initialize USB MSC subsystem
 * @return APP_OK on success
 */
app_err_t usb_msc_init(void);

/**
 * @brief Enable USB MSC mode
 * @note This will unmount the SD card from the filesystem
 * @return APP_OK on success
 */
app_err_t usb_msc_enable(void);

/**
 * @brief Disable USB MSC mode
 * @note This allows the filesystem to remount the SD card
 * @return APP_OK on success
 */
app_err_t usb_msc_disable(void);

/**
 * @brief Get current USB MSC state
 * @return Current state
 */
usb_msc_state_t usb_msc_get_state(void);

/**
 * @brief Check if USB MSC is active
 * @return true if MSC is active and connected
 */
bool usb_msc_is_active(void);

/**
 * @brief Check if USB MSC is available (disk present)
 * @return true if MSC can be enabled
 */
bool usb_msc_is_available(void);

/**
 * @brief Get SD card disk information
 * @param sector_count Pointer to store sector count (can be NULL)
 * @param sector_size Pointer to store sector size (can be NULL)
 * @return APP_OK on success
 */
app_err_t usb_msc_get_disk_info(uint32_t *sector_count, uint32_t *sector_size);

/**
 * @brief Process USB MSC events
 */
void usb_msc_process(void);

/**
 * @brief Get string representation of MSC state
 * @param state MSC state
 * @return State string
 */
const char *usb_msc_state_str(usb_msc_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* USB_MSC_H */
