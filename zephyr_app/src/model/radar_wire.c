/**
 * @file radar_wire.c
 * @brief Turning what a radar sends into a frame the model takes
 *
 * Caveats and the one number to check on a bench in model/radar_wire.h.
 */

#include <string.h>

#include "model/radar_wire.h"

bool radar_wire_varia(const uint8_t *buf, size_t len, struct radar_frame *out)
{
    if (out == NULL) {
        return false;
    }

    (void)memset(out, 0, sizeof(*out));
    if ((buf == NULL) || (len < RADAR_VARIA_HEADER)) {
        return false;
    }

    size_t body = len - RADAR_VARIA_HEADER;

    if ((body % RADAR_VARIA_PER_TARGET) != 0U) {
        return false;   /* not a whole number of vehicles: not our packet */
    }

    /*
     * An empty notification, with the counter and nothing else, is the
     * radar saying the road is clear. It is a valid frame with no targets,
     * and the model needs it: it is what makes a mark stop fading.
     */
    size_t n = body / RADAR_VARIA_PER_TARGET;

    for (size_t i = 0U; (i < n) && (out->n < RADAR_TARGETS_MAX); i++) {
        const uint8_t *g = &buf[RADAR_VARIA_HEADER + (i * RADAR_VARIA_PER_TARGET)];
        struct radar_target *t = &out->t[out->n];

        if (g[1] == 0U) {
            continue;   /* no distance: the slot is empty */
        }

        t->id = g[0];
        t->range_m = g[1];
        t->closing_kmh = (uint16_t)((float)g[2] * RADAR_VARIA_SPEED_KMH);
        t->side = (uint8_t)RADAR_SIDE_UNKNOWN;
        /* the service carries no threat level: the model works one out */
        t->level = (uint8_t)RADAR_LEVEL_NONE;
        out->n++;
    }

    return true;
}
