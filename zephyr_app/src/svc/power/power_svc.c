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
 *
 * Every POWER_GAUGE_PERIOD_MS it reads the fuel gauge of the alias
 * fuel-gauge0 (the MAX17262 of the new board) through the Zephyr fuel gauge
 * API: voltage, average current, charge, temperature and time to empty go
 * to power_status; battery.c turns the charge into the low-battery
 * notification and the critical event of the system machine.
 *
 * With the nPM1300 (aliases pmic and pmic-charger) it also reads the
 * charger: VBUS goes to the system machine (ship mode or System OFF), the
 * USB-C current of the source raises the VBUS limit, and charge.c turns
 * the charger registers into the charge state of the screens. The PMIC
 * events (VBUS in and out, charge done, charger error) arrive from its
 * work item as inbox messages and trigger a new reading.
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/mfd/npm13xx.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/npm13xx_charger.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/zbus/zbus.h>

#include "app/app_channels.h"
#include "app/app_svc.h"
#include "svc/battery.h"
#include "svc/charge.h"
#include "svc/sys_fsm.h"

LOG_MODULE_REGISTER(power_svc, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * The critical battery runs the system machine from the gauge reading into a
 * zbus publication with a log (about 1.1 KB measured with CONFIG_STACK_USAGE);
 * the gauge read over I2C takes about 0.3 KB and the power-off flushes the
 * log with LOG_PANIC (about 0.75 KB).
 */
#define POWER_STACK_SIZE        3072
#define POWER_INBOX_LEN         16
/** power_status is published at least this often (docs/16, Eventos) */
#define POWER_STATUS_PERIOD_MS  60000U
/** The fuel gauge is read this often; a change on screen publishes at once */
#define POWER_GAUGE_PERIOD_MS   10000U
/** Tenths of a kelvin at 0 degC, the fuel gauge API unit */
#define POWER_DECI_KELVIN_0C    2732

/** Services that take part in the shutdown: all of them */
#define POWER_ACK_MASK          ((1UL << APP_SVC_COUNT) - 1UL)

#define PMIC_REGULATORS         DT_ALIAS(pmic_regulators)
#define FUEL_GAUGE              DT_ALIAS(fuel_gauge0)

#if DT_NODE_HAS_STATUS_OKAY(FUEL_GAUGE) && defined(CONFIG_FUEL_GAUGE)
static const struct device *const gauge = DEVICE_DT_GET(FUEL_GAUGE);
#else
static const struct device *const gauge = NULL;
#endif

#define PMIC_NODE               DT_ALIAS(pmic)
#define CHARGER_NODE            DT_ALIAS(pmic_charger)

#if DT_NODE_HAS_STATUS_OKAY(PMIC_NODE) && DT_NODE_HAS_STATUS_OKAY(CHARGER_NODE) && \
    defined(CONFIG_MFD_NPM13XX) && defined(CONFIG_NPM13XX_CHARGER)
#define POWER_HAS_PMIC          1
static const struct device *const pmic = DEVICE_DT_GET(PMIC_NODE);
static const struct device *const charger = DEVICE_DT_GET(CHARGER_NODE);
#else
#define POWER_HAS_PMIC          0
#endif

/* nPM1300 datasheet 6.2.14: charger block, NTC comparator status */
#define NPM1300_CHARGER_BASE    0x03U
#define NPM1300_NTCSTATUS       0x32U
/* VBUSINSTATUS.VBUSINPRESENT */
#define NPM1300_VBUS_PRESENT    0x01U

enum power_msg_kind {
    PM_CMD = 0,
    PM_ACK,
    PM_FIX,
    PM_TRAINER,
    PM_MODE,
    PM_READY,
    PM_PMIC                     /**< arg: the event bit of the nPM1300 */
};

struct power_msg {
    uint8_t kind;               /**< enum power_msg_kind */
    int32_t arg;
};

K_MSGQ_DEFINE(power_inbox, sizeof(struct power_msg), POWER_INBOX_LEN, 4);

static struct sys_fsm fsm;
static struct app_power_status status;
static struct app_power_status published;
static battery_t battery;

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
    published = status;
    (void)app_publish(&chan_power_status, &status);
}

/** What the screens show changed since the last publication */
static bool status_changed(void)
{
    int32_t dmv = (int32_t)status.mv - (int32_t)published.mv;

    return (status.gauge != published.gauge) || (status.pct != published.pct) ||
           (status.charge != published.charge) || (status.vbus != published.vbus) ||
           (status.critical != published.critical) || (dmv > 20) || (dmv < -20);
}

/** Tenths of a kelvin to whole degrees Celsius, rounded */
static int8_t celsius(uint16_t deci_kelvin)
{
    int32_t tenths = (int32_t)deci_kelvin - POWER_DECI_KELVIN_0C;

    return (int8_t)((tenths >= 0) ? ((tenths + 5) / 10) : ((tenths - 5) / 10));
}

static void on_battery_event(battery_event_t ev)
{
    if (ev == BATTERY_EV_LOW) {
        LOG_WRN("battery low: %u %%", status.pct);
        app_notify("Bateria", "Carga baixa", "10%", false, 0U);
    } else if (ev == BATTERY_EV_CRITICAL) {
        LOG_WRN("battery at its end");
        app_notify("Bateria", "No fim: desligando", "0%", false, 0U);
        sys_fsm_event(&fsm, SYS_EV_BATT_CRITICAL, 0);
    } else {
        /* nothing to say */
    }
}

#if POWER_HAS_PMIC
static charge_state_t charge = CHARGE_BATTERY;
static struct gpio_callback pmic_cb;

static void on_charge_change(charge_state_t now)
{
    if (now == CHARGE_THERMAL_PAUSE) {
        LOG_WRN("charging paused: cell or die temperature");
        app_notify("Carga", "Pausada: temperatura", NULL, false, 0U);
    } else if (now == CHARGE_FAULT) {
        LOG_ERR("charger error");
        app_notify("Carga", "Erro no carregador", NULL, false, 0U);
    } else {
        LOG_INF("charge state %u", (unsigned int)now);
    }
}

/** PMIC event, from the work item of the MFD driver: only a message here */
static void pmic_event(const struct device *dev, struct gpio_callback *cb, uint32_t events)
{
    struct power_msg msg = {.kind = PM_PMIC, .arg = (int32_t)events};

    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    app_inbox_put(&power_inbox, &msg, "power");
}

/** A new VBUS: take what the USB-C source offers (500 mA or 1.5 A), reset on removal */
static void vbus_limit(void)
{
    struct sensor_value cap;

    if ((sensor_attr_get(charger, SENSOR_CHAN_CURRENT, SENSOR_ATTR_UPPER_THRESH, &cap) == 0) &&
        ((cap.val1 != 0) || (cap.val2 != 0))) {
        (void)sensor_attr_set(charger, SENSOR_CHAN_CURRENT, SENSOR_ATTR_CONFIGURATION, &cap);
        LOG_INF("VBUS limit %d mA", (int)((cap.val1 * 1000) + (cap.val2 / 1000)));
    }
}

/** Charger registers into VBUS and the charge state */
static void read_charger(void)
{
    struct sensor_value val = {0};
    charge_inputs_t in = {0};
    uint8_t ntc = 0U;

    if (!device_is_ready(charger) || (sensor_sample_fetch(charger) != 0)) {
        return;
    }
    (void)sensor_channel_get(charger, (enum sensor_channel)SENSOR_CHAN_NPM13XX_CHARGER_VBUS_STATUS,
                             &val);
    in.vbus = ((uint32_t)val.val1 & NPM1300_VBUS_PRESENT) != 0U;
    (void)sensor_channel_get(charger, (enum sensor_channel)SENSOR_CHAN_NPM13XX_CHARGER_STATUS, &val);
    in.status = (uint8_t)val.val1;
    (void)sensor_channel_get(charger, (enum sensor_channel)SENSOR_CHAN_NPM13XX_CHARGER_ERROR, &val);
    in.error = (uint8_t)val.val1;
    (void)mfd_npm13xx_reg_read(pmic, NPM1300_CHARGER_BASE, NPM1300_NTCSTATUS, &ntc);
    in.ntc = ntc;

    if (in.vbus != status.vbus) {
        LOG_INF("VBUS %s", in.vbus ? "in" : "out");
        status.vbus = in.vbus;
        sys_fsm_event(&fsm, SYS_EV_VBUS, in.vbus ? 1 : 0);
        if (in.vbus) {
            vbus_limit();
        }
    }

    charge_state_t now = charge_state(&in);

    if (now != charge) {
        charge = now;
        on_charge_change(now);
    }
    status.charge = charge_app(now);
}

static void pmic_init(void)
{
    if (!device_is_ready(pmic) || !device_is_ready(charger)) {
        LOG_WRN("nPM1300 not ready: no charger readings");
        return;
    }
    gpio_init_callback(&pmic_cb, pmic_event,
                       BIT(NPM13XX_EVENT_VBUS_DETECTED) | BIT(NPM13XX_EVENT_VBUS_REMOVED) |
                           BIT(NPM13XX_EVENT_CHG_COMPLETED) | BIT(NPM13XX_EVENT_CHG_ERROR));
    if (mfd_npm13xx_add_callback(pmic, &pmic_cb) != 0) {
        LOG_WRN("nPM1300 events not enabled");
    }
}
#else
static void read_charger(void)
{
}

static void pmic_init(void)
{
    LOG_INF("no nPM1300 (aliases pmic, pmic-charger): no charger readings");
}
#endif

/**
 * Read the fuel gauge into the status. A gauge that stops answering keeps the
 * last values and clears status.gauge, so the screens show no battery data.
 */
static void read_gauge(void)
{
    static const fuel_gauge_prop_t props[] = {
        FUEL_GAUGE_VOLTAGE,
        FUEL_GAUGE_AVG_CURRENT,
        FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE,
        FUEL_GAUGE_TEMPERATURE,
        FUEL_GAUGE_RUNTIME_TO_EMPTY,
    };
    union fuel_gauge_prop_val vals[ARRAY_SIZE(props)];
    battery_reading_t reading;
    int32_t ma;

    if ((gauge == NULL) || !device_is_ready(gauge)) {
        status.gauge = false;
        return;
    }
    if (fuel_gauge_get_props(gauge, props, vals, ARRAY_SIZE(props)) != 0) {
        if (status.gauge) {
            LOG_WRN("fuel gauge not answering");
        }
        status.gauge = false;
        return;
    }
    ma = vals[1].avg_current / 1000;
    status.gauge = true;
    status.mv = (uint16_t)CLAMP(vals[0].voltage / 1000, 0, UINT16_MAX);
    status.ma = (int16_t)CLAMP(ma, INT16_MIN, INT16_MAX);
    status.pct = vals[2].relative_state_of_charge;
    status.temp_c = celsius(vals[3].temperature);
    /* hours left while discharging; unknown or charging shows nothing */
    if ((vals[4].runtime_to_empty == UINT32_MAX) || (vals[1].avg_current >= 0)) {
        status.autonomy_h = 0U;
    } else {
        status.autonomy_h = (uint16_t)MIN(vals[4].runtime_to_empty / 60U, (uint32_t)UINT16_MAX);
    }
    reading.pct = status.pct;
    reading.avg_ua = vals[1].avg_current;
    reading.vbus = status.vbus;
    battery_event_t ev = battery_update(&battery, &reading);

    status.critical = battery.critical;
    on_battery_event(ev);
}

static void power_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct power_msg msg;
    uint32_t last_status_ms = k_uptime_get_32();
    uint32_t last_gauge_ms = last_status_ms;

    sys_fsm_init(&fsm, &fsm_ops, POWER_ACK_MASK);
    battery_init(&battery);
    (void)memset(&status, 0, sizeof(status));
    if (gauge == NULL) {
        LOG_INF("no fuel gauge (alias fuel-gauge0): no battery readings");
    }
    pmic_init();
    read_charger();
    read_gauge();
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
            case PM_PMIC:
                read_charger();
                break;
            default:
                break;
            }
        } else {
            sys_fsm_event(&fsm, SYS_EV_TICK, 0);
        }
        if ((k_uptime_get_32() - last_gauge_ms) >= POWER_GAUGE_PERIOD_MS) {
            read_charger();
            read_gauge();
            last_gauge_ms = k_uptime_get_32();
        }
        if (status_changed() ||
            ((k_uptime_get_32() - last_status_ms) >= POWER_STATUS_PERIOD_MS)) {
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
