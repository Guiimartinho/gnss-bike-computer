/**
 * @file attitude.c
 * @brief Attitude manager implementation
 *
 * Based on original Attitude.cpp with 3-state Kalman filter for
 * altitude fusion using barometer and accelerometer pitch.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <math.h>

#include "model/attitude.h"
#include "model/distance.h"
#include "model/power_estimate.h"
#include "model/locator.h"
#include "model/kalman_altitude.h"
#include "model/crash_recovery.h"
#include "model/user_settings.h"

LOG_MODULE_REGISTER(attitude, CONFIG_LOG_DEFAULT_LEVEL);

/* ==========================================================================
 * Private Definitions
 * ========================================================================== */

/** Minimum speed for moving time (km/h) */
#define MOVING_SPEED_THRESHOLD  2.0f

/** Minimum speed for Kalman update (m/s) */
#define MIN_SPEED_FOR_KALMAN    1.5f

/** Power estimation constants */
#define DEFAULT_RIDER_WEIGHT_KG 79.0f /* USER_WEIGHT of the legacy */

/** Climb calculation hysteresis (meters) - from original */
#define CLIMB_ELEVATION_HYSTERESIS_M    2.0f

/** GPS points before slope calculation is valid */
#define MIN_PTS_FOR_SLOPE       60U

/* ==========================================================================
 * Private Variables
 * ========================================================================== */

/** Current attitude data */
static attitude_t current_att;

/** Extended attitude data */
static attitude_ext_t current_ext;

/** Start timestamp */
static uint32_t start_time;

/** Last update timestamp */
static uint32_t last_update;

/** Elapsed time counters */
static uint32_t elapsed_seconds;
static uint32_t moving_seconds;

/** Speed accumulator for average */
static float speed_sum;
static uint32_t speed_count;

/** Kalman filter for altitude fusion */
static kalman_altitude_t altitude_kf;

/** Current filtered elevation */
static float current_elevation;

/** Last stored elevation for climb calculation */
static float last_stored_elevation;

/** Accumulated climb */
static float accumulated_climb;

/** Current speed in m/s */
static float current_speed_ms;

/** Distance ridden, with the rule of the legacy */
static struct distance_acc ridden;

/** Last Kalman update time */
static uint32_t last_kalman_time;

/** Current pitch from IMU (radians) */
static float current_pitch_rad;

/** Sea level pressure reference */
static float sea_level_pressure;

/** Baro/GPS altitude correction filter */
static float altitude_correction;

/** Initialization flags */
static bool is_initialized;
static bool is_altitude_initialized;
static bool has_sea_level_ref;

/** User rider weight in kg (from settings) */
static float rider_weight_kg;

/**
 * The legacy averages the last ten pressures (FILTRE_NB, 1 s at 10 Hz) and
 * turns the average into altitude (`libraries/AltiBaro/AltiBaro.cpp:137-156`);
 * until the buffer is full, `computeAlti()` answers false.
 */
#define BARO_FILTER_NB      10U

static float baro_ring[BARO_FILTER_NB];
static uint8_t baro_ring_count;
static uint8_t baro_ring_head;

/** Average of the ring, or 0 while it is not full */
static float baro_pressure_avg(void)
{
    if (baro_ring_count < BARO_FILTER_NB) {
        return 0.0f;
    }

    float sum = 0.0f;

    for (uint8_t i = 0U; i < BARO_FILTER_NB; i++) {
        sum += baro_ring[i];
    }

    return sum / (float)BARO_FILTER_NB;
}

/* ==========================================================================
 * Private Functions
 * ========================================================================== */

/**
 * @brief Compute barometric altitude from pressure
 * @param pressure Pressure in Pa
 * @return Altitude in meters
 */
static float compute_baro_altitude(float pressure)
{
    if (pressure <= 0.0f) {
        return 0.0f;
    }

    /* Using standard atmosphere if no sea level ref */
    float p0 = (sea_level_pressure > 0.0f) ? sea_level_pressure : 101325.0f;

    /* h = 44330 * (1 - (P/P0)^0.1903) */
    float altitude = 44330.0f * (1.0f - powf(pressure / p0, 0.1903f));

    /* Apply GPS/baro correction */
    return altitude - altitude_correction;
}

/**
 * @brief Filter altitude using Kalman and update climb
 *
 * Implements the 3-state Kalman filter for altitude fusion:
 * State: [elevation, pitch, alpha_zero]
 * Model: d(elevation) = (pitch - alpha_zero) * dl
 */
static void compute_altitude_fusion(void)
{
    /* Barometric altitude of the last second, as the legacy averages it */
    float baro_ele = compute_baro_altitude(baro_pressure_avg());

    /* Initialize Kalman if not done */
    if (!kalman_altitude_is_init(&altitude_kf)) {
        kalman_altitude_init(&altitude_kf, baro_ele);
        last_kalman_time = k_uptime_get_32();
        LOG_INF("Kalman altitude filter initialized (ele=%.1f)", (double)baro_ele);
        return;
    }

    /* Skip if not moving fast enough */
    if (current_speed_ms < MIN_SPEED_FOR_KALMAN) {
        last_kalman_time = k_uptime_get_32();
        return;
    }

    /* Calculate time delta */
    uint32_t now = k_uptime_get_32();

    /* Prepare feed structure for Kalman filter */
    kalman_alt_feed_t feed = {
        .baro_altitude = baro_ele,
        .pitch_rad = current_pitch_rad,
        .speed_ms = current_speed_ms,
        .timestamp_ms = now
    };

    /* Update Kalman filter and get output */
    kalman_alt_output_t output;
    bool updated = kalman_altitude_update(&altitude_kf, &feed, &output);

    last_kalman_time = now;

    if (updated) {
        /* Get filtered values */
        current_elevation = output.elevation;

        /* Update slope and vertical speed after enough data points */
        if (current_att.nbpts > MIN_PTS_FOR_SLOPE) {
            current_att.slope = (int8_t)(100.0f * output.slope);
            current_att.vit_asc = output.vit_asc;
        }

        LOG_DBG("Fusion: ele=%.1f slope=%d vit_asc=%.2f alpha0=%.3f",
                (double)current_elevation, current_att.slope,
                (double)current_att.vit_asc, (double)output.alpha_zero);
    }
}

/**
 * @brief Update climb calculation with hysteresis
 */
static void update_climb(void)
{
    if (!is_altitude_initialized) {
        last_stored_elevation = current_elevation;
        accumulated_climb = 0.0f;
        is_altitude_initialized = true;
        return;
    }

    /* Only count climb with hysteresis to avoid noise */
    if (current_elevation > last_stored_elevation + CLIMB_ELEVATION_HYSTERESIS_M) {
        /* Going up - add to accumulated climb */
        accumulated_climb += current_elevation - last_stored_elevation;
        last_stored_elevation = current_elevation;
    } else if (current_elevation + CLIMB_ELEVATION_HYSTERESIS_M < last_stored_elevation) {
        /* Going down - update reference without adding climb */
        last_stored_elevation = current_elevation;
    }

    /* Sanity check */
    if (accumulated_climb < 0.0f) {
        accumulated_climb = 0.0f;
        is_altitude_initialized = false;
    }

    current_att.climb = accumulated_climb;
}

/**
 * @brief Filter GPS/baro altitude difference to remove drift
 * @param gps_alt GPS altitude
 * @param baro_alt Barometer altitude
 */
static void filter_altitude_correction(float gps_alt, float baro_alt)
{
    /* High time-constant filter (about 800 samples) */
    const float tau = 800.0f / (800.0f + 1.0f);
    float input = baro_alt - gps_alt;

    altitude_correction = tau * altitude_correction + (1.0f - tau) * input;

    LOG_DBG("Alt correction: gps=%.1f baro=%.1f corr=%.2f",
            (double)gps_alt, (double)baro_alt, (double)altitude_correction);
}

/** A ride was picked up again, for the service to say it on the screen */
static bool fdir_restored;

/**
 * Take the state of the ride back, as the legacy does when the sea level
 * reference appears (`legacy/source/model/Attitude.cpp:393-417`): the block
 * has to pass its CRC and carry today's date, and it is used once.
 */
static void restore_from_crash(void)
{
    saved_data_t saved;

    if (!crash_recovery_has_data() || !crash_recovery_get_saved_state(&saved)) {
        return;
    }

    if (saved.date.date != current_att.date.date) {
        LOG_WRN("FDIR block is from another day (%u)", (unsigned int)saved.date.date);
        crash_recovery_clear_saved_state();
        return;
    }

    LOG_WRN("FDIR: ride restored, %.1f m and %.1f m of climb", (double)saved.dist,
            (double)saved.climb);

    current_att.dist = saved.dist;
    distance_restore(&ridden, saved.dist);
    current_att.climb = saved.climb;
    accumulated_climb = saved.climb;
    current_att.nbsec_act = saved.nbsec_act;
    current_att.pr = saved.pr;

    crash_recovery_clear_saved_state();
    fdir_restored = true;
}

/**
 * @brief Save current state for crash recovery
 */
static void save_crash_recovery_state(void)
{
    crash_recovery_save_state(&current_att.loc,
                              &current_att.date,
                              current_att.dist,
                              current_att.climb,
                              current_att.nbpts,
                              current_att.nbsec_act,
                              current_att.pr);
}

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

bool attitude_take_fdir_notice(void)
{
    bool notice = fdir_restored;

    fdir_restored = false;

    return notice;
}

app_err_t attitude_init(void)
{
    if (is_initialized) {
        return APP_ERR_ALREADY_INIT;
    }

    /* Initialize locator */
    app_err_t err = locator_init();
    if ((err != APP_OK) && (err != APP_ERR_ALREADY_INIT)) {
        return err;
    }

    /* Initialize crash recovery */
    (void)crash_recovery_init();

    /* Clear data */
    (void)memset(&current_att, 0, sizeof(current_att));
    (void)memset(&current_ext, 0, sizeof(current_ext));
    (void)memset(&altitude_kf, 0, sizeof(altitude_kf));

    start_time = 0U;
    last_update = 0U;
    elapsed_seconds = 0U;
    moving_seconds = 0U;
    speed_sum = 0.0f;
    speed_count = 0U;

    current_elevation = 0.0f;
    last_stored_elevation = 0.0f;
    accumulated_climb = 0.0f;
    current_speed_ms = 0.0f;
    last_kalman_time = 0U;
    current_pitch_rad = 0.0f;
    sea_level_pressure = 0.0f;
    altitude_correction = 0.0f;

    is_altitude_initialized = false;
    has_sea_level_ref = false;
    distance_init(&ridden);
    baro_ring_count = 0U;
    baro_ring_head = 0U;

    /* Load rider weight from user settings (stored in hectograms) */
    const user_settings_t *settings = user_settings_get_global();
    uint16_t weight_hg = user_settings_get_weight(settings);
    if (weight_hg > 0U) {
        rider_weight_kg = (float)weight_hg / 10.0f;
    } else {
        rider_weight_kg = DEFAULT_RIDER_WEIGHT_KG;
    }
    LOG_INF("Rider weight: %.1f kg", (double)rider_weight_kg);

    /*
     * The state of a ride is not restored here: the legacy waits for the
     * sea level reference, when it already knows the date, and only takes
     * the block if it is from today (`Attitude.cpp:393-417`). See
     * restore_from_crash() below.
     */
    fdir_restored = false;

    is_initialized = true;
    LOG_INF("Attitude manager initialized");

    return APP_OK;
}

app_err_t attitude_update_gps(const loc_data_t *loc)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (loc == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    /*
     * The speed only changes at the end of the epoch: the legacy feeds the
     * altitude filter and the power with the speed of the previous location
     * and updates m_speed_ms afterwards
     * (`legacy/source/model/Attitude.cpp:509-524`).
     */

    /* Add to locator for distance calculation */
    app_err_t err = locator_add_point(loc);
    if (err != APP_OK) {
        return err;
    }

    /* Update current attitude */
    current_att.loc = *loc;
    /*
     * Distance as the legacy does it: between raw positions, whatever the
     * speed, throwing the first 25 m away and saving the state for the
     * crash recovery every 15 m (`Attitude::computeDistance`). The filtered
     * distance of the locator stays for whoever wants it.
     */
    bool snapshot = distance_add(&ridden, loc->lat, loc->lon);

    current_att.dist = distance_total(&ridden);
    current_att.nbpts++;

    /* Initialize sea level pressure if barometer ready and enough GPS points */
    if (!has_sea_level_ref &&
        (current_ext.pressure > 0.0f) &&
        (current_att.nbpts > 15U)) {

        /* Calculate sea level pressure from GPS altitude */
        /* P0 = P / (1 - alt/44330)^5.255 */
        float alt = loc->alt;
        float factor = 1.0f - (alt / 44330.0f);
        if (factor > 0.0f) {
            sea_level_pressure = current_ext.pressure / powf(factor, 5.255f);
            has_sea_level_ref = true;

            /* Initialize altitude */
            current_elevation = alt;
            is_altitude_initialized = false;

            LOG_INF("Sea level pressure initialized: %.1f Pa (GPS alt: %.1f m)",
                    (double)sea_level_pressure, (double)alt);

            /* the moment the legacy picks a ride up again */
            restore_from_crash();
        }
    }

    /* Apply GPS/baro altitude correction filter */
    if (has_sea_level_ref && (baro_pressure_avg() > 0.0f)) {
        float baro_alt = compute_baro_altitude(baro_pressure_avg());

        filter_altitude_correction(loc->alt, baro_alt);

        /* one fusion per epoch, as the legacy does */
        compute_altitude_fusion();
    }

    /* Update climb from Kalman-filtered altitude */
    update_climb();
    current_att.climb = accumulated_climb;

    /* Update extended data */
    current_ext.base = current_att;

    /* Track moving time */
    uint32_t now = k_uptime_get_32();
    if (start_time == 0U) {
        start_time = now;
    }

    if ((now - last_update) >= 1000U) {  /* 1 second elapsed */
        elapsed_seconds++;

        if (loc->speed > MOVING_SPEED_THRESHOLD) {
            moving_seconds++;
            current_att.nbsec_act = (uint16_t)moving_seconds;
        }

        last_update = now;
    }

    /* the legacy saves the state every 15 m, not every second */
    if (snapshot) {
        save_crash_recovery_state();
    }

    /* Accumulate speed for average */
    if (loc->speed > 0.0f) {
        speed_sum += loc->speed;
        speed_count++;
    }

    /*
     * Power with the speed of the previous epoch, as the legacy does: it
     * calls computePower() before updating m_speed_ms
     * (`legacy/source/model/Attitude.cpp:516-524`).
     */
    current_att.pwr = power_estimate_w(rider_weight_kg, current_speed_ms, current_att.vit_asc);
    current_speed_ms = loc->speed / 3.6f;

    return APP_OK;
}

app_err_t attitude_update_baro(float pressure, float temperature)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    current_ext.pressure = pressure;
    current_ext.temperature = temperature;

    if (pressure > 0.0f) {
        baro_ring[baro_ring_head] = pressure;
        baro_ring_head = (uint8_t)((baro_ring_head + 1U) % BARO_FILTER_NB);
        if (baro_ring_count < BARO_FILTER_NB) {
            baro_ring_count++;
        }
        current_ext.baro_altitude = compute_baro_altitude(baro_pressure_avg());
    }

    /*
     * The fusion does not run here: the legacy runs it once per location
     * (`Attitude::addNewLocation` calls `computeElevation`), with the
     * average of the last second of pressure.
     */

    return APP_OK;
}

app_err_t attitude_update_imu(float heading, float pitch, float roll)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    current_ext.heading = heading;

    /* Store pitch in radians for Kalman filter */
    /* The pitch angle is used to estimate slope when moving */
    current_pitch_rad = pitch * (3.14159265f / 180.0f);

    (void)roll; /* Roll not used currently */

    return APP_OK;
}

void attitude_update_battery(uint8_t soc, float voltage)
{
    if (!is_initialized) {
        return;
    }

    current_ext.battery_soc = soc;
    current_ext.battery_voltage = voltage;
}

void attitude_update_hrm(uint8_t bpm, bool connected)
{
    if (!is_initialized) {
        return;
    }

    current_ext.hrm_bpm = bpm;
    current_ext.hrm_connected = connected;
}

void attitude_update_datetime(const date_data_t *date)
{
    if (!is_initialized || (date == NULL)) {
        return;
    }

    current_att.date = *date;
}

app_err_t attitude_get(attitude_t *att)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (att == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    *att = current_att;
    return APP_OK;
}

app_err_t attitude_get_ext(attitude_ext_t *att)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (att == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    *att = current_ext;
    return APP_OK;
}

app_err_t attitude_get_location(loc_data_t *loc)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (loc == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    *loc = current_att.loc;
    return APP_OK;
}

float attitude_get_distance(void)
{
    return current_att.dist;
}

float attitude_get_climb(void)
{
    return current_att.climb;
}

uint32_t attitude_get_elapsed_time(void)
{
    return elapsed_seconds;
}

uint32_t attitude_get_moving_time(void)
{
    return moving_seconds;
}

float attitude_get_avg_speed(void)
{
    if (speed_count == 0U) {
        return 0.0f;
    }

    return speed_sum / (float)speed_count;
}

uint16_t attitude_get_power(void)
{
    return current_att.pwr;
}

void attitude_set_rider_weight(float weight_kg)
{
    if (weight_kg > 20.0f && weight_kg < 200.0f) {
        rider_weight_kg = weight_kg;
        LOG_INF("Rider weight updated: %.1f kg", (double)rider_weight_kg);
    }
}

void attitude_reset(void)
{
    (void)memset(&current_att, 0, sizeof(current_att));
    (void)memset(&current_ext, 0, sizeof(current_ext));
    (void)memset(&altitude_kf, 0, sizeof(altitude_kf));

    start_time = 0U;
    last_update = 0U;
    elapsed_seconds = 0U;
    moving_seconds = 0U;
    speed_sum = 0.0f;
    speed_count = 0U;

    current_elevation = 0.0f;
    last_stored_elevation = 0.0f;
    accumulated_climb = 0.0f;
    current_speed_ms = 0.0f;
    last_kalman_time = 0U;
    altitude_correction = 0.0f;

    is_altitude_initialized = false;
    has_sea_level_ref = false;
    distance_init(&ridden);
    baro_ring_count = 0U;
    baro_ring_head = 0U;

    locator_reset();
    crash_recovery_clear();

    LOG_INF("Attitude reset");
}

void attitude_compute(void)
{
    if (!is_initialized) {
        return;
    }

    /* Update elapsed time */
    if (start_time > 0U) {
        uint32_t now = k_uptime_get_32();
        elapsed_seconds = (now - start_time) / 1000U;
    }

    /* Copy base to extended */
    current_ext.base = current_att;
}
