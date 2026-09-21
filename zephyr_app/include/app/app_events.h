/**
 * @file app_events.h
 * @brief Messages of the zbus channels (docs/16-arquitetura-firmware.md, Eventos)
 *
 * Plain C types, without Zephyr driver types, so the model and its host tests
 * can use them. Each service publishes on its channels and never calls
 * another service: the only calls go down to Zephyr and to the model code.
 */

#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * GNSS
 * ========================================================================== */

/** Receiver power state, as the GNSS service drives it (docs/16, GNSS) */
enum app_gnss_mode {
    APP_GNSS_MODE_BACKUP = 0,   /**< backup or off: FEC, Zwift, shutdown */
    APP_GNSS_MODE_ACQ,          /**< on, acquiring */
    APP_GNSS_MODE_LEAP,         /**< tracking in the low power mode */
    APP_GNSS_MODE_FULL          /**< tracking at full power */
};

/** Channel gnss_fix: one per navigation epoch (1 Hz), with or without a fix */
struct app_gnss_fix {
    uint32_t uptime_ms;         /**< when the epoch reached the service */
    bool fix;                   /**< position valid */
    bool sim;                   /**< position given by a PC ($LOC), which wins over the receiver */
    uint8_t mode;               /**< enum app_gnss_mode */
    uint8_t nsat;               /**< satellites used */
    int32_t lat_e7;             /**< latitude, 1e-7 degree */
    int32_t lon_e7;             /**< longitude, 1e-7 degree */
    int32_t alt_mm;             /**< height above mean sea level */
    uint32_t speed_mms;         /**< ground speed */
    uint32_t course_mdeg;       /**< course over ground, 0 to 359999 */
    uint32_t hdop_milli;        /**< dilution of precision, thousandths */
    bool time_valid;            /**< UTC below is valid */
    uint8_t hour;
    uint8_t minute;
    uint16_t millisecond;       /**< milliseconds within the minute */
    uint8_t day;
    uint8_t month;
    uint8_t year2;              /**< year modulo 100 */
};

/** Satellites of the sky plot */
#define APP_SAT_MAX     32U

/** System of a satellite, as ui_sat_t */
enum app_gnss_sys {
    APP_SYS_GPS = 0,
    APP_SYS_GALILEO,
    APP_SYS_BEIDOU,
    APP_SYS_QZSS,
    APP_SYS_GLONASS,
    APP_SYS_SBAS
};

struct app_gnss_sat {
    uint16_t az_deg;
    uint8_t el_deg;
    uint8_t cn0;                /**< dB-Hz */
    uint8_t sys;                /**< enum app_gnss_sys */
    bool used;
};

/** Channel gnss_sky: satellites in view, about once a second */
struct app_gnss_sky {
    uint8_t n;
    struct app_gnss_sat sat[APP_SAT_MAX];
};

/* ==========================================================================
 * On-board sensors
 * ========================================================================== */

/** Channel baro: one sample, 10 Hz as the legacy (Attitude.cpp) */
struct app_baro {
    uint32_t uptime_ms;
    float pressure_pa;
    float temp_c;
};

/** Channel imu: attitude from the accelerometer, 1 Hz */
struct app_imu {
    uint32_t uptime_ms;
    float pitch_deg;            /**< nose up positive */
    float roll_deg;
    float rough[3];             /**< mean deviation of X, Y and Z (legacy fxos roughness) */
};

/** Channel mag: tilt-compensated heading, 1 Hz */
struct app_mag {
    uint32_t uptime_ms;
    bool valid;
    float heading_deg;          /**< magnetic heading, 0 to 360 */
};

/** Channel ambient: ambient light, 1 Hz */
struct app_ambient {
    float lux;
};

/* ==========================================================================
 * External sensors and phone (radio service)
 * ========================================================================== */

/** Kinds of external sensor, as ui_sensor_kind_t */
enum app_ext_kind {
    APP_EXT_HR = 0,
    APP_EXT_BSC,
    APP_EXT_POWER,
    APP_EXT_FEC,
    APP_EXT_RADAR,
    APP_EXT_LIGHT,
    APP_EXT_KINDS
};

/** Channel ext_sensor: one reading of an external sensor */
struct app_ext_sensor {
    uint32_t uptime_ms;
    uint8_t kind;               /**< enum app_ext_kind */
    uint8_t hr_bpm;
    uint16_t rr_ms;
    uint8_t cadence_rpm;
    uint16_t speed_kmh100;      /**< wheel speed, 0.01 km/h */
    uint16_t power_w;
    uint16_t elapsed_s;         /**< trainer elapsed time */
    int8_t grade_pct;
};

/** State of an external link, as ui_link_t */
enum app_link {
    APP_LINK_NONE = 0,
    APP_LINK_CONNECTED,
    APP_LINK_LOST,
    APP_LINK_SEARCH
};

/** Channel link_status: a sensor connected, lost, searching or unpaired */
struct app_link_status {
    uint8_t kind;               /**< enum app_ext_kind */
    uint8_t link;               /**< enum app_link */
    bool ant;                   /**< ANT+ or BLE */
    uint32_t dev_id;
    char name[20];
};

/** Devices listed while pairing (legacy ant_device_manager lists 7) */
#define APP_PAIR_MAX    7U

/** Channel pair_list: devices found by the pairing search */
struct app_pair_list {
    bool searching;
    uint8_t kind;               /**< enum app_ext_kind being paired */
    uint8_t n;
    struct {
        bool ant;
        uint32_t id;
        char name[20];
        int8_t rssi;
    } dev[APP_PAIR_MAX];
};

/** Turn of the phone navigation, as ui_turn_t */
enum app_turn {
    APP_TURN_NONE = 0,
    APP_TURN_STRAIGHT,
    APP_TURN_SLIGHT_LEFT,
    APP_TURN_LEFT,
    APP_TURN_SHARP_LEFT,
    APP_TURN_SLIGHT_RIGHT,
    APP_TURN_RIGHT,
    APP_TURN_SHARP_RIGHT,
    APP_TURN_UTURN,
    APP_TURN_ARRIVE
};

/** Channel phone: navigation from the phone (Komoot) */
struct app_phone_nav {
    bool valid;
    uint16_t dist_m;
    uint8_t turn;               /**< enum app_turn */
    char street[40];
};

/* ==========================================================================
 * Energy
 * ========================================================================== */

/** Charging source, as ui_charge_t */
enum app_charge {
    APP_CHARGE_NONE = 0,
    APP_CHARGE_SOLAR,
    APP_CHARGE_USB,
    APP_CHARGE_USB_FULL
};

/** Channel power_status: battery and charge, on change and every 60 s */
struct app_power_status {
    bool gauge;                 /**< a fuel gauge answered: the values below are measured */
    uint16_t mv;
    int16_t ma;                 /**< negative when discharging */
    uint8_t pct;
    uint8_t charge;             /**< enum app_charge */
    bool vbus;
    uint16_t solar_mw;
    uint16_t solar_limit_mv;
    int8_t temp_c;
    uint16_t autonomy_h;
    bool critical;              /**< the gauge flags the end of the battery */
};

/* ==========================================================================
 * System
 * ========================================================================== */

/** Modes of the device, as ui_mode_t (legacy Boucle modes and the DBG screen) */
enum app_mode {
    APP_MODE_ID_CRS = 0,
    APP_MODE_ID_PRC,
    APP_MODE_ID_FEC,
    APP_MODE_ID_ZWIFT,
    APP_MODE_ID_DBG
};

/** Requests from the interface and the services, as ui_action_t */
enum app_cmd_id {
    APP_CMD_SET_MODE = 0,       /**< arg: enum app_mode */
    APP_CMD_SHUTDOWN,
    APP_CMD_PAIR_START,         /**< arg: enum app_ext_kind */
    APP_CMD_PAIR_SELECT,        /**< arg: index in the pairing list */
    APP_CMD_PAIR_CANCEL,
    APP_CMD_SET_FTP,            /**< arg: watts */
    APP_CMD_SET_WEIGHT,         /**< arg: kg */
    APP_CMD_CALIB_COMPASS,
    APP_CMD_GNSS_TOGGLE,        /**< LEAP or full power */
    APP_CMD_LIGHT_TOGGLE,
    APP_CMD_THEME_TOGGLE,
    APP_CMD_FORMAT,
    APP_CMD_ZOOM,               /**< arg: +1 closer, -1 farther */
    APP_CMD_KEY,                /**< any key: feeds the backlight */
    APP_CMD_ROUTE_SELECT,       /**< arg: index in the route list */
    APP_CMD_MSC,                /**< expose the card over USB */
    APP_CMD_STORAGE_RESCAN,     /**< a file arrived: list the storage again */
    APP_CMD_LAP                 /**< close the lap being ridden and start another */
};

/** Channel system_cmd */
struct app_system_cmd {
    uint8_t id;                 /**< enum app_cmd_id */
    int32_t arg;
};

/** System state, the machine of docs/16 (Sistema e energia) */
enum app_sys_state {
    APP_SYS_BOOT = 0,
    APP_SYS_ON,
    APP_SYS_MSC,
    APP_SYS_SHUTDOWN,           /**< services save and stop, then acknowledge */
    APP_SYS_OFF
};

/** Channel system_state: published by the energy service only */
struct app_system_state {
    uint8_t state;              /**< enum app_sys_state */
};

/** Channel mode: the mode in force, published by the model only */
struct app_mode_state {
    uint8_t mode;               /**< enum app_mode */
    bool route;                 /**< PRC with a route loaded */
    bool recording;             /**< an activity is running: positions are being logged */
};

/** Services that take part in the shutdown */
enum app_svc_id {
    APP_SVC_MODEL = 0,
    APP_SVC_GNSS,
    APP_SVC_SENSORS,
    APP_SVC_RADIO,
    APP_SVC_STORAGE,
    APP_SVC_UI,
    APP_SVC_USB,
    APP_SVC_COUNT
};

/** Channel shutdown_ack: a service finished its part of the shutdown */
struct app_shutdown_ack {
    uint8_t svc;                /**< enum app_svc_id */
};

/** Channel notif: a notification for the screen (legacy Vue::addNotif) */
struct app_notif {
    char title[16];
    char text[32];
    char value[16];
    bool good;
    uint16_t duration_ms;       /**< 0: the interface default */
};

/** Channel log_point: one model snapshot for the activity log, per epoch */
/**
 * Channel log_point: one point per epoch, with the nineteen fields the
 * legacy writes in `@DDMMYY.txt` (`legacy/source/sd/sd_functions.cpp:605-644`).
 */
struct app_log_point {
    loc_data_t loc;
    date_data_t date;
    uint32_t fit_time;          /**< FIT date_time of this epoch; 0 without a date */
    int16_t power_w;
    uint8_t hr_bpm;
    uint8_t cadence_rpm;
    float alpha_bar;            /**< pitch of the filter, rad */
    float alpha_zero;           /**< mounting offset of the accelerometer, rad */
    float baro_alt;             /**< barometer altitude */
    float baro_corr;            /**< GPS/barometer drift correction */
    float filt_alt;             /**< filtered altitude */
    float vit_asc;              /**< vertical speed, m/s */
    float rough[3];             /**< roughness of the accelerometer, legacy counts */
    float b_rough;              /**< roughness of the barometer, Pa */
    int8_t slope_pct;
    float dist_m;
    float climb_m;
};

/**
 * Totals of a lap or of the whole ride, as the FIT file needs them.
 *
 * The compact form of `struct activity_totals` (`model/activity.h`), with
 * the averages already worked out: the service that writes the file has no
 * business running the accumulator again.
 */
struct app_totals {
    uint32_t start_time;        /**< FIT date_time */
    uint32_t end_time;
    uint32_t elapsed_ms;        /**< wall time, pauses included */
    uint32_t timer_ms;          /**< moving time */
    float dist_m;
    float ascent_m;
    float descent_m;
    float avg_speed_kmh;
    float max_speed_kmh;
    uint16_t avg_power_w;
    uint16_t max_power_w;
    uint16_t calories_kcal;
    uint8_t avg_hr_bpm;
    uint8_t max_hr_bpm;
    uint8_t avg_cadence_rpm;
};

/** What the ride is doing, once per epoch (`model/activity.h`) */
struct app_activity {
    struct app_totals ride;     /**< always the totals so far */
    struct app_totals lap;      /**< the lap that closed, when event is LAP */
    float lap_dist_m;           /**< of the lap being ridden */
    uint32_t lap_timer_ms;
    uint16_t laps;              /**< laps already closed */
    uint8_t event;              /**< enum activity_event */
    bool running;               /**< the timer counts */
    bool finished;              /**< the ride ended: the file can be closed */
};

/**
 * Channel radar: one frame of a rear radar (`model/radar.h`).
 *
 * The frame goes whole, not one vehicle at a time, because a radar
 * reports everything it sees at once and a target missing from a frame
 * means it is gone.
 */
struct app_radar {
    uint32_t uptime_ms;
    uint8_t n;                  /**< vehicles in the frame */
    uint8_t id[8];
    uint16_t range_m[8];
    uint16_t closing_kmh[8];
    uint8_t level[8];           /**< enum radar_level; 0 means "work it out" */
    uint8_t side[8];
    bool linked;                /**< a radar is connected */
};

/** Keys of the device */
enum app_key {
    APP_KEY_LEFT = 0,
    APP_KEY_CENTER,
    APP_KEY_RIGHT
};

/** Channel input: one key press, short or long */
struct app_input {
    uint8_t key;                /**< enum app_key */
    bool long_press;
};

/** Routes listed in Modo PRC (ui_model.h UI_ROUTE_LIST_MAX) */
#define APP_ROUTE_LIST_MAX  10U

/** Channel storage_info: what the card holds */
/**
 * Channel dfu: how a firmware update over Bluetooth is going, published by
 * the radio service (`src/rf/dfu.c`, `model/dfu_state.c`).
 */
struct app_dfu {
    uint8_t phase;              /**< enum dfu_phase */
    uint8_t percent;            /**< 0 to 100, 0 while the size is unknown */
};

struct app_storage_info {
    bool mounted;
    uint16_t segments;          /**< segments loaded */
    uint8_t nroutes;
    char route[APP_ROUTE_LIST_MAX][20];
};

#ifdef __cplusplus
}
#endif

#endif /* APP_EVENTS_H */
