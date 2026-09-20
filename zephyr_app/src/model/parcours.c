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
#include "model/gpx_scan.h"
#include "model/route_file.h"
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

/** Turns of the route, when the file carried a cue sheet */
static parcours_cue_t cues[PARCOURS_MAX_CUES];
static uint16_t num_cues;

/** Called while a long file is read, so the watchdog keeps quiet */
static parcours_progress_fn progress_fn;

/** Name the file gives the route, when it has one */
static char route_name[PARCOURS_NAME_LEN];

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

/**
 * @brief Read a route file of this project (`.RTE`)
 *
 * The header says how much is there and carries the CRC of the body, so a
 * transfer cut in half over the radio is caught before the rider follows a
 * route that ends in the middle of nowhere. The points come in with the
 * same halving as the text of the legacy, and the cue sheet, when the file
 * has one, gives the turns of the navigation.
 */
static app_err_t load_rte_file(struct fs_file_t *file, const uint8_t *head, size_t file_size)
{
    struct route_header h;

    if (!route_file_header(&h, head, ROUTE_FILE_HEADER_SIZE, file_size)) {
        LOG_ERR("route file refused: header");
        return APP_ERR_CHECKSUM;
    }

    /* the CRC of the body, read in chunks so nothing big sits on the stack */
    uint8_t chunk[128];
    uint32_t crc = 0U;
    size_t body = (size_t)(h.points * ROUTE_FILE_POINT_SIZE) +
                  (size_t)(h.cues * ROUTE_FILE_CUE_SIZE);
    size_t left = body;

    if (fs_seek(file, (off_t)ROUTE_FILE_HEADER_SIZE, FS_SEEK_SET) < 0) {
        return APP_ERR_IO;
    }
    while (left > 0U) {
        size_t want = (left < sizeof(chunk)) ? left : sizeof(chunk);
        ssize_t got = fs_read(file, chunk, want);

        if (got <= 0) {
            LOG_ERR("route file refused: short");
            return APP_ERR_CHECKSUM;
        }
        crc = route_file_crc32(crc, chunk, (size_t)got);
        left -= (size_t)got;
    }

    if (crc != h.crc32) {
        LOG_ERR("route file refused: crc %08x against %08x", (unsigned int)crc,
                (unsigned int)h.crc32);
        return APP_ERR_CHECKSUM;
    }

    /* the points, halved as they come in if there are more than fit */
    if (fs_seek(file, (off_t)ROUTE_FILE_HEADER_SIZE, FS_SEEK_SET) < 0) {
        return APP_ERR_IO;
    }

    uint32_t taken = 0U;

    while (taken < h.points) {
        uint32_t want = (h.points - taken);

        if (want > (sizeof(chunk) / ROUTE_FILE_POINT_SIZE)) {
            want = sizeof(chunk) / ROUTE_FILE_POINT_SIZE;
        }

        ssize_t got = fs_read(file, chunk, (size_t)want * ROUTE_FILE_POINT_SIZE);

        if (got < (ssize_t)((size_t)want * ROUTE_FILE_POINT_SIZE)) {
            break;
        }
        for (uint32_t i = 0U; i < want; i++) {
            struct route_point pt;

            if (route_file_point(&pt, &chunk[i * ROUTE_FILE_POINT_SIZE],
                                 ROUTE_FILE_POINT_SIZE)) {
                add_point((float)((double)pt.lat_e7 * 1e-7), (float)((double)pt.lon_e7 * 1e-7),
                          (float)pt.alt_m);
            }
        }
        taken += want;
    }

    if (num_points < 2U) {
        return APP_ERR_INVALID_PARAM;
    }

    /* the cue sheet, with the indexes moved to what is in memory */
    num_cues = 0U;
    for (uint32_t i = 0U; (i < h.cues) && (num_cues < PARCOURS_MAX_CUES); i++) {
        struct route_cue c;
        ssize_t got = fs_read(file, chunk, ROUTE_FILE_CUE_SIZE);

        if (got < (ssize_t)ROUTE_FILE_CUE_SIZE) {
            break;
        }
        if (!route_file_cue(&c, chunk, ROUTE_FILE_CUE_SIZE)) {
            continue;
        }

        uint32_t at = c.point / route_stride;

        cues[num_cues].point = (uint16_t)((at < num_points) ? at : (num_points - 1U));
        cues[num_cues].turn = c.turn;
        (void)strncpy(cues[num_cues].street, c.street, PARCOURS_STREET_LEN);
        cues[num_cues].street[PARCOURS_STREET_LEN] = '\0';
        num_cues++;
    }

    if (h.name[0] != '\0') {
        (void)strncpy(route_name, h.name, sizeof(route_name) - 1U);
        route_name[sizeof(route_name) - 1U] = '\0';
    }

    LOG_INF("route %s: %u of %u points, %u turns, %u m, %u m of climb", route_name,
            (unsigned int)num_points, (unsigned int)h.points, (unsigned int)num_cues,
            (unsigned int)h.distance_m, (unsigned int)h.climb_m);

    return APP_OK;
}

/** One point out of the GPX reader goes straight into the route */
static void gpx_point(float lat, float lon, float alt, void *user)
{
    (void)user;
    add_point(lat, lon, alt);
}

/**
 * @brief Read a course as the services export it, in GPX
 *
 * Megabytes of XML, read in chunks and scanned as they come
 * (`model/gpx_scan.c`), so nothing but the chunk and the element being
 * read sits in memory. The watchdog is fed along the way, because this
 * takes longer than its four seconds on a big file.
 */
static app_err_t load_gpx_file(struct fs_file_t *file, const char *head, size_t head_len)
{
    struct gpx_scan scan;
    char chunk[128];
    ssize_t got;
    uint32_t reads = 0U;

    gpx_scan_init(&scan);
    gpx_scan_feed(&scan, head, head_len, gpx_point, NULL);

    while ((got = fs_read(file, chunk, sizeof(chunk))) > 0) {
        gpx_scan_feed(&scan, chunk, (size_t)got, gpx_point, NULL);

        /* every few kilobytes, say the thread is alive */
        reads++;
        if (((reads % 32U) == 0U) && (progress_fn != NULL)) {
            progress_fn();
        }
    }
    gpx_scan_end(&scan, gpx_point, NULL);

    if (num_points < 2U) {
        LOG_ERR("no course in the GPX");
        return APP_ERR_INVALID_PARAM;
    }

    LOG_INF("GPX: %u of %u points, one of every %u", (unsigned int)num_points,
            (unsigned int)scan.points, (unsigned int)route_stride);

    return APP_OK;
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
    num_cues = 0U;
    route_stride = 1U;
    route_seen = 0U;
    route_name[0] = '\0';

    /*
     * The file says which format it is: a route of this project begins with
     * "RTE1" (`model/route_file.h`), anything else is read as the text of
     * the legacy.
     */
    uint8_t head[ROUTE_FILE_HEADER_SIZE];
    ssize_t head_len = fs_read(&file, head, sizeof(head));

    if ((head_len >= (ssize_t)ROUTE_FILE_HEADER_SIZE) && route_file_is_rte(head, (size_t)head_len)) {
        struct fs_dirent entry;
        size_t size = 0U;

        if (fs_stat(filename, &entry) == 0) {
            size = (size_t)entry.size;
        }

        app_err_t err = load_rte_file(&file, head, size);

        (void)fs_close(&file);

        return err;
    }

    /* a course as a service exported it, in GPX */
    if ((head_len > 0) && gpx_scan_looks_like_gpx((const char *)head, (size_t)head_len)) {
        app_err_t err = load_gpx_file(&file, (const char *)head, (size_t)head_len);

        (void)fs_close(&file);

        return err;
    }

    /* the text of the legacy: back to the start of the file */
    if (fs_seek(&file, 0, FS_SEEK_SET) < 0) {
        (void)fs_close(&file);
        return APP_ERR_IO;
    }

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

bool parcours_get_next_cue(parcours_cue_t *out, float *dist_m)
{
    if ((out == NULL) || (num_cues == 0U) || (num_points == 0U)) {
        return false;
    }

    for (uint16_t i = 0U; i < num_cues; i++) {
        if (cues[i].point < current_idx) {
            continue;
        }

        *out = cues[i];
        if (dist_m != NULL) {
            float d = 0.0f;

            for (uint16_t k = current_idx; (k + 1U) <= cues[i].point; k++) {
                d += calc_distance(points[k].lat, points[k].lon, points[k + 1U].lat,
                                   points[k + 1U].lon);
            }
            *dist_m = d;
        }

        return true;
    }

    return false;
}

void parcours_set_progress(parcours_progress_fn fn)
{
    progress_fn = fn;
}

const char *parcours_get_name(void)
{
    return (route_name[0] != '\0') ? route_name : parcours_name;
}

void parcours_unload(void)
{
    k_mutex_lock(&parcours_mutex, K_FOREVER);

    num_points = 0U;
    num_cues = 0U;
    current_idx = 0U;
    total_distance = 0.0f;
    total_climb = 0.0f;
    dist_completed = 0.0f;
    (void)memset(parcours_name, 0, sizeof(parcours_name));
    (void)memset(route_name, 0, sizeof(route_name));
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
