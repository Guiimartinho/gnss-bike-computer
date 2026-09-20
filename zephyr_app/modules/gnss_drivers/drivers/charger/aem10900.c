/**
 * @file aem10900.c
 * @brief e-peas AEM10900 solar charger by the Zephyr charger API (e-peas,aem10900)
 *
 * The configuration pins of the board charge the cell up to 3.90 V; with the
 * board on, the I2C configuration takes over to charge up to about 4.05 V
 * (docs/14, Carga). Only the thresholds and the average power monitor are
 * written: the register reset values equal the pins for everything else
 * (aem10900_regs.h). The keep-alive holds the I2C configuration while the
 * 3V0 is off; before the ship mode the power service hands the
 * configuration back to the pins (AEM10900_PROP_PIN_CONFIG). An AEM that
 * reset (VINT lost) comes back on its pins: the driver sees CTRL.UPDATE
 * cleared and writes its configuration again.
 *
 * Not tested with an AEM10900 yet.
 */

#define DT_DRV_COMPAT e_peas_aem10900

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/charger.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/charger/aem10900.h>
#include <drivers/charger/aem10900_regs.h>

LOG_MODULE_REGISTER(aem10900, CONFIG_GNSS_CHARGER_AEM10900_LOG_LEVEL);

/** CTRL.SYNCBUSY clears once the registers are applied */
#define AEM10900_SYNC_TIMEOUT_MS    50U

struct aem10900_config {
    struct i2c_dt_spec i2c;
    uint16_t charge_mv;         /**< VOVCH with the board on */
    uint16_t discharge_mv;      /**< VOVDIS */
    uint16_t pins_charge_mv;    /**< VOVCH of the configuration pins */
    uint32_t apm_scale_pj;      /**< energy of one APM unit, 0: not calibrated */
};

struct aem10900_data {
    struct k_mutex lock;
    uint16_t charge_mv;         /**< VOVCH asked */
    bool i2c_config;            /**< the I2C configuration should be in force */
};

static int reg_read(const struct device *dev, uint8_t reg, uint8_t *val)
{
    const struct aem10900_config *cfg = dev->config;

    return i2c_reg_read_byte_dt(&cfg->i2c, reg, val);
}

static int reg_write(const struct device *dev, uint8_t reg, uint8_t val)
{
    const struct aem10900_config *cfg = dev->config;

    return i2c_reg_write_byte_dt(&cfg->i2c, reg, val);
}

static int wait_sync(const struct device *dev)
{
    for (uint32_t waited = 0U; waited <= AEM10900_SYNC_TIMEOUT_MS; waited++) {
        uint8_t ctrl;
        int err = reg_read(dev, AEM10900_CTRL, &ctrl);

        if (err != 0) {
            return err;
        }
        if ((ctrl & AEM10900_CTRL_SYNCBUSY) == 0U) {
            return 0;
        }
        k_msleep(1);
    }
    return -ETIMEDOUT;
}

/** Thresholds and the power monitor, then the I2C configuration in force. Lock held. */
static int apply_i2c_config(const struct device *dev)
{
    const struct aem10900_config *cfg = dev->config;
    struct aem10900_data *data = dev->data;
    const uint8_t vovdis = aem10900_vovdis_thresh(cfg->discharge_mv);
    const uint8_t vovch = aem10900_vovch_thresh(data->charge_mv);
    int err = wait_sync(dev);

    if (err == 0) {
        err = reg_write(dev, AEM10900_VOVDIS, vovdis);
    }
    if (err == 0) {
        err = reg_write(dev, AEM10900_VOVCH, vovch);
    }
    if (err == 0) {
        err = reg_write(dev, AEM10900_APM, AEM10900_APM_EN | AEM10900_APM_POWER_METER |
                                               AEM10900_APM_WINDOW_128MS);
    }
    if (err == 0) {
        err = reg_write(dev, AEM10900_CTRL, AEM10900_CTRL_UPDATE);
    }
    if (err == 0) {
        err = wait_sync(dev);
    }
    if (err == 0) {
        data->i2c_config = true;
    }
    return err;
}

/** Back on the pins (3.90 V). Lock held. */
static int apply_pin_config(const struct device *dev)
{
    int err = wait_sync(dev);

    if (err == 0) {
        err = reg_write(dev, AEM10900_CTRL, 0U);
    }
    if (err == 0) {
        struct aem10900_data *data = dev->data;

        data->i2c_config = false;
    }
    return err;
}

/** A reset of the AEM brings the pins back: take the I2C configuration again. Lock held. */
static int keep_config(const struct device *dev)
{
    const struct aem10900_data *data = dev->data;
    uint8_t ctrl;
    int err;

    if (!data->i2c_config) {
        return 0;
    }
    err = reg_read(dev, AEM10900_CTRL, &ctrl);
    if ((err == 0) && ((ctrl & AEM10900_CTRL_UPDATE) == 0U)) {
        LOG_WRN("configuration lost, written again");
        err = apply_i2c_config(dev);
    }
    return err;
}

static uint64_t read_apm(const struct device *dev, int *err)
{
    uint8_t apm[3] = {0U, 0U, 0U};
    const struct aem10900_config *cfg = dev->config;

    *err = i2c_burst_read_dt(&cfg->i2c, AEM10900_APM0, apm, sizeof(apm));
    return aem10900_apm_units(apm[0], apm[1], apm[2]);
}

static enum charger_status status_of(uint8_t st, uint64_t units)
{
    if ((st & AEM10900_STATUS_SRCLOW) != 0U) {
        return CHARGER_STATUS_DISCHARGING;
    }
    if (((st & AEM10900_STATUS_CHARGEDIS) != 0U) || ((st & AEM10900_STATUS_CHARGERDY) == 0U)) {
        return CHARGER_STATUS_NOT_CHARGING;
    }
    if ((st & AEM10900_STATUS_VOVCH) != 0U) {
        return CHARGER_STATUS_FULL;
    }
    return (units > 0U) ? CHARGER_STATUS_CHARGING : CHARGER_STATUS_NOT_CHARGING;
}

/** Cold or hot by the thresholds: a colder NTC gives a higher TEMP.DATA */
static int health_of(const struct device *dev, uint8_t st, enum charger_health *health)
{
    uint8_t temp;
    uint8_t cold;
    int err;

    if ((st & AEM10900_STATUS_TEMP) == 0U) {
        *health = CHARGER_HEALTH_GOOD;
        return 0;
    }
    err = reg_read(dev, AEM10900_TEMP, &temp);
    if (err == 0) {
        err = reg_read(dev, AEM10900_TEMPCOLD, &cold);
    }
    if (err == 0) {
        *health = (temp >= cold) ? CHARGER_HEALTH_COLD : CHARGER_HEALTH_HOT;
    }
    return err;
}

/** One property, with the lock held and STATUS read */
static int prop_of(const struct device *dev, uint8_t st, charger_prop_t prop,
                   union charger_propval *val)
{
    const struct aem10900_config *cfg = dev->config;
    const struct aem10900_data *data = dev->data;
    uint8_t raw = 0U;
    int err = 0;

    switch (prop) {
    case CHARGER_PROP_ONLINE:
        val->online = ((st & AEM10900_STATUS_SRCLOW) != 0U) ? CHARGER_ONLINE_OFFLINE
                                                            : CHARGER_ONLINE_FIXED;
        break;
    case CHARGER_PROP_STATUS:
        val->status = status_of(st, read_apm(dev, &err));
        break;
    case CHARGER_PROP_HEALTH:
        err = health_of(dev, st, &val->health);
        break;
    case CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV:
        val->const_charge_voltage_uv =
            (data->i2c_config ? aem10900_vovch_mv(aem10900_vovch_thresh(data->charge_mv))
                              : cfg->pins_charge_mv) *
            1000U;
        break;
    case AEM10900_PROP_APM_UNITS: {
        uint64_t units = read_apm(dev, &err);

        val->custom_uint = (units > UINT32_MAX) ? UINT32_MAX : (uint32_t)units;
        break;
    }
    case AEM10900_PROP_POWER_UW:
        val->custom_uint = aem10900_power_uw(read_apm(dev, &err), cfg->apm_scale_pj,
                                             AEM10900_APM_WINDOW_MS);
        break;
    case AEM10900_PROP_STO_UV:
        err = reg_read(dev, AEM10900_STO, &raw);
        val->custom_uint = aem10900_sto_mv(raw) * 1000U;
        break;
    default:
        err = -ENOTSUP;
        break;
    }
    return err;
}

static int aem10900_get_prop(const struct device *dev, const charger_prop_t prop,
                             union charger_propval *val)
{
    struct aem10900_data *data = dev->data;
    uint8_t st = 0U;
    int err;

    (void)k_mutex_lock(&data->lock, K_FOREVER);
    err = keep_config(dev);
    if (err == 0) {
        err = reg_read(dev, AEM10900_STATUS, &st);
    }
    if (err == 0) {
        err = prop_of(dev, st, prop, val);
    }
    k_mutex_unlock(&data->lock);
    return err;
}

static int aem10900_set_prop(const struct device *dev, const charger_prop_t prop,
                             const union charger_propval *val)
{
    struct aem10900_data *data = dev->data;
    int err;

    (void)k_mutex_lock(&data->lock, K_FOREVER);
    switch (prop) {
    case CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV:
        data->charge_mv = (uint16_t)(val->const_charge_voltage_uv / 1000U);
        err = apply_i2c_config(dev);
        break;
    case AEM10900_PROP_PIN_CONFIG:
        err = val->custom_bool ? apply_pin_config(dev) : apply_i2c_config(dev);
        break;
    default:
        err = -ENOTSUP;
        break;
    }
    k_mutex_unlock(&data->lock);
    return err;
}

static int aem10900_charge_enable(const struct device *dev, const bool enable)
{
    struct aem10900_data *data = dev->data;
    uint8_t pwr;
    int err;

    (void)k_mutex_lock(&data->lock, K_FOREVER);
    err = wait_sync(dev);
    if (err == 0) {
        err = reg_read(dev, AEM10900_PWR, &pwr);
    }
    if (err == 0) {
        /* PWR bit 3: CHARGEDIS */
        pwr = enable ? (uint8_t)(pwr & (uint8_t)~0x08U) : (uint8_t)(pwr | 0x08U);
        err = reg_write(dev, AEM10900_PWR, pwr);
    }
    if (err == 0) {
        err = apply_i2c_config(dev);
    }
    k_mutex_unlock(&data->lock);
    return err;
}

static int aem10900_init(const struct device *dev)
{
    const struct aem10900_config *cfg = dev->config;
    struct aem10900_data *data = dev->data;
    char pn[AEM10900_PN_LEN + 1U] = {0};
    int err;

    k_mutex_init(&data->lock);
    data->charge_mv = cfg->charge_mv;
    if (!i2c_is_ready_dt(&cfg->i2c)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }
    /* PN4..PN0 hold the part number in ASCII, "10900" */
    err = i2c_burst_read_dt(&cfg->i2c, AEM10900_PN0, (uint8_t *)pn, AEM10900_PN_LEN);
    if (err != 0) {
        LOG_ERR("no answer at 0x%02x: %d", cfg->i2c.addr, err);
        return err;
    }
    for (uint32_t i = 0U; i < (AEM10900_PN_LEN / 2U); i++) {
        char c = pn[i];

        pn[i] = pn[AEM10900_PN_LEN - 1U - i];
        pn[AEM10900_PN_LEN - 1U - i] = c;
    }
    if ((strcmp(pn, "10900") != 0) && (strcmp(pn, "10901") != 0)) {
        LOG_ERR("unexpected part number \"%s\"", pn);
        return -ENODEV;
    }
    (void)k_mutex_lock(&data->lock, K_FOREVER);
    err = apply_i2c_config(dev);
    k_mutex_unlock(&data->lock);
    if (err != 0) {
        LOG_ERR("I2C configuration failed: %d", err);
        return err;
    }
    LOG_INF("AEM%s: charge to %u mV with the board on", pn, cfg->charge_mv);
    return 0;
}

static DEVICE_API(charger, aem10900_api) = {
    .get_property = aem10900_get_prop,
    .set_property = aem10900_set_prop,
    .charge_enable = aem10900_charge_enable,
};

#define AEM10900_DEFINE(n)                                                                   \
    static struct aem10900_data aem10900_data_##n;                                           \
    static const struct aem10900_config aem10900_cfg_##n = {                                 \
        .i2c = I2C_DT_SPEC_INST_GET(n),                                                      \
        .charge_mv = DT_INST_PROP(n, charge_voltage_mv),                                     \
        .discharge_mv = DT_INST_PROP(n, discharge_voltage_mv),                               \
        .pins_charge_mv = DT_INST_PROP(n, pins_charge_voltage_mv),                           \
        .apm_scale_pj = DT_INST_PROP(n, apm_scale_pj),                                       \
    };                                                                                       \
    DEVICE_DT_INST_DEFINE(n, aem10900_init, NULL, &aem10900_data_##n, &aem10900_cfg_##n,     \
                          POST_KERNEL, CONFIG_CHARGER_INIT_PRIORITY, &aem10900_api);

DT_INST_FOREACH_STATUS_OKAY(AEM10900_DEFINE)
