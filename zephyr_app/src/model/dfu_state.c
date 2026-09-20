/**
 * @file dfu_state.c
 * @brief State and rules of a firmware update over the air
 */

#include "model/dfu_state.h"

void dfu_state_init(struct dfu_state *state)
{
    if (state == NULL) {
        return;
    }

    state->phase = DFU_PHASE_IDLE;
    state->total = 0U;
    state->written = 0U;
}

bool dfu_state_allow(const struct dfu_conditions *cond)
{
    if (cond == NULL) {
        return false;
    }

    if (cond->ride_active) {
        return false;
    }

    if (cond->usb_present) {
        return true;
    }

    return (cond->battery_pct >= DFU_MIN_BATTERY_PCT);
}

void dfu_state_started(struct dfu_state *state)
{
    if (state == NULL) {
        return;
    }

    state->phase = DFU_PHASE_RUNNING;
    state->total = 0U;
    state->written = 0U;
}

void dfu_state_progress(struct dfu_state *state, uint32_t offset, uint32_t total)
{
    if (state == NULL) {
        return;
    }

    /* A chunk can arrive before the started event of the transport */
    state->phase = DFU_PHASE_RUNNING;

    if (total > 0U) {
        state->total = total;
    }

    /* The client may resend a piece: the bar never walks backwards */
    if (offset > state->written) {
        state->written = offset;
    }
    if ((state->total > 0U) && (state->written > state->total)) {
        state->written = state->total;
    }
}

void dfu_state_pending(struct dfu_state *state)
{
    if (state == NULL) {
        return;
    }

    state->phase = DFU_PHASE_DONE;
    if (state->total > 0U) {
        state->written = state->total;
    }
}

void dfu_state_stopped(struct dfu_state *state, bool ok)
{
    if (state == NULL) {
        return;
    }

    /* The image was already marked: the stop is the end of the transfer */
    if (state->phase == DFU_PHASE_DONE) {
        return;
    }

    state->phase = ok ? DFU_PHASE_DONE : DFU_PHASE_FAILED;
}

uint8_t dfu_state_percent(const struct dfu_state *state)
{
    if (state == NULL) {
        return 0U;
    }

    if (state->phase == DFU_PHASE_DONE) {
        return 100U;
    }
    if ((state->phase != DFU_PHASE_RUNNING) || (state->total == 0U)) {
        return 0U;
    }

    uint32_t pct = (state->written * 100U) / state->total;

    return (pct > 100U) ? 100U : (uint8_t)pct;
}

bool dfu_state_is_busy(const struct dfu_state *state)
{
    if (state == NULL) {
        return false;
    }

    return (state->phase == DFU_PHASE_RUNNING) || (state->phase == DFU_PHASE_DONE);
}
