/**
 * @file sys_fsm.c
 * @brief System and energy state machine on the Zephyr SMF
 *
 * States (docs/16, Sistema e energia): Partida, Ligado and MSC share the
 * parent "running", which takes the shutdown requests; Desligando waits for
 * the services; Desligado cuts the power.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>

#include "svc/sys_fsm.h"
#include "model/power_scheduler.h"

LOG_MODULE_REGISTER(sys_fsm, CONFIG_LOG_DEFAULT_LEVEL);

enum sys_state_id {
    S_RUNNING = 0,
    S_BOOT,
    S_ON,
    S_MSC,
    S_SHUTDOWN,
    S_OFF,
    S_COUNT
};

static const struct smf_state sys_states[S_COUNT];

static void enter(struct sys_fsm *f, enum app_sys_state state)
{
    f->state = state;
    if ((f->ops != NULL) && (f->ops->publish != NULL)) {
        f->ops->publish(state, f->ops->user);
    }
}

/* ---- running: the parent of Partida, Ligado and MSC -------------------- */

static enum smf_state_result running_run(void *o)
{
    struct sys_fsm *f = o;

    switch (f->ev) {
    case SYS_EV_CMD:
        if (f->arg == APP_CMD_SHUTDOWN) {
            LOG_INF("shutdown requested");
            smf_set_state(SMF_CTX(f), &sys_states[S_SHUTDOWN]);
        }
        break;
    case SYS_EV_BATT_CRITICAL:
        LOG_WRN("battery at its end: shutting down");
        smf_set_state(SMF_CTX(f), &sys_states[S_SHUTDOWN]);
        break;
    default:
        break;
    }
    return SMF_EVENT_HANDLED;
}

/* ---- Partida ------------------------------------------------------------- */

static void boot_entry(void *o)
{
    enter(o, APP_SYS_BOOT);
}

static enum smf_state_result boot_run(void *o)
{
    struct sys_fsm *f = o;

    if (f->ev == SYS_EV_READY) {
        smf_set_state(SMF_CTX(f), &sys_states[S_ON]);
        return SMF_EVENT_HANDLED;
    }
    return SMF_EVENT_PROPAGATE;
}

/* ---- Ligado -------------------------------------------------------------- */

static void on_entry(void *o)
{
    power_scheduler_init();
    enter(o, APP_SYS_ON);
}

static bool mode_rides_outdoors(uint8_t mode)
{
    /* CRS, PRC and the DBG screen run the CRS loop (legacy Menuable.cpp:83) */
    return (mode == APP_MODE_ID_CRS) || (mode == APP_MODE_ID_PRC) || (mode == APP_MODE_ID_DBG);
}

static enum smf_state_result on_run(void *o)
{
    struct sys_fsm *f = o;

    switch (f->ev) {
    case SYS_EV_TICK:
        if (power_scheduler_run()) {
            LOG_INF("no activity for %u min: shutting down",
                    (unsigned int)POWER_SCHEDULER_MAX_IDLE_MIN);
            smf_set_state(SMF_CTX(f), &sys_states[S_SHUTDOWN]);
        }
        return SMF_EVENT_HANDLED;
    case SYS_EV_FIX:
        if (mode_rides_outdoors(f->mode)) {
            power_scheduler_ping(POWER_PING_CRS);
        }
        return SMF_EVENT_HANDLED;
    case SYS_EV_TRAINER:
        if (f->mode == APP_MODE_ID_FEC) {
            power_scheduler_ping(POWER_PING_FEC);
        }
        return SMF_EVENT_HANDLED;
    case SYS_EV_CMD:
        if (f->arg == APP_CMD_MSC) {
            smf_set_state(SMF_CTX(f), &sys_states[S_MSC]);
            return SMF_EVENT_HANDLED;
        }
        break;
    default:
        break;
    }
    return SMF_EVENT_PROPAGATE;
}

/* ---- MSC: the card is on the USB; the legacy leaves it by a reset --------- */

static void msc_entry(void *o)
{
    enter(o, APP_SYS_MSC);
}

static enum smf_state_result msc_run(void *o)
{
    (void)o;
    return SMF_EVENT_PROPAGATE;
}

/* ---- Desligando ---------------------------------------------------------- */

static void shutdown_entry(void *o)
{
    struct sys_fsm *f = o;

    f->acks = 0U;
    f->since_ms = k_uptime_get_32();
    enter(f, APP_SYS_SHUTDOWN);
}

static enum smf_state_result shutdown_run(void *o)
{
    struct sys_fsm *f = o;

    if ((f->ev == SYS_EV_ACK) && (f->arg >= 0) && (f->arg < 32)) {
        f->acks |= (1UL << (uint32_t)f->arg);
    }
    if ((f->acks & f->ack_mask) == f->ack_mask) {
        smf_set_state(SMF_CTX(f), &sys_states[S_OFF]);
    } else if ((k_uptime_get_32() - f->since_ms) >= SYS_SHUTDOWN_TIMEOUT_MS) {
        LOG_WRN("shutdown: services 0x%02x did not answer, going off anyway",
                (unsigned int)(f->ack_mask & ~f->acks));
        smf_set_state(SMF_CTX(f), &sys_states[S_OFF]);
    }
    return SMF_EVENT_HANDLED;
}

/* ---- Desligado ----------------------------------------------------------- */

static void off_entry(void *o)
{
    struct sys_fsm *f = o;

    enter(f, APP_SYS_OFF);
    if ((f->ops != NULL) && (f->ops->power_off != NULL)) {
        f->ops->power_off(f->vbus, f->ops->user);
    }
}

static enum smf_state_result off_run(void *o)
{
    /* Still running: a DK without a power switch. Nothing else to do. */
    (void)o;
    return SMF_EVENT_HANDLED;
}

static const struct smf_state sys_states[S_COUNT] = {
    [S_RUNNING] = SMF_CREATE_STATE(NULL, running_run, NULL, NULL, NULL),
    [S_BOOT] = SMF_CREATE_STATE(boot_entry, boot_run, NULL, &sys_states[S_RUNNING], NULL),
    [S_ON] = SMF_CREATE_STATE(on_entry, on_run, NULL, &sys_states[S_RUNNING], NULL),
    [S_MSC] = SMF_CREATE_STATE(msc_entry, msc_run, NULL, &sys_states[S_RUNNING], NULL),
    [S_SHUTDOWN] = SMF_CREATE_STATE(shutdown_entry, shutdown_run, NULL, NULL, NULL),
    [S_OFF] = SMF_CREATE_STATE(off_entry, off_run, NULL, NULL, NULL),
};

void sys_fsm_init(struct sys_fsm *f, const struct sys_fsm_ops *ops, uint32_t ack_mask)
{
    *f = (struct sys_fsm){.ops = ops, .ack_mask = ack_mask, .mode = APP_MODE_ID_CRS};
    smf_set_initial(SMF_CTX(f), &sys_states[S_BOOT]);
}

void sys_fsm_event(struct sys_fsm *f, enum sys_ev ev, int32_t arg)
{
    /* facts that every state keeps, before the state runs */
    if (ev == SYS_EV_MODE) {
        f->mode = (uint8_t)arg;
    } else if (ev == SYS_EV_VBUS) {
        f->vbus = (arg != 0);
    }
    f->ev = ev;
    f->arg = arg;
    (void)smf_run_state(SMF_CTX(f));
}

enum app_sys_state sys_fsm_state(const struct sys_fsm *f)
{
    return f->state;
}
