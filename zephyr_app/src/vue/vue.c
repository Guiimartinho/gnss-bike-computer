/**
 * @file vue.c
 * @brief Display/View management implementation
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vue/vue.h"
#include "vue/menu.h"
#include "vue/font_5x7.h"
#include "vue/gfxfont.h"
#include "drivers/ls027.h"
#include "model/boucle.h"
#include "model/attitude.h"
#include "model/segment.h"
#include "model/parcours.h"
#include "drivers/gps_mgmt.h"
#include "rf/ble_hrs_client.h"
#include "rf/ble_bsc_client.h"
#include "model/user_settings.h"

LOG_MODULE_REGISTER(vue, CONFIG_LOG_DEFAULT_LEVEL);

/* ==========================================================================
 * Private Definitions
 * ========================================================================== */

/** Font sizes */
#define FONT_SMALL_H        8U
#define FONT_MEDIUM_H       16U
#define FONT_LARGE_H        32U

/** Screen layout constants */
#define HEADER_HEIGHT       24U
#define FOOTER_HEIGHT       20U
#define CONTENT_START_Y     (HEADER_HEIGHT + 2U)

/** Zoom constants */
#define BASE_ZOOM_LEVEL     5U
#define BASE_ZOOM_METERS    60.0f
#define MAX_ZOOM_LEVEL      100U

/** Math constants */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#define TWO_PI (2.0f * M_PI)

/* ==========================================================================
 * Histogram Types
 * ========================================================================== */

/** Histogram value type */
typedef uint16_t histo_value_t;

/** Histogram read function pointer */
typedef histo_value_t (*histo_read_func_t)(uint16_t index);

/** Histogram configuration */
typedef struct {
    int16_t nb_elem_tot;        /**< Total number of elements */
    int16_t cur_elem_nb;        /**< Current number of elements */
    histo_value_t max_value;    /**< Maximum value for scaling */
    histo_value_t ref_value;    /**< Reference value (for comparison line) */
    histo_read_func_t read_func; /**< Function to read values */
} histo_config_t;

/** RR Zone bin data */
typedef struct {
    uint8_t nb_bins;            /**< Number of bins/zones */
    uint8_t cur_bin;            /**< Current active bin */
    float values[8];            /**< Values per bin */
    float val_max;              /**< Maximum value */
} rr_zone_t;

/** Zoom state */
typedef struct {
    uint16_t h_size;            /**< Horizontal size in pixels */
    uint16_t v_size;            /**< Vertical size in pixels */
    float last_zoom;            /**< Last computed zoom in meters */
    uint8_t zoom_level;         /**< Current zoom level */
} zoom_state_t;

/* ==========================================================================
 * Private Variables
 * ========================================================================== */

/** Zoom state */
static zoom_state_t zoom = {
    .h_size = 300U,
    .v_size = 150U,
    .last_zoom = 0.0f,
    .zoom_level = BASE_ZOOM_LEVEL
};

/** Current view mode */
static vue_mode_t current_mode = VUE_MODE_CRS;

/** Current page */
static vue_page_t current_page = VUE_PAGE_MAIN;

/** Active notification */
static notification_t active_notif;
static bool notif_active;
static uint32_t notif_start_time;

/** Initialization flag */
static bool is_initialized;

/* ==========================================================================
 * Private Functions - Drawing Primitives
 * ========================================================================== */

/**
 * @brief Draw a single character using 5x7 font with scaling
 * @param x X position (top-left)
 * @param y Y position (top-left)
 * @param c Character to draw
 * @param size Scale factor (1=5x7, 2=10x14, 3=15x21)
 * @param color Pixel color
 */
static void draw_char(uint16_t x, uint16_t y, char c, uint8_t size)
{
    uint8_t char_idx = (uint8_t)c;

    /* Bounds check - screen limits */
    if ((x >= LS027_WIDTH) || (y >= LS027_HEIGHT)) {
        return;
    }

    /* For each column of the character (5 columns) */
    for (uint8_t col = 0U; col < FONT_5X7_GLYPH_WIDTH; col++) {
        uint8_t col_data = font_5x7[char_idx][col];

        /* For each row in this column (7 rows, LSB = top) */
        for (uint8_t row = 0U; row < FONT_5X7_GLYPH_HEIGHT; row++) {
            /* Check if this pixel is set */
            if ((col_data & (1U << row)) != 0U) {
                /* Draw scaled pixel */
                if (size == 1U) {
                    /* 1:1 scale */
                    uint16_t px = x + col;
                    uint16_t py = y + row;
                    if ((px < LS027_WIDTH) && (py < LS027_HEIGHT)) {
                        ls027_draw_pixel(px, py, LS027_COLOR_BLACK);
                    }
                } else {
                    /* Scaled: draw size x size block for each pixel */
                    for (uint8_t sy = 0U; sy < size; sy++) {
                        for (uint8_t sx = 0U; sx < size; sx++) {
                            uint16_t px = x + (col * size) + sx;
                            uint16_t py = y + (row * size) + sy;
                            if ((px < LS027_WIDTH) && (py < LS027_HEIGHT)) {
                                ls027_draw_pixel(px, py, LS027_COLOR_BLACK);
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief Draw a string using the 5x7 font with scaling
 * @param x X position (top-left)
 * @param y Y position (top-left)
 * @param str Null-terminated string
 * @param size Scale factor (1=small, 2=medium, 3=large)
 */
static void draw_string(uint16_t x, uint16_t y, const char *str, uint8_t size)
{
    if (str == NULL) {
        return;
    }

    uint16_t char_w = FONT_5X7_CHAR_WIDTH * size;
    uint16_t px = x;

    while (*str != '\0') {
        /* Handle newline */
        if (*str == '\n') {
            px = x;
            y += FONT_5X7_CHAR_HEIGHT * size;
            str++;
            continue;
        }

        /* Skip carriage return */
        if (*str == '\r') {
            str++;
            continue;
        }

        /* Bounds check */
        if (px >= LS027_WIDTH) {
            break;
        }

        draw_char(px, y, *str, size);
        px += char_w;
        str++;
    }
}

/**
 * @brief Draw a string centered horizontally
 * @param y Y position (top)
 * @param str Null-terminated string
 * @param size Scale factor
 */
__attribute__((unused))
static void draw_string_centered(uint16_t y, const char *str, uint8_t size)
{
    if (str == NULL) {
        return;
    }

    uint16_t len = (uint16_t)strlen(str);
    uint16_t char_w = FONT_5X7_CHAR_WIDTH * size;
    uint16_t total_w = len * char_w;

    uint16_t x = 0U;
    if (total_w < LS027_WIDTH) {
        x = (LS027_WIDTH - total_w) / 2U;
    }

    draw_string(x, y, str, size);
}

/**
 * @brief Draw a string right-aligned
 * @param x_right Right edge X position
 * @param y Y position (top)
 * @param str Null-terminated string
 * @param size Scale factor
 */
__attribute__((unused))
static void draw_string_right(uint16_t x_right, uint16_t y, const char *str, uint8_t size)
{
    if (str == NULL) {
        return;
    }

    uint16_t len = (uint16_t)strlen(str);
    uint16_t char_w = FONT_5X7_CHAR_WIDTH * size;
    uint16_t total_w = len * char_w;

    uint16_t x = 0U;
    if (total_w < x_right) {
        x = x_right - total_w;
    }

    draw_string(x, y, str, size);
}

/**
 * @brief Draw horizontal line
 */
static void draw_hline(uint16_t x, uint16_t y, uint16_t len)
{
    ls027_draw_hline(x, y, len, LS027_COLOR_BLACK);
}

/**
 * @brief Draw rectangle outline
 */
static void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    ls027_draw_hline(x, y, w, LS027_COLOR_BLACK);
    ls027_draw_hline(x, y + h - 1U, w, LS027_COLOR_BLACK);
    ls027_draw_vline(x, y, h, LS027_COLOR_BLACK);
    ls027_draw_vline(x + w - 1U, y, h, LS027_COLOR_BLACK);
}

/**
 * @brief Draw dashed horizontal line
 */
static void draw_dashed_hline(uint16_t x, uint16_t y, uint16_t len, uint8_t dash_len)
{
    for (uint16_t i = 0U; i < len; i += (dash_len * 2U)) {
        uint16_t seg_len = (i + dash_len > len) ? (len - i) : dash_len;
        ls027_draw_hline(x + i, y, seg_len, LS027_COLOR_BLACK);
    }
}

/* ==========================================================================
 * Private Functions - Histogram Drawing
 * ========================================================================== */

/**
 * @brief Draw horizontal histogram (full width)
 * @param p_row Row position (1-based)
 * @param nb_rows Total number of rows
 * @param config Histogram configuration
 */
static void draw_histogram_h(uint8_t p_row, uint8_t nb_rows, const histo_config_t *config)
{
    if ((config == NULL) || (config->read_func == NULL)) {
        return;
    }

    uint16_t y_base = (uint16_t)(LS027_HEIGHT / nb_rows * p_row);
    uint16_t row_height = LS027_HEIGHT / nb_rows;

    if (config->cur_elem_nb > 0) {
        uint16_t dx = LS027_WIDTH / (uint16_t)config->nb_elem_tot;
        if (dx < 1U) {
            dx = 1U;
        }

        /* Iterate through buffer elements */
        for (int16_t i = 0; i < config->cur_elem_nb; i++) {
            uint16_t x_pos = (uint16_t)i * dx;
            histo_value_t elem = config->read_func((uint16_t)i);

            /* Calculate bar height */
            uint16_t height = (uint16_t)((uint32_t)elem * row_height /
                              ((uint32_t)config->max_value));

            if (height > row_height) {
                height = row_height;
            }

            uint16_t bar_height = 4U;  /* Minimum bar thickness */

            /* Handle reference value comparison */
            if (config->ref_value > 0U) {
                uint16_t ref_height = (uint16_t)((uint32_t)config->ref_value * row_height /
                                      ((uint32_t)config->max_value));

                if (elem <= config->ref_value) {
                    height = ref_height;
                }

                int16_t diff = (int16_t)elem - (int16_t)config->ref_value;
                bar_height = (uint16_t)((diff > 0 ? diff : -diff) * row_height /
                            ((int32_t)config->max_value));
                if (bar_height < 1U) {
                    bar_height = 1U;
                }

                /* Draw reference line */
                uint16_t ref_y = y_base - ref_height;
                draw_dashed_hline(0U, ref_y, LS027_WIDTH, 4U);
            }

            /* Draw bar */
            if ((y_base >= height) && (x_pos < LS027_WIDTH)) {
                ls027_fill_rect(x_pos, y_base - height, dx, bar_height, LS027_COLOR_BLACK);
            }
        }
    }

    /* Draw row delimiters */
    if (p_row > 1U) {
        draw_hline(0U, (uint16_t)(LS027_HEIGHT / nb_rows * (p_row - 1U)), LS027_WIDTH);
    }
    if (p_row < nb_rows) {
        draw_hline(0U, y_base, LS027_WIDTH);
    }
}

/**
 * @brief Draw histogram in half-width column
 * @param p_row Row position (1-based)
 * @param nb_rows Total number of rows
 * @param p_col Column (1=left, 2=right)
 * @param config Histogram configuration
 */
static void draw_histogram(uint8_t p_row, uint8_t nb_rows, uint8_t p_col,
                          const histo_config_t *config)
{
    if ((config == NULL) || (config->read_func == NULL)) {
        return;
    }

    uint16_t x_base = (uint16_t)(LS027_WIDTH / 2U * (p_col - 1U));
    uint16_t y_base = (uint16_t)(LS027_HEIGHT / nb_rows * p_row);
    uint16_t row_height = LS027_HEIGHT / nb_rows;

    if (config->cur_elem_nb > 0) {
        uint16_t dx = LS027_WIDTH / (2U * (uint16_t)config->nb_elem_tot);
        if (dx < 1U) {
            dx = 1U;
        }

        /* Iterate through buffer elements */
        for (int16_t i = 0; i < config->cur_elem_nb; i++) {
            uint16_t x_pos = x_base + ((uint16_t)i * dx);
            histo_value_t elem = config->read_func((uint16_t)i);

            /* Calculate bar height */
            uint16_t height = (uint16_t)((uint32_t)elem * row_height /
                              ((uint32_t)config->max_value));

            if (height > row_height) {
                height = row_height;
            }

            /* Draw small bar at top of each column */
            if ((y_base >= height) && (x_pos < LS027_WIDTH)) {
                ls027_fill_rect(x_pos, y_base - height, dx, 4U, LS027_COLOR_BLACK);
            }
        }
    }

    /* Draw delimiters */
    ls027_draw_vline(LS027_WIDTH / 2U, (uint16_t)(LS027_HEIGHT / nb_rows * (p_row - 1U)),
                     row_height, LS027_COLOR_BLACK);

    if (p_row > 1U) {
        draw_hline((uint16_t)(LS027_WIDTH * (p_col - 1U) / 2U),
                   (uint16_t)(LS027_HEIGHT / nb_rows * (p_row - 1U)),
                   LS027_WIDTH / 2U);
    }
    if (p_row < nb_rows) {
        draw_hline((uint16_t)(LS027_WIDTH * (p_col - 1U) / 2U),
                   y_base,
                   LS027_WIDTH / 2U);
    }
}

/* ==========================================================================
 * Private Functions - RR Zone Drawing
 * ========================================================================== */

/**
 * @brief Linear interpolation with limits
 */
static inline float reg_fen_lim(float val, float in_min, float in_max,
                                float out_min, float out_max)
{
    if (val < in_min) {
        return out_min;
    }
    if (val > in_max) {
        return out_max;
    }
    return out_min + (val - in_min) * (out_max - out_min) / (in_max - in_min);
}

/**
 * @brief Draw RR Zone display (heart rate variability zones)
 * @param p_row Row position (1-based)
 * @param nb_rows Total number of rows
 * @param p_col Column (1=left, 2=right)
 * @param label Label text
 * @param zone RR zone data
 */
static void draw_rr_zone(uint8_t p_row, uint8_t nb_rows, uint8_t p_col,
                        const char *label, const rr_zone_t *zone)
{
    if (zone == NULL) {
        return;
    }

    uint16_t x = (uint16_t)(LS027_WIDTH / 2U * p_col);
    uint16_t y = (uint16_t)(LS027_HEIGHT / nb_rows * (p_row - 1U));
    uint16_t half_width = LS027_WIDTH / 2U;

    /* Draw label */
    if (label != NULL) {
        draw_string(x + 5U - half_width, y + 8U, label, 1U);
    }

    /* Limit max value display range */
    float val_max = zone->val_max;
    if (val_max > 50.0f) {
        val_max = 50.0f;
    }
    if (val_max < 5.0f) {
        val_max = 5.0f;
    }

    /* Draw bars for each zone */
    for (uint8_t i = 0U; i < zone->nb_bins; i++) {
        float val = zone->values[i];

        /* Calculate bar width */
        int16_t bar_width = (int16_t)reg_fen_lim(val, 0.0f, val_max,
                                                  2.0f, (float)(half_width - 35U));

        /* Draw bar */
        uint16_t bar_y = y + 20U + (i * 6U);
        if ((bar_width > 0) && (bar_y < LS027_HEIGHT)) {
            ls027_fill_rect(x - half_width + 20U, bar_y,
                           (uint16_t)bar_width, 4U, LS027_COLOR_BLACK);
        }

        /* Draw cursor for current zone */
        if (i == zone->cur_bin) {
            draw_string(x - half_width + 7U, y + 15U + (i * 6U), ">", 2U);
        }
    }

    /* Draw max value label */
    char buf[16];
    (void)snprintf(buf, sizeof(buf), "%u", (unsigned)zone->val_max);
    draw_string(x - half_width + 40U, y + 17U + (zone->nb_bins * 6U), buf, 2U);

    /* Draw delimiters */
    ls027_draw_vline(half_width, (uint16_t)(LS027_HEIGHT / nb_rows * (p_row - 1U)),
                     LS027_HEIGHT / nb_rows, LS027_COLOR_BLACK);

    if (p_row > 1U) {
        draw_hline((uint16_t)(LS027_WIDTH * (p_col - 1U) / 2U),
                   (uint16_t)(LS027_HEIGHT / nb_rows * (p_row - 1U)),
                   half_width);
    }
    if (p_row < nb_rows) {
        draw_hline((uint16_t)(LS027_WIDTH * (p_col - 1U) / 2U),
                   (uint16_t)(LS027_HEIGHT / nb_rows * p_row),
                   half_width);
    }
}

/* ==========================================================================
 * Private Functions - Map/Rotation Utilities
 * ========================================================================== */

/**
 * @brief Rotate point around center
 * @param angle Angle in degrees (positive = clockwise)
 * @param cx Center X
 * @param cy Center Y
 * @param x1 Input X
 * @param y1 Input Y
 * @param x2 Output X (pointer)
 * @param y2 Output Y (pointer)
 */
static void rotate_point(float angle, int16_t cx, int16_t cy,
                        int16_t x1, int16_t y1, int16_t *x2, int16_t *y2)
{
    float angle_rad = angle * M_PI / 180.0f;

    /* Translate to origin */
    float tmp1 = (float)(x1 - cx);
    float tmp2 = (float)(y1 - cy);

    /* Rotate */
    float tmp3 = tmp1 * cosf(angle_rad) - tmp2 * sinf(angle_rad);
    float tmp4 = tmp1 * sinf(angle_rad) + tmp2 * cosf(angle_rad);

    /* Translate back */
    *x2 = (int16_t)(tmp3 + (float)cx);
    *y2 = (int16_t)(tmp4 + (float)cy);
}

/**
 * @brief Calculate course/bearing between two points
 * @param lat1 Start latitude (degrees)
 * @param lon1 Start longitude (degrees)
 * @param lat2 End latitude (degrees)
 * @param lon2 End longitude (degrees)
 * @return Course in degrees (North=0, East=90, West=270)
 */
static float course_to(float lat1, float lon1, float lat2, float lon2)
{
    float dlon = (lon2 - lon1) * M_PI / 180.0f;
    lat1 = lat1 * M_PI / 180.0f;
    lat2 = lat2 * M_PI / 180.0f;

    float a1 = sinf(dlon) * cosf(lat2);
    float a2 = cosf(lat1) * sinf(lat2) - sinf(lat1) * cosf(lat2) * cosf(dlon);
    float result = atan2f(a1, a2);

    if (result < 0.0f) {
        result += TWO_PI;
    }

    return result * 180.0f / M_PI;
}

/**
 * @brief Compute zoom levels for map display
 * @param lat Current latitude
 * @param distance Distance to closest point
 * @param h_zoom Output horizontal zoom (degrees)
 * @param v_zoom Output vertical zoom (degrees)
 */
static void compute_zoom(float lat, float distance, float *h_zoom, float *v_zoom)
{
    /* Calculate conversion factors between degrees and meters */
    float deglon_to_m = 1000.0f * distance_between(lat, 0.0f, lat, 0.001f);
    float deglat_to_m = 1000.0f * distance_between(lat, 0.0f, lat + 0.001f, 0.0f);

    /* Calculate zoom in meters based on zoom level */
    float h_zoom_m = (float)(zoom.zoom_level * zoom.zoom_level) * BASE_ZOOM_METERS /
                     (float)(BASE_ZOOM_LEVEL * BASE_ZOOM_LEVEL);
    float v_zoom_m = h_zoom_m * (float)zoom.v_size / (float)zoom.h_size;

    /* Convert to degrees */
    if (deglon_to_m > 0.0f) {
        *h_zoom = h_zoom_m / deglon_to_m;
    } else {
        *h_zoom = 0.001f;
    }

    if (deglat_to_m > 0.0f) {
        *v_zoom = v_zoom_m / deglat_to_m;
    } else {
        *v_zoom = 0.001f;
    }

    zoom.last_zoom = h_zoom_m;

    (void)distance;  /* Used for auto-zoom in future */
}

/**
 * @brief Increase zoom level (zoom out)
 */
static void zoom_decrease(void)
{
    if (zoom.zoom_level < MAX_ZOOM_LEVEL) {
        zoom.zoom_level++;
    }
}

/**
 * @brief Decrease zoom level (zoom in)
 */
static void zoom_increase(void)
{
    if (zoom.zoom_level > 1U) {
        zoom.zoom_level--;
    } else {
        zoom.zoom_level = BASE_ZOOM_LEVEL * 2U;
    }
}

/**
 * @brief Reset zoom to default
 */
static void zoom_reset(void)
{
    zoom.zoom_level = BASE_ZOOM_LEVEL;
}

/**
 * @brief Convert GPS coordinates to screen coordinates
 * @param lat Latitude
 * @param lon Longitude
 * @param center_lat Center latitude
 * @param center_lon Center longitude
 * @param h_zoom Horizontal zoom span (degrees)
 * @param v_zoom Vertical zoom span (degrees)
 * @param screen_x Output screen X
 * @param screen_y Output screen Y
 * @param map_cx Map center X on screen
 * @param map_cy Map center Y on screen
 * @param map_w Map width in pixels
 * @param map_h Map height in pixels
 * @return true if point is within screen bounds
 */
static bool gps_to_screen(float lat, float lon, float center_lat, float center_lon,
                         float h_zoom, float v_zoom,
                         int16_t *screen_x, int16_t *screen_y,
                         uint16_t map_cx, uint16_t map_cy,
                         uint16_t map_w, uint16_t map_h)
{
    /* Calculate relative position */
    float rel_lon = lon - center_lon;
    float rel_lat = lat - center_lat;

    /* Convert to screen coordinates */
    if ((h_zoom > 0.0f) && (v_zoom > 0.0f)) {
        *screen_x = (int16_t)map_cx + (int16_t)((rel_lon / h_zoom) * ((float)map_w / 2.0f));
        *screen_y = (int16_t)map_cy - (int16_t)((rel_lat / v_zoom) * ((float)map_h / 2.0f));
    } else {
        *screen_x = (int16_t)map_cx;
        *screen_y = (int16_t)map_cy;
    }

    /* Check bounds */
    return (*screen_x >= 0) && (*screen_x < (int16_t)LS027_WIDTH) &&
           (*screen_y >= 0) && (*screen_y < (int16_t)LS027_HEIGHT);
}

/* ==========================================================================
 * Private Functions - Screen Rendering
 * ========================================================================== */

/**
 * @brief Draw header bar
 */
static void draw_header(void)
{
    /* Background line */
    draw_hline(0U, HEADER_HEIGHT, LS027_WIDTH);

    /* GPS status indicator */
    draw_string(5U, 4U, "GPS", 1U);

    /* Time display (center) */
    attitude_t att;
    if (attitude_get(&att) == APP_OK) {
        char time_str[16];
        uint32_t secj = att.date.secj;
        uint8_t h = (uint8_t)(secj / 3600U);
        uint8_t m = (uint8_t)((secj % 3600U) / 60U);

        (void)snprintf(time_str, sizeof(time_str), "%02u:%02u", h, m);
        draw_string(180U, 4U, time_str, 1U);
    }

    /* Battery indicator (right) */
    draw_rect(LS027_WIDTH - 30U, 4U, 25U, 12U);
}

/**
 * @brief Draw main cycling page
 */
static void draw_page_main(void)
{
    attitude_t att;
    char buf[32];

    if (attitude_get(&att) != APP_OK) {
        return;
    }

    uint16_t y = CONTENT_START_Y;

    /* Speed - large display */
    (void)snprintf(buf, sizeof(buf), "%.1f", (double)att.loc.speed);
    draw_string(20U, y, buf, 3U);
    draw_string(150U, y + 8U, "km/h", 1U);
    y += 40U;

    /* Distance */
    (void)snprintf(buf, sizeof(buf), "%.2f km", (double)(att.dist / 1000.0f));
    draw_string(20U, y, "Dist:", 1U);
    draw_string(100U, y, buf, 2U);
    y += 24U;

    /* Elevation gain */
    (void)snprintf(buf, sizeof(buf), "%.0f m", (double)att.climb);
    draw_string(20U, y, "Elev:", 1U);
    draw_string(100U, y, buf, 2U);
    y += 24U;

    /* Current slope */
    (void)snprintf(buf, sizeof(buf), "%d%%", att.slope);
    draw_string(20U, y, "Slope:", 1U);
    draw_string(100U, y, buf, 2U);
    y += 24U;

    /* Power */
    (void)snprintf(buf, sizeof(buf), "%u W", att.pwr);
    draw_string(20U, y, "Power:", 1U);
    draw_string(100U, y, buf, 2U);
    y += 24U;

    /* Elapsed time */
    uint32_t secs = attitude_get_elapsed_time();
    uint8_t h = (uint8_t)(secs / 3600U);
    uint8_t m = (uint8_t)((secs % 3600U) / 60U);
    uint8_t s = (uint8_t)(secs % 60U);

    (void)snprintf(buf, sizeof(buf), "%02u:%02u:%02u", h, m, s);
    draw_string(20U, y, "Time:", 1U);
    draw_string(100U, y, buf, 2U);
}

/**
 * @brief Draw segment page
 */
static void draw_page_segment(void)
{
    segment_t seg;
    char buf[32];

    uint16_t y = CONTENT_START_Y;

    if (segment_get_best(&seg) == APP_OK) {
        /* Segment name */
        draw_string(20U, y, seg.name, 2U);
        y += 24U;

        /* Progress */
        (void)snprintf(buf, sizeof(buf), "Progress: %.0f%%", (double)(seg.pct_dist * 100.0f));
        draw_string(20U, y, buf, 1U);
        y += 16U;

        /* Time advantage/deficit */
        const char *sign = (seg.advance >= 0.0f) ? "+" : "";
        (void)snprintf(buf, sizeof(buf), "%s%.1f s", sign, (double)seg.advance);
        draw_string(20U, y, "vs PR:", 1U);
        draw_string(100U, y, buf, 2U);
        y += 24U;

        /* Progress bar */
        uint16_t bar_w = 300U;
        uint16_t bar_h = 20U;
        draw_rect(50U, y, bar_w, bar_h);

        uint16_t fill_w = (uint16_t)((float)bar_w * seg.pct_dist);
        if (fill_w > 0U) {
            ls027_fill_rect(50U, y, fill_w, bar_h, LS027_COLOR_BLACK);
        }
    } else {
        draw_string(100U, 100U, "No active segment", 1U);
    }
}

/**
 * @brief Draw statistics page
 */
static void draw_page_stats(void)
{
    boucle_stats_t stats;
    char buf[32];

    if (boucle_get_stats(&stats) != APP_OK) {
        return;
    }

    uint16_t y = CONTENT_START_Y;

    draw_string(20U, y, "STATISTICS", 2U);
    y += 30U;

    /* Distance */
    (void)snprintf(buf, sizeof(buf), "%.2f km", (double)(stats.total_distance / 1000.0f));
    draw_string(20U, y, "Distance:", 1U);
    draw_string(150U, y, buf, 1U);
    y += 16U;

    /* Climb */
    (void)snprintf(buf, sizeof(buf), "%.0f m", (double)stats.total_climb);
    draw_string(20U, y, "Climb:", 1U);
    draw_string(150U, y, buf, 1U);
    y += 16U;

    /* Max speed */
    (void)snprintf(buf, sizeof(buf), "%.1f km/h", (double)stats.max_speed);
    draw_string(20U, y, "Max Speed:", 1U);
    draw_string(150U, y, buf, 1U);
    y += 16U;

    /* Average speed */
    (void)snprintf(buf, sizeof(buf), "%.1f km/h", (double)stats.avg_speed);
    draw_string(20U, y, "Avg Speed:", 1U);
    draw_string(150U, y, buf, 1U);
    y += 16U;

    /* Moving time */
    uint32_t secs = stats.moving_time;
    uint8_t h = (uint8_t)(secs / 3600U);
    uint8_t m = (uint8_t)((secs % 3600U) / 60U);

    (void)snprintf(buf, sizeof(buf), "%uh %02um", h, m);
    draw_string(20U, y, "Moving:", 1U);
    draw_string(150U, y, buf, 1U);
    y += 16U;

    /* GPS points */
    (void)snprintf(buf, sizeof(buf), "%u", stats.gps_points);
    draw_string(20U, y, "GPS Points:", 1U);
    draw_string(150U, y, buf, 1U);
}

/**
 * @brief Draw parcours/route navigation page
 */
static void draw_page_parcours(void)
{
    parcours_info_t pinfo;
    nav_info_t nav;
    char buf[32];

    uint16_t y = CONTENT_START_Y;

    if (!parcours_is_loaded()) {
        draw_string(60U, 100U, "No route loaded", 2U);
        draw_string(40U, 130U, "Load .CRS from SD card", 1U);
        return;
    }

    if (parcours_get_info(&pinfo) != APP_OK) {
        return;
    }

    /* Route name */
    draw_string(20U, y, pinfo.name, 2U);
    y += 28U;

    draw_hline(20U, y, LS027_WIDTH - 40U);
    y += 8U;

    if (!parcours_is_active()) {
        draw_string(60U, y + 30U, "Press START to begin", 1U);
        return;
    }

    if (parcours_get_nav_info(&nav) != APP_OK) {
        return;
    }

    /* Progress percentage */
    (void)snprintf(buf, sizeof(buf), "Progress: %.0f%%", (double)nav.pct_complete);
    draw_string(20U, y, buf, 2U);
    y += 24U;

    /* Progress bar */
    uint16_t bar_w = LS027_WIDTH - 60U;
    uint16_t bar_h = 16U;
    draw_rect(30U, y, bar_w, bar_h);
    uint16_t fill_w = (uint16_t)((float)bar_w * (nav.pct_complete / 100.0f));
    if (fill_w > 0U) {
        ls027_fill_rect(30U, y, fill_w, bar_h, LS027_COLOR_BLACK);
    }
    y += 24U;

    /* Distance remaining */
    (void)snprintf(buf, sizeof(buf), "%.2f km", (double)(nav.dist_remaining / 1000.0f));
    draw_string(20U, y, "Remaining:", 1U);
    draw_string(140U, y, buf, 2U);
    y += 24U;

    /* Distance to route */
    if (nav.on_route) {
        (void)snprintf(buf, sizeof(buf), "%.0f m", (double)nav.dist_to_route);
    } else {
        (void)snprintf(buf, sizeof(buf), "OFF ROUTE (%.0f m)", (double)nav.dist_to_route);
    }
    draw_string(20U, y, "To Route:", 1U);
    draw_string(140U, y, buf, 1U);
    y += 20U;

    /* Bearing to next point */
    (void)snprintf(buf, sizeof(buf), "%.0f", (double)nav.bearing);
    draw_string(20U, y, "Bearing:", 1U);
    draw_string(140U, y, buf, 2U);
    y += 24U;

    /* Next point altitude */
    (void)snprintf(buf, sizeof(buf), "%.0f m", (double)nav.altitude_next);
    draw_string(20U, y, "Next Alt:", 1U);
    draw_string(140U, y, buf, 1U);
}

/**
 * @brief Draw GPS debug page
 */
static void draw_page_gps(void)
{
    gps_data_t gps;
    char buf[48];

    uint16_t y = CONTENT_START_Y;

    draw_string(100U, y, "GPS STATUS", 2U);
    y += 30U;

    gps_state_t state = gps_mgmt_get_state();
    const char *state_str;
    switch (state) {
    case GPS_STATE_OFF:
        state_str = "OFF";
        break;
    case GPS_STATE_INIT:
        state_str = "INIT";
        break;
    case GPS_STATE_ACQUIRING:
        state_str = "ACQUIRING";
        break;
    case GPS_STATE_FIX_2D:
        state_str = "FIX 2D";
        break;
    case GPS_STATE_FIX_3D:
        state_str = "FIX 3D";
        break;
    case GPS_STATE_STANDBY:
        state_str = "STANDBY";
        break;
    default:
        state_str = "UNKNOWN";
        break;
    }

    (void)snprintf(buf, sizeof(buf), "State: %s", state_str);
    draw_string(20U, y, buf, 1U);
    y += 16U;

    if (gps_mgmt_get_data(&gps) == APP_OK) {
        /* Satellites */
        (void)snprintf(buf, sizeof(buf), "Satellites: %u", gps.satellites);
        draw_string(20U, y, buf, 1U);
        y += 16U;

        /* HDOP */
        (void)snprintf(buf, sizeof(buf), "HDOP: %.1f", (double)gps.hdop);
        draw_string(20U, y, buf, 1U);
        y += 16U;

        if (gps.fix_valid) {
            /* Latitude */
            (void)snprintf(buf, sizeof(buf), "Lat:  %.6f", (double)gps.location.lat);
            draw_string(20U, y, buf, 1U);
            y += 16U;

            /* Longitude */
            (void)snprintf(buf, sizeof(buf), "Lon:  %.6f", (double)gps.location.lon);
            draw_string(20U, y, buf, 1U);
            y += 16U;

            /* Altitude */
            (void)snprintf(buf, sizeof(buf), "Alt:  %.1f m", (double)gps.location.alt);
            draw_string(20U, y, buf, 1U);
            y += 16U;

            /* Speed */
            (void)snprintf(buf, sizeof(buf), "Speed: %.1f km/h", (double)gps.location.speed);
            draw_string(20U, y, buf, 1U);
            y += 16U;

            /* Course */
            (void)snprintf(buf, sizeof(buf), "Course: %.1f", (double)gps.location.course);
            draw_string(20U, y, buf, 1U);
        } else {
            draw_string(60U, y + 20U, "No valid fix", 2U);
        }
    } else {
        draw_string(60U, y + 20U, "GPS data unavailable", 1U);
    }
}

/**
 * @brief Draw line using Bresenham's algorithm
 */
static void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    int16_t dx = (int16_t)((x1 > x0) ? (x1 - x0) : (x0 - x1));
    int16_t dy = (int16_t)((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t x = (int16_t)x0;
    int16_t y = (int16_t)y0;

    while (true) {
        if ((x >= 0) && (x < (int16_t)LS027_WIDTH) &&
            (y >= 0) && (y < (int16_t)LS027_HEIGHT)) {
            ls027_draw_pixel((uint16_t)x, (uint16_t)y, LS027_COLOR_BLACK);
        }

        if ((x == (int16_t)x1) && (y == (int16_t)y1)) {
            break;
        }

        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

/**
 * @brief Draw a simple filled circle (for map markers)
 */
static void draw_filled_circle(uint16_t cx, uint16_t cy, uint16_t r)
{
    for (int16_t y = -(int16_t)r; y <= (int16_t)r; y++) {
        for (int16_t x = -(int16_t)r; x <= (int16_t)r; x++) {
            if ((x * x + y * y) <= (int16_t)(r * r)) {
                uint16_t px = (uint16_t)((int16_t)cx + x);
                uint16_t py = (uint16_t)((int16_t)cy + y);
                if ((px < LS027_WIDTH) && (py < LS027_HEIGHT)) {
                    ls027_draw_pixel(px, py, LS027_COLOR_BLACK);
                }
            }
        }
    }
}

/**
 * @brief Draw a circle outline
 */
static void draw_circle(uint16_t cx, uint16_t cy, uint16_t r)
{
    int16_t x = (int16_t)r;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        ls027_draw_pixel(cx + (uint16_t)x, cy + (uint16_t)y, LS027_COLOR_BLACK);
        ls027_draw_pixel(cx + (uint16_t)y, cy + (uint16_t)x, LS027_COLOR_BLACK);
        ls027_draw_pixel(cx - (uint16_t)x, cy + (uint16_t)y, LS027_COLOR_BLACK);
        ls027_draw_pixel(cx - (uint16_t)y, cy + (uint16_t)x, LS027_COLOR_BLACK);
        ls027_draw_pixel(cx - (uint16_t)x, cy - (uint16_t)y, LS027_COLOR_BLACK);
        ls027_draw_pixel(cx - (uint16_t)y, cy - (uint16_t)x, LS027_COLOR_BLACK);
        ls027_draw_pixel(cx + (uint16_t)x, cy - (uint16_t)y, LS027_COLOR_BLACK);
        ls027_draw_pixel(cx + (uint16_t)y, cy - (uint16_t)x, LS027_COLOR_BLACK);

        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

/**
 * @brief Draw compass arrow pointing in direction
 */
static void draw_compass_arrow(uint16_t cx, uint16_t cy, uint16_t len, float bearing_deg)
{
    /* Convert bearing to radians, adjust for screen coords (0=up, CW positive) */
    float rad = (bearing_deg - 90.0f) * 3.14159265f / 180.0f;

    /* Arrow tip */
    int16_t tip_x = (int16_t)cx + (int16_t)((float)len * cosf(rad));
    int16_t tip_y = (int16_t)cy + (int16_t)((float)len * sinf(rad));

    /* Arrow base (opposite direction) */
    int16_t base_x = (int16_t)cx - (int16_t)(((float)len / 3.0f) * cosf(rad));
    int16_t base_y = (int16_t)cy - (int16_t)(((float)len / 3.0f) * sinf(rad));

    /* Draw arrow line */
    draw_line((uint16_t)base_x, (uint16_t)base_y,
              (uint16_t)tip_x, (uint16_t)tip_y);

    /* Draw arrowhead */
    float head_angle = 0.5f;  /* radians offset for head */
    int16_t h1_x = tip_x - (int16_t)(10.0f * cosf(rad - head_angle));
    int16_t h1_y = tip_y - (int16_t)(10.0f * sinf(rad - head_angle));
    int16_t h2_x = tip_x - (int16_t)(10.0f * cosf(rad + head_angle));
    int16_t h2_y = tip_y - (int16_t)(10.0f * sinf(rad + head_angle));

    draw_line((uint16_t)tip_x, (uint16_t)tip_y, (uint16_t)h1_x, (uint16_t)h1_y);
    draw_line((uint16_t)tip_x, (uint16_t)tip_y, (uint16_t)h2_x, (uint16_t)h2_y);
}

/**
 * @brief Draw map page with position and nearby info
 */
static void draw_page_map(void)
{
    attitude_t att;
    char buf[32];

    uint16_t y = CONTENT_START_Y;

    if (attitude_get(&att) != APP_OK) {
        draw_string(80U, 100U, "No position data", 1U);
        return;
    }

    /* Map area dimensions */
    uint16_t map_cx = LS027_WIDTH / 2U;
    uint16_t map_cy = 110U;
    uint16_t map_w = 280U;
    uint16_t map_h = 140U;
    uint16_t map_radius = 70U;

    /* Compute zoom based on current position */
    float h_zoom, v_zoom;
    float seg_dist = segment_get_nearest_distance();
    compute_zoom(att.loc.lat, (seg_dist >= 0.0f) ? seg_dist : 100.0f, &h_zoom, &v_zoom);

    /* Draw map boundary */
    draw_rect(map_cx - map_w/2U, map_cy - map_h/2U, map_w, map_h);

    /* Draw parcours/route if loaded */
    if (parcours_is_loaded()) {
        int16_t prev_x = 0, prev_y = 0;
        bool prev_valid = false;
        uint16_t total_points = parcours_get_num_points();
        uint16_t step = (total_points > 100U) ? (total_points / 100U) : 1U;

        for (uint16_t i = 0U; i < total_points; i += step) {
            const point_t *point = parcours_get_point(i);
            if (point != NULL) {
                int16_t sx, sy;
                bool valid = gps_to_screen(point->lat, point->lon,
                                          att.loc.lat, att.loc.lon,
                                          h_zoom, v_zoom,
                                          &sx, &sy,
                                          map_cx, map_cy, map_w, map_h);

                if (valid && prev_valid) {
                    draw_line((uint16_t)prev_x, (uint16_t)prev_y,
                             (uint16_t)sx, (uint16_t)sy);
                }

                prev_x = sx;
                prev_y = sy;
                prev_valid = valid;
            }
        }
    }

    /* Draw current position marker (center) */
    draw_filled_circle(map_cx, map_cy, 5U);

    /* Draw course/heading arrow if moving */
    if (att.loc.speed > 2.0f) {
        /* Rotate north indicator based on heading */
        int16_t north_x, north_y;
        rotate_point(-att.loc.course, (int16_t)map_cx, (int16_t)map_cy,
                    (int16_t)map_cx, (int16_t)(map_cy - 40),
                    &north_x, &north_y);

        /* Draw direction arrow */
        draw_compass_arrow(map_cx, map_cy, 35U, att.loc.course);

        /* Rotated north indicator */
        if ((north_x >= 0) && (north_x < (int16_t)LS027_WIDTH) &&
            (north_y >= 0) && (north_y < (int16_t)LS027_HEIGHT)) {
            draw_string((uint16_t)north_x - 3U, (uint16_t)north_y - 4U, "N", 1U);
        }
    } else {
        /* Static cardinal directions */
        draw_string(map_cx - 3U, map_cy - map_radius - 5U, "N", 1U);
        draw_string(map_cx - 3U, map_cy + map_radius - 2U, "S", 1U);
        draw_string(map_cx - map_radius - 8U, map_cy - 4U, "W", 1U);
        draw_string(map_cx + map_radius + 2U, map_cy - 4U, "E", 1U);
    }

    /* Zoom indicator */
    (void)snprintf(buf, sizeof(buf), "%.0fm", (double)zoom.last_zoom);
    draw_string(map_cx - map_w/2U + 5U, map_cy - map_h/2U + 5U, buf, 1U);

    /* Info below map */
    y = map_cy + map_h/2U + 8U;

    /* Coordinates */
    (void)snprintf(buf, sizeof(buf), "%.5f, %.5f",
                   (double)att.loc.lat, (double)att.loc.lon);
    draw_string(60U, y, buf, 1U);
    y += 14U;

    /* Altitude and Speed */
    (void)snprintf(buf, sizeof(buf), "Alt: %.0f m", (double)att.loc.alt);
    draw_string(20U, y, buf, 1U);

    (void)snprintf(buf, sizeof(buf), "%.1f km/h", (double)att.loc.speed);
    draw_string(200U, y, buf, 1U);
    y += 14U;

    /* Course and Zoom level */
    (void)snprintf(buf, sizeof(buf), "Hdg: %.0f", (double)att.loc.course);
    draw_string(20U, y, buf, 1U);

    /* Distance to route or nearest segment */
    if (parcours_is_active()) {
        nav_info_t nav;
        if (parcours_get_nav_info(&nav) == APP_OK) {
            (void)snprintf(buf, sizeof(buf), "Rte: %.0fm", (double)nav.dist_to_route);
            draw_string(180U, y, buf, 1U);
        }
    } else if (seg_dist >= 0.0f) {
        (void)snprintf(buf, sizeof(buf), "Seg: %.0fm", (double)seg_dist);
        draw_string(180U, y, buf, 1U);
    }

    /* Instructions */
    y += 16U;
    draw_string(60U, y, "L/R: Zoom  C: Reset", 1U);
}

/**
 * @brief Draw a sensor status indicator box
 */
static void draw_sensor_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            const char *name, bool connected, const char *value)
{
    /* Draw box */
    draw_rect(x, y, w, h);

    /* Connection indicator (filled circle if connected, empty if not) */
    uint16_t ind_x = x + 8U;
    uint16_t ind_y = y + (h / 2U);
    if (connected) {
        draw_filled_circle(ind_x, ind_y, 4U);
    } else {
        draw_circle(ind_x, ind_y, 4U);
    }

    /* Sensor name */
    draw_string(x + 20U, y + 4U, name, 1U);

    /* Value or status */
    if (connected) {
        draw_string(x + 20U, y + 18U, value, 2U);
    } else {
        draw_string(x + 20U, y + 18U, "--", 2U);
    }
}

/**
 * @brief Draw sensors status page
 */
static void draw_page_sensors(void)
{
    char buf[32];
    uint16_t y = CONTENT_START_Y;

    draw_string(100U, y, "SENSORS", 2U);
    y += 30U;

    /* Box dimensions */
    uint16_t box_w = 180U;
    uint16_t box_h = 45U;
    uint16_t col1_x = 10U;
    uint16_t col2_x = 200U;

    /* Heart Rate Monitor */
    bool hrs_conn = ble_hrs_client_is_connected();
    uint8_t bpm = ble_hrs_client_get_bpm();
    (void)snprintf(buf, sizeof(buf), "%u bpm", bpm);
    draw_sensor_box(col1_x, y, box_w, box_h, "Heart Rate", hrs_conn, buf);

    /* Speed/Cadence Sensor */
    bool bsc_conn = ble_bsc_client_is_connected();
    uint8_t cadence = ble_bsc_client_get_cadence();
    (void)snprintf(buf, sizeof(buf), "%u rpm", cadence);
    draw_sensor_box(col2_x, y, box_w, box_h, "Cadence", bsc_conn, buf);
    y += box_h + 10U;

    /* GPS Status */
    gps_state_t gps_state = gps_mgmt_get_state();
    bool gps_fix = (gps_state == GPS_STATE_FIX_2D) || (gps_state == GPS_STATE_FIX_3D);
    gps_data_t gps;
    if (gps_mgmt_get_data(&gps) == APP_OK) {
        (void)snprintf(buf, sizeof(buf), "%u sats", gps.satellites);
    } else {
        (void)snprintf(buf, sizeof(buf), "---");
    }
    draw_sensor_box(col1_x, y, box_w, box_h, "GPS", gps_fix, buf);

    /* Barometer (always connected if initialized) */
    attitude_ext_t att_ext;
    bool baro_ok = false;
    if (attitude_get_ext(&att_ext) == APP_OK) {
        baro_ok = (att_ext.pressure > 50000.0f);
        (void)snprintf(buf, sizeof(buf), "%.0f hPa", (double)(att_ext.pressure / 100.0f));
    } else {
        (void)snprintf(buf, sizeof(buf), "---");
    }
    draw_sensor_box(col2_x, y, box_w, box_h, "Barometer", baro_ok, buf);
    y += box_h + 10U;

    /* Battery */
    bool batt_ok = (att_ext.battery_soc > 0U);
    (void)snprintf(buf, sizeof(buf), "%u%%", att_ext.battery_soc);
    draw_sensor_box(col1_x, y, box_w, box_h, "Battery", batt_ok, buf);

    /* Temperature */
    bool temp_ok = (att_ext.temperature > -40.0f) && (att_ext.temperature < 85.0f);
    (void)snprintf(buf, sizeof(buf), "%.1f C", (double)att_ext.temperature);
    draw_sensor_box(col2_x, y, box_w, box_h, "Temperature", temp_ok, buf);
    y += box_h + 15U;

    /* Summary line */
    uint8_t connected_count = 0U;
    if (hrs_conn) { connected_count++; }
    if (bsc_conn) { connected_count++; }
    if (gps_fix) { connected_count++; }
    if (baro_ok) { connected_count++; }

    (void)snprintf(buf, sizeof(buf), "%u/4 sensors active", connected_count);
    draw_string(120U, y, buf, 1U);
}

/**
 * @brief Draw menu page
 */
static void draw_page_menu(void)
{
    const menu_state_t *mstate = menu_get_state();
    char buf[32];
    uint16_t y = CONTENT_START_Y;

    /* Title based on depth */
    if (mstate->depth == 0U) {
        draw_string(140U, y, "MENU", 2U);
    } else {
        draw_string(100U, y, "SETTINGS", 2U);
    }
    y += 28U;

    draw_hline(20U, y, LS027_WIDTH - 40U);
    y += 8U;

    if (mstate->current_menu == NULL) {
        draw_string(100U, y + 40U, "Menu not available", 1U);
        return;
    }

    /* Draw visible menu items */
    uint8_t visible_count = 5U;
    uint16_t item_height = 32U;

    for (uint8_t i = 0U; i < visible_count; i++) {
        uint8_t item_idx = mstate->scroll_offset + i;
        if (item_idx >= mstate->menu_count) {
            break;
        }

        const menu_item_t *item = &mstate->current_menu[item_idx];
        uint16_t item_y = y + (i * item_height);

        /* Selection highlight */
        if (item_idx == mstate->selected_index) {
            ls027_fill_rect(15U, item_y, LS027_WIDTH - 30U, item_height - 2U, LS027_COLOR_BLACK);
            /* Draw text inverted (white on black) - simplified */
            /* For now just draw indicator */
            draw_string(20U, item_y + 8U, ">", 2U);
        }

        /* Item name */
        uint16_t text_x = (item_idx == mstate->selected_index) ? 45U : 30U;
        draw_string(text_x, item_y + 8U, item->name, 1U);

        /* Type indicator / value */
        if (item->type == MENU_TYPE_SUBMENU) {
            draw_string(LS027_WIDTH - 50U, item_y + 8U, ">", 2U);
        } else if (item->type == MENU_TYPE_VALUE) {
            uint16_t val;
            if (mstate->editing_value && (item_idx == mstate->selected_index)) {
                val = menu_get_edit_value();
                /* Show editing indicator */
                (void)snprintf(buf, sizeof(buf), "[%u]", val);
            } else {
                /* Show current value */
                if (strstr(item->name, "FTP") != NULL) {
                    val = user_settings_get_ftp(user_settings_get_global());
                } else if (strstr(item->name, "Weight") != NULL) {
                    val = user_settings_get_weight(user_settings_get_global());
                    /* Convert hectograms to kg with decimal */
                    (void)snprintf(buf, sizeof(buf), "%.1f", (double)val / 10.0);
                } else {
                    val = 0U;
                    (void)snprintf(buf, sizeof(buf), "%u", val);
                }
                if (strstr(item->name, "Weight") == NULL) {
                    (void)snprintf(buf, sizeof(buf), "%u", val);
                }
            }
            draw_string(LS027_WIDTH - 80U, item_y + 8U, buf, 1U);
        }
    }

    /* Scroll indicators */
    if (mstate->scroll_offset > 0U) {
        draw_string(LS027_WIDTH / 2U - 5U, CONTENT_START_Y + 20U, "^", 1U);
    }
    if ((mstate->scroll_offset + visible_count) < mstate->menu_count) {
        draw_string(LS027_WIDTH / 2U - 5U, y + (visible_count * item_height), "v", 1U);
    }

    /* Instructions at bottom */
    y = LS027_HEIGHT - 20U;
    draw_string(20U, y, "L/R:Navigate  C:Select  Long:Back", 1U);
}

/**
 * @brief Draw debug info page
 */
static void draw_page_debug(void)
{
    char buf[48];
    uint16_t y = CONTENT_START_Y;

    draw_string(100U, y, "DEBUG INFO", 2U);
    y += 30U;

    /* Uptime */
    uint32_t uptime_ms = k_uptime_get_32();
    uint32_t secs = uptime_ms / 1000U;
    uint32_t mins = secs / 60U;
    uint32_t hours = mins / 60U;

    (void)snprintf(buf, sizeof(buf), "Uptime: %02u:%02u:%02u",
                   (unsigned)(hours % 100U),
                   (unsigned)(mins % 60U),
                   (unsigned)(secs % 60U));
    draw_string(20U, y, buf, 1U);
    y += 16U;

    /* Heap configured size */
    (void)snprintf(buf, sizeof(buf), "Heap: %u bytes configured",
                   (unsigned)CONFIG_HEAP_MEM_POOL_SIZE);
    draw_string(20U, y, buf, 1U);
    y += 16U;

    /* BLE state */
    draw_string(20U, y, "BLE:", 1U);
    draw_string(100U, y, "Active", 1U);
    y += 16U;

    /* Boucle state */
    boucle_state_t bstate = boucle_get_state();
    const char *bstate_str;
    switch (bstate) {
    case BOUCLE_STATE_IDLE:
        bstate_str = "IDLE";
        break;
    case BOUCLE_STATE_RUNNING:
        bstate_str = "RUNNING";
        break;
    case BOUCLE_STATE_PAUSED:
        bstate_str = "PAUSED";
        break;
    default:
        bstate_str = "UNKNOWN";
        break;
    }

    (void)snprintf(buf, sizeof(buf), "Boucle: %s", bstate_str);
    draw_string(20U, y, buf, 1U);
    y += 16U;

    /* Version */
    (void)snprintf(buf, sizeof(buf), "Version: %u.%u.%u",
                   APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH);
    draw_string(20U, y, buf, 1U);
}

/**
 * @brief Draw notification overlay
 */
static void draw_notification(void)
{
    if (!notif_active) {
        return;
    }

    /* Check timeout */
    uint32_t elapsed = k_uptime_get_32() - notif_start_time;
    if (elapsed >= (active_notif.duration * 1000U)) {
        notif_active = false;
        return;
    }

    /* Draw notification box */
    uint16_t box_w = 300U;
    uint16_t box_h = 60U;
    uint16_t box_x = (LS027_WIDTH - box_w) / 2U;
    uint16_t box_y = 80U;

    /* Clear area */
    ls027_fill_rect(box_x, box_y, box_w, box_h, LS027_COLOR_WHITE);

    /* Draw border */
    draw_rect(box_x, box_y, box_w, box_h);

    /* Draw title */
    draw_string(box_x + 10U, box_y + 8U, active_notif.title, 2U);

    /* Draw message */
    draw_string(box_x + 10U, box_y + 32U, active_notif.message, 1U);
}

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

app_err_t vue_init(void)
{
    if (is_initialized) {
        return APP_ERR_ALREADY_INIT;
    }

    /* Initialize LCD driver */
    app_err_t err = ls027_init();
    if ((err != APP_OK) && (err != APP_ERR_ALREADY_INIT)) {
        LOG_ERR("LCD init failed: %d", err);
        return err;
    }

    /* Clear display */
    ls027_clear();

    current_mode = VUE_MODE_CRS;
    current_page = VUE_PAGE_MAIN;
    notif_active = false;

    is_initialized = true;
    LOG_INF("Vue initialized");

    return APP_OK;
}

void vue_update(void)
{
    if (!is_initialized) {
        return;
    }

    /* Clear buffer */
    ls027_clear();

    /* Draw header */
    draw_header();

    /* Draw current page */
    switch (current_page) {
    case VUE_PAGE_MAIN:
        draw_page_main();
        break;
    case VUE_PAGE_SEGMENT:
        draw_page_segment();
        break;
    case VUE_PAGE_PARCOURS:
        draw_page_parcours();
        break;
    case VUE_PAGE_STATS:
        draw_page_stats();
        break;
    case VUE_PAGE_GPS:
        draw_page_gps();
        break;
    case VUE_PAGE_DEBUG:
        draw_page_debug();
        break;
    case VUE_PAGE_MAP:
        draw_page_map();
        break;
    case VUE_PAGE_SENSORS:
        draw_page_sensors();
        break;
    case VUE_PAGE_MENU:
        draw_page_menu();
        break;
    default:
        draw_string(100U, 100U, "Unknown page", 1U);
        break;
    }

    /* Draw notification if active */
    draw_notification();

    /* Update display */
    (void)ls027_update();
}

void vue_refresh(void)
{
    vue_update();
}

void vue_set_mode(vue_mode_t mode)
{
    current_mode = mode;
}

vue_mode_t vue_get_mode(void)
{
    return current_mode;
}

void vue_next_page(void)
{
    current_page = (vue_page_t)(((uint8_t)current_page + 1U) % (uint8_t)VUE_PAGE_COUNT);
}

void vue_prev_page(void)
{
    if (current_page == VUE_PAGE_MAIN) {
        current_page = (vue_page_t)((uint8_t)VUE_PAGE_COUNT - 1U);
    } else {
        current_page = (vue_page_t)((uint8_t)current_page - 1U);
    }
}

void vue_set_page(vue_page_t page)
{
    if (page < VUE_PAGE_COUNT) {
        current_page = page;
    }
}

vue_page_t vue_get_page(void)
{
    return current_page;
}

void vue_handle_button(btn_event_t event)
{
    if (!is_initialized) {
        return;
    }

    switch (event) {
    case BTN_EVENT_LEFT:
        vue_prev_page();
        break;

    case BTN_EVENT_RIGHT:
        vue_next_page();
        break;

    case BTN_EVENT_CENTER:
    case BTN_EVENT_LONG_LEFT:
    case BTN_EVENT_LONG_CENTER:
    case BTN_EVENT_LONG_RIGHT:
        /* Handled by boucle */
        break;

    default:
        break;
    }
}

void vue_show_notification(const notification_t *notif)
{
    if ((notif == NULL) || !is_initialized) {
        return;
    }

    active_notif = *notif;
    notif_active = true;
    notif_start_time = k_uptime_get_32();
}

void vue_clear_notification(void)
{
    notif_active = false;
}

void vue_toggle_backlight(void)
{
    /* LS027 is reflective, no backlight */
}

void vue_set_brightness(uint8_t level)
{
    /* LS027 is reflective, no brightness control */
    (void)level;
}
