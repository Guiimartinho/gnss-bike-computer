/**
 * @file segment.c
 * @brief Strava Segment management implementation
 *
 * Based on original Segment.cpp with proper activation/deactivation
 * using vector math and Pythagorean geometry.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <string.h>
#include <stdio.h>

#include "model/segment.h"
#include "model/segment_file.h"

#include "model/vecteur.h"
#include "model/liste_points.h"
#include <math.h>

LOG_MODULE_REGISTER(segment, CONFIG_LOG_DEFAULT_LEVEL);

/* ==========================================================================
 * Private Definitions
 * ========================================================================== */

/** The segments of the legacy live in the root of the card */
#define SEG_DIR             "/SD:"

/** Longest line of a segment file: `lat ; lon ; rtime ; alt` */
#define SEG_LINE_MAX        96U

/** Maximum filename length */
#define MAX_FILENAME_LEN    32U

/** Segment countdown after finish */
#define SEG_FINISH_COUNTDOWN    (-5)

/** Scalar product limit for activation (from original Segment.h line 25) */
#define PSCAL_LIM           0.0f

/** Margin factor for deactivation (from original Segment.h line 22) */
#define MARGE_ACT           1.5f

/**
 * Segments near the rider hold their points in a pool of slots. The legacy
 * puts them on the heap and frees them when the rider goes away
 * (`legacy/source/sd/sd_functions.cpp:518-596`, `Segment::init()`); the
 * port allocates nothing after boot, so the number of segments that can be
 * loaded at the same time and the points each one holds are fixed here.
 *
 * The files of the legacy go up to about 1300 points (`tools/TDD/DB`); a
 * longer one is halved while it loads (`liste_decimate()`), which keeps the
 * start, the end and the shape of the segment, and only loses resolution.
 */
#define SEG_SLOTS           3U

#if defined(CONFIG_GNSS_SEGMENT_POINTS)
#define SEG_SLOT_POINTS     ((uint16_t)CONFIG_GNSS_SEGMENT_POINTS)
#else
#define SEG_SLOT_POINTS     256U
#endif

/** No segment holds the slot */
#define SEG_NO_OWNER        UINT16_MAX

/* ==========================================================================
 * Private Types
 * ========================================================================== */

/**
 * @brief Points and running state of a loaded segment
 */
typedef struct {
    uint16_t owner;         /**< Segment holding the slot, or SEG_NO_OWNER */
    uint16_t stride;        /**< One point of the file kept out of this many */
    float start_time;       /**< Time when segment was activated */
    float cur_time;         /**< Current time on segment */
    float advance;          /**< Time advance/behind reference */
    float elev_start;       /**< Elevation at start */
    float elev_total;       /**< Total elevation of segment */
    float pct_dist;         /**< Progress percentage by distance */
    float pct_elev;         /**< Progress percentage by elevation */
    liste_points_t pts;     /**< Segment points list, over `storage` */
    point_t storage[SEG_SLOT_POINTS]; /**< Points of the segment */
} seg_runtime_t;

/* ==========================================================================
 * Private Variables
 * ========================================================================== */

/** Loaded segments */
static segment_t segments[MAX_SEGMENTS];
static uint16_t segment_count;

/** Segment headers (metadata) */
static seg_header_t seg_headers[MAX_SEGMENTS];

/**
 * Start of each segment, taken from its name (`segment_file_position()`):
 * this is what the allocator uses to decide whether to open the file, as
 * the legacy does (`legacy/source/sd/sd_functions.cpp:547-556`).
 */
static struct {
    float lat;
    float lon;
} seg_start_pos[MAX_SEGMENTS];

/** Points and running state of the segments that are loaded */
static seg_runtime_t seg_slots[SEG_SLOTS];

/** Distance to the nearest segment, as the last allocator run measured it */
static float nearest_dist = -1.0f;

/** User's GPS position history */
static liste_points_t user_history;

/** Status change callback */
static seg_status_callback_t status_callback;

/** Initialization flag */
static bool is_initialized;

/* ==========================================================================
 * Private Function Forward Declarations
 * ========================================================================== */

static int load_segment_points(uint16_t seg_idx);
static void unload_segment_points(uint16_t seg_idx);
static float dist_to_seg_header(uint16_t seg_idx, float lat, float lon);

/* ==========================================================================
 * Private Functions
 * ========================================================================== */

/**
 * @brief Slot holding the points of a segment, or NULL when it has none
 */
static seg_runtime_t *slot_of(uint16_t seg_idx)
{
    for (uint8_t i = 0U; i < SEG_SLOTS; i++) {
        if (seg_slots[i].owner == seg_idx) {
            return &seg_slots[i];
        }
    }

    return NULL;
}

/**
 * @brief Give a free slot to a segment, empty and ready to be filled
 * @return The slot, or NULL when every slot is taken
 */
static seg_runtime_t *slot_take(uint16_t seg_idx)
{
    for (uint8_t i = 0U; i < SEG_SLOTS; i++) {
        if (seg_slots[i].owner != SEG_NO_OWNER) {
            continue;
        }

        seg_runtime_t *rt = &seg_slots[i];

        rt->owner = seg_idx;
        rt->stride = 1U;
        rt->start_time = 0.0f;
        rt->cur_time = 0.0f;
        rt->advance = 0.0f;
        rt->elev_start = 0.0f;
        rt->elev_total = 0.0f;
        rt->pct_dist = 0.0f;
        rt->pct_elev = 0.0f;
        liste_init_static(&rt->pts, rt->storage, SEG_SLOT_POINTS);

        return rt;
    }

    return NULL;
}

/**
 * @brief Take the points of a segment back into the pool
 */
static void slot_give(uint16_t seg_idx)
{
    seg_runtime_t *rt = slot_of(seg_idx);

    if (rt == NULL) {
        return;
    }

    liste_clear(&rt->pts);
    rt->owner = SEG_NO_OWNER;
}

/**
 * @brief Test segment activation using vector math
 *
 * Uses scalar product of movement vector and segment direction,
 * plus Pythagorean geometry to determine if segment should activate.
 *
 * @param seg_idx Segment index
 * @return true if segment should be activated
 */
static bool test_activation(uint16_t seg_idx)
{
    const seg_runtime_t *rt = slot_of(seg_idx);

    /* Need at least 2 points in both lists */
    if ((rt == NULL) || (rt->pts.count < 2U) || (user_history.count < 2U)) {
        return false;
    }

    /* Get segment first two points */
    const point_t *seg_p1 = liste_get_at(&rt->pts, 0);
    const point_t *seg_p2 = liste_get_at(&rt->pts, 1);
    if ((seg_p1 == NULL) || (seg_p2 == NULL)) {
        return false;
    }

    /* Get user's current and previous positions */
    const point_t *cur_pos = liste_get_at(&user_history, 0);
    const point_t *prev_pos = liste_get_at(&user_history, 1);
    if ((cur_pos == NULL) || (prev_pos == NULL)) {
        return false;
    }

    /* Test activation using vector math (from vecteur module) */
    return test_segment_activation(cur_pos, prev_pos,
                                   seg_p1, seg_p2,
                                   SEG_ACTIVATE_DIST, PSCAL_LIM);
}

/**
 * @brief Test segment deactivation using Pythagorean geometry
 *
 * Tests if user has passed the end of the segment.
 *
 * @param seg_idx Segment index
 * @return true if segment should be deactivated (finished)
 */
static bool test_deactivation(uint16_t seg_idx)
{
    const seg_runtime_t *rt = slot_of(seg_idx);

    /* Need points in segment */
    if ((rt == NULL) || (rt->pts.count < 3U)) {
        return false;
    }

    /* Get user's current position */
    const point_t *cur_pos = liste_get_at(&user_history, 0);
    if (cur_pos == NULL) {
        return false;
    }

    /* Get segment last two points */
    const point_t *seg_last = liste_get_at(&rt->pts, (int16_t)(rt->pts.count - 1U));
    const point_t *seg_prev = liste_get_at(&rt->pts, (int16_t)(rt->pts.count - 2U));
    if ((seg_last == NULL) || (seg_prev == NULL)) {
        return false;
    }

    /* Test deactivation using vector math */
    return test_segment_deactivation(cur_pos, seg_prev,
                                     seg_last, SEG_ACTIVATE_DIST);
}

/**
 * @brief Calculate distance from point to segment start
 */
static float dist_to_start(uint16_t seg_idx, const loc_data_t *loc)
{
    const seg_runtime_t *rt = slot_of(seg_idx);

    if ((rt == NULL) || (rt->pts.count == 0U) || (loc == NULL)) {
        return 9999.0f;
    }

    const point_t *seg_start = liste_get_at(&rt->pts, 0);
    if (seg_start == NULL) {
        return 9999.0f;
    }

    point_t cur_pos = {
        .lat = loc->lat,
        .lon = loc->lon,
        .alt = loc->alt,
        .rtime = 0.0f
    };

    return point_distance(&cur_pos, seg_start);
}

/**
 * @brief Update segment progress and advance
 * @param seg_idx Segment index
 * @param loc Current location
 * @param current_time Current ride time
 */
static void update_progress(uint16_t seg_idx, const loc_data_t *loc,
                           float current_time)
{
    seg_runtime_t *rt = slot_of(seg_idx);
    segment_t *seg = &segments[seg_idx];

    if ((rt == NULL) || (rt->pts.count == 0U)) {
        return;
    }

    /* Convert location to point */
    point_t cur_pos = {
        .lat = loc->lat,
        .lon = loc->lon,
        .alt = loc->alt,
        .rtime = current_time
    };

    /* Update relative position on segment */
    liste_update_relative_position(&rt->pts, &cur_pos);

    /* Get interpolated position */
    float rel_time = rt->pts.pos_rel.t;
    float rel_dist = rt->pts.pos_rel.y; /* Perpendicular distance */

    /* Check if too far from segment line */
    if (fabsf(rel_dist) > MARGE_ACT * SEG_ACTIVATE_DIST) {
        seg->status = SEG_OFF;
        LOG_INF("Segment %s deactivated (too far from line)", seg->name);
        return;
    }

    /* Calculate current time on segment */
    rt->cur_time = current_time - rt->start_time;

    /* Get reference time from segment first point */
    const point_t *seg_start = liste_get_at(&rt->pts, 0);
    if (seg_start != NULL) {
        rt->advance = (rel_time - seg_start->rtime) - rt->cur_time;
    }

    /* Update progress percentage */
    rt->pct_dist = (float)rt->pts.idx_p1 / (float)rt->pts.count;

    /* Update elevation progress */
    if (rt->elev_total > 5.0f) {
        float elev_at_pos = rt->pts.pos_rel.z;
        rt->pct_elev = (elev_at_pos - rt->elev_start) / rt->elev_total;
    }

    /* Copy to segment structure */
    seg->cur_time = rt->cur_time;
    seg->advance = rt->advance;
    seg->pct_dist = rt->pct_dist;
    seg->pct_elev = rt->pct_elev;
}

/**
 * @brief Add user position to history
 */
static void add_user_position(const loc_data_t *loc, float current_time)
{
    liste_add_iso(&user_history, loc->lat, loc->lon, loc->alt, current_time, HISTO_POINT_SIZE);
}

/**
 * @brief Update single segment state
 *
 * Implements the state machine from original Segment::majPerformance()
 */
static void update_segment(uint16_t idx, const loc_data_t *loc, float current_time)
{
    segment_t *seg = &segments[idx];
    seg_runtime_t *rt = slot_of(idx);

    /* Skip if no points loaded */
    if ((rt == NULL) || (rt->pts.count < 3U)) {
        return;
    }

    switch (seg->status) {
    case SEG_OFF:
        /* Check if close enough to start to test activation */
        {
            float dist = dist_to_start(idx, loc);
            if (dist > SEG_ACTIVATE_DIST) {
                break; /* Too far, skip */
            }

            /* Test activation with vector math */
            if (test_activation(idx)) {
                /* Get interpolated start time from user history */
                const point_t *seg_first = liste_get_at(&rt->pts, 0);
                if (seg_first != NULL) {
                    liste_update_relative_position(&user_history, seg_first);
                    rt->start_time = user_history.pos_rel.t;
                } else {
                    rt->start_time = current_time;
                }

                rt->cur_time = 0.0f;
                rt->advance = 0.0f;
                rt->elev_start = loc->alt;
                rt->pct_dist = 0.0f;
                rt->pct_elev = 0.0f;

                seg->status = SEG_START;
                seg->cur_time = 0.0f;
                seg->advance = 0.0f;
                seg->pct_dist = 0.0f;
                seg->pct_elev = 0.0f;
                seg->score = 2; /* Starting priority */

                LOG_INF("Segment %s activated", seg->name);

                if (status_callback != NULL) {
                    status_callback(seg);
                }
            }
        }
        break;

    case SEG_START:
        /* Transition to active */
        seg->status = SEG_ON;
        /* Fall through */
        __attribute__((fallthrough));

    case SEG_ON:
        /* Test for segment finish (deactivation) */
        if (test_deactivation(idx)) {
            /* Segment finished - calculate final advance */
            const point_t *seg_last = liste_get_at(&rt->pts, (int16_t)(rt->pts.count - 1U));
            if (seg_last != NULL) {
                liste_update_relative_position(&user_history, seg_last);
                rt->cur_time = user_history.pos_rel.t - rt->start_time;
                rt->advance = seg->total_time - rt->cur_time;
            }

            rt->pct_dist = 1.0f;
            seg->cur_time = rt->cur_time;
            seg->advance = rt->advance;
            seg->pct_dist = 1.0f;
            seg->status = SEG_FIN;
            seg->score = 10; /* Highest priority for finished segment */

            LOG_INF("Segment %s finished! Time: %.1f s, Advance: %.1f s",
                    seg->name, (double)seg->cur_time, (double)seg->advance);

            if (status_callback != NULL) {
                status_callback(seg);
            }
        } else {
            /* Update progress */
            update_progress(idx, loc, current_time);

            /* Update score based on progress (2 to 9) */
            seg->score = 2 + (int8_t)(rt->pct_dist * 7.0f);
        }
        break;

    case SEG_FIN:
        /* Countdown to removal from display */
        seg->score--;
        if (seg->score < SEG_FINISH_COUNTDOWN) {
            seg->status = SEG_OFF;
            seg->score = 0;
        }
        break;

    default:
        seg->status = SEG_OFF;
        break;
    }
}

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

app_err_t segment_init(void)
{
    if (is_initialized) {
        return APP_ERR_ALREADY_INIT;
    }

    /* Clear segments */
    (void)memset(segments, 0, sizeof(segments));
    (void)memset(seg_headers, 0, sizeof(seg_headers));
    (void)memset(seg_start_pos, 0, sizeof(seg_start_pos));
    segment_count = 0U;
    nearest_dist = -1.0f;

    /* Every slot of the pool is free and points at its own storage */
    for (uint8_t i = 0U; i < SEG_SLOTS; i++) {
        seg_slots[i].owner = SEG_NO_OWNER;
        liste_init_static(&seg_slots[i].pts, seg_slots[i].storage, SEG_SLOT_POINTS);
    }

    /* Initialize user position history */
    liste_init(&user_history, LISTE_MAX_HISTORY);

    is_initialized = true;
    LOG_INF("Segment manager initialized");

    return APP_OK;
}

int segment_load_all(void)
{
    if (!is_initialized) {
        return (int)APP_ERR_NOT_INIT;
    }

    struct fs_dir_t dir;
    struct fs_dirent entry;
    int count = 0;

    fs_dir_t_init(&dir);

    int err = fs_opendir(&dir, SEG_DIR);
    if (err < 0) {
        LOG_WRN("Cannot open segments directory: %d", err);
        return 0;
    }

    while ((fs_readdir(&dir, &entry) == 0) && (entry.name[0] != '\0')) {
        if (entry.type != FS_DIR_ENTRY_FILE) {
            continue;
        }

        /*
         * The name says what the file is and where the segment starts
         * (`sd_functions.cpp:279-333`): nothing is opened here, as in the
         * legacy, so a card full of segments costs one directory pass.
         */
        if (!segment_file_name_is_valid(entry.name)) {
            continue;
        }

        if (segment_count >= MAX_SEGMENTS) {
            LOG_WRN("Maximum segments reached");
            break;
        }

        float start_lat = 0.0f;
        float start_lon = 0.0f;

        if (segment_file_position(entry.name, &start_lat, &start_lon)) {
            segment_t *seg = &segments[segment_count];

            (void)strncpy(seg->name, entry.name, sizeof(seg->name) - 1U);
            seg->name[sizeof(seg->name) - 1U] = '\0';
            seg->num_points = 0U;   /* the points come with the allocator */
            seg->total_time = 0.0f;
            seg->total_elev = 0.0f;
            seg->status = SEG_OFF;
            seg->score = 0;

            (void)memset(&seg_headers[segment_count], 0, sizeof(seg_header_t));
            (void)strncpy(seg_headers[segment_count].name, entry.name,
                          sizeof(seg_headers[segment_count].name) - 1U);
            seg_start_pos[segment_count].lat = start_lat;
            seg_start_pos[segment_count].lon = start_lon;

            segment_count++;
            count++;

            LOG_DBG("Segment %s at %.5f %.5f", entry.name, (double)start_lat,
                    (double)start_lon);
        }
    }

    (void)fs_closedir(&dir);

    LOG_INF("Loaded %d segments", count);
    return count;
}

app_err_t segment_update(const loc_data_t *loc)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (loc == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    /* Get current time from location timestamp */
    float current_time = (float)loc->timestamp / 1000.0f;

    /* Add position to user history */
    add_user_position(loc, current_time);

    /* Update all segments */
    for (uint16_t i = 0U; i < segment_count; i++) {
        update_segment(i, loc, current_time);
    }

    return APP_OK;
}

uint8_t segment_get_active_count(void)
{
    uint8_t count = 0U;

    for (uint16_t i = 0U; i < segment_count; i++) {
        if ((segments[i].status == SEG_START) ||
            (segments[i].status == SEG_ON) ||
            (segments[i].status == SEG_FIN)) {
            count++;
        }
    }

    return count;
}

app_err_t segment_get(uint8_t index, segment_t *seg)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if ((index >= segment_count) || (seg == NULL)) {
        return APP_ERR_INVALID_PARAM;
    }

    *seg = segments[index];
    return APP_OK;
}

app_err_t segment_get_best(segment_t *seg)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (seg == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    /* Find active segment with highest score */
    const segment_t *best = NULL;
    int8_t best_score = INT8_MIN;

    for (uint16_t i = 0U; i < segment_count; i++) {
        if ((segments[i].status == SEG_ON) ||
            (segments[i].status == SEG_START)) {
            if (segments[i].score > best_score) {
                best = &segments[i];
                best_score = segments[i].score;
            }
        }
    }

    if (best == NULL) {
        return APP_ERR_NOT_FOUND;
    }

    *seg = *best;
    return APP_OK;
}

uint8_t segment_get_active(segment_t *segs, uint8_t max_count)
{
    if (!is_initialized || (segs == NULL) || (max_count == 0U)) {
        return 0U;
    }

    uint8_t count = 0U;

    /* Collect active segments */
    for (uint16_t i = 0U; (i < segment_count) && (count < max_count); i++) {
        if ((segments[i].status == SEG_START) ||
            (segments[i].status == SEG_ON) ||
            (segments[i].status == SEG_FIN)) {
            segs[count] = segments[i];
            count++;
        }
    }

    /* Sort by score (descending) using insertion sort - efficient for small arrays */
    for (uint8_t i = 1U; i < count; i++) {
        segment_t temp = segs[i];
        int8_t j = (int8_t)i - 1;

        while ((j >= 0) && (segs[j].score < temp.score)) {
            segs[j + 1] = segs[j];
            j--;
        }
        segs[j + 1] = temp;
    }

    return count;
}

uint8_t segment_get_nearby(segment_t *segs, uint8_t max_count, float lat, float lon)
{
    if (!is_initialized || (segs == NULL) || (max_count == 0U)) {
        return 0U;
    }

    /* Temporary array to store segment indices and distances */
    typedef struct {
        uint16_t idx;
        float dist;
    } seg_dist_t;

    seg_dist_t seg_dists[MAX_SEGMENTS] = {0};
    uint8_t count = 0U;

    /*
     * Every segment of the card counts, loaded or not: the start of each
     * one comes from its name, which is what lets the list be shown before
     * anything is read from the card.
     */
    for (uint16_t i = 0U; i < segment_count; i++) {
        float dist = dist_to_seg_header(i, lat, lon);

        seg_dists[count].idx = i;
        seg_dists[count].dist = dist;
        count++;

        if (count >= MAX_SEGMENTS) {
            break;
        }
    }

    /* Sort by distance (ascending) using insertion sort */
    for (uint8_t i = 1U; i < count; i++) {
        seg_dist_t temp = seg_dists[i];
        int8_t j = (int8_t)i - 1;

        while ((j >= 0) && (seg_dists[j].dist > temp.dist)) {
            seg_dists[j + 1] = seg_dists[j];
            j--;
        }
        seg_dists[j + 1] = temp;
    }

    /* Copy sorted segments to output array */
    uint8_t result_count = (count < max_count) ? count : max_count;
    for (uint8_t i = 0U; i < result_count; i++) {
        segs[i] = segments[seg_dists[i].idx];
    }

    return result_count;
}

uint8_t segment_get_screen_list(uint8_t *index, uint8_t max, float lat, float lon)
{
    if (!is_initialized || (index == NULL) || (max == 0U)) {
        return 0U;
    }

    uint8_t count = 0U;

    /* the ones running, best score first (legacy getScore) */
    while (count < max) {
        int16_t best = -1;
        int8_t best_score = INT8_MIN;

        for (uint16_t i = 0U; i < segment_count; i++) {
            bool taken = false;

            if (segments[i].status == SEG_OFF) {
                continue;
            }
            for (uint8_t k = 0U; k < count; k++) {
                if (index[k] == (uint8_t)i) {
                    taken = true;
                    break;
                }
            }
            if (taken) {
                continue;
            }
            if (segments[i].score > best_score) {
                best_score = segments[i].score;
                best = (int16_t)i;
            }
        }

        if (best < 0) {
            break;
        }
        index[count] = (uint8_t)best;
        count++;
    }

    /* then the nearest of the loaded ones, which is what the rider comes to */
    while (count < max) {
        int16_t best = -1;
        float best_dist = 9999.0f;

        for (uint16_t i = 0U; i < segment_count; i++) {
            bool taken = false;

            if (slot_of(i) == NULL) {
                continue;
            }
            for (uint8_t k = 0U; k < count; k++) {
                if (index[k] == (uint8_t)i) {
                    taken = true;
                    break;
                }
            }
            if (taken) {
                continue;
            }

            float dist = dist_to_seg_header(i, lat, lon);

            if (dist < best_dist) {
                best_dist = dist;
                best = (int16_t)i;
            }
        }

        if (best < 0) {
            break;
        }
        index[count] = (uint8_t)best;
        count++;
    }

    return count;
}

uint16_t segment_point_count(uint8_t index)
{
    const seg_runtime_t *rt = slot_of(index);

    return (rt != NULL) ? rt->pts.count : 0U;
}

bool segment_point_at(uint8_t index, uint16_t i, point_t *out)
{
    const seg_runtime_t *rt = slot_of(index);

    if ((rt == NULL) || (out == NULL) || (i >= rt->pts.count)) {
        return false;
    }

    const point_t *p = liste_get_at(&rt->pts, (int16_t)i);

    if (p == NULL) {
        return false;
    }
    *out = *p;

    return true;
}

app_err_t segment_register_callback(seg_status_callback_t callback)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    status_callback = callback;
    return APP_OK;
}

float segment_get_nearest_distance(void)
{
    if (!is_initialized || (segment_count == 0U)) {
        return -1.0f;
    }

    /*
     * What the last allocator run measured. The allocator sees every
     * segment of the card, so this answer does not depend on which ones
     * have their points in the pool.
     */
    if (nearest_dist >= 0.0f) {
        return nearest_dist;
    }

    /* No allocator run yet: measure from the position of the rider */
    const point_t *cur_pos = liste_get_at(&user_history, 0);

    if (cur_pos == NULL) {
        return -1.0f;
    }

    float nearest = 9999.0f;

    for (uint16_t i = 0U; i < segment_count; i++) {
        float dist = dist_to_seg_header(i, cur_pos->lat, cur_pos->lon);

        if (dist < nearest) {
            nearest = dist;
        }
    }

    return (nearest < 9000.0f) ? nearest : -1.0f;
}

uint16_t segment_get_total_count(void)
{
    return segment_count;
}

bool segment_is_any_active(void)
{
    return (segment_get_active_count() > 0U);
}

void segment_reset_all(void)
{
    for (uint16_t i = 0U; i < segment_count; i++) {
        segments[i].status = SEG_OFF;
        segments[i].cur_time = 0.0f;
        segments[i].advance = 0.0f;
        segments[i].pct_dist = 0.0f;
        segments[i].pct_elev = 0.0f;
        segments[i].score = 0;

        /* Reset runtime state but keep points loaded */
        seg_runtime_t *rt = slot_of(i);

        if (rt != NULL) {
            rt->start_time = 0.0f;
            rt->cur_time = 0.0f;
            rt->advance = 0.0f;
            rt->pct_dist = 0.0f;
            rt->pct_elev = 0.0f;
        }
    }

    /* Clear user position history */
    liste_clear(&user_history);

    LOG_INF("All segments reset");
}

void segment_unload_all(void)
{
    /* Give every slot of the pool back */
    for (uint8_t i = 0U; i < SEG_SLOTS; i++) {
        seg_slots[i].owner = SEG_NO_OWNER;
        liste_init_static(&seg_slots[i].pts, seg_slots[i].storage, SEG_SLOT_POINTS);
    }

    (void)memset(segments, 0, sizeof(segments));
    (void)memset(seg_headers, 0, sizeof(seg_headers));
    (void)memset(seg_start_pos, 0, sizeof(seg_start_pos));
    segment_count = 0U;
    nearest_dist = -1.0f;

    /* Clear user position history */
    liste_clear(&user_history);

    LOG_INF("All segments unloaded");
}

/**
 * @brief Load segment points from file
 */
static int load_segment_points(uint16_t seg_idx)
{
    if (seg_idx >= segment_count) {
        return -1;
    }

    segment_t *seg = &segments[seg_idx];

    if (slot_of(seg_idx) != NULL) {
        return 0;  /* Already loaded */
    }

    char path[32];

    (void)snprintf(path, sizeof(path), "%s/%s", SEG_DIR, seg->name);

    seg_runtime_t *rt = slot_take(seg_idx);

    if (rt == NULL) {
        LOG_WRN("No free slot for segment %s", seg->name);
        return -1;
    }

    struct fs_file_t file;

    fs_file_t_init(&file);

    if (fs_open(&file, path, FS_O_READ) < 0) {
        LOG_WRN("Cannot open segment file: %s", path);
        slot_give(seg_idx);
        return -1;
    }

    /*
     * Text of the legacy: a `<Name>` line and then `lat ; lon ; rtime ;
     * alt`, with the time of each point taken from the first
     * (`legacy/source/sd/sd_functions.cpp`, `load_segment`). The file is
     * read in chunks and split into lines here, because fs_read() knows
     * nothing about lines.
     */
    char chunk[128];
    char line[SEG_LINE_MAX];
    size_t line_len = 0U;
    float first_time = 0.0f;
    float first_alt = 0.0f;
    struct segment_file_point last_sp = {0};
    uint32_t read_points = 0U;   /* points of the file, kept or not */
    bool last_kept = false;
    bool truncated = false;
    ssize_t got;

    while (!truncated && ((got = fs_read(&file, chunk, sizeof(chunk))) > 0)) {
        for (ssize_t i = 0; i < got; i++) {
            char c = chunk[i];

            if ((c != '\n') && (c != '\r')) {
                if (line_len < (SEG_LINE_MAX - 1U)) {
                    line[line_len] = c;
                    line_len++;
                }
                continue;
            }

            line[line_len] = '\0';
            line_len = 0U;

            struct segment_file_point sp;

            if (!segment_file_parse_line(line, &sp)) {
                continue;
            }
            if (read_points == 0U) {
                first_time = sp.rtime;
                first_alt = sp.alt;
            }
            last_sp = sp;

            /* The slot is full: halve what is there and go on, as the
             * comment on SEG_SLOTS explains. */
            if (rt->pts.count >= SEG_SLOT_POINTS) {
                if (liste_decimate(&rt->pts) >= SEG_SLOT_POINTS) {
                    LOG_WRN("Segment %s truncated at %u points", seg->name,
                            (unsigned int)rt->pts.count);
                    truncated = true;
                    break;
                }
                rt->stride *= 2U;
            }

            last_kept = ((read_points % rt->stride) == 0U);
            if (last_kept) {
                liste_add_back(&rt->pts, sp.lat, sp.lon, sp.alt, sp.rtime - first_time);
            }
            read_points++;
        }
    }

    (void)fs_close(&file);

    /*
     * The end of the segment decides when it is finished and how long it
     * took, so the last point of the file always goes in, even when
     * decimation had dropped it.
     */
    if ((read_points > 0U) && !last_kept) {
        if (rt->pts.count >= SEG_SLOT_POINTS) {
            (void)liste_decimate(&rt->pts);
            rt->stride *= 2U;
        }
        liste_add_back(&rt->pts, last_sp.lat, last_sp.lon, last_sp.alt,
                       last_sp.rtime - first_time);
    }

    if (rt->pts.count > 0U) {
        const point_t *last = liste_get_last(&rt->pts);

        rt->elev_total = last_sp.alt - first_alt;
        seg->num_points = rt->pts.count;
        seg->total_time = (last != NULL) ? last->rtime : 0.0f;
        seg->total_elev = rt->elev_total;

        LOG_INF("Loaded %u of %u points for segment %s", (unsigned int)rt->pts.count,
                (unsigned int)read_points, seg->name);

        return (int)rt->pts.count;
    }

    slot_give(seg_idx);

    return -1;
}

/**
 * @brief Unload segment points to free memory
 */
static void unload_segment_points(uint16_t seg_idx)
{
    if (seg_idx >= segment_count) {
        return;
    }

    segment_t *seg = &segments[seg_idx];

    if (slot_of(seg_idx) == NULL) {
        return;
    }

    slot_give(seg_idx);

    /* Reset segment state: nothing of the file is left in memory */
    seg->status = SEG_OFF;
    seg->cur_time = 0.0f;
    seg->advance = 0.0f;
    seg->pct_dist = 0.0f;
    seg->pct_elev = 0.0f;
    seg->score = 0;
    seg->num_points = 0U;
    seg->total_time = 0.0f;
    seg->total_elev = 0.0f;

    LOG_INF("Unloaded segment %s", seg->name);
}

/**
 * @brief Calculate distance from coordinates to segment start using header
 */
static float dist_to_seg_header(uint16_t seg_idx, float lat, float lon)
{
    if (seg_idx >= segment_count) {
        return 9999.0f;
    }

    const seg_runtime_t *rt = slot_of(seg_idx);

    /* Loaded: the distance to the first point, as the legacy measures it */
    if ((rt != NULL) && (rt->pts.count > 0U)) {
        const point_t *seg_start = liste_get_at(&rt->pts, 0);

        if (seg_start != NULL) {
            point_t cur = {.lat = lat, .lon = lon, .alt = 0.0f, .rtime = 0.0f};

            return point_distance(&cur, seg_start);
        }
    }

    /*
     * Not loaded: the start comes from the name of the file, which is what
     * lets the allocator work without opening anything
     * (`legacy/source/sd/sd_functions.cpp:547-556`).
     */
    return distance_between(lat, lon, seg_start_pos[seg_idx].lat, seg_start_pos[seg_idx].lon);
}

float segment_allocator(uint16_t seg_idx, float lat, float lon)
{
    if (!is_initialized || (seg_idx >= segment_count)) {
        return -1.0f;
    }

    const seg_runtime_t *rt = slot_of(seg_idx);
    segment_t *seg = &segments[seg_idx];
    float dist_to_seg = 9999.0f;

    /* Segment is loaded with points */
    if ((rt != NULL) && (rt->pts.count > 0U)) {
        /* Get distance to first point */
        const point_t *seg_start = liste_get_at(&rt->pts, 0);
        if (seg_start != NULL) {
            point_t cur = { .lat = lat, .lon = lon, .alt = 0.0f, .rtime = 0.0f };
            dist_to_seg = point_distance(&cur, seg_start);
        }

        /* Check if segment is inactive and too far - unload */
        if ((seg->status == SEG_OFF) && (dist_to_seg > SEG_ALLOC_DIST)) {
            unload_segment_points(seg_idx);
            LOG_DBG("Unallocated segment %s (dist=%.0f)", seg->name, (double)dist_to_seg);
        }
        /* Check if active but way too far - force unload */
        else if (dist_to_seg > SEG_MARGE_DESACT * SEG_ALLOC_DIST) {
            unload_segment_points(seg_idx);
            LOG_WRN("Force unallocated segment %s", seg->name);
        }
    }
    /* Segment not loaded - check if should load */
    else {
        dist_to_seg = dist_to_seg_header(seg_idx, lat, lon);

        if (dist_to_seg < SEG_ALLOC_DIST) {
            int res = load_segment_points(seg_idx);
            if (res > 0) {
                LOG_INF("Allocated segment %s (dist=%.0f)", seg->name, (double)dist_to_seg);
            }
        }
    }

    return dist_to_seg;
}

float segment_run_allocator(float lat, float lon)
{
    if (!is_initialized || (segment_count == 0U)) {
        return -1.0f;
    }

    float nearest = 9999.0f;

    for (uint16_t i = 0U; i < segment_count; i++) {
        float dist = segment_allocator(i, lat, lon);
        if ((dist >= 0.0f) && (dist < nearest)) {
            nearest = dist;
        }
    }

    nearest_dist = (nearest < 9000.0f) ? nearest : -1.0f;

    return nearest_dist;
}
