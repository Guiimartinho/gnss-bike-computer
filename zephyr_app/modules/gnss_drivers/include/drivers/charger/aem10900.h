/**
 * @file aem10900.h
 * @brief What the AEM10900 driver adds to the Zephyr charger API
 */

#ifndef AEM10900_H
#define AEM10900_H

#include <zephyr/drivers/charger.h>

#ifdef __cplusplus
extern "C" {
#endif

enum aem10900_prop {
    /** custom_uint: APM energy units of the last window */
    AEM10900_PROP_APM_UNITS = CHARGER_PROP_CUSTOM_BEGIN,
    /** custom_uint: average solar power of the last window in uW (0 when not calibrated) */
    AEM10900_PROP_POWER_UW,
    /** custom_uint: battery voltage measured by the AEM in uV */
    AEM10900_PROP_STO_UV,
    /**
     * custom_bool, set only: true hands the configuration back to the pins
     * (the 3.90 V of the board), before the ship mode; the next set of the
     * charge voltage takes it back
     */
    AEM10900_PROP_PIN_CONFIG,
};

#ifdef __cplusplus
}
#endif

#endif /* AEM10900_H */
