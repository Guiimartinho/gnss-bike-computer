/**
 * @file power_svc.c
 * @brief Energy service: the system machine, the power-off and the battery
 *
 * docs/16-arquitetura-firmware.md (Serviços, Sistema e energia). The thread
 * runs the system machine (sys_fsm.c) on the events of its inbox: commands,
 * shutdown acknowledgements, positions and trainer data for the automatic
 * power-off, the mode in force. When the machine turns the device off, the
 * nPM1300 enters ship mode (alias pmic-regulators); with VBUS present, or on
 * a board without the PMIC, the MCU enters System OFF.
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/zbus/zbus.h>

#include "app/app_channels.h"
#include "app/app_svc.h"
#include "svc/sys_fsm.h"

LOG_MODULE_REGISTER(power_svc, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * A state change publishes on zbus and logs (about 0.8 KB); the power-off
 * flushes the log with LOG_PANIC in this thread (about 0.75 KB).
 */
#define POWER_STACK_SIZE        2048
#define POWER_INBOX_LEN         16
/** power_status is published at least this often (docs/16, Eventos) */
#define POWER_STATUS_PERIOD_MS  60000U

/** Services that take part in the shutdown: all of them */
#define POWER_ACK_MASK          ((1UL << APP_SVC_COUNT) - 1UL)

#define PMIC_REGULATORS         DT_ALIAS(pmic_regulators)

enum power_msg_kind {
    PM_CMD = 0,
    PM_ACK,
    PM_FIX,
    PM_TRAINER,
    PM_MODE,
    PM_READY
};

struct power_msg {
    uint8_t kind;               /**< enum power_msg_kind */
    int32_t arg;
};

K_MSGQ_DEFINE(power_inbox, sizeof(struct power_msg), POWER_INBOX_LEN, 4);

static struct sys_fsm fsm;
static struct app_power_status status;

/* Only what the machine uses enters the inbox */
static void power_listener(const struct zbus_channel *chan)
{
    struct power_msg msg = {0};

    if (chan == &chan_system_cmd) {
        const struct app_system_cmd *cmd = zbus_chan_const_msg(chan);

        if ((cmd->id != APP_CMD_SHUTDOWN) && (cmd->id != APP_CMD_MSC)) {
            return;
        }
        msg.kind = PM_CMD;
        msg.arg = cmd->id;
    } else if (chan == &chan_shutdown_ack) {
        const struct app_shutdown_ack *ack = zbus_chan_const_msg(chan);

        msg.kind = PM_ACK;
        msg.arg = ack->svc;
    } else if (chan == &chan_gnss_fix) {
        const struct app_gnss_fix *fix = zbus_chan_const_msg(chan);

        if (!fix->fix) {
            return;
        }
        msg.kind = PM_FIX;
    } else if (chan == &chan_ext_sensor) {
        const struct app_ext_sensor *ext = zbus_chan_const_msg(chan);

        if (ext->kind != APP_EXT_FEC) {
            return;
        }
        msg.kind = PM_TRAINER;
    } else if (chan == &chan_mode) {
        const struct app_mode_state *mode = zbus_chan_const_msg(chan);

        msg.kind = PM_MODE;
        msg.arg = mode->mode;
    } else {
        return;
    }
    app_inbox_put(&power_inbox, &msg, "power");
}

ZBUS_LISTENER_DEFINE(power_lis, power_listener);
ZBUS_CHAN_ADD_OBS(chan_system_cmd, power_lis, 2);
ZBUS_CHAN_ADD_OBS(chan_shutdown_ack, power_lis, 2);
ZBUS_CHAN_ADD_OBS(chan_gnss_fix, power_lis, 2);
ZBUS_CHAN_ADD_OBS(chan_ext_sensor, power_lis, 2);
ZBUS_CHAN_ADD_OBS(chan_mode, power_lis, 2);

static void publish_state(enum app_sys_state state, void *user)
{
    struct app_system_state s = {.state = (uint8_t)state};

    ARG_UNUSED(user);
    LOG_INF("system state %u", (unsigned int)state);
    (void)app_publish(&chan_system_state, &s);
}

/**
 * Cut the power. Ship mode takes everything down but the backup rails
 * (docs/14, Estados de energia); the nPM1300 refuses it with VBUS, and then
 * the MCU sleeps in System OFF (docs/16: CarregandoDesligado).
 */
static void power_off(bool vbus, void *user)
{
    ARG_UNUSED(user);

#if DT_NODE_HAS_STATUS(PMIC_REGULATORS, okay)
    if (!vbus) {
        const struct device *regs = DEVICE_DT_GET(PMIC_REGULATORS);

        if (device_is_ready(regs)) {
            LOG_INF("ship mode");
            LOG_PANIC();
            int err = regulator_parent_ship_mode(regs);

            LOG_ERR("ship mode refused: %d", err);
        }
    }
#else
    ARG_UNUSED(vbus);
#endif
    LOG_INF("System OFF");
    LOG_PANIC();
    sys_poweroff();
}

static const struct sys_fsm_ops fsm_ops = {
    .publish = publish_state,
    .power_off = power_off,
    .user = NULL,
};

static void publish_status(void)
{
    (void)app_publish(&chan_power_status, &status);
}

static void power_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct power_msg msg;
    uint32_t last_status_ms = k_uptime_get_32();

    sys_fsm_init(&fsm, &fsm_ops, POWER_ACK_MASK);
    (void)memset(&status, 0, sizeof(status));
    publish_status();

    int wdt = app_wdt_add("power");

    for (;;) {
        if (app_inbox_get(&power_inbox, &msg, wdt, APP_SVC_TICK_MS) == 0) {
            switch (msg.kind) {
            case PM_CMD:
                sys_fsm_event(&fsm, SYS_EV_CMD, msg.arg);
                break;
            case PM_ACK:
                sys_fsm_event(&fsm, SYS_EV_ACK, msg.arg);
                break;
            case PM_FIX:
                sys_fsm_event(&fsm, SYS_EV_FIX, 0);
                break;
            case PM_TRAINER:
                sys_fsm_event(&fsm, SYS_EV_TRAINER, 0);
                break;
            case PM_MODE:
                sys_fsm_event(&fsm, SYS_EV_MODE, msg.arg);
                break;
            case PM_READY:
                sys_fsm_event(&fsm, SYS_EV_READY, 0);
                break;
            default:
                break;
            }
        } else {
            sys_fsm_event(&fsm, SYS_EV_TICK, 0);
        }
        if ((k_uptime_get_32() - last_status_ms) >= POWER_STATUS_PERIOD_MS) {
            publish_status();
            last_status_ms = k_uptime_get_32();
        }
    }
}

K_THREAD_DEFINE(power_tid, POWER_STACK_SIZE, power_thread, NULL, NULL, NULL, APP_PRIO_POWER, 0,
                SYS_FOREVER_MS);

void power_svc_start(void)
{
    k_thread_name_set(power_tid, "power");
    k_thread_start(power_tid);
}

void power_svc_ready(void)
{
    struct power_msg msg = {.kind = PM_READY};

    app_inbox_put(&power_inbox, &msg, "power");
}
