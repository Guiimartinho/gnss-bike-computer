/**
 * @file fake_shutdown.c
 * @brief Fakes of stc3100_shutdown() and crash_recovery_clear_saved_state().
 */

#include "drivers/stc3100.h"
#include "model/crash_recovery.h"

#include "fake_shutdown.h"

static unsigned int s_latch_calls;
static unsigned int s_clear_calls;
static app_err_t s_latch_result = APP_OK;
static bool s_cleared_first = true;

void fake_shutdown_reset(void)
{
    s_latch_calls = 0U;
    s_clear_calls = 0U;
    s_latch_result = APP_OK;
    s_cleared_first = true;
}

unsigned int fake_shutdown_latch_calls(void)
{
    return s_latch_calls;
}

unsigned int fake_shutdown_clear_calls(void)
{
    return s_clear_calls;
}

void fake_shutdown_set_latch_result(app_err_t result)
{
    s_latch_result = result;
}

bool fake_shutdown_cleared_first(void)
{
    return s_cleared_first;
}

app_err_t stc3100_shutdown(void)
{
    s_latch_calls++;
    if (s_clear_calls < s_latch_calls) {
        s_cleared_first = false;
    }
    return s_latch_result;
}

void crash_recovery_clear_saved_state(void)
{
    s_clear_calls++;
}
