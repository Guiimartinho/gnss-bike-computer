/**
 * @file app_cmd.h
 * @brief What the device does with a command sentence of the legacy
 *
 * The sentences come from two places, the Nordic UART Service and the USB
 * serial, and both do the same thing with them: turn them into events on
 * the channels. The reading itself is in `model/cmd_parser.c`.
 */

#ifndef APP_CMD_H
#define APP_CMD_H

#include "model/cmd_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Act on a sentence that came in
 *
 * Publishes on the channels, so it runs in whichever thread read the
 * characters; it never waits for anyone.
 */
void app_cmd_handle(const struct cmd_data *cmd);

#ifdef __cplusplus
}
#endif

#endif /* APP_CMD_H */
