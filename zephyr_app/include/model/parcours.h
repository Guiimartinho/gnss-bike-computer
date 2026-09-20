/**
 * @file parcours.h
 * @brief Parcours (Route) management for navigation
 *
 * Handles loading and following of GPX routes for navigation.
 */

#ifndef MODEL_PARCOURS_H
#define MODEL_PARCOURS_H

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"
#include "model/liste_points.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Configuration
 * ========================================================================== */

/** Maximum points per parcours */
/*
 * Points of a route held in memory (`CONFIG_GNSS_ROUTE_POINTS`). A longer
 * file is halved as it loads, so the whole route fits with less
 * resolution; the host tests, which have no Kconfig, take the default.
 */
#if defined(CONFIG_GNSS_ROUTE_POINTS)
#define PARCOURS_MAX_POINTS     ((uint16_t)CONFIG_GNSS_ROUTE_POINTS)
#else
#define PARCOURS_MAX_POINTS     500U
#endif

/** Maximum name length */
#define PARCOURS_NAME_LEN       32U

/** Longest line of a route file: `lat lon alt`, as the legacy writes it */
#define PARCOURS_LINE_MAX       64U

/** Off-route threshold in meters */
/** Turns of the cue sheet kept in memory (`CONFIG_GNSS_ROUTE_CUES`) */
#if defined(CONFIG_GNSS_ROUTE_CUES)
#define PARCOURS_MAX_CUES       ((uint16_t)CONFIG_GNSS_ROUTE_CUES)
#else
#define PARCOURS_MAX_CUES       64U
#endif

/** Longest street name of a turn, as the file carries it */
#define PARCOURS_STREET_LEN     22U

#define PARCOURS_OFF_ROUTE_M    50.0f

/** Point proximity threshold in meters */
#define PARCOURS_NEAR_POINT_M   30.0f

/* ==========================================================================
 * Type Definitions
 * ========================================================================== */

/** Parcours state */
typedef enum {
    PARCOURS_STATE_IDLE = 0,    /**< No parcours loaded */
    PARCOURS_STATE_LOADED,      /**< Parcours loaded, not started */
    PARCOURS_STATE_ACTIVE,      /**< Actively navigating */
    PARCOURS_STATE_FINISHED,    /**< Reached end of route */
    PARCOURS_STATE_OFF_ROUTE    /**< Too far from route */
} parcours_state_t;

/** Parcours info structure */
typedef struct {
    char name[PARCOURS_NAME_LEN];   /**< Route name */
    uint16_t num_points;            /**< Number of points */
    float total_distance;           /**< Total distance in meters */
    float total_climb;              /**< Total elevation gain */
    bool valid;                     /**< Is parcours valid */
} parcours_info_t;

/** One turn of the route, from the cue sheet of the file */
typedef struct {
    uint16_t point;                         /**< point of the loaded route */
    uint8_t turn;                           /**< enum route_turn */
    char street[PARCOURS_STREET_LEN + 1U];
} parcours_cue_t;

/** Navigation info structure */
typedef struct {
    float dist_to_route;        /**< Distance to nearest point on route (m) */
    float dist_remaining;       /**< Distance remaining to end (m) */
    float dist_completed;       /**< Distance completed (m) */
    float pct_complete;         /**< Percentage complete (0-100) */
    float bearing;              /**< Bearing to next point (degrees) */
    float altitude_next;        /**< Altitude of next point (m) */
    uint16_t current_idx;       /**< Current point index */
    bool on_route;              /**< True if within threshold of route */
} nav_info_t;

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

/**
 * @brief Initialize parcours module
 * @return APP_OK on success
 */
app_err_t parcours_init(void);

/**
 * @brief Load parcours from file
 * @param filename Path to GPX/CRS file
 * @return APP_OK on success
 */
app_err_t parcours_load(const char *filename);

/**
 * @brief Unload current parcours
 */
void parcours_unload(void);

/**
 * @brief Start navigation
 * @return APP_OK on success
 */
app_err_t parcours_start(void);

/**
 * @brief Stop navigation
 */
void parcours_stop(void);

/**
 * @brief Update navigation with current position
 * @param lat Current latitude
 * @param lon Current longitude
 * @param alt Current altitude
 */
void parcours_update(float lat, float lon, float alt);

/**
 * @brief Get parcours state
 * @return Current state
 */
parcours_state_t parcours_get_state(void);

/**
 * @brief Get parcours info
 * @param info Pointer to info structure to fill
 * @return APP_OK on success
 */
app_err_t parcours_get_info(parcours_info_t *info);

/**
 * @brief Get navigation info
 * @param nav Pointer to nav info structure to fill
 * @return APP_OK on success
 */
app_err_t parcours_get_nav_info(nav_info_t *nav);

/**
 * @brief Check if parcours is loaded
 * @return true if loaded
 */
bool parcours_is_loaded(void);

/**
 * @brief The next turn of the route, when the file brought a cue sheet
 *
 * @param out Where to write the turn
 * @param dist_m Distance to it, in metres
 * @return true when there is a turn ahead
 */
bool parcours_get_next_cue(parcours_cue_t *out, float *dist_m);

/**
 * @brief Name of the route, as the file says it (`.RTE`) or its file name
 */
const char *parcours_get_name(void);

/** Called now and then while a file is being read, to feed the watchdog */
typedef void (*parcours_progress_fn)(void);

/**
 * @brief Who to call while a long file is being read
 *
 * A course as it comes from a service is megabytes of XML, and reading it
 * takes longer than the four seconds of the watchdog; the service that
 * loads it says here how to feed its channel.
 */
void parcours_set_progress(parcours_progress_fn fn);

/**
 * @brief Check if currently navigating
 * @return true if active
 */
bool parcours_is_active(void);

/**
 * @brief Get point at index
 * @param index Point index
 * @return Pointer to point or NULL
 */
const point_t *parcours_get_point(uint16_t index);

/**
 * @brief Get number of points
 * @return Number of points
 */
uint16_t parcours_get_num_points(void);

/**
 * @brief Get current point index (nearest passed point)
 * @return Current index
 */
uint16_t parcours_get_current_index(void);

/**
 * @brief Get distance to next waypoint
 * @return Distance in meters
 */
float parcours_get_dist_to_next(void);

/**
 * @brief Get bearing to next waypoint
 * @return Bearing in degrees (0-360)
 */
float parcours_get_bearing_to_next(void);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_PARCOURS_H */
