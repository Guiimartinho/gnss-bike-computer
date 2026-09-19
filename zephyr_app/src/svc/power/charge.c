/**
 * @file charge.c
 * @brief Where the energy comes from (charge.h, docs/16 Carga)
 */

#include "svc/charge.h"

#include "app/app_events.h"

charge_state_t charge_state(const charge_inputs_t *in)
{
    if (!in->vbus) {
        return in->solar ? CHARGE_SOLAR : CHARGE_BATTERY;
    }
    if (in->error != 0U) {
        return CHARGE_FAULT;
    }
    if ((in->status & CHARGE_STATUS_COMPLETED) != 0U) {
        return CHARGE_USB_FULL;
    }
    if ((in->status & (CHARGE_STATUS_TRICKLE | CHARGE_STATUS_CC | CHARGE_STATUS_CV)) != 0U) {
        return CHARGE_USB;
    }
    if (((in->status & CHARGE_STATUS_DIE_HOT) != 0U) ||
        ((in->ntc & (CHARGE_NTC_COLD | CHARGE_NTC_HOT)) != 0U)) {
        return CHARGE_THERMAL_PAUSE;
    }
    /* VBUS just came: the charger measures the cell before it starts */
    return CHARGE_USB;
}

uint8_t charge_app(charge_state_t state)
{
    switch (state) {
    case CHARGE_SOLAR:
        return APP_CHARGE_SOLAR;
    case CHARGE_USB:
    case CHARGE_THERMAL_PAUSE:
    case CHARGE_FAULT:
        return APP_CHARGE_USB;
    case CHARGE_USB_FULL:
        return APP_CHARGE_USB_FULL;
    default:
        return APP_CHARGE_NONE;
    }
}
