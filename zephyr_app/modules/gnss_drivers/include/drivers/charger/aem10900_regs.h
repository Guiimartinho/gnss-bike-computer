/**
 * @file aem10900_regs.h
 * @brief Registers of the e-peas AEM10900 solar charger and their units
 *
 * Plain C, no Zephyr, so that the host tests check it. From the AEM1090x
 * datasheet (DS-AEM1090x-v2.4.0), sections 8 and 9:
 *
 * | Value | Formula |
 * |---|---|
 * | VOVCH (charge threshold) | 1.2375 V + THRESH x 56.25 mV, 2.70 V at least |
 * | VOVDIS (discharge threshold) | 0.50625 V + THRESH x 56.25 mV, 2.51 V at least |
 * | STO (battery voltage) | 4.8 V x DATA / 256 |
 * | APM, power meter mode | energy = (POWER << OFFSET) x (theta x L), theta from e-peas |
 *
 * The configuration pins of the new board (docs/14) select an MPPT ratio of
 * 80 % with the default timings, charge up to 3.90 V and cut at 3.01 V; the
 * register reset values are 80 %, the same timings, 0 degC and 45 degC for
 * RDIV 22k with a 10k B3380 NTC, keep-alive, high-power mode and
 * temperature monitoring on. Only the thresholds need writing when the
 * I2C configuration takes over.
 */

#ifndef AEM10900_REGS_H
#define AEM10900_REGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Registers (datasheet Table 12) */
#define AEM10900_VERSION        0x00U
#define AEM10900_MPPTCFG        0x01U
#define AEM10900_VOVDIS         0x02U
#define AEM10900_VOVCH          0x03U
#define AEM10900_TEMPCOLD       0x04U
#define AEM10900_TEMPHOT        0x05U
#define AEM10900_PWR            0x06U
#define AEM10900_SLEEP          0x07U
#define AEM10900_APM            0x09U
#define AEM10900_IRQEN          0x0AU
#define AEM10900_CTRL           0x0BU
#define AEM10900_IRQFLG         0x0CU
#define AEM10900_STATUS         0x0DU
#define AEM10900_APM0           0x0EU
#define AEM10900_TEMP           0x11U
#define AEM10900_STO            0x12U
#define AEM10900_SRC            0x13U
#define AEM10900_PN0            0xE0U
#define AEM10900_PN_LEN         5U

/* STATUS (9.12) */
#define AEM10900_STATUS_VOVDIS      0x02U   /**< battery at or below the discharge threshold */
#define AEM10900_STATUS_VOVCH       0x04U   /**< battery at or above the charge threshold */
#define AEM10900_STATUS_SRCLOW      0x08U   /**< source below the sleep threshold: dark */
#define AEM10900_STATUS_TEMP        0x10U   /**< temperature out of the charging range */
#define AEM10900_STATUS_CHARGERDY   0x40U   /**< the AEM may charge the battery */
#define AEM10900_STATUS_CHARGEDIS   0x80U   /**< DIS_STO_CH high: VBUS blocks the solar charge */

/* CTRL (9.10) */
#define AEM10900_CTRL_UPDATE        0x01U   /**< 1: the I2C registers configure, 0: the pins */
#define AEM10900_CTRL_SYNCBUSY      0x04U

/* APM (9.8): enabled, power meter mode, 128 ms window */
#define AEM10900_APM_EN             0x01U
#define AEM10900_APM_POWER_METER    0x02U
#define AEM10900_APM_WINDOW_128MS   0x00U
#define AEM10900_APM_WINDOW_MS      128U

/* Threshold steps, in microvolts */
#define AEM10900_STEP_UV            56250U
#define AEM10900_VOVCH_BASE_UV      1237500U
#define AEM10900_VOVDIS_BASE_UV     506250U
#define AEM10900_THRESH_MAX         0x3FU

/** Nearest THRESH of a charge threshold (VOVCH) in mV */
static inline uint8_t aem10900_vovch_thresh(uint32_t mv)
{
    uint32_t uv = mv * 1000U;
    uint32_t t;

    if (uv <= AEM10900_VOVCH_BASE_UV) {
        return 0U;
    }
    t = (uv - AEM10900_VOVCH_BASE_UV + (AEM10900_STEP_UV / 2U)) / AEM10900_STEP_UV;
    return (uint8_t)((t > AEM10900_THRESH_MAX) ? AEM10900_THRESH_MAX : t);
}

/** Charge threshold of a VOVCH THRESH in mV (2.70 V at least) */
static inline uint32_t aem10900_vovch_mv(uint8_t thresh)
{
    uint32_t mv = (AEM10900_VOVCH_BASE_UV + ((uint32_t)(thresh & AEM10900_THRESH_MAX) *
                                             AEM10900_STEP_UV)) / 1000U;

    return (mv < 2700U) ? 2700U : mv;
}

/** Nearest THRESH of a discharge threshold (VOVDIS) in mV */
static inline uint8_t aem10900_vovdis_thresh(uint32_t mv)
{
    uint32_t uv = mv * 1000U;
    uint32_t t;

    if (uv <= AEM10900_VOVDIS_BASE_UV) {
        return 0U;
    }
    t = (uv - AEM10900_VOVDIS_BASE_UV + (AEM10900_STEP_UV / 2U)) / AEM10900_STEP_UV;
    return (uint8_t)((t > AEM10900_THRESH_MAX) ? AEM10900_THRESH_MAX : t);
}

/** Discharge threshold of a VOVDIS THRESH in mV (2.51 V at least) */
static inline uint32_t aem10900_vovdis_mv(uint8_t thresh)
{
    uint32_t mv = (AEM10900_VOVDIS_BASE_UV + ((uint32_t)(thresh & AEM10900_THRESH_MAX) *
                                              AEM10900_STEP_UV)) / 1000U;

    return (mv < 2510U) ? 2510U : mv;
}

/** Battery voltage in mV from STO.DATA: 4800 mV x DATA / 256 */
static inline uint32_t aem10900_sto_mv(uint8_t data)
{
    return ((uint32_t)data * 4800U) / 256U;
}

/**
 * APM energy units of the last window in power meter mode: POWER in bits
 * 18:0, OFFSET in bits 22:19, energy = POWER << OFFSET (Table 32)
 */
static inline uint64_t aem10900_apm_units(uint8_t apm0, uint8_t apm1, uint8_t apm2)
{
    uint32_t data = (uint32_t)apm0 | ((uint32_t)apm1 << 8) | ((uint32_t)apm2 << 16);
    uint32_t power = data & 0x7FFFFU;
    uint32_t offset = (data >> 19) & 0x0FU;

    return (uint64_t)power << offset;
}

/**
 * Average power over the window in microwatts: picojoules per microsecond.
 * scale_pj is the energy of one unit (theta x L of the datasheet, from
 * e-peas); 0 means not calibrated and gives 0.
 */
static inline uint32_t aem10900_power_uw(uint64_t units, uint32_t scale_pj, uint32_t window_ms)
{
    uint64_t pj = units * (uint64_t)scale_pj;
    uint64_t uw = pj / ((uint64_t)window_ms * 1000U);

    return (uw > UINT32_MAX) ? UINT32_MAX : (uint32_t)uw;
}

#ifdef __cplusplus
}
#endif

#endif /* AEM10900_REGS_H */
