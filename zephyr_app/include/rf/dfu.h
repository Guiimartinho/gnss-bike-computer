/**
 * @file dfu.h
 * @brief Firmware update over Bluetooth (mcumgr SMP)
 *
 * The phone app talks SMP to the device (`docs/07-radio-ant-ble.md`, seção
 * Atualização por BLE); this module is what the firmware puts around it:
 * it confirms the image that is running, tells the rest of the device how
 * the upload is going and refuses an upload that would be unsafe
 * (`model/dfu_state.c`).
 *
 * Without CONFIG_MCUMGR every function here does nothing, so the callers
 * stay free of #if.
 */

#ifndef RF_DFU_H
#define RF_DFU_H

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the mcumgr callbacks and confirm the running image
 *
 * Called once by the radio service, after the Bluetooth stack is up.
 */
app_err_t rf_dfu_init(void);

/**
 * @brief Tell the module what the device is doing
 *
 * The radio service feeds this from the mode and the power channels; an
 * upload that arrives while a ride is being recorded, or on a low battery
 * off the charger, is refused.
 */
void rf_dfu_set_conditions(bool ride_active, bool usb_present, uint8_t battery_pct);

/** @brief Whether an update is being received or waits for the reset */
bool rf_dfu_is_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* RF_DFU_H */
