/**
 * @file mode_fsm.h
 * @brief Which kind of ride the device is in (docs/16, Modos)
 *
 * Five modes, in two families:
 *
 * ```
 * outdoor                      indoor
 * ├── CRS   free ride          ├── FEC    trainer
 * ├── PRC   following a route  └── ZWIFT  a ride driven by a PC
 * └── DBG   CRS with the
 *           diagnostics screen
 * ```
 *
 * The families are parent states and not a helper function, because the
 * difference is real and the rest of the firmware asks about it: outdoors
 * the position comes from the receiver and feeds the distance; indoors it
 * comes from the trainer or from the PC, and a fix means nothing.
 *
 * The machine is deliberately **not** the `boucle__change_mode()` of the
 * legacy (`legacy/source/model/Boucle.cpp:101-142`). That one frees the
 * route points on the way out (the port keeps them in a static array, so
 * there is nothing to free), resets the charge counter of the STC3100 (a
 * gauge the new board does not carry) and, most of all, guards nothing: it
 * lets the rider walk into any mode at any moment. Three rules were added
 * here instead, each of which was a way to lose a ride:
 *
 * 1. **A route mode needs a route.** Entering PRC with nothing loaded left
 *    the rider on a navigation screen with no navigation.
 * 2. **A recorded ride does not cross families.** Going from the road to
 *    the trainer in the middle of a recording used to mix simulated or
 *    trainer data into the file of a real ride.
 * 3. **Session numbers belong to the activity, not to the mode.** Nothing
 *    here touches the power zones or the suffer score. They start with the
 *    ride and end with it.
 *
 * The hardware and the other modules stay outside, behind `mode_fsm_ops`,
 * so every rule above runs in the host tests.
 */

#ifndef MODEL_MODE_FSM_H
#define MODEL_MODE_FSM_H

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/smf.h>

#include "app/app_events.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Why a mode change did not happen */
enum mode_refusal {
    MODE_REFUSED_NONE = 0,
    MODE_REFUSED_UNKNOWN,       /**< no such mode */
    MODE_REFUSED_SAME,          /**< already there */
    MODE_REFUSED_NO_ROUTE,      /**< PRC without a route loaded */
    MODE_REFUSED_RECORDING      /**< a ride is being recorded, and the
                                     families differ */
};

/** What the machine asks of the model service */
struct mode_fsm_ops {
    /** The mode in force changed */
    void (*publish)(enum app_mode mode, void *user);
    /** Start following the loaded route (entering PRC) */
    void (*route_start)(void *user);
    /** Stop following it (leaving PRC) */
    void (*route_stop)(void *user);
    /**
     * A change was refused. The interface turns this into a message; the
     * rider is never left wondering why the key did nothing.
     */
    void (*refused)(enum app_mode wanted, enum mode_refusal why, void *user);
    void *user;
};

struct mode_fsm {
    struct smf_ctx ctx;         /**< first member, for SMF_CTX() */
    const struct mode_fsm_ops *ops;
    enum app_mode mode;
    bool route_loaded;          /**< a route file is in memory */
    bool recording;             /**< a ride is being recorded */
};

/**
 * @brief Start the machine in CRS, the free ride
 *
 * CRS is the mode that needs nothing: no route, no trainer, no PC.
 */
void mode_fsm_init(struct mode_fsm *f, const struct mode_fsm_ops *ops);

/**
 * @brief Ask for a mode
 * @return true if the machine moved
 */
bool mode_fsm_select(struct mode_fsm *f, int32_t mode);

/** Tell the machine whether a route is loaded */
void mode_fsm_set_route_loaded(struct mode_fsm *f, bool loaded);

/** Tell the machine whether a ride is being recorded */
void mode_fsm_set_recording(struct mode_fsm *f, bool recording);

/** Mode in force */
enum app_mode mode_fsm_mode(const struct mode_fsm *f);

/** Whether this mode rides outdoors, on the receiver */
bool mode_is_outdoor(enum app_mode mode);

/** Whether the mode in force rides outdoors */
bool mode_fsm_is_outdoor(const struct mode_fsm *f);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_MODE_FSM_H */
