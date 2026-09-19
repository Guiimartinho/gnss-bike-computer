/**
 * @file charge.h
 * @brief Where the energy comes from: the charge machine of docs/16 (Carga)
 *
 * Plain C, no Zephyr (host test test_charge). The two chargers work on
 * their own (docs/15, Convivência das duas cargas): with VBUS, the hardware
 * blocks the solar charger and only the nPM1300 charges. The state is read
 * from them at each event and poll, so it is a mapping of their registers,
 * not a memory of its own:
 *
 * | State | When |
 * |---|---|
 * | Battery | no VBUS, the panel gives nothing |
 * | Solar | no VBUS, the AEM10900 transfers energy |
 * | USB | VBUS, the nPM1300 charges (trickle, constant current or voltage) |
 * | USB full | VBUS, charging completed |
 * | Thermal pause | VBUS, the NTC is cold or hot, or the die too hot |
 * | Fault | VBUS, the charger latched an error |
 *
 * Register bits: nPM1300 datasheet, BCHGCHARGESTATUS (0x34), BCHGERRREASON
 * (0x36) and NTCSTATUS (0x32) of the charger block.
 */

#ifndef SVC_CHARGE_H
#define SVC_CHARGE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BCHGCHARGESTATUS */
#define CHARGE_STATUS_COMPLETED     0x02U
#define CHARGE_STATUS_TRICKLE       0x04U
#define CHARGE_STATUS_CC            0x08U
#define CHARGE_STATUS_CV            0x10U
#define CHARGE_STATUS_DIE_HOT       0x40U
/* NTCSTATUS */
#define CHARGE_NTC_COLD             0x01U
#define CHARGE_NTC_HOT              0x08U

typedef enum {
    CHARGE_BATTERY = 0,
    CHARGE_SOLAR,
    CHARGE_USB,
    CHARGE_USB_FULL,
    CHARGE_THERMAL_PAUSE,
    CHARGE_FAULT
} charge_state_t;

typedef struct {
    bool vbus;
    uint8_t status;         /**< BCHGCHARGESTATUS */
    uint8_t error;          /**< BCHGERRREASON, latched */
    uint8_t ntc;            /**< NTCSTATUS */
    bool solar;             /**< the solar charger transfers energy */
} charge_inputs_t;

/** The state from the registers of the chargers */
charge_state_t charge_state(const charge_inputs_t *in);

/** What the screens show (enum app_charge): a pause or a fault shows the USB */
uint8_t charge_app(charge_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* SVC_CHARGE_H */
