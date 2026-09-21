/**
 * @file ui_model.h
 * @brief Snapshot of the device state that the user interface draws
 *
 * Plain C, without Zephyr or LVGL types: the model thread fills one snapshot
 * per epoch and the ui thread copies it (zbus channel model_state); the host
 * renderer in tests/ui fills it with sample rides. Maps come already
 * projected by the model into their window, in per mille (UI_PM), so the
 * interface only scales them to the box it draws in (the legacy projects in
 * VueCRS::afficheSegment() and VuePRC with regFenLim()).
 */

#ifndef UI_MODEL_H
#define UI_MODEL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Sizes
 * ========================================================================== */

/** Segments shown at once (legacy SegmentManager keeps 2 on screen) */
#define UI_SEG_MAX          2U
/** Points of a segment mini-map */
#define UI_SEG_PTS_MAX      64U
/** Points of the route map in PRC */
#define UI_ROUTE_PTS_MAX    160U
/** Columns of the elevation profile (model/route_profile.h) */
#define UI_PROFILE_PTS      120U
/** Satellites in the sky plot and in DBG */
#define UI_SAT_MAX          32U
/** Paired sensors listed in Sensores */
#define UI_SENSOR_MAX       6U
/** Devices in the pairing list (legacy ant_device_manager lists 7) */
#define UI_PAIR_MAX         7U
/** RR zones (legacy RRZone bins) */
#define UI_RR_ZONES         5U
/** Power zones (PW_ZONES_NB) */
#define UI_PWR_ZONES        7U
/** Samples of the slope histogram on CRS page 3 */
#define UI_HISTO_MAX        36U
/** Roughness values on CRS page 3: X, Y and Z of the accelerometer, barometer */
#define UI_ROUGH_N          4U
/** Samples of the power vector over one crank turn */
#define UI_VECTOR_PTS       24U
/** Routes in the PRC list (the list page shows Voltar and up to 10 names) */
#define UI_ROUTE_LIST_MAX   10U
/** Text fields */
#define UI_NAME_LEN         20U

/** Full scale of map coordinates: 0..UI_PM from left to right, bottom to top */
#define UI_PM               1000
/** Time of day not known yet */
#define UI_TIME_UNKNOWN     86400U
/** Heading or course not known */
#define UI_ANGLE_UNKNOWN    (-1)

/* ==========================================================================
 * Types
 * ========================================================================== */

/** Point of a map, in per mille of its window, y up */
typedef struct {
    int16_t x;
    int16_t y;
} ui_pt_t;

/** GNSS state on the status bar */
typedef enum {
    UI_GNSS_OFF = 0,        /**< receiver in backup (FEC, Zwift) */
    UI_GNSS_SEARCH,         /**< on, no fix */
    UI_GNSS_FIX             /**< fix */
} ui_gnss_t;

/** GNSS power mode (docs/16, state machine GNSS) */
typedef enum {
    UI_GNSS_MODE_BACKUP = 0,
    UI_GNSS_MODE_ACQ,
    UI_GNSS_MODE_LEAP,
    UI_GNSS_MODE_FULL
} ui_gnss_mode_t;

/** Charging source */
typedef enum {
    UI_CHARGE_NONE = 0,
    UI_CHARGE_SOLAR,
    UI_CHARGE_USB,
    UI_CHARGE_USB_FULL
} ui_charge_t;

/** State of an external sensor */
typedef enum {
    UI_LINK_NONE = 0,       /**< not paired */
    UI_LINK_CONNECTED,
    UI_LINK_LOST,
    UI_LINK_SEARCH
} ui_link_t;

/** Kind of external sensor */
typedef enum {
    UI_SENSOR_HR = 0,
    UI_SENSOR_BSC,
    UI_SENSOR_POWER,
    UI_SENSOR_FEC,
    UI_SENSOR_RADAR,
    UI_SENSOR_LIGHT,
    UI_SENSOR_KINDS
} ui_sensor_kind_t;

/** Turn of the navigation (Komoot) */
typedef enum {
    UI_TURN_NONE = 0,
    UI_TURN_STRAIGHT,
    UI_TURN_SLIGHT_LEFT,
    UI_TURN_LEFT,
    UI_TURN_SHARP_LEFT,
    UI_TURN_SLIGHT_RIGHT,
    UI_TURN_RIGHT,
    UI_TURN_SHARP_RIGHT,
    UI_TURN_UTURN,
    UI_TURN_ARRIVE
} ui_turn_t;

/** Status bar */
typedef struct {
    uint32_t time_s;        /**< seconds of the day, local time, or UI_TIME_UNKNOWN */
    ui_gnss_t gnss;
    bool ant_link;          /**< at least one ANT+ sensor connected */
    bool ble_link;          /**< at least one BLE sensor connected */
    bool recording;
    bool paused;            /**< the timer is held: the bike is still */
    ui_charge_t charge;
    uint8_t batt_pct;
} ui_status_t;

/**
 * The climb ahead (`model/climb.h`).
 *
 * The legacy shows the whole route and the total climb, and nothing about
 * the climb the rider is on; this is what the climb page draws.
 */
typedef struct {
    bool on_climb;          /**< riding one right now */
    float remain_m;         /**< to the top */
    float remain_gain_m;
    float grade_pct;        /**< average of what is left */
    float ahead_grade_pct;  /**< of the next 200 m */
    float done_pct;
    float to_next_m;        /**< to the foot of the next climb, when not on one */
    float next_len_m;       /**< of that next climb */
    float next_gain_m;
    uint8_t cat;            /**< enum climb_cat of the one being ridden */
    uint8_t next_cat;
    uint8_t index;          /**< which climb of the route, counting from one */
    uint8_t total;          /**< climbs the route has */
    float prof_span_m;      /**< horizontal length the profile covers */
    uint8_t prof_n;         /**< columns of the profile of this climb */
    uint8_t prof_here;      /**< column of the rider */
    int16_t prof_m[UI_PROFILE_PTS];
    int16_t prof_min_m;
    int16_t prof_max_m;
} ui_climb_t;

/**
 * The lap and the totals of the ride (`model/activity.h`).
 *
 * None of this is in the legacy, which has neither timer nor lap; it is
 * what the FIT file carries and what the lap page shows.
 */
typedef struct {
    uint32_t timer_s;       /**< moving time of the ride */
    uint32_t elapsed_s;     /**< wall time of the ride */
    uint32_t lap_timer_s;   /**< moving time of the lap being ridden */
    float lap_dist_m;
    float avg_kmh;          /**< of the ride, over the moving time */
    float max_kmh;
    float descent_m;
    uint16_t laps;          /**< laps already closed */
    uint16_t kcal;
} ui_activity_t;

/** Ride values of the data pages (legacy att, bsc_info, hrm_info) */
typedef struct {
    float dist_m;
    float speed_kmh;
    float avg_kmh;
    float climb_m;
    float alt_m;
    float va_ms;            /**< vertical speed */
    float score;            /**< suffer score */
    int16_t pwr_w;          /**< estimated power, negative going down */
    uint16_t next_seg_m;    /**< distance to the next segment */
    uint16_t solar_mw;      /**< power from the panels */
    uint8_t cad_rpm;
    uint8_t hr_bpm;
    int8_t slope_pct;
    uint8_t pr;             /**< records beaten (legacy att.pr) */
} ui_ride_t;

/** Segment on screen with its mini-map */
typedef struct {
    bool on;                /**< running (legacy status > SEG_OFF) */
    bool done;              /**< just finished (legacy status < SEG_OFF) */
    uint8_t pct;            /**< share of the segment done */
    float advance_s;        /**< ahead (+) or behind (-) the record */
    float cur_s;            /**< time on the segment */
    char name[UI_NAME_LEN];
    uint8_t npts;
    ui_pt_t pts[UI_SEG_PTS_MAX]; /**< in the segment window on the CRS pages, in the
                                      route window in PRC */
    ui_pt_t rider;
    int16_t course_deg;     /**< UI_ANGLE_UNKNOWN draws a dot */
} ui_segment_t;

/** Next turn of the navigation */
typedef struct {
    bool valid;
    uint16_t dist_m;
    ui_turn_t turn;
    char street[2U * UI_NAME_LEN];
} ui_nav_t;

/** RR zones (legacy RRZone) */
typedef struct {
    uint8_t nzones;
    uint8_t cur;            /**< current zone */
    float val[UI_RR_ZONES];
} ui_rr_t;

/** CRS page 3: inclination, heading and roughness */
typedef struct {
    float pitch_pct;        /**< slope from the accelerometer */
    uint8_t histo_n;
    int8_t histo[UI_HISTO_MAX]; /**< slope history, % */
    int16_t heading_deg;    /**< magnetic heading or UI_ANGLE_UNKNOWN */
    float rough[UI_ROUGH_N]; /**< mean deviation of the X, Y and Z accelerations and of the
                                  barometer (legacy fxos_get_roughness, baro.getRoughness) */
} ui_attitude_t;

/** PRC map */
typedef struct {
    float remain_km;
    uint16_t n;
    uint16_t done;          /**< points [0, done) already ridden */
    ui_pt_t pts[UI_ROUTE_PTS_MAX];
    ui_pt_t rider;
    int16_t course_deg;
    uint16_t scale_m;       /**< scale bar length in meters */
    uint16_t scale_pm;      /**< scale bar length in per mille of the window width */
} ui_route_t;

/** Elevation profile of the route (PRC) */
typedef struct {
    uint8_t n;                      /**< columns filled */
    uint8_t here;                   /**< column of the rider */
    int16_t alt_m[UI_PROFILE_PTS];  /**< altitude of each column */
    int16_t min_m;
    int16_t max_m;
    uint16_t climb_left_m;          /**< climb still ahead */
    float remain_km;
} ui_profile_t;

/** Trainer (FE-C) values */
typedef struct {
    uint32_t time_s;
    float score;
    uint16_t pwr_w;
    uint16_t rr_ms;
    uint8_t cad_rpm;
    uint8_t hr_bpm;
    uint8_t zone;
    uint8_t zone_pct[UI_PWR_ZONES];     /**< time share in each power zone */
    bool vector_valid;
    uint8_t vector[UI_VECTOR_PTS];      /**< power over one turn, 0..100 */
} ui_fec_t;

/** One satellite */
typedef struct {
    int16_t az_deg;
    int8_t el_deg;
    uint8_t cn0;            /**< dB-Hz */
    uint8_t sys;            /**< 0 GPS, 1 Galileo, 2 BeiDou, 3 QZSS, 4 GLONASS, 5 SBAS */
    bool used;
} ui_sat_t;

/** GNSS details (GNSS screen and DBG) */
typedef struct {
    ui_gnss_mode_t mode;
    bool fix3d;
    uint8_t nsat;
    uint8_t used;
    ui_sat_t sat[UI_SAT_MAX];
    uint32_t fix_age_s;     /**< age of the last position */
    float hacc_m;           /**< horizontal accuracy */
} ui_gnss_info_t;

/** Energy screen */
typedef struct {
    uint16_t mv;
    int16_t ma;             /**< negative when discharging */
    uint8_t pct;
    ui_charge_t source;
    uint16_t solar_mw;
    uint16_t solar_limit_mv;
    int8_t temp_c;
    uint16_t autonomy_h;
} ui_energy_t;

/** One paired sensor */
typedef struct {
    uint8_t kind;           /**< ui_sensor_kind_t */
    ui_link_t link;
    bool ant;               /**< ANT+ or BLE */
    uint32_t dev_id;
    char dev_name[UI_NAME_LEN];
    char value[12];         /**< last reading, formatted by the model */
} ui_sensor_t;

typedef struct {
    uint8_t n;
    ui_sensor_t s[UI_SENSOR_MAX];
} ui_sensors_t;

/** Device found while pairing */
typedef struct {
    bool ant;
    uint32_t id;
    char name[UI_NAME_LEN];
    int8_t rssi;
} ui_pair_item_t;

typedef struct {
    bool searching;
    uint8_t kind;           /**< ui_sensor_kind_t being paired */
    uint8_t n;
    ui_pair_item_t item[UI_PAIR_MAX];
} ui_pair_t;

/** Settings shown in the menus */
typedef struct {
    uint16_t ftp_w;
    uint8_t weight_kg;
    bool gnss_leap;
    bool light_auto;
    uint16_t solar_limit_mv;
} ui_settings_t;

/** Routes on the card, for the PRC list (legacy mes_parcours) */
typedef struct {
    uint8_t n;
    char name[UI_ROUTE_LIST_MAX][UI_NAME_LEN];
} ui_routes_t;

/** DBG screen extras */
typedef struct {
    uint8_t seg_loaded;
    char version[12];
} ui_debug_t;

/** The whole snapshot */
typedef struct {
    ui_status_t status;
    ui_ride_t ride;
    ui_activity_t act;
    ui_climb_t climb;
    uint8_t nseg;           /**< segments on screen: 0, 1 or 2 */
    ui_segment_t seg[UI_SEG_MAX];
    ui_nav_t nav;
    ui_rr_t rr;
    ui_attitude_t att;
    ui_route_t route;
    ui_profile_t profile;
    ui_fec_t fec;
    ui_gnss_info_t gnss;
    ui_energy_t energy;
    ui_sensors_t sensors;
    ui_pair_t pair;
    ui_settings_t settings;
    ui_routes_t routes;
    ui_debug_t debug;
} ui_model_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_MODEL_H */
