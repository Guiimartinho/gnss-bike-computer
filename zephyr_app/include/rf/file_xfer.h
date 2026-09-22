/**
 * @file file_xfer.h
 * @brief Routes and segments arriving over Bluetooth (mcumgr file group)
 */

#ifndef RF_FILE_XFER_H
#define RF_FILE_XFER_H

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the file access rules of the device
 *
 * Called once by the radio service, after the Bluetooth stack is up.
 * Without CONFIG_MCUMGR_GRP_FS it does nothing.
 */
app_err_t rf_file_xfer_init(void);

#ifdef __cplusplus
}
#endif

#endif /* RF_FILE_XFER_H */
