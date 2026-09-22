/**
 * @file ftms_parse.c
 * @brief The Indoor Bike Data of a trainer, read field by field
 *
 * The wire order and the widths in model/ftms_parse.h. The walk below
 * follows that table from the top and never skips a line, because the
 * position of every field depends on the flags before it.
 */

#include <string.h>

#include "model/ftms_parse.h"

/** One field: is it there, and how many bytes does it take */
struct field {
    uint16_t flag;
    uint8_t bytes;
    bool inverted;      /**< present when the flag is zero (More Data) */
};

/*
 * Every field of the characteristic, in the order it is sent. Keeping the
 * whole table, including the ones nothing is read from, is the point: a
 * field left out would shift everything after it.
 */
static const struct field fields[] = {
    {FTMS_F_MORE_DATA, 2U, true},       /* instantaneous speed */
    {FTMS_F_AVG_SPEED, 2U, false},
    {FTMS_F_CADENCE, 2U, false},        /* instantaneous cadence */
    {FTMS_F_AVG_CADENCE, 2U, false},
    {FTMS_F_TOTAL_DISTANCE, 3U, false},
    {FTMS_F_RESISTANCE, 2U, false},
    {FTMS_F_POWER, 2U, false},          /* instantaneous power */
    {FTMS_F_AVG_POWER, 2U, false},
    {FTMS_F_ENERGY, 4U, false},         /* total, per hour, per minute */
    {FTMS_F_HEART_RATE, 1U, false},
    {FTMS_F_MET, 1U, false},
    {FTMS_F_ELAPSED_TIME, 2U, false},
    {FTMS_F_REMAINING_TIME, 2U, false},
};

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

bool ftms_parse_bike_data(const uint8_t *data, uint16_t len, struct ftms_bike_data *out)
{
    if (out == NULL) {
        return false;
    }

    (void)memset(out, 0, sizeof(*out));
    if ((data == NULL) || (len < 2U)) {
        return false;
    }

    uint16_t flags = rd16(data);
    uint16_t at = 2U;

    for (size_t i = 0U; i < (sizeof(fields) / sizeof(fields[0])); i++) {
        const struct field *f = &fields[i];
        bool present = ((flags & f->flag) != 0U);

        if (f->inverted) {
            present = !present;
        }
        if (!present) {
            continue;
        }
        if ((uint32_t)at + f->bytes > len) {
            /*
             * The flags promise a field the payload does not hold. Reading
             * on would take the following fields from the wrong bytes, so
             * the notification is dropped whole.
             */
            (void)memset(out, 0, sizeof(*out));

            return false;
        }

        switch (f->flag) {
        case FTMS_F_MORE_DATA:
            out->speed_kmh100 = rd16(&data[at]);
            out->have_speed = true;
            break;
        case FTMS_F_CADENCE:
            /* the profile sends halves of a turn per minute */
            out->cadence_rpm = (uint16_t)(rd16(&data[at]) / 2U);
            out->have_cadence = true;
            break;
        case FTMS_F_POWER:
            out->power_w = (int16_t)rd16(&data[at]);
            out->have_power = true;
            break;
        case FTMS_F_HEART_RATE:
            out->hr_bpm = data[at];
            out->have_hr = true;
            break;
        case FTMS_F_ELAPSED_TIME:
            out->elapsed_s = rd16(&data[at]);
            out->have_elapsed = true;
            break;
        default:
            break;      /* a field the device does not use: walked past */
        }
        at = (uint16_t)(at + f->bytes);
    }

    return true;
}
