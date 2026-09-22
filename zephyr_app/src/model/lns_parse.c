/**
 * @file lns_parse.c
 * @brief Where the phone says it is, read field by field
 *
 * The field table, the units and the rule about the position status are in
 * model/lns_parse.h.
 */

#include <string.h>

#include "model/lns_parse.h"

/** Widths of every optional field, in wire order, for the walk */
struct lns_field {
    uint16_t flag;
    uint8_t bytes;
};

static const struct lns_field lns_fields[] = {
    {LNS_F_SPEED, 2U},
    {LNS_F_DISTANCE, 3U},
    {LNS_F_LOCATION, 8U},       /* two sint32 */
    {LNS_F_ELEVATION, 3U},      /* sint24 */
    {LNS_F_HEADING, 2U},
    {LNS_F_ROLLING_TIME, 1U},
    {LNS_F_UTC_TIME, 7U},
};

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int32_t rd32(const uint8_t *p)
{
    uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                 ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    return (int32_t)v;
}

/** A signed 24-bit value, sign extended into 32 bits */
static int32_t rd24s(const uint8_t *p)
{
    uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);

    if ((v & 0x00800000UL) != 0U) {
        v |= 0xFF000000UL;
    }

    return (int32_t)v;
}

bool lns_parse_location(const uint8_t *data, uint16_t len, struct lns_location *out)
{
    if ((data == NULL) || (out == NULL) || (len < 2U)) {
        return false;
    }

    (void)memset(out, 0, sizeof(*out));

    uint16_t flags = rd16(data);
    uint16_t pos = 2U;

    out->status = (uint8_t)((flags & LNS_F_POS_STATUS_MASK) >> LNS_F_POS_STATUS_SHIFT);

    for (unsigned int i = 0U; i < (sizeof(lns_fields) / sizeof(lns_fields[0])); i++) {
        const struct lns_field *f = &lns_fields[i];

        if ((flags & f->flag) == 0U) {
            continue;
        }

        if ((uint32_t)pos + f->bytes > len) {
            /* the flags promise more than the payload holds */
            (void)memset(out, 0, sizeof(*out));

            return false;
        }

        switch (f->flag) {
        case LNS_F_SPEED:
            out->speed_cms = rd16(&data[pos]);
            out->have_speed = true;
            break;
        case LNS_F_LOCATION:
            out->lat_e7 = rd32(&data[pos]);
            out->lon_e7 = rd32(&data[pos + 4U]);
            out->have_location = true;
            break;
        case LNS_F_ELEVATION:
            out->elevation_cm = rd24s(&data[pos]);
            out->have_elevation = true;
            break;
        case LNS_F_HEADING:
            out->heading_cdeg = rd16(&data[pos]);
            out->have_heading = true;
            break;
        default:
            /*
             * The total distance, the rolling time and the clock. They are
             * walked past so that everything after them lands on the right
             * bytes, and not kept: the device measures its own distance
             * and keeps its own time.
             */
            break;
        }

        pos += f->bytes;
    }

    return true;
}

bool lns_position_is_usable(const struct lns_location *loc)
{
    if ((loc == NULL) || !loc->have_location) {
        return false;
    }

    /*
     * Only a real position. A phone with no fix still sends the field, and
     * an estimated or last-known one would put the rider where the phone
     * last thought it was — which is worse than showing nothing, because
     * it looks like an answer.
     */
    return loc->status == (uint8_t)LNS_POS_OK;
}
