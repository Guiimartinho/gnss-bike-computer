/**
 * @file max17262.c
 * @brief MAX17262 fuel gauge by the Zephyr fuel gauge API (adi,max17262)
 *
 * The ModelGauge m5 EZ configuration of the datasheet (DesignCap, IChgTerm,
 * VEmpty, ModelCfg) after a power-on reset, with the register units of
 * max17262_regs.h, and the readings in the units of the fuel gauge API.
 * The configuration stays in the gauge while the cell is connected: it is
 * only written again after a power-on reset (Status.POR).
 *
 * Why not the driver of the Zephyr tree (maxim,max17262, sensor API): it
 * writes the capacity in mAh where the register counts 0.5 mAh, writes the
 * charging current where IChgTerm wants the termination current, in raw
 * steps, returns capacities without the 0.5 mAh step and reads TTE as a
 * signed value, so 0xFFFF (unknown) never matches.
 *
 * Not tested on a gauge yet.
 */

#define DT_DRV_COMPAT adi_max17262

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <drivers/fuel_gauge/max17262_regs.h>

LOG_MODULE_REGISTER(max17262, CONFIG_GNSS_FUEL_GAUGE_MAX17262_LOG_LEVEL);

/** FStat.DNR clears in about 710 ms after a power-on reset (MAX1726x guides) */
#define MAX17262_READY_TIMEOUT_MS   1500U
/** ModelCfg.Refresh clears when the model is loaded */
#define MAX17262_REFRESH_TIMEOUT_MS 1000U
#define MAX17262_POLL_MS            10U

struct max17262_config {
    struct i2c_dt_spec i2c;
    uint16_t design_mah;
    uint16_t term_ma;
    uint16_t empty_mv;
    uint16_t recovery_mv;
    uint16_t charge_mv;
    bool thermistor;
};

static int reg_read(const struct device *dev, uint8_t reg, uint16_t *val)
{
    const struct max17262_config *cfg = dev->config;
    uint8_t buf[2];
    int err = i2c_burst_read_dt(&cfg->i2c, reg, buf, sizeof(buf));

    if (err == 0) {
        *val = sys_get_le16(buf);
    }
    return err;
}

static int reg_write(const struct device *dev, uint8_t reg, uint16_t val)
{
    const struct max17262_config *cfg = dev->config;
    uint8_t buf[3] = {reg, (uint8_t)(val & 0xFFU), (uint8_t)(val >> 8)};

    return i2c_write_dt(&cfg->i2c, buf, sizeof(buf));
}

/** Wait for bits of a register to clear */
static int wait_clear(const struct device *dev, uint8_t reg, uint16_t mask, uint32_t timeout_ms)
{
    for (uint32_t waited = 0U; waited <= timeout_ms; waited += MAX17262_POLL_MS) {
        uint16_t val;
        int err = reg_read(dev, reg, &val);

        if (err != 0) {
            return err;
        }
        if ((val & mask) == 0U) {
            return 0;
        }
        k_msleep(MAX17262_POLL_MS);
    }
    return -ETIMEDOUT;
}

/** ModelGauge m5 EZ configuration, the steps of the datasheet and its guide */
static int ez_config(const struct device *dev)
{
    const struct max17262_config *cfg = dev->config;
    uint16_t hibcfg;
    uint16_t val;
    int err;

    err = wait_clear(dev, MAX17262_FSTAT, MAX17262_FSTAT_DNR, MAX17262_READY_TIMEOUT_MS);
    if (err == 0) {
        err = reg_read(dev, MAX17262_HIBCFG, &hibcfg);
    }
    /* Out of hibernation while the model loads */
    if (err == 0) {
        err = reg_write(dev, MAX17262_SOFTWAKEUP, MAX17262_SOFTWAKEUP_ON);
    }
    if (err == 0) {
        err = reg_write(dev, MAX17262_HIBCFG, 0x0000U);
    }
    if (err == 0) {
        err = reg_write(dev, MAX17262_SOFTWAKEUP, MAX17262_SOFTWAKEUP_OFF);
    }
    if (err == 0) {
        err = reg_write(dev, MAX17262_DESIGNCAP, max17262_designcap(cfg->design_mah));
    }
    if (err == 0) {
        err = reg_write(dev, MAX17262_ICHGTERM, max17262_ichgterm(cfg->term_ma));
    }
    if (err == 0) {
        err = reg_write(dev, MAX17262_VEMPTY, max17262_vempty(cfg->empty_mv, cfg->recovery_mv));
    }
    if (err == 0) {
        err = reg_write(dev, MAX17262_MODELCFG, max17262_modelcfg(cfg->charge_mv));
    }
    if (err == 0) {
        err = wait_clear(dev, MAX17262_MODELCFG, MAX17262_MODELCFG_REFRESH,
                         MAX17262_REFRESH_TIMEOUT_MS);
    }
    if (err == 0) {
        err = reg_write(dev, MAX17262_HIBCFG, hibcfg);
    }
    /* TH tied to BATT: temperature from the die, no thermistor bias */
    if ((err == 0) && !cfg->thermistor) {
        err = reg_read(dev, MAX17262_CONFIG, &val);
        if (err == 0) {
            err = reg_write(dev, MAX17262_CONFIG, val & (uint16_t)~MAX17262_CONFIG_ETHRM);
        }
    }
    /* Configured: the next power-on reset sets POR again */
    if (err == 0) {
        err = reg_read(dev, MAX17262_STATUS, &val);
    }
    if (err == 0) {
        err = reg_write(dev, MAX17262_STATUS, val & (uint16_t)~MAX17262_STATUS_POR);
    }
    return err;
}

static int read_prop(const struct device *dev, uint8_t reg, uint16_t *raw)
{
    int err = reg_read(dev, reg, raw);

    if (err != 0) {
        LOG_DBG("register 0x%02x: %d", reg, err);
    }
    return err;
}

static int max17262_get_prop(const struct device *dev, fuel_gauge_prop_t prop,
                             union fuel_gauge_prop_val *val)
{
    const struct max17262_config *cfg = dev->config;
    uint16_t raw = 0U;
    int err = 0;

    switch (prop) {
    case FUEL_GAUGE_VOLTAGE:
        err = read_prop(dev, MAX17262_VCELL, &raw);
        val->voltage = max17262_microvolts(raw);
        break;
    case FUEL_GAUGE_CURRENT:
        err = read_prop(dev, MAX17262_CURRENT, &raw);
        val->current = max17262_microamps(raw);
        break;
    case FUEL_GAUGE_AVG_CURRENT:
        err = read_prop(dev, MAX17262_AVGCURRENT, &raw);
        val->avg_current = max17262_microamps(raw);
        break;
    case FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE:
        err = read_prop(dev, MAX17262_REPSOC, &raw);
        val->relative_state_of_charge = max17262_percent(raw);
        break;
    case FUEL_GAUGE_REMAINING_CAPACITY:
        err = read_prop(dev, MAX17262_REPCAP, &raw);
        val->remaining_capacity = max17262_microamp_hours(raw);
        break;
    case FUEL_GAUGE_FULL_CHARGE_CAPACITY:
        err = read_prop(dev, MAX17262_FULLCAPREP, &raw);
        val->full_charge_capacity = max17262_microamp_hours(raw);
        break;
    case FUEL_GAUGE_RUNTIME_TO_EMPTY:
        err = read_prop(dev, MAX17262_TTE, &raw);
        val->runtime_to_empty = max17262_minutes(raw);
        break;
    case FUEL_GAUGE_RUNTIME_TO_FULL:
        err = read_prop(dev, MAX17262_TTF, &raw);
        val->runtime_to_full = max17262_minutes(raw);
        break;
    case FUEL_GAUGE_TEMPERATURE:
        err = read_prop(dev, MAX17262_TEMP, &raw);
        val->temperature = max17262_deci_kelvin(raw);
        break;
    case FUEL_GAUGE_CYCLE_COUNT:
        /* 1 % of a cycle per step, the API unit */
        err = read_prop(dev, MAX17262_CYCLES, &raw);
        val->cycle_count = raw;
        break;
    case FUEL_GAUGE_STATUS:
        err = read_prop(dev, MAX17262_STATUS, &raw);
        val->fg_status = raw;
        break;
    case FUEL_GAUGE_DESIGN_CAPACITY:
        val->design_cap = cfg->design_mah;
        break;
    case FUEL_GAUGE_CHARGE_VOLTAGE:
        val->chg_voltage = (uint32_t)cfg->charge_mv * 1000U;
        break;
    default:
        err = -ENOTSUP;
        break;
    }
    return err;
}

static int max17262_init(const struct device *dev)
{
    const struct max17262_config *cfg = dev->config;
    uint16_t status;
    int err;

    if (!i2c_is_ready_dt(&cfg->i2c)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }
    err = reg_read(dev, MAX17262_STATUS, &status);
    if (err != 0) {
        LOG_ERR("no answer at 0x%02x: %d", cfg->i2c.addr, err);
        return err;
    }
    if ((status & MAX17262_STATUS_POR) == 0U) {
        /* configured before: the model and what it learned stay */
        return 0;
    }
    err = ez_config(dev);
    if (err != 0) {
        LOG_ERR("EZ configuration failed: %d", err);
        return err;
    }
    LOG_INF("EZ configuration: %u mAh, end of charge %u mA, empty %u mV", cfg->design_mah,
            cfg->term_ma, cfg->empty_mv);
    return 0;
}

static DEVICE_API(fuel_gauge, max17262_api) = {
    .get_property = max17262_get_prop,
};

#define MAX17262_DEFINE(n)                                                                   \
    static const struct max17262_config max17262_cfg_##n = {                                 \
        .i2c = I2C_DT_SPEC_INST_GET(n),                                                      \
        .design_mah = DT_INST_PROP(n, design_capacity_mah),                                  \
        .term_ma = DT_INST_PROP(n, charge_term_current_ma),                                  \
        .empty_mv = DT_INST_PROP(n, empty_voltage_mv),                                       \
        .recovery_mv = DT_INST_PROP(n, recovery_voltage_mv),                                 \
        .charge_mv = DT_INST_PROP(n, charge_voltage_mv),                                     \
        .thermistor = DT_INST_PROP(n, thermistor),                                           \
    };                                                                                       \
    DEVICE_DT_INST_DEFINE(n, max17262_init, NULL, NULL, &max17262_cfg_##n, POST_KERNEL,      \
                          CONFIG_FUEL_GAUGE_INIT_PRIORITY, &max17262_api);

DT_INST_FOREACH_STATUS_OKAY(MAX17262_DEFINE)
