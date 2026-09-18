/**
 * @file vue_fec.c
 * @brief FE-C (Indoor trainer) mode display implementation
 *
 * Displays indoor trainer data including power, cadence, heart rate,
 * power zones, and performance metrics.
 *
 * Follows MISRA C:2012 guidelines.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vue/vue.h"
#include "vue/vue_fec.h"
#include "vue/font_5x7.h"
#include "drivers/ls027.h"
#include "model/boucle.h"
#include "model/attitude.h"
#include "model/power_zone.h"
#include "model/suffer_score.h"
#include "model/user_settings.h"
#include "rf/ble_fec_client.h"
#include "rf/ble_hrs_client.h"

LOG_MODULE_REGISTER(vue_fec, CONFIG_LOG_DEFAULT_LEVEL);

/* ==========================================================================
 * Private Definitions
 * ========================================================================== */

/** Screen layout constants */
#define FEC_HEADER_Y            0U
#define FEC_HEADER_HEIGHT       24U
#define FEC_CONTENT_Y           26U

/** Power zone bar dimensions */
#define ZONE_BAR_X              10U
#define ZONE_BAR_WIDTH          220U
#define ZONE_BAR_HEIGHT         16U

/** Cadran (dial/gauge) dimensions */
#define CADRAN_WIDTH            180U
#define CADRAN_HEIGHT           50U
#define CADRAN_LABEL_OFFSET     4U
#define CADRAN_VALUE_OFFSET     18U
#define CADRAN_UNIT_OFFSET      38U

/** Power vector polar display */
#define POLAR_CENTER_X          300U
#define POLAR_CENTER_Y          140U
#define POLAR_RADIUS            60U

/* ==========================================================================
 * Private Variables
 * ========================================================================== */

/** Power history for smoothing (5 second average) */
static uint16_t power_history[5];
static uint8_t power_history_idx;

/** FEC page state - use values from vue_fec.h */
#define FEC_PAGE_MAIN       VUE_FEC_PAGE_MAIN
#define FEC_PAGE_ZONES      VUE_FEC_PAGE_ZONES
#define FEC_PAGE_DETAILS    VUE_FEC_PAGE_DETAILS

static vue_fec_page_t current_fec_page = FEC_PAGE_MAIN;

/* ==========================================================================
 * Private Functions - Drawing Helpers
 * ========================================================================== */

/**
 * @brief Draw a single character using 5x7 font
 */
static void fec_draw_char(uint16_t x, uint16_t y, char c, uint8_t size)
{
    uint8_t char_idx = (uint8_t)c;

    if ((x >= LS027_WIDTH) || (y >= LS027_HEIGHT)) {
        return;
    }

    for (uint8_t col = 0U; col < FONT_5X7_GLYPH_WIDTH; col++) {
        uint8_t col_data = font_5x7[char_idx][col];

        for (uint8_t row = 0U; row < FONT_5X7_GLYPH_HEIGHT; row++) {
            if ((col_data & (1U << row)) != 0U) {
                if (size == 1U) {
                    uint16_t px = x + col;
                    uint16_t py = y + row;
                    if ((px < LS027_WIDTH) && (py < LS027_HEIGHT)) {
                        ls027_draw_pixel(px, py, LS027_COLOR_BLACK);
                    }
                } else {
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
 * @brief Draw string
 */
static void fec_draw_string(uint16_t x, uint16_t y, const char *str, uint8_t size)
{
    if (str == NULL) {
        return;
    }

    uint16_t char_w = FONT_5X7_CHAR_WIDTH * size;
    uint16_t px = x;

    while (*str != '\0') {
        if (px >= LS027_WIDTH) {
            break;
        }
        fec_draw_char(px, y, *str, size);
        px += char_w;
        str++;
    }
}

/**
 * @brief Draw string centered
 */
static void fec_draw_string_centered(uint16_t y, const char *str, uint8_t size)
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

    fec_draw_string(x, y, str, size);
}

/**
 * @brief Draw horizontal line
 */
static void fec_draw_hline(uint16_t x, uint16_t y, uint16_t len)
{
    ls027_draw_hline(x, y, len, LS027_COLOR_BLACK);
}

/**
 * @brief Draw vertical line
 */
static void fec_draw_vline(uint16_t x, uint16_t y, uint16_t len)
{
    ls027_draw_vline(x, y, len, LS027_COLOR_BLACK);
}

/**
 * @brief Draw rectangle outline
 */
static void fec_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    fec_draw_hline(x, y, w);
    fec_draw_hline(x, y + h - 1U, w);
    fec_draw_vline(x, y, h);
    fec_draw_vline(x + w - 1U, y, h);
}

/**
 * @brief Draw filled rectangle
 */
static void fec_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    ls027_fill_rect(x, y, w, h, LS027_COLOR_BLACK);
}

/* ==========================================================================
 * Private Functions - Cadran (Gauge/Dial) Display
 * ========================================================================== */

/**
 * @brief Draw a cadran (gauge) display with label, value, and unit
 * @param x X position
 * @param y Y position
 * @param label Label text (e.g., "Power")
 * @param value Numeric value
 * @param unit Unit text (e.g., "W")
 * @param width Display width
 */
static void draw_cadran(uint16_t x, uint16_t y, const char *label,
                        int32_t value, const char *unit, uint16_t width)
{
    char buf[16];

    /* Draw box outline */
    fec_draw_rect(x, y, width, CADRAN_HEIGHT);

    /* Draw label (small, top-left) */
    fec_draw_string(x + CADRAN_LABEL_OFFSET, y + 4U, label, 1U);

    /* Draw separator line */
    fec_draw_hline(x, y + 14U, width);

    /* Draw value (large, centered) */
    (void)snprintf(buf, sizeof(buf), "%d", (int)value);
    uint16_t val_len = (uint16_t)strlen(buf);
    uint16_t val_w = val_len * 12U;  /* Size 2 */
    uint16_t val_x = x + ((width - val_w) / 2U);
    fec_draw_string(val_x, y + CADRAN_VALUE_OFFSET, buf, 2U);

    /* Draw unit (small, bottom-right) */
    uint16_t unit_len = (uint16_t)strlen(unit);
    uint16_t unit_x = x + width - (unit_len * 6U) - 4U;
    fec_draw_string(unit_x, y + CADRAN_UNIT_OFFSET, unit, 1U);
}

/**
 * @brief Draw horizontal cadran (label left, value right)
 */
__attribute__((unused))
static void draw_cadran_h(uint16_t x, uint16_t y, const char *label,
                          int32_t value, const char *unit, uint16_t width)
{
    char buf[24];

    /* Draw label (left) */
    fec_draw_string(x, y, label, 1U);

    /* Draw value with unit (right-aligned) */
    (void)snprintf(buf, sizeof(buf), "%d %s", (int)value, unit);
    uint16_t val_len = (uint16_t)strlen(buf);
    uint16_t val_x = x + width - (val_len * 12U);
    fec_draw_string(val_x, y, buf, 2U);
}

/* ==========================================================================
 * Private Functions - Power Zone Display
 * ========================================================================== */

/**
 * @brief Get power zone color pattern (for future use with dithering)
 * @param zone Zone number (1-7)
 * @return Fill pattern (0=empty, 1=25%, 2=50%, 3=75%, 4=full)
 */
__attribute__((unused))
static uint8_t get_zone_pattern(uint8_t zone)
{
    /* Zones: 1=Recovery, 2=Endurance, 3=Tempo, 4=Threshold,
     *        5=VO2max, 6=Anaerobic, 7=Neuromuscular */
    switch (zone) {
    case 1U:
        return 1U;  /* Light */
    case 2U:
        return 1U;
    case 3U:
        return 2U;  /* Medium-light */
    case 4U:
        return 2U;
    case 5U:
        return 3U;  /* Medium-dark */
    case 6U:
        return 3U;
    case 7U:
        return 4U;  /* Full */
    default:
        return 0U;
    }
}

/**
 * @brief Draw power zone bar indicator
 * @param x X position
 * @param y Y position
 * @param power Current power
 * @param ftp Functional Threshold Power
 */
static void draw_power_zone_bar(uint16_t x, uint16_t y, uint16_t power, uint16_t ftp)
{
    if (ftp == 0U) {
        ftp = 200U;  /* Default FTP */
    }

    /* Calculate zone boundaries as percentage of FTP */
    const uint16_t zone_pct[8] = { 0, 55, 75, 90, 105, 120, 150, 200 };

    /* Draw zone boxes */
    uint16_t box_w = ZONE_BAR_WIDTH / 7U;

    for (uint8_t z = 0U; z < 7U; z++) {
        uint16_t bx = x + (z * box_w);

        /* Draw box outline */
        fec_draw_rect(bx, y, box_w, ZONE_BAR_HEIGHT);

        /* Check if current power is in this zone */
        uint16_t pct = (power * 100U) / ftp;
        if ((pct >= zone_pct[z]) && (pct < zone_pct[z + 1U])) {
            /* Fill this zone */
            fec_fill_rect(bx + 1U, y + 1U, box_w - 2U, ZONE_BAR_HEIGHT - 2U);
        }

        /* Draw zone number */
        char zn[2] = { (char)('1' + z), '\0' };
        fec_draw_string(bx + (box_w / 2U) - 3U, y + 4U, zn, 1U);
    }

    /* Draw current % of FTP below */
    char buf[16];
    uint16_t pct = (power * 100U) / ftp;
    (void)snprintf(buf, sizeof(buf), "%u%% FTP", pct);
    fec_draw_string(x, y + ZONE_BAR_HEIGHT + 2U, buf, 1U);
}

/**
 * @brief Draw power zones histogram (time in zone)
 * @param x X position
 * @param y Y position
 */
static void draw_zones_histogram(uint16_t x, uint16_t y)
{
    uint16_t bar_w = 30U;
    uint16_t bar_max_h = 80U;
    uint16_t spacing = 4U;

    /* Get time in each zone (placeholder - would come from power_zone module) */
    uint32_t zone_time[7] = { 120, 300, 450, 200, 100, 50, 20 };  /* Example */
    uint32_t max_time = 1U;

    for (uint8_t i = 0U; i < 7U; i++) {
        if (zone_time[i] > max_time) {
            max_time = zone_time[i];
        }
    }

    /* Draw bars */
    for (uint8_t z = 0U; z < 7U; z++) {
        uint16_t bx = x + (z * (bar_w + spacing));
        uint16_t bh = (uint16_t)((zone_time[z] * bar_max_h) / max_time);

        if (bh < 2U) {
            bh = 2U;
        }

        /* Draw bar (from bottom up) */
        uint16_t by = y + bar_max_h - bh;
        fec_fill_rect(bx, by, bar_w, bh);

        /* Draw zone label below */
        char zn[2] = { (char)('1' + z), '\0' };
        fec_draw_string(bx + (bar_w / 2U) - 3U, y + bar_max_h + 4U, zn, 1U);

        /* Draw time above bar */
        char tbuf[12];
        uint32_t mins = zone_time[z] / 60U;
        if (mins > 0U) {
            (void)snprintf(tbuf, sizeof(tbuf), "%um", (unsigned)mins);
        } else {
            (void)snprintf(tbuf, sizeof(tbuf), "%us", (unsigned)zone_time[z]);
        }
        fec_draw_string(bx, by - 10U, tbuf, 1U);
    }
}

/* ==========================================================================
 * Private Functions - Page Rendering
 * ========================================================================== */

/**
 * @brief Draw FEC header
 */
static void draw_fec_header(void)
{
    /* Title */
    fec_draw_string_centered(4U, "INDOOR TRAINER", 1U);

    /* Connection status */
    if (ble_fec_client_is_connected()) {
        fec_draw_string(LS027_WIDTH - 60U, 4U, "LINKED", 1U);
    } else {
        fec_draw_string(LS027_WIDTH - 78U, 4U, "SCANNING", 1U);
    }

    /* Separator */
    fec_draw_hline(0U, FEC_HEADER_HEIGHT, LS027_WIDTH);
}

/**
 * @brief Draw main FEC metrics page
 */
static void draw_fec_page_main(void)
{
    fec_data_t fec;
    char buf[32];
    uint16_t y = FEC_CONTENT_Y;

    /* Get FEC data */
    if (ble_fec_client_get_data(&fec) != APP_OK) {
        (void)memset(&fec, 0, sizeof(fec));
    }

    /* Get heart rate */
    uint8_t hr = ble_hrs_client_get_bpm();

    /* Get user FTP */
    const user_settings_t *settings = user_settings_get_global();
    uint16_t ftp = user_settings_get_ftp(settings);

    /* ===== POWER (large display) ===== */
    fec_draw_string(10U, y, "POWER", 1U);
    y += 10U;

    (void)snprintf(buf, sizeof(buf), "%u", fec.power);
    fec_draw_string(20U, y, buf, 4U);  /* Extra large */
    fec_draw_string(120U, y + 16U, "W", 2U);
    y += 50U;

    /* ===== Power Zone Bar ===== */
    draw_power_zone_bar(10U, y, fec.power, ftp);
    y += 30U;

    /* ===== Row of cadrans ===== */
    uint16_t cadran_w = 100U;
    uint16_t cadran_x1 = 10U;
    uint16_t cadran_x2 = 120U;
    uint16_t cadran_x3 = 230U;

    /* Cadence */
    draw_cadran(cadran_x1, y, "Cadence", fec.cadence, "rpm", cadran_w);

    /* Heart Rate */
    draw_cadran(cadran_x2, y, "Heart Rate", hr, "bpm", cadran_w);

    /* Speed */
    uint16_t speed_kmh = fec.speed / 100U;  /* Convert from 0.01 km/h */
    draw_cadran(cadran_x3, y, "Speed", speed_kmh, "km/h", cadran_w);
    y += CADRAN_HEIGHT + 10U;

    /* ===== Second row ===== */
    /* Elapsed time */
    uint32_t el_sec = fec.elapsed_time;
    uint8_t hrs = (uint8_t)(el_sec / 3600U);
    uint8_t mins = (uint8_t)((el_sec % 3600U) / 60U);
    uint8_t secs = (uint8_t)(el_sec % 60U);

    fec_draw_string(cadran_x1, y, "Time:", 1U);
    (void)snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hrs, mins, secs);
    fec_draw_string(cadran_x1 + 40U, y, buf, 2U);

    /* Grade (if in simulation mode) */
    if (fec.mode == FEC_MODE_SIMULATION) {
        fec_draw_string(cadran_x2 + 40U, y, "Grade:", 1U);
        (void)snprintf(buf, sizeof(buf), "%d%%", fec.grade);
        fec_draw_string(cadran_x2 + 90U, y, buf, 2U);
    }

    /* Target power (if in ERG mode) */
    if (fec.mode == FEC_MODE_ERG) {
        fec_draw_string(cadran_x2 + 40U, y, "Target:", 1U);
        (void)snprintf(buf, sizeof(buf), "%uW", fec.target_power);
        fec_draw_string(cadran_x2 + 90U, y, buf, 2U);
    }

    y += 24U;

    /* ===== Suffer Score ===== */
    suffer_score_t ss;
    if (suffer_score_get(&ss) == APP_OK) {
        fec_draw_string(cadran_x1, y, "Suffer:", 1U);
        (void)snprintf(buf, sizeof(buf), "%.0f", (double)ss.score);
        fec_draw_string(cadran_x1 + 50U, y, buf, 2U);
    }

    /* Average power */
    attitude_t att;
    if (attitude_get(&att) == APP_OK) {
        fec_draw_string(cadran_x2, y, "Avg Pwr:", 1U);
        (void)snprintf(buf, sizeof(buf), "%uW", att.pwr);
        fec_draw_string(cadran_x2 + 60U, y, buf, 2U);
    }
}

/**
 * @brief Draw power zones page
 */
static void draw_fec_page_zones(void)
{
    uint16_t y = FEC_CONTENT_Y;

    fec_draw_string_centered(y, "POWER ZONES", 2U);
    y += 30U;

    /* Draw histogram */
    draw_zones_histogram(20U, y);
    y += 110U;

    /* Zone legend */
    fec_draw_string(10U, y, "Z1:Rec Z2:End Z3:Tempo Z4:Thresh", 1U);
    y += 12U;
    fec_draw_string(10U, y, "Z5:VO2max Z6:Anaerobic Z7:NM", 1U);
}

/**
 * @brief Draw detailed metrics page
 */
static void draw_fec_page_details(void)
{
    fec_data_t fec;
    char buf[32];
    uint16_t y = FEC_CONTENT_Y;

    if (ble_fec_client_get_data(&fec) != APP_OK) {
        (void)memset(&fec, 0, sizeof(fec));
    }

    fec_draw_string_centered(y, "FEC DETAILS", 2U);
    y += 30U;

    /* Status */
    const char *status_str;
    switch (fec.status) {
    case TRAINER_STATUS_READY:
        status_str = "Ready";
        break;
    case TRAINER_STATUS_IN_USE:
        status_str = "In Use";
        break;
    case TRAINER_STATUS_PAUSED:
        status_str = "Paused";
        break;
    default:
        status_str = "Unknown";
        break;
    }
    fec_draw_string(10U, y, "Status:", 1U);
    fec_draw_string(80U, y, status_str, 1U);
    y += 16U;

    /* Mode */
    const char *mode_str;
    switch (fec.mode) {
    case FEC_MODE_SIMULATION:
        mode_str = "Simulation";
        break;
    case FEC_MODE_ERG:
        mode_str = "ERG";
        break;
    case FEC_MODE_RESISTANCE:
        mode_str = "Resistance";
        break;
    default:
        mode_str = "Unknown";
        break;
    }
    fec_draw_string(10U, y, "Mode:", 1U);
    fec_draw_string(80U, y, mode_str, 1U);
    y += 16U;

    /* Power details */
    fec_draw_string(10U, y, "Instant Power:", 1U);
    (void)snprintf(buf, sizeof(buf), "%u W", fec.power);
    fec_draw_string(120U, y, buf, 1U);
    y += 16U;

    /* 5-second average */
    power_history[power_history_idx] = fec.power;
    power_history_idx = (power_history_idx + 1U) % 5U;

    uint32_t avg = 0U;
    for (uint8_t i = 0U; i < 5U; i++) {
        avg += power_history[i];
    }
    avg /= 5U;

    fec_draw_string(10U, y, "5s Avg Power:", 1U);
    (void)snprintf(buf, sizeof(buf), "%u W", (unsigned)avg);
    fec_draw_string(120U, y, buf, 1U);
    y += 16U;

    /* Cadence */
    fec_draw_string(10U, y, "Cadence:", 1U);
    (void)snprintf(buf, sizeof(buf), "%u RPM", fec.cadence);
    fec_draw_string(120U, y, buf, 1U);
    y += 16U;

    /* Speed */
    fec_draw_string(10U, y, "Speed:", 1U);
    (void)snprintf(buf, sizeof(buf), "%u.%02u km/h",
                   fec.speed / 100U, fec.speed % 100U);
    fec_draw_string(120U, y, buf, 1U);
    y += 16U;

    /* Grade/Target */
    if (fec.mode == FEC_MODE_SIMULATION) {
        fec_draw_string(10U, y, "Grade:", 1U);
        (void)snprintf(buf, sizeof(buf), "%d %%", fec.grade);
        fec_draw_string(120U, y, buf, 1U);
    } else if (fec.mode == FEC_MODE_ERG) {
        fec_draw_string(10U, y, "Target Power:", 1U);
        (void)snprintf(buf, sizeof(buf), "%u W", fec.target_power);
        fec_draw_string(120U, y, buf, 1U);
    } else {
        fec_draw_string(10U, y, "Resistance:", 1U);
        (void)snprintf(buf, sizeof(buf), "%u %%", fec.resistance);
        fec_draw_string(120U, y, buf, 1U);
    }
    y += 16U;

    /* Connection info */
    fec_draw_string(10U, y, "Connected:", 1U);
    fec_draw_string(120U, y, fec.connected ? "Yes" : "No", 1U);
}

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

/**
 * @brief Initialize FEC view
 */
void vue_fec_init(void)
{
    (void)memset(power_history, 0, sizeof(power_history));
    power_history_idx = 0U;
    current_fec_page = FEC_PAGE_MAIN;

    LOG_INF("FEC view initialized");
}

/**
 * @brief Render FEC display
 */
void vue_fec_render(void)
{
    /* Clear screen */
    ls027_clear();

    /* Draw header */
    draw_fec_header();

    /* Draw current page */
    switch (current_fec_page) {
    case FEC_PAGE_MAIN:
        draw_fec_page_main();
        break;

    case FEC_PAGE_ZONES:
        draw_fec_page_zones();
        break;

    case FEC_PAGE_DETAILS:
        draw_fec_page_details();
        break;

    default:
        draw_fec_page_main();
        break;
    }

    /* Page indicator at bottom */
    char page_ind[16];
    (void)snprintf(page_ind, sizeof(page_ind), "Page %u/3",
                   (unsigned)(current_fec_page + 1U));
    fec_draw_string(LS027_WIDTH / 2U - 24U, LS027_HEIGHT - 12U, page_ind, 1U);

    /* Update display */
    (void)ls027_update();
}

/**
 * @brief Handle FEC view button event
 * @param event Button event
 */
void vue_fec_handle_button(btn_event_t event)
{
    switch (event) {
    case BTN_EVENT_LEFT:
        /* Previous page */
        if (current_fec_page == FEC_PAGE_MAIN) {
            current_fec_page = FEC_PAGE_DETAILS;
        } else {
            current_fec_page = (vue_fec_page_t)((uint8_t)current_fec_page - 1U);
        }
        break;

    case BTN_EVENT_RIGHT:
        /* Next page */
        current_fec_page = (vue_fec_page_t)(((uint8_t)current_fec_page + 1U) % 3U);
        break;

    default:
        /* Other buttons handled by main vue */
        break;
    }
}

/**
 * @brief Get current FEC page
 * @return Current page
 */
vue_fec_page_t vue_fec_get_page(void)
{
    return current_fec_page;
}

/**
 * @brief Set FEC page
 * @param page Page to display
 */
void vue_fec_set_page(vue_fec_page_t page)
{
    if (page <= FEC_PAGE_DETAILS) {
        current_fec_page = page;
    }
}
