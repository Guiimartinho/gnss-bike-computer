/**
 * @file parcours.c
 * @brief Parcours (Route) management implementation
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "model/parcours.h"
#include "model/vecteur.h"

LOG_MODULE_REGISTER(parcours, CONFIG_LOG_DEFAULT_LEVEL);

/* ==========================================================================
 * Private Definitions
 * ========================================================================== */

/** Earth radius in meters */
#define EARTH_RADIUS_M      6371000.0f

/** Degrees to radians */
#define DEG_TO_RAD(x)       ((x) * 0.017453292519943295f)

/** Radians to degrees */
#define RAD_TO_DEG(x)       ((x) * 57.29577951308232f)

/* ==========================================================================
 * Private Variables
 * ========================================================================== */

/** Point storage */
static point_t points[PARCOURS_MAX_POINTS];

/** One point of the file out of this many is kept (see add_point()) */
static uint16_t route_stride = 1U;

/** Points read from the file, kept or not */
static uint32_t route_seen;

/** Number of points loaded */
static uint16_t num_points;

/** Parcours name */
static char parcours_name[PARCOURS_NAME_LEN];

/** Current state */
static parcours_state_t state = PARCOURS_STATE_IDLE;

/** Current navigation index */
static uint16_t current_idx;

/** Total distance */
static float total_distance;

/** Total elevation gain */
static float total_climb;

/** Distance completed */
static float dist_completed;

/** Distance to route */
static float dist_to_route;

/** Bearing to next point */
static float bearing_to_next;

/** Initialization flag */
static bool is_initialized;

/** Mutex for data protection */
static K_MUTEX_DEFINE(parcours_mutex);

/* ==========================================================================
 * Private Functions
 * ========================================================================== */

/**
 * @brief Calculate distance between two GPS points (Haversine)
 */
static float calc_distance(float lat1, float lon1, float lat2, float lon2)
{
    float dlat = DEG_TO_RAD(lat2 - lat1);
    float dlon = DEG_TO_RAD(lon2 - lon1);
    float a = sinf(dlat / 2.0f) * sinf(dlat / 2.0f) +
              cosf(DEG_TO_RAD(lat1)) * cosf(DEG_TO_RAD(lat2)) *
              sinf(dlon / 2.0f) * sinf(dlon / 2.0f);
    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    return EARTH_RADIUS_M * c;
}

/**
 * @brief Calculate bearing between two GPS points
 */
static float calc_bearing(float lat1, float lon1, float lat2, float lon2)
{
    float dlon = DEG_TO_RAD(lon2 - lon1);
    float y = sinf(dlon) * cosf(DEG_TO_RAD(lat2));
    float x = cosf(DEG_TO_RAD(lat1)) * sinf(DEG_TO_RAD(lat2)) -
              sinf(DEG_TO_RAD(lat1)) * cosf(DEG_TO_RAD(lat2)) * cosf(dlon);
    float bearing = RAD_TO_DEG(atan2f(y, x));

    /* Normalize to 0-360 */
    if (bearing < 0.0f) {
        bearing += 360.0f;
    }
    return bearing;
}

/**
 * @brief Calculate total route statistics
 */
static void calc_route_stats(void)
{
    total_distance = 0.0f;
    total_climb = 0.0f;

    for (uint16_t i = 1U; i < num_points; i++) {
        /* Distance */
        total_distance += calc_distance(
            points[i - 1U].lat, points[i - 1U].lon,
            points[i].lat, points[i].lon
        );

        /* Climb (only count ascending) */
        float elev_diff = points[i].alt - points[i - 1U].alt;
        if (elev_diff > 0.0f) {
            total_climb += elev_diff;
        }
    }

    LOG_INF("Route stats: %.1f km, %.0f m climb",
            (double)(total_distance / 1000.0f), (double)total_climb);
}

/**
 * @brief Find nearest point on route
 */
static uint16_t find_nearest_point(float lat, float lon)
{
    float min_dist = INFINITY;
    uint16_t nearest = 0U;

    /* Start search from current position */
    uint16_t start = (current_idx > 10U) ? (current_idx - 10U) : 0U;
    uint16_t end = (current_idx + 50U < num_points) ? (current_idx + 50U) : num_points;

    for (uint16_t i = start; i < end; i++) {
        float dist = calc_distance(lat, lon, points[i].lat, points[i].lon);
        if (dist < min_dist) {
            min_dist = dist;
            nearest = i;
        }
    }

    dist_to_route = min_dist;
    return nearest;
}

/**
 * @brief Parse a line from CRS file
 */
static bool parse_route_line(const char *line, float *lat, float *lon, float *alt)
{
    /*
     * The routes of the legacy are three numbers separated by spaces,
     * `lat lon alt`, and the altitude may be missing
     * (`legacy/source/parsers/file_parser.cpp:79-123`, `chargerPointPar()`,
     * with the real files of `tools/TDD/DB`). A line with `<` is metadata.
     */
    if (strchr(line, '<') != NULL) {
        return false;
    }

    char *endptr = NULL;

    *lat = strtof(line, &endptr);
    if ((endptr == NULL) || (endptr == line)) {
        return false;
    }

    const char *cursor = endptr;

    *lon = strtof(cursor, &endptr);
    if ((endptr == NULL) || (endptr == cursor)) {
        return false;
    }

    cursor = endptr;
    *alt = strtof(cursor, &endptr);
    if ((endptr == NULL) || (endptr == cursor)) {
        *alt = 0.0f; /* the legacy takes the point without the altitude */
    }

    return true;
}

/**
 * @brief Take one more point of the route, halving it when the array fills
 *
 * The routes of the legacy go past what fits here (`tools/TDD/DB` holds one
 * of 950 points, and a course exported from Strava has ten thousand),
 * and the legacy kept them on the heap. Instead of cutting the route, which
 * would leave the rider without its end, the resolution drops by half and
 * the loading goes on, as the segments do (`model/segment.c`).
 *
 * The step has to be applied to what comes **after** a halving too,
 * otherwise the end of the file arrives at full resolution while the
 * beginning has been halved over and over, and the route comes out with a
 * sparse start and a dense finish.
 */
static void add_point(float lat, float lon, float alt)
{
    bool keep = ((route_seen % route_stride) == 0U);

    route_seen++;
    if (!keep) {
        return;
    }

    if (num_points >= PARCOURS_MAX_POINTS) {
        uint16_t kept = 0U;

        for (uint16_t i = 0U; i < num_points; i += 2U) {
            points[kept] = points[i];
            kept++;
        }
        num_points = kept;
        route_stride *= 2U;

        /* with the step twice as long, this point may not belong any more */
        if (((route_seen - 1U) % route_stride) != 0U) {
            return;
        }
    }

    points[num_points].lat = lat;
    points[num_points].lon = lon;
    points[num_points].alt = alt;
    points[num_points].rtime = 0.0f;
    num_points++;
}

/** The end of the course is where the rider stops: it always stays */
static void keep_last_point(float lat, float lon, float alt)
{
    if (num_points == 0U) {
        return;
    }

    point_t *last = &points[num_points - 1U];

    if ((last->lat == lat) && (last->lon == lon)) {
        return;
    }

    if (num_points < PARCOURS_MAX_POINTS) {
        num_points++;
        last = &points[num_points - 1U];
    }
    last->lat = lat;
    last->lon = lon;
    last->alt = alt;
    last->rtime = 0.0f;
}

static app_err_t load_route_file(const char *filename)
{
    struct fs_file_t file;
    int ret;

    fs_file_t_init(&file);
    ret = fs_open(&file, filename, FS_O_READ);
    if (ret < 0) {
        LOG_ERR("Failed to open %s: %d", filename, ret);
        return APP_ERR_IO;
    }

    num_points = 0U;
    route_stride = 1U;
    route_seen = 0U;

    /*
     * Read in chunks and split into lines here: fs_read() knows nothing
     * about lines, and the files of the legacy end them with CRLF, which
     * leaves an empty line between points. Reading a byte at a time would
     * also cost one FatFs call per character.
     */
    char chunk[128];
    char line[PARCOURS_LINE_MAX];
    size_t line_len = 0U;
    ssize_t got;
    float last_lat = 0.0f;
    float last_lon = 0.0f;
    float last_alt = 0.0f;
    bool have_last = false;

    while ((got = fs_read(&file, chunk, sizeof(chunk))) > 0) {
        for (ssize_t i = 0; i < got; i++) {
            char c = chunk[i];

            if ((c != '\n') && (c != '\r')) {
                if (line_len < (PARCOURS_LINE_MAX - 1U)) {
                    line[line_len] = c;
                    line_len++;
                }
                continue;
            }

            line[line_len] = '\0';
            line_len = 0U;

            float lat;
            float lon;
            float alt;

            if (!parse_route_line(line, &lat, &lon, &alt)) {
                continue;
            }
            add_point(lat, lon, alt);
            last_lat = lat;
            last_lon = lon;
            last_alt = alt;
            have_last = true;
        }
    }

    /* a file whose last line has no end of line still gives its point */
    if (line_len > 0U) {
        float lat;
        float lon;
        float alt;

        line[line_len] = '\0';
        if (parse_route_line(line, &lat, &lon, &alt)) {
            add_point(lat, lon, alt);
            last_lat = lat;
            last_lon = lon;
            last_alt = alt;
            have_last = true;
        }
    }

    /* the end of the course always stays, whatever the step dropped */
    if (have_last) {
        keep_last_point(last_lat, last_lon, last_alt);
    }

    (void)fs_close(&file);

    if (num_points < 2U) {
        LOG_ERR("Not enough points in file");
        return APP_ERR_INVALID_PARAM;
    }

    LOG_INF("Loaded %u points from %s, one of every %u", num_points, filename,
            (unsigned int)route_stride);
    return APP_OK;
}

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

app_err_t parcours_init(void)
{
    if (is_initialized) {
        return APP_ERR_ALREADY_INIT;
    }

    /* Clear data */
    (void)memset(points, 0, sizeof(points));
    (void)memset(parcours_name, 0, sizeof(parcours_name));
    num_points = 0U;
    current_idx = 0U;
    total_distance = 0.0f;
    total_climb = 0.0f;
    state = PARCOURS_STATE_IDLE;

    is_initialized = true;
    LOG_INF("Parcours module initialized");

    return APP_OK;
}

app_err_t parcours_load(const char *filename)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (filename == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    k_mutex_lock(&parcours_mutex, K_FOREVER);

    /* Unload previous */
    parcours_unload();

    /* Extract name from filename */
    const char *name = strrchr(filename, '/');
    if (name != NULL) {
        name++;
    } else {
        name = filename;
    }
    strncpy(parcours_name, name, PARCOURS_NAME_LEN - 1U);
    parcours_name[PARCOURS_NAME_LEN - 1U] = '\0';

    /* The legacy writes `.PAR`; `.CRS` is the same text with another name */
    app_err_t err = load_route_file(filename);

    if (err == APP_OK) {
        calc_route_stats();
        state = PARCOURS_STATE_LOADED;
        LOG_INF("Parcours '%s' loaded", parcours_name);
    }

    k_mutex_unlock(&parcours_mutex);

    return err;
}

void parcours_unload(void)
{
    k_mutex_lock(&parcours_mutex, K_FOREVER);

    num_points = 0U;
    current_idx = 0U;
    total_distance = 0.0f;
    total_climb = 0.0f;
    dist_completed = 0.0f;
    (void)memset(parcours_name, 0, sizeof(parcours_name));
    state = PARCOURS_STATE_IDLE;

    k_mutex_unlock(&parcours_mutex);

    LOG_INF("Parcours unloaded");
}

app_err_t parcours_start(void)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    k_mutex_lock(&parcours_mutex, K_FOREVER);

    if (state != PARCOURS_STATE_LOADED) {
        k_mutex_unlock(&parcours_mutex);
        return APP_ERR_NOT_FOUND;
    }

    current_idx = 0U;
    dist_completed = 0.0f;
    state = PARCOURS_STATE_ACTIVE;

    k_mutex_unlock(&parcours_mutex);

    LOG_INF("Navigation started");

    return APP_OK;
}

void parcours_stop(void)
{
    k_mutex_lock(&parcours_mutex, K_FOREVER);

    if (state == PARCOURS_STATE_ACTIVE) {
        state = PARCOURS_STATE_LOADED;
    }

    k_mutex_unlock(&parcours_mutex);

    LOG_INF("Navigation stopped");
}

void parcours_update(float lat, float lon, float alt)
{
    (void)alt;

    /*
     * Off the route the rider is still riding it: the legacy calls
     * updatePosAuParcours() every epoch while a route is loaded
     * (`legacy/source/model/BoucleCRS.cpp:104-111`), and without this the
     * state below could never come back to ACTIVE.
     */
    if ((state != PARCOURS_STATE_ACTIVE) && (state != PARCOURS_STATE_OFF_ROUTE)) {
        return;
    }

    k_mutex_lock(&parcours_mutex, K_FOREVER);

    /* Find nearest point */
    uint16_t nearest = find_nearest_point(lat, lon);

    /* Check if we've progressed along the route */
    if (nearest > current_idx) {
        /* Calculate distance covered */
        for (uint16_t i = current_idx; i < nearest; i++) {
            dist_completed += calc_distance(
                points[i].lat, points[i].lon,
                points[i + 1U].lat, points[i + 1U].lon
            );
        }
        current_idx = nearest;
    }

    /* Check if off-route */
    if (dist_to_route > PARCOURS_OFF_ROUTE_M) {
        state = PARCOURS_STATE_OFF_ROUTE;
    } else if (state == PARCOURS_STATE_OFF_ROUTE) {
        state = PARCOURS_STATE_ACTIVE;
    }

    /* Check if finished */
    if (current_idx >= (num_points - 1U)) {
        if (dist_to_route < PARCOURS_NEAR_POINT_M) {
            state = PARCOURS_STATE_FINISHED;
            LOG_INF("Route finished!");
        }
    }

    /* Calculate bearing to next point */
    if ((current_idx + 1U) < num_points) {
        bearing_to_next = calc_bearing(lat, lon,
                                       points[current_idx + 1U].lat,
                                       points[current_idx + 1U].lon);
    }

    k_mutex_unlock(&parcours_mutex);
}

parcours_state_t parcours_get_state(void)
{
    return state;
}

app_err_t parcours_get_info(parcours_info_t *info)
{
    if (info == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    k_mutex_lock(&parcours_mutex, K_FOREVER);

    strncpy(info->name, parcours_name, PARCOURS_NAME_LEN);
    info->num_points = num_points;
    info->total_distance = total_distance;
    info->total_climb = total_climb;
    info->valid = (state != PARCOURS_STATE_IDLE);

    k_mutex_unlock(&parcours_mutex);

    return APP_OK;
}

app_err_t parcours_get_nav_info(nav_info_t *nav)
{
    if (nav == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    k_mutex_lock(&parcours_mutex, K_FOREVER);

    nav->dist_to_route = dist_to_route;
    nav->dist_completed = dist_completed;
    nav->dist_remaining = total_distance - dist_completed;
    nav->pct_complete = (total_distance > 0.0f) ?
                        (dist_completed / total_distance * 100.0f) : 0.0f;
    nav->bearing = bearing_to_next;
    nav->current_idx = current_idx;
    nav->on_route = (dist_to_route <= PARCOURS_OFF_ROUTE_M);

    if ((current_idx + 1U) < num_points) {
        nav->altitude_next = points[current_idx + 1U].alt;
    } else {
        nav->altitude_next = 0.0f;
    }

    k_mutex_unlock(&parcours_mutex);

    return APP_OK;
}

bool parcours_is_loaded(void)
{
    return (state != PARCOURS_STATE_IDLE);
}

bool parcours_is_active(void)
{
    /* being away from the line does not end the navigation */
    return (state == PARCOURS_STATE_ACTIVE) || (state == PARCOURS_STATE_OFF_ROUTE);
}

const point_t *parcours_get_point(uint16_t index)
{
    if (index >= num_points) {
        return NULL;
    }
    return &points[index];
}

uint16_t parcours_get_num_points(void)
{
    return num_points;
}

uint16_t parcours_get_current_index(void)
{
    return current_idx;
}

float parcours_get_dist_to_next(void)
{
    return dist_to_route;
}

float parcours_get_bearing_to_next(void)
{
    return bearing_to_next;
}
