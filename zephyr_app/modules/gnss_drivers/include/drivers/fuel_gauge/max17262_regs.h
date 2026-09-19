/**
 * @file max17262_regs.h
 * @brief Registers of the MAX17262 fuel gauge and their units
 *
 * Plain C, no Zephyr, so that the host tests check it. Units from the
 * MAX17262 datasheet (Maxim, now Analog Devices), Table 2 "ModelGauge m5
 * Register Standard Resolutions", for the internal current sense:
 *
 * | Register type | LSB |
 * |---|---|
 * | capacity | 0.5 mAh |
 * | percentage | 1/256 % |
 * | voltage | 1.25 mV / 16 = 78.125 uV |
 * | current | 156.25 uA, two's complement, positive when charging |
 * | temperature | 1/256 degC, two's complement |
 * | time | 5.625 s |
 *
 * The Zephyr driver maxim,max17262 writes DesignCap and IChgTerm without
 * these steps and returns capacities as if the step were 1 mAh; this header
 * is the reason the port has its own driver (adi,max17262).
 */

#ifndef MAX17262_REGS_H
#define MAX17262_REGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Registers (datasheet Table 16, register memory map) */
#define MAX17262_STATUS         0x00U
#define MAX17262_REPCAP         0x05U
#define MAX17262_REPSOC         0x06U
#define MAX17262_TEMP           0x08U
#define MAX17262_VCELL          0x09U
#define MAX17262_CURRENT        0x0AU
#define MAX17262_AVGCURRENT     0x0BU
#define MAX17262_FULLCAPREP     0x10U
#define MAX17262_TTE            0x11U
#define MAX17262_CYCLES         0x17U
#define MAX17262_DESIGNCAP      0x18U
#define MAX17262_CONFIG         0x1DU
#define MAX17262_ICHGTERM       0x1EU
#define MAX17262_TTF            0x20U
#define MAX17262_VEMPTY         0x3AU
#define MAX17262_FSTAT          0x3DU
#define MAX17262_SOFTWAKEUP     0x60U
#define MAX17262_HIBCFG         0xBAU
#define MAX17262_MODELCFG       0xDBU

/** Status.POR: a power-on reset happened, the EZ configuration must be written */
#define MAX17262_STATUS_POR     0x0002U
/** FStat.DNR: data not ready yet after a power-on reset */
#define MAX17262_FSTAT_DNR      0x0001U
/** ModelCfg.Refresh: the model reload is still running */
#define MAX17262_MODELCFG_REFRESH 0x8000U
/** Config.ETHRM: measure the TH pin; off when TH is tied to BATT */
#define MAX17262_CONFIG_ETHRM   0x0010U
/** SoftWakeup commands, to leave hibernation while configuring */
#define MAX17262_SOFTWAKEUP_ON  0x0090U
#define MAX17262_SOFTWAKEUP_OFF 0x0000U
/** TTE and TTF when the gauge does not know */
#define MAX17262_TIME_UNKNOWN   0xFFFFU
/** Minutes returned for an unknown time */
#define MAX17262_MINUTES_UNKNOWN 0xFFFFFFFFU

/* ---- Readings ---------------------------------------------------------- */

/** VCell to microvolts */
static inline int32_t max17262_microvolts(uint16_t vcell)
{
    return (int32_t)(((uint32_t)vcell * 625U) / 8U);
}

/** Current or AvgCurrent to microamperes, negative when discharging */
static inline int32_t max17262_microamps(uint16_t current)
{
    return ((int32_t)(int16_t)current * 625) / 4;
}

/** RepSOC to a rounded percentage, 0 to 100 */
static inline uint8_t max17262_percent(uint16_t repsoc)
{
    uint32_t pct = ((uint32_t)repsoc + 128U) >> 8;

    return (uint8_t)((pct > 100U) ? 100U : pct);
}

/** RepCap, FullCapRep to microampere-hours */
static inline uint32_t max17262_microamp_hours(uint16_t cap)
{
    return (uint32_t)cap * 500U;
}

/** TTE, TTF to minutes (5.625 s = 3/32 min), or MAX17262_MINUTES_UNKNOWN */
static inline uint32_t max17262_minutes(uint16_t time)
{
    if (time == MAX17262_TIME_UNKNOWN) {
        return MAX17262_MINUTES_UNKNOWN;
    }
    return ((uint32_t)time * 3U) / 32U;
}

/** Temp to tenths of a kelvin (the fuel gauge API unit): degC * 10 + 2731.5 */
static inline uint16_t max17262_deci_kelvin(uint16_t temp)
{
    return (uint16_t)((((int32_t)(int16_t)temp * 10) + 699264) / 256);
}

/** Temp to whole degrees Celsius, rounded towards zero */
static inline int16_t max17262_celsius(uint16_t temp)
{
    return (int16_t)((int16_t)temp / 256);
}

/* ---- EZ configuration --------------------------------------------------- */

/** DesignCap from the label capacity in mAh */
static inline uint16_t max17262_designcap(uint32_t mah)
{
    return (uint16_t)(mah * 2U);
}

/** IChgTerm from the charger termination current in mA (1000 / 156.25 = 32 / 5) */
static inline uint16_t max17262_ichgterm(uint32_t ma)
{
    return (uint16_t)((ma * 32U) / 5U);
}

/** VEmpty: empty voltage in 10 mV steps over bits 15:7, recovery voltage in 40 mV steps below */
static inline uint16_t max17262_vempty(uint32_t empty_mv, uint32_t recovery_mv)
{
    return (uint16_t)(((empty_mv / 10U) << 7) | ((recovery_mv / 40U) & 0x7FU));
}

/** ModelCfg: Refresh, lithium cobalt model (ModelID 0), 10k NTC, VChg above 4.25 V */
static inline uint16_t max17262_modelcfg(uint32_t charge_mv)
{
    return (uint16_t)(MAX17262_MODELCFG_REFRESH | ((charge_mv > 4250U) ? 0x0400U : 0U));
}

#ifdef __cplusplus
}
#endif

#endif /* MAX17262_REGS_H */
