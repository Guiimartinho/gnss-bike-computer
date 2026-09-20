/**
 * @file sd_logger.c
 * @brief SD Card position and sensor logging implementation
 *
 * Based on original Attitude.cpp sd_save_pos_buffer() implementation.
 *
 * Log file format (CSV):
 * timestamp,lat,lon,alt_gps,alt_baro,alt_filt,speed,power,bpm,cadence,slope,dist,climb
 */

#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>

#include "model/sd_logger.h"

LOG_MODULE_REGISTER(sd_logger, CONFIG_LOG_DEFAULT_LEVEL);

/* ==========================================================================
 * Private Definitions
 * ========================================================================== */

/* ==========================================================================
 * Private Functions
 * ========================================================================== */

int sd_logger_format_line(char *out, size_t size, const sd_log_entry_t *e)
{
    if ((out == NULL) || (e == NULL) || (size == 0U)) {
        return -1;
    }

    /*
     * The nineteen fields of `sd_save_pos_buffer`
     * (`legacy/source/sd/sd_functions.cpp:622-636`), with its formats: six
     * decimals in the position, two in the altitude, three in the angles,
     * none in the roughness of the accelerometer and one in the roughness
     * of the barometer. Every field ends with `;`, and the line with CRLF.
     */
    return snprintf(out, size,
                    "%f;%f;%.2f;%u;"
                    "%d;%u;%u;"
                    "%.3f;%.3f;%.3f;%.2f;"
                    "%.1f;%.2f;%.2f;%.2f;"
                    "%.0f;%.0f;%.0f;"
                    "%.1f;"
                    "\r\n",
                    (double)e->loc.lat, (double)e->loc.lon, (double)e->loc.alt,
                    (unsigned int)e->date.secj,
                    (int)e->sensors.power, (unsigned int)e->sensors.bpm,
                    (unsigned int)e->sensors.cadence,
                    (double)e->alti.alpha_bar, (double)e->alti.alpha_zero,
                    (double)e->alti.baro_alt, (double)e->alti.baro_corr,
                    (double)e->climb, (double)e->alti.filt_alt, (double)e->alti.gps_alt,
                    (double)e->alti.vit_asc,
                    (double)e->alti.rough[0], (double)e->alti.rough[1],
                    (double)e->alti.rough[2],
                    (double)e->alti.b_rough);
}

void sd_logger_filename(char *out, size_t size, uint32_t date)
{
    if ((out == NULL) || (size == 0U)) {
        return;
    }

    /*
     * `fname = HISTO_MARKER_CHAR + att->date.date + ".txt"` of the legacy
     * (`sd_functions.cpp:607-609`): the date goes out as the number it is,
     * so 5 September 2025 (50925) gives `@50925.txt`. A point without a
     * date would overwrite the file of the day zero, so it gets the uptime.
     */
    if (date == 0U) {
        (void)snprintf(out, size, "%s/%c%08X.txt", SD_LOG_DIRECTORY, SD_LOG_MARKER,
                       (unsigned int)(k_uptime_get_32() / 1000U));
    } else {
        (void)snprintf(out, size, "%s/%c%u.txt", SD_LOG_DIRECTORY, SD_LOG_MARKER,
                       (unsigned int)date);
    }
}

/**
 * @brief Name of the file of the day, as the legacy builds it
 */
static void generate_filename(char *filename, size_t size, const date_data_t *date)
{
    sd_logger_filename(filename, size, (date != NULL) ? date->date : 0U);
}

/**
 * @brief Write buffer to file
 */
static app_err_t write_buffer_to_file(sd_logger_t *logger)
{
    struct fs_file_t file;
    int ret;

    if (logger->buffer_count == 0U) {
        return APP_OK;
    }

    fs_file_t_init(&file);

    /* Open or create file */
    ret = fs_open(&file, logger->filename,
                  FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
    if (ret < 0) {
        LOG_ERR("Failed to open log file: %d", ret);
        return APP_ERR_IO;
    }

    /* Write each entry */
    char line[256];

    for (uint8_t i = 0U; i < logger->buffer_count; i++) {
        const sd_log_entry_t *e = &logger->buffer[i];

        int len = sd_logger_format_line(line, sizeof(line), e);

        if (len > 0) {
            ret = fs_write(&file, line, (size_t)len);
            if (ret < 0) {
                LOG_ERR("Failed to write entry: %d", ret);
                (void)fs_close(&file);
                return APP_ERR_IO;
            }
            logger->file_size += (uint32_t)ret;
            logger->total_entries++;
        }
    }

    (void)fs_close(&file);

    LOG_DBG("Wrote %u entries to %s", logger->buffer_count, logger->filename);
    logger->buffer_count = 0U;

    return APP_OK;
}

/**
 * @brief Ensure log directory exists
 */
static void ensure_directory(void)
{
    struct fs_dirent entry;

    if (fs_stat(SD_LOG_DIRECTORY, &entry) != 0) {
        int ret = fs_mkdir(SD_LOG_DIRECTORY);
        if (ret < 0) {
            LOG_WRN("Failed to create log directory: %d", ret);
        }
    }
}

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

app_err_t sd_logger_init(sd_logger_t *logger)
{
    if (logger == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    (void)memset(logger, 0, sizeof(sd_logger_t));
    logger->is_initialized = true;

    LOG_INF("SD logger initialized (buffer=%u entries)", SD_LOG_BUFFER_SIZE);

    return APP_OK;
}

app_err_t sd_logger_start(sd_logger_t *logger, const date_data_t *date)
{
    if ((logger == NULL) || !logger->is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    /* Ensure directory exists */
    ensure_directory();

    /* Generate filename */
    generate_filename(logger->filename, sizeof(logger->filename), date);

    /* Reset state */
    logger->buffer_count = 0U;
    logger->total_entries = 0U;
    logger->file_size = 0U;
    logger->last_log_distance = 0.0f;
    logger->file_open = true;

    LOG_INF("Started logging to %s", logger->filename);

    return APP_OK;
}

app_err_t sd_logger_stop(sd_logger_t *logger)
{
    if ((logger == NULL) || !logger->is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    /* Flush remaining buffer */
    if (logger->buffer_count > 0U) {
        (void)write_buffer_to_file(logger);
    }

    logger->file_open = false;

    LOG_INF("Stopped logging. Total entries: %u", (unsigned)logger->total_entries);

    return APP_OK;
}

app_err_t sd_logger_add_entry(sd_logger_t *logger,
                              const sd_log_entry_t *entry,
                              float current_distance)
{
    if ((logger == NULL) || !logger->is_initialized || (entry == NULL)) {
        return APP_ERR_INVALID_PARAM;
    }

    if (!logger->file_open) {
        return APP_ERR_NOT_INIT;
    }

    /* Check distance threshold */
    if ((current_distance - logger->last_log_distance) < SD_LOG_MIN_DISTANCE_M) {
        return APP_OK; /* Not enough distance traveled */
    }

    /*
     * A failed flush leaves the buffer full. Retry it; if the card is still
     * unavailable, drop the unwritten entries instead of writing past the
     * end of the buffer (which corrupted the rest of .bss).
     */
    if (logger->buffer_count >= SD_LOG_BUFFER_SIZE) {
        if (write_buffer_to_file(logger) != APP_OK) {
            LOG_WRN("Log storage unavailable, dropping %u entries",
                    (unsigned)logger->buffer_count);
            logger->buffer_count = 0U;
        }
    }

    /* Add to buffer */
    logger->buffer[logger->buffer_count] = *entry;
    logger->buffer_count++;
    logger->last_log_distance = current_distance;

    /* Check if buffer is full */
    if (logger->buffer_count >= SD_LOG_BUFFER_SIZE) {
        return write_buffer_to_file(logger);
    }

    return APP_OK;
}

app_err_t sd_logger_flush(sd_logger_t *logger)
{
    if ((logger == NULL) || !logger->is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (logger->buffer_count > 0U) {
        return write_buffer_to_file(logger);
    }

    return APP_OK;
}

uint32_t sd_logger_get_count(const sd_logger_t *logger)
{
    if (logger == NULL) {
        return 0U;
    }

    return logger->total_entries;
}

bool sd_logger_is_active(const sd_logger_t *logger)
{
    if (logger == NULL) {
        return false;
    }

    return logger->file_open;
}

const char *sd_logger_get_filename(const sd_logger_t *logger)
{
    if ((logger == NULL) || !logger->file_open) {
        return NULL;
    }

    return logger->filename;
}

void sd_logger_build_entry(sd_log_entry_t *entry,
                           const loc_data_t *loc,
                           const date_data_t *date,
                           int16_t power,
                           uint8_t bpm,
                           uint8_t cadence,
                           uint16_t speed,
                           const sd_log_altitude_t *alti,
                           float distance,
                           float climb)
{
    if (entry == NULL) {
        return;
    }

    (void)memset(entry, 0, sizeof(sd_log_entry_t));

    if (loc != NULL) {
        entry->loc = *loc;
        entry->alti.gps_alt = loc->alt;
    }

    if (date != NULL) {
        entry->date = *date;
    }

    entry->sensors.power = power;
    entry->sensors.bpm = bpm;
    entry->sensors.cadence = cadence;
    entry->sensors.speed = speed;

    if (alti != NULL) {
        float gps_alt = entry->alti.gps_alt;

        entry->alti = *alti;
        if (alti->gps_alt == 0.0f) {
            entry->alti.gps_alt = gps_alt; /* the one of the position */
        }
    }

    entry->distance = distance;
    entry->climb = climb;
    entry->timestamp = k_uptime_get_32();
}
