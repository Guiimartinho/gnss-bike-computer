/**
 * @file model_internal.h
 * @brief State of the model service shared by its files
 *
 * The model thread is the only writer of everything here (docs/16,
 * Princípios: um dono por dado). The other threads get copies through
 * chan_model_state.
 */

#ifndef MODEL_INTERNAL_H
#define MODEL_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/smf.h>

#include "app/app_events.h"
#include "model/climb.h"

/**
 * Points of the thinned copy of the route the climb scan walks.
 *
 * A hundred-kilometre route becomes 512 samples, about 200 m apart,
 * which is enough for a climb that has to be 500 m long to count, and
 * costs 4 KB instead of the 32 KB the route itself would.
 */
#define MODEL_CLIMB_SCAN_MAX    512U
#include "model/power_zone.h"
#include "model/rr_zone.h"
#include "model/suffer_score.h"
#include "ui/ui_model.h"

/** What the model keeps between events */
/** A position older than this shows the GNSS screen (legacy LOCATOR_MAX_DATA_AGE_MS) */
#define POS_MAX_AGE_MS  6000U

struct model_ctx {
    struct smf_ctx smf;             /**< first member: mode machine */
    uint8_t mode;                   /**< enum app_mode in force */
    uint8_t mode_req;               /**< mode asked by the last command */
    bool shutting_down;
    bool recording;                 /**< an activity is being recorded, as published */

    /* inputs, as they last arrived */
    struct app_gnss_fix fix;
    bool have_fix_msg;
    uint32_t fix_uptime_ms;         /**< uptime of the last valid position */
    uint32_t sim_uptime_ms;         /**< uptime of the last simulated position ($LOC) */
    struct app_gnss_sky sky;
    struct app_power_status power;
    struct app_phone_nav nav;
    struct app_pair_list pair;
    struct app_storage_info storage;
    struct app_activity act;        /**< totals, auto-pause and laps (model/activity.h) */
    struct climb_list climbs;       /**< climbs of the loaded route (model/climb.h) */
    struct climb_state climb;       /**< where the rider is on the one ahead */
    float heading_deg;
    bool heading_valid;
    float pitch_deg;
    float rough[3];
    int8_t pitch_histo[UI_HISTO_MAX];
    uint8_t pitch_histo_n;

    /* external sensors */
    struct app_link_status link[APP_EXT_KINDS];
    struct app_ext_sensor ext[APP_EXT_KINDS];
    uint32_t ext_uptime_ms[APP_EXT_KINDS];

    /* legacy accumulators (Boucle, PowerZone, SufferScore, RRZone) */
    power_zone_t zones;
    suffer_score_t suffer;
    rr_zone_t rr;

    /* PRC */
    int8_t route_sel;               /**< index in the route list, or -1 */
    uint8_t zoom;                   /**< legacy Zoom level */
};

/**
 * @brief Fill the snapshot the interface draws
 */
void model_ui_fill(const struct model_ctx *ctx, ui_model_t *m);

/**
 * @brief Seconds of the day of the last GNSS time, or UI_TIME_UNKNOWN
 */
uint32_t model_time_of_day(const struct model_ctx *ctx);

#endif /* MODEL_INTERNAL_H */
