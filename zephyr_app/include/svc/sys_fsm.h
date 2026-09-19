/**
 * @file sys_fsm.h
 * @brief System and energy state machine (docs/16-arquitetura-firmware.md, Sistema e energia)
 *
 * Runs in the energy service on the Zephyr SMF. The hardware stays outside:
 * the service passes the events in and does what the two operations ask
 * (publish the state, cut the power), so the rules run on the host tests.
 *
 *   Partida -> Ligado: the boot finished
 *   Ligado -> Desligando: menu, long centre key, 15 min without a ping
 *                         (legacy power_scheduler), battery at its end
 *   Ligado -> MSC: $DWN,16 or the menu; only a reset leaves it
 *   Desligando -> Desligado: every service acknowledged, or 5 s passed
 *
 * The ping of the automatic power-off follows the legacy: each location in
 * CRS and PRC (BoucleCRS.cpp:197) and each trainer update in FEC
 * (BoucleFEC.cpp:83).
 */

#ifndef SYS_FSM_H
#define SYS_FSM_H

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/smf.h>

#include "app/app_events.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Longest wait for the services to finish their part of the shutdown */
#define SYS_SHUTDOWN_TIMEOUT_MS     5000U

/** Events of the machine */
enum sys_ev {
    SYS_EV_TICK = 0,            /**< periodic, about once a second */
    SYS_EV_READY,               /**< the services started */
    SYS_EV_CMD,                 /**< arg: enum app_cmd_id */
    SYS_EV_ACK,                 /**< arg: enum app_svc_id */
    SYS_EV_FIX,                 /**< a location with a fix */
    SYS_EV_TRAINER,             /**< trainer data */
    SYS_EV_MODE,                /**< arg: enum app_mode */
    SYS_EV_VBUS,                /**< arg: 1 present, 0 absent */
    SYS_EV_BATT_CRITICAL        /**< the gauge flags the end of the battery */
};

/** What the machine asks of the energy service */
struct sys_fsm_ops {
    /** Publish the new state on chan_system_state */
    void (*publish)(enum app_sys_state state, void *user);
    /** Cut the power: ship mode without VBUS, System OFF with it; may return on a DK */
    void (*power_off)(bool vbus, void *user);
    void *user;
};

struct sys_fsm {
    struct smf_ctx ctx;         /**< first member, for SMF_CTX() */
    const struct sys_fsm_ops *ops;
    uint32_t ack_mask;          /**< services that take part in the shutdown */
    uint32_t acks;
    uint32_t since_ms;          /**< entry time of the shutdown */
    enum app_sys_state state;
    uint8_t mode;               /**< enum app_mode */
    bool vbus;
    enum sys_ev ev;             /**< event being run */
    int32_t arg;
};

/**
 * @brief Start the machine in Partida
 * @param ack_mask Bits (1 << enum app_svc_id) of the services that acknowledge
 */
void sys_fsm_init(struct sys_fsm *f, const struct sys_fsm_ops *ops, uint32_t ack_mask);

/** Run one event */
void sys_fsm_event(struct sys_fsm *f, enum sys_ev ev, int32_t arg);

/** State now */
enum app_sys_state sys_fsm_state(const struct sys_fsm *f);

#ifdef __cplusplus
}
#endif

#endif /* SYS_FSM_H */
