/**
 * @file cps_parse.c
 * @brief The Cycling Power Measurement of a power meter, read field by field
 *
 * The field table, the units and the trap about the two clocks are in
 * model/cps_parse.h.
 */

#include <string.h>

#include "model/cps_parse.h"

/** Widths of every optional field, in wire order, for the walk */
struct cps_field {
    uint16_t flag;
    uint8_t bytes;
};

static const struct cps_field cps_fields[] = {
    {CPS_F_BALANCE, 1U},
    {CPS_F_TORQUE, 2U},
    {CPS_F_WHEEL_REV, 6U},          /* uint32 turns + uint16 time */
    {CPS_F_CRANK_REV, 4U},          /* uint16 turns + uint16 time */
    {CPS_F_EXTREME_FORCE, 4U},
    {CPS_F_EXTREME_TORQUE, 4U},
    {CPS_F_EXTREME_ANGLES, 3U},
    {CPS_F_TOP_DEAD_SPOT, 2U},
    {CPS_F_BOTTOM_DEAD_SPOT, 2U},
    {CPS_F_ENERGY, 2U},
};

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool cps_parse_measurement(const uint8_t *data, uint16_t len, struct cps_measurement *out)
{
    if ((data == NULL) || (out == NULL) || (len < 4U)) {
        /* two bytes of flags and two of power are the least there can be */
        return false;
    }

    (void)memset(out, 0, sizeof(*out));

    uint16_t flags = rd16(data);
    uint16_t pos = 2U;

    out->power_w = (int16_t)rd16(&data[pos]);
    pos += 2U;

    /* the two bits that carry no field of their own */
    out->balance_is_left = ((flags & CPS_F_BALANCE_LEFT) != 0U);
    out->torque_from_crank = ((flags & CPS_F_TORQUE_CRANK) != 0U);
    out->offset_needed = ((flags & CPS_F_OFFSET_NEEDED) != 0U);

    for (unsigned int i = 0U; i < (sizeof(cps_fields) / sizeof(cps_fields[0])); i++) {
        const struct cps_field *f = &cps_fields[i];

        if ((flags & f->flag) == 0U) {
            continue;
        }

        if ((uint32_t)pos + f->bytes > len) {
            /* the flags promise more than the payload holds */
            (void)memset(out, 0, sizeof(*out));

            return false;
        }

        switch (f->flag) {
        case CPS_F_BALANCE:
            /* the wire carries half-percent steps */
            out->balance_pct = (uint8_t)(data[pos] / 2U);
            out->have_balance = true;
            break;
        case CPS_F_TORQUE:
            out->torque_32nm = rd16(&data[pos]);
            out->have_torque = true;
            break;
        case CPS_F_WHEEL_REV:
            out->wheel_revs = rd32(&data[pos]);
            out->wheel_time = rd16(&data[pos + 4U]);
            out->have_wheel = true;
            break;
        case CPS_F_CRANK_REV:
            out->crank_revs = rd16(&data[pos]);
            out->crank_time = rd16(&data[pos + 2U]);
            out->have_crank = true;
            break;
        case CPS_F_ENERGY:
            out->energy_kj = rd16(&data[pos]);
            out->have_energy = true;
            break;
        default:
            /*
             * The pedal analysis fields. They are walked past so that
             * everything after them lands on the right bytes, and not
             * kept: nothing on this device draws a force curve yet.
             */
            break;
        }

        pos += f->bytes;
    }

    return true;
}

uint8_t cps_cadence_rpm(const struct cps_measurement *prev, const struct cps_measurement *now)
{
    if ((prev == NULL) || (now == NULL) || !prev->have_crank || !now->have_crank) {
        return 0U;
    }

    uint16_t rev_delta = (uint16_t)(now->crank_revs - prev->crank_revs);
    uint16_t time_delta = (uint16_t)(now->crank_time - prev->crank_time);

    if ((time_delta == 0U) || (rev_delta == 0U)) {
        return 0U;      /* coasting, or the meter repeated itself */
    }

    uint32_t rpm = ((uint32_t)rev_delta * 60U * CPS_CRANK_TICKS_PER_S) / (uint32_t)time_delta;

    if (rpm > 250U) {
        return 0U;      /* a wrap of more than one turn of the counter */
    }

    return (uint8_t)rpm;
}

uint16_t cps_speed_kmh100(const struct cps_measurement *prev, const struct cps_measurement *now,
                          uint16_t wheel_mm)
{
    if ((prev == NULL) || (now == NULL) || !prev->have_wheel || !now->have_wheel ||
        (wheel_mm == 0U)) {
        return 0U;
    }

    uint32_t rev_delta = now->wheel_revs - prev->wheel_revs;
    uint16_t time_delta = (uint16_t)(now->wheel_time - prev->wheel_time);

    if ((time_delta == 0U) || (rev_delta == 0U)) {
        return 0U;
    }

    /*
     * The same arithmetic as `model/csc_calc.c`, with the clock of **this**
     * service: 2048 ticks a second and not 1024. Getting that wrong is a
     * factor of two on the screen.
     *
     *   0,01 km/h = rev_delta x wheel_mm x 2048 x 36 / (time_delta x 100)
     */
    uint64_t num = (uint64_t)rev_delta * wheel_mm * CPS_WHEEL_TICKS_PER_S * 36U;
    uint64_t speed = num / ((uint64_t)time_delta * 100U);

    if (speed > 15000U) {
        return 0U;      /* past 150 km/h: a wrap, or noise */
    }

    return (uint16_t)speed;
}
