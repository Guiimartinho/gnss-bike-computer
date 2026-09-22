/**
 * @file loc_arbiter.h
 * @brief Which of the position sources the model listens to
 *
 * The device can be told where it is by three different things, and they
 * disagree: the GNSS receiver, a PC driving a simulated ride over the
 * serial commands (`$LOC`, the Zwift mode), and a phone over Bluetooth.
 * Picking one per epoch is not "take the newest": the rule of the legacy
 * has a shape worth keeping.
 *
 * `legacy/source/model/Locator.cpp:111-134`, read line by line:
 *
 * ```
 * if   sim just arrived              -> SIM
 * else if sim is younger than 2000   -> none
 * if   gps just arrived              -> GPS
 * else if gps is younger than 1500   -> none
 * if   lns just arrived and no fix   -> LNS
 * else if lns just arrived           -> none, refused
 * -> none
 * ```
 *
 * Two things in there are easy to miss and are the reason this is a module
 * of its own:
 *
 * 1. **A source that is recent but not new gives nothing at all.** It does
 *    not fall through to the next one. That is what the `else if ... <
 *    2000 -> none` lines are for: between two frames of a simulated ride,
 *    the receiver must not slip a position in and make the bike jump.
 * 2. **The phone is refused while the receiver has a fix**, not merely
 *    while the receiver is recent. A stale fix still counts as a fix.
 *
 * Pure C on the caller's structure, so the host tests read the rule back.
 */

#ifndef MODEL_LOC_ARBITER_H
#define MODEL_LOC_ARBITER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A simulated position blocks the others for this long (`Locator.cpp:117`) */
#define LOC_ARB_SIM_BLOCK_MS    2000U

/** And a position from the receiver, for this long (`Locator.cpp:123`) */
#define LOC_ARB_GPS_BLOCK_MS    1500U

/** Where a position came from */
enum loc_arb_src {
    LOC_ARB_NONE = 0,
    LOC_ARB_GPS,
    LOC_ARB_LNS,    /**< the phone, over the Bluetooth navigation service */
    LOC_ARB_SIM     /**< a PC driving a ride over the serial commands */
};

/** What the rule needs to remember */
struct loc_arbiter {
    uint32_t seen_ms[4];    /**< uptime of the last sample of each source */
    bool fresh[4];          /**< arrived and not yet taken */
    bool started[4];        /**< a first sample ever arrived */
};

/** Start with nothing from anywhere */
void loc_arbiter_init(struct loc_arbiter *a);

/**
 * A source produced a position.
 *
 * @param src which one; LOC_ARB_NONE is ignored
 * @param now_ms uptime
 */
void loc_arbiter_feed(struct loc_arbiter *a, enum loc_arb_src src, uint32_t now_ms);

/**
 * Pick the source for this epoch, and take it.
 *
 * Taking is the point: a sample is offered once. Calling twice without a
 * new sample in between gives LOC_ARB_NONE the second time, which is what
 * `isUpdated()` does in the legacy.
 *
 * @param gps_has_fix whether the receiver has a fix right now; the phone
 * is refused while it does, however old the fix is
 */
enum loc_arb_src loc_arbiter_pick(struct loc_arbiter *a, uint32_t now_ms, bool gps_has_fix);

/** Age of a source in milliseconds, or UINT32_MAX if it never spoke */
uint32_t loc_arbiter_age(const struct loc_arbiter *a, enum loc_arb_src src, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_LOC_ARBITER_H */
