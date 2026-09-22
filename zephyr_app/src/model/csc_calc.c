/**
 * @file csc_calc.c
 * @brief Speed and cadence from the counters a CSC sensor sends
 *
 * Units and rules in model/csc_calc.h.
 */

#include <string.h>

#include "model/csc_calc.h"

void csc_calc_init(struct csc_calc *c, uint16_t wheel_mm)
{
    if (c == NULL) {
        return;
    }

    (void)memset(c, 0, sizeof(*c));
    c->wheel_mm = (wheel_mm != 0U) ? wheel_mm : (uint16_t)CSC_WHEEL_MM;
}

void csc_calc_set_wheel(struct csc_calc *c, uint16_t wheel_mm)
{
    if ((c == NULL) || (wheel_mm == 0U)) {
        return;
    }

    c->wheel_mm = wheel_mm;
}

uint16_t csc_calc_speed(struct csc_calc *c, uint32_t wheel_revs, uint16_t wheel_time)
{
    if (c == NULL) {
        return 0U;
    }

    if (!c->have_wheel) {
        /* nothing to compare against yet */
        c->wheel_revs = wheel_revs;
        c->wheel_time = wheel_time;
        c->have_wheel = true;

        return 0U;
    }

    /* both counters wrap, and unsigned arithmetic gives the right
     * difference as long as less than one full turn of the counter went by */
    uint32_t rev_delta = wheel_revs - c->wheel_revs;
    uint16_t time_delta = (uint16_t)(wheel_time - c->wheel_time);

    c->wheel_revs = wheel_revs;
    c->wheel_time = wheel_time;

    if ((time_delta == 0U) || (rev_delta == 0U)) {
        /* the sensor keeps sending with the bike standing still */
        return 0U;
    }

    /*
     * From the units of the profile to hundredths of a kilometre per hour:
     *
     *   distance = rev_delta x wheel_mm                    [mm]
     *   time     = time_delta / 1024                       [s]
     *   speed    = distance x 1024 / time_delta            [mm/s]
     *   km/h     = mm/s x 3600 / 1e6 = mm/s x 0,0036
     *   0,01km/h = mm/s x 0,36 = mm/s x 36 / 100
     *
     * so 0,01 km/h = rev_delta x wheel_mm x 1024 x 36 / (time_delta x 100).
     * In 64 bits because the numerator passes four thousand million as
     * soon as the gap between two notifications grows.
     */
    uint64_t num = (uint64_t)rev_delta * c->wheel_mm * CSC_TICKS_PER_S * 36U;
    uint64_t speed = num / ((uint64_t)time_delta * 100U);

    if (speed > CSC_SPEED_MAX) {
        /* a wrap that took more than one turn of the counter, or noise */
        return 0U;
    }

    return (uint16_t)speed;
}

uint8_t csc_calc_cadence(struct csc_calc *c, uint16_t crank_revs, uint16_t crank_time)
{
    if (c == NULL) {
        return 0U;
    }

    if (!c->have_crank) {
        c->crank_revs = crank_revs;
        c->crank_time = crank_time;
        c->have_crank = true;

        return 0U;
    }

    uint16_t rev_delta = (uint16_t)(crank_revs - c->crank_revs);
    uint16_t time_delta = (uint16_t)(crank_time - c->crank_time);

    c->crank_revs = crank_revs;
    c->crank_time = crank_time;

    if ((time_delta == 0U) || (rev_delta == 0U)) {
        return 0U;      /* coasting */
    }

    /* turns per minute: rev_delta over time_delta/1024 seconds, times 60 */
    uint32_t rpm = ((uint32_t)rev_delta * 60U * CSC_TICKS_PER_S) / (uint32_t)time_delta;

    if (rpm > CSC_CADENCE_MAX) {
        return 0U;
    }

    return (uint8_t)rpm;
}
