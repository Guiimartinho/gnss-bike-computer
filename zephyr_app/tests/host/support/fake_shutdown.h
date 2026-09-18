/**
 * @file fake_shutdown.h
 * @brief Fakes of the power-off side effects used by model/power_scheduler.c:
 *        stc3100_shutdown() and crash_recovery_clear_saved_state().
 */

#ifndef FAKE_SHUTDOWN_H
#define FAKE_SHUTDOWN_H

#include <stdbool.h>

#include "app_types.h"

/** Clears the counters; stc3100_shutdown() succeeds again. */
void fake_shutdown_reset(void);

/** Number of stc3100_shutdown() calls since the last reset. */
unsigned int fake_shutdown_latch_calls(void);

/** Number of crash_recovery_clear_saved_state() calls since the last reset. */
unsigned int fake_shutdown_clear_calls(void);

/** Makes stc3100_shutdown() fail with the given error (APP_OK to succeed). */
void fake_shutdown_set_latch_result(app_err_t result);

/** True if the saved state was cleared before the latch was released. */
bool fake_shutdown_cleared_first(void);

#endif /* FAKE_SHUTDOWN_H */
