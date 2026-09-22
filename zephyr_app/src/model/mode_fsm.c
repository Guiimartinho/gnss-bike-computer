/**
 * @file mode_fsm.c
 * @brief Which kind of ride the device is in
 *
 * The states, the families and the three rules in model/mode_fsm.h.
 */

#include <zephyr/logging/log.h>
#include <zephyr/smf.h>

#include "model/mode_fsm.h"

LOG_MODULE_REGISTER(mode_fsm, CONFIG_LOG_DEFAULT_LEVEL);

/* The two parents come after the children, so SMF_CREATE_STATE can name them */
enum mode_state_id {
    M_OUTDOOR = 0,
    M_INDOOR,
    M_CRS,
    M_PRC,
    M_FEC,
    M_ZWIFT,
    M_DBG,
    M_COUNT
};

static const struct smf_state mode_states[M_COUNT];

/** Which state runs each mode */
static enum mode_state_id state_of(enum app_mode mode)
{
    switch (mode) {
    case APP_MODE_ID_CRS:
        return M_CRS;
    case APP_MODE_ID_PRC:
        return M_PRC;
    case APP_MODE_ID_FEC:
        return M_FEC;
    case APP_MODE_ID_ZWIFT:
        return M_ZWIFT;
    default:
        return M_DBG;
    }
}

bool mode_is_outdoor(enum app_mode mode)
{
    return (mode == APP_MODE_ID_CRS) ||
           (mode == APP_MODE_ID_PRC) ||
           (mode == APP_MODE_ID_DBG);
}

static void settle(struct mode_fsm *f, enum app_mode mode)
{
    f->mode = mode;
    if ((f->ops != NULL) && (f->ops->publish != NULL)) {
        f->ops->publish(mode, f->ops->user);
    }
}

/*
 * The parents carry no action of their own: what they carry is the
 * membership, which `mode_fsm_is_outdoor()` and the guard of rule 2 read.
 * SMF runs a parent's entry only when the machine comes from outside the
 * family, which is exactly when the family changed.
 */
static void outdoor_entry(void *o)
{
    ARG_UNUSED(o);
    LOG_DBG("riding outdoors");
}

static void indoor_entry(void *o)
{
    ARG_UNUSED(o);
    LOG_DBG("riding indoors");
}

static void crs_entry(void *o)
{
    settle((struct mode_fsm *)o, APP_MODE_ID_CRS);
}

static void prc_entry(void *o)
{
    struct mode_fsm *f = (struct mode_fsm *)o;

    /*
     * The guard of `mode_fsm_select()` already refused the mode without a
     * route, so there is one here to follow.
     */
    if ((f->ops != NULL) && (f->ops->route_start != NULL)) {
        f->ops->route_start(f->ops->user);
    }
    settle(f, APP_MODE_ID_PRC);
}

static void prc_exit(void *o)
{
    struct mode_fsm *f = (struct mode_fsm *)o;

    if ((f->ops != NULL) && (f->ops->route_stop != NULL)) {
        f->ops->route_stop(f->ops->user);
    }
}

static void fec_entry(void *o)
{
    settle((struct mode_fsm *)o, APP_MODE_ID_FEC);
}

static void zwift_entry(void *o)
{
    settle((struct mode_fsm *)o, APP_MODE_ID_ZWIFT);
}

static void dbg_entry(void *o)
{
    /* the diagnostics screen over the free ride: same family, other page */
    settle((struct mode_fsm *)o, APP_MODE_ID_DBG);
}

static const struct smf_state mode_states[M_COUNT] = {
    [M_OUTDOOR] = SMF_CREATE_STATE(outdoor_entry, NULL, NULL, NULL, NULL),
    [M_INDOOR] = SMF_CREATE_STATE(indoor_entry, NULL, NULL, NULL, NULL),
    [M_CRS] = SMF_CREATE_STATE(crs_entry, NULL, NULL, &mode_states[M_OUTDOOR], NULL),
    [M_PRC] = SMF_CREATE_STATE(prc_entry, NULL, prc_exit, &mode_states[M_OUTDOOR], NULL),
    [M_FEC] = SMF_CREATE_STATE(fec_entry, NULL, NULL, &mode_states[M_INDOOR], NULL),
    [M_ZWIFT] = SMF_CREATE_STATE(zwift_entry, NULL, NULL, &mode_states[M_INDOOR], NULL),
    [M_DBG] = SMF_CREATE_STATE(dbg_entry, NULL, NULL, &mode_states[M_OUTDOOR], NULL),
};

void mode_fsm_init(struct mode_fsm *f, const struct mode_fsm_ops *ops)
{
    if (f == NULL) {
        return;
    }

    f->ops = ops;
    f->mode = APP_MODE_ID_CRS;
    f->route_loaded = false;
    f->recording = false;

    smf_set_initial(SMF_CTX(f), &mode_states[M_CRS]);
}

static void refuse(struct mode_fsm *f, enum app_mode wanted, enum mode_refusal why)
{
    LOG_INF("mode %d refused (%d)", (int)wanted, (int)why);
    if ((f->ops != NULL) && (f->ops->refused != NULL)) {
        f->ops->refused(wanted, why, f->ops->user);
    }
}

bool mode_fsm_select(struct mode_fsm *f, int32_t mode)
{
    if (f == NULL) {
        return false;
    }

    if ((mode < 0) || (mode > (int32_t)APP_MODE_ID_DBG)) {
        refuse(f, APP_MODE_ID_CRS, MODE_REFUSED_UNKNOWN);
        return false;
    }

    enum app_mode wanted = (enum app_mode)mode;

    if (wanted == f->mode) {
        refuse(f, wanted, MODE_REFUSED_SAME);
        return false;
    }

    /* rule 1: a route mode needs a route */
    if ((wanted == APP_MODE_ID_PRC) && !f->route_loaded) {
        refuse(f, wanted, MODE_REFUSED_NO_ROUTE);
        return false;
    }

    /* rule 2: a recorded ride does not cross families */
    if (f->recording && (mode_is_outdoor(wanted) != mode_is_outdoor(f->mode))) {
        refuse(f, wanted, MODE_REFUSED_RECORDING);
        return false;
    }

    LOG_INF("mode %d -> %d", (int)f->mode, (int)wanted);
    smf_set_state(SMF_CTX(f), &mode_states[state_of(wanted)]);

    return true;
}

void mode_fsm_set_route_loaded(struct mode_fsm *f, bool loaded)
{
    if (f == NULL) {
        return;
    }

    f->route_loaded = loaded;

    /*
     * A route that goes away under a rider who is following it drops them
     * back to the free ride, which is the mode that needs nothing. Staying
     * in PRC would leave a navigation screen with nothing to navigate.
     */
    if (!loaded && (f->mode == APP_MODE_ID_PRC)) {
        smf_set_state(SMF_CTX(f), &mode_states[M_CRS]);
    }
}

void mode_fsm_set_recording(struct mode_fsm *f, bool recording)
{
    if (f != NULL) {
        f->recording = recording;
    }
}

enum app_mode mode_fsm_mode(const struct mode_fsm *f)
{
    return (f != NULL) ? f->mode : APP_MODE_ID_CRS;
}

bool mode_fsm_is_outdoor(const struct mode_fsm *f)
{
    return (f != NULL) && mode_is_outdoor(f->mode);
}
