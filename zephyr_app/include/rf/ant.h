/**
 * @file ant.h
 * @brief ANT radio stack start-up on the sdk-ant add-on
 *
 * Built only with CONFIG_ANT (tools/fw/fw.sh or build.bat with ANT=1): the
 * ANT for nRF Connect SDK add-on (sdk-ant v2.1.1) on NCS v3.3.0, through
 * modules/ant_ncs33_compat. The profiles, the background search and the
 * pairing of legacy/rf/ come in phase 4 of the roadmap.
 */

#ifndef RF_ANT_H
#define RF_ANT_H

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the ANT stack and load the ANT+ network key
 *
 * Port of ant_stack_init() in legacy/rf/ant.c:139-146, which enabled the
 * S340 ANT stack and set the ANT+ key on network 0. The name changed:
 * sdk-ant already defines ant_stack_init() in its stack API. Call it before
 * bt_enable(), as the sdk-ant sample with BLE and ANT does.
 *
 * @return APP_OK, or APP_ERR_IO when the stack or the key fails
 */
app_err_t rf_ant_init(void);

#ifdef __cplusplus
}
#endif

#endif /* RF_ANT_H */
