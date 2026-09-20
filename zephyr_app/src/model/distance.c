/**
 * @file distance.c
 * @brief Distance ridden, accumulated as the legacy does
 *
 * Rules and origin in model/distance.h.
 */

#include "model/distance.h"

#include <string.h>

#include "model/vecteur.h"

void distance_init(struct distance_acc *d)
{
    if (d == NULL) {
        return;
    }
    (void)memset(d, 0, sizeof(*d));
}

bool distance_add(struct distance_acc *d, float lat, float lon)
{
    if (d == NULL) {
        return false;
    }

    if (!d->has_prev) {
        /* the legacy starts from (0, 0), which the first jump throws away */
        d->has_prev = true;
        d->prev_lat = lat;
        d->prev_lon = lon;

        return false;
    }

    d->total_m += distance_between(d->prev_lat, d->prev_lon, lat, lon);
    d->prev_lat = lat;
    d->prev_lon = lon;

    if (d->started) {
        if (d->total_m > (d->last_save_m + DISTANCE_SNAPSHOT_M)) {
            d->last_save_m = d->total_m;

            return true;
        }

        return false;
    }

    if (d->total_m > (d->last_save_m + DISTANCE_START_M)) {
        /* first real move: what came before was the receiver settling */
        d->total_m = d->last_save_m;
        d->started = true;
    }

    return false;
}

float distance_total(const struct distance_acc *d)
{
    return (d == NULL) ? 0.0f : d->total_m;
}

void distance_restore(struct distance_acc *d, float total_m)
{
    if (d == NULL) {
        return;
    }
    d->total_m = total_m;
    d->last_save_m = total_m;
    d->started = true;
}
