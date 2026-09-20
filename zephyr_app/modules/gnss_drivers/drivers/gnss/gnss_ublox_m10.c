/**
 * @file gnss_ublox_m10.c
 * @brief u-blox M10 and F10 receivers on a UART, by UBX
 *
 * The Zephyr of the NCS v3.3.0 has drivers for the u-blox M8 and F9P, not for
 * the M10: the M8 configures with the legacy UBX-CFG messages, which the M10
 * does not accept, and the F9P is an RTK receiver with buffers and messages
 * that this board does not need. This driver uses the UBX layer of Zephyr
 * (modem_ubx over a UART pipe) with the frames of ubx_m10.c, which the host
 * tests check against the u-blox documents.
 *
 * The board takes two parts in the same footprint, and the driver serves
 * both: the single-band MAX-M10N-10B ("u-blox,max-m10") and the dual-band
 * MAX-F10S ("u-blox,max-f10"), which is what the new board carries.
 *
 * | What | Where |
 * |---|---|
 * | Configuration by UBX-CFG-VALSET in the RAM and BBR layers | interface description 3.10.5, 5.3 |
 * | LEAP by CFG-PM-OPERATEMODE = 2, 1 Hz, without time pulse | integration manual 3.7.2 |
 * | Software standby by UBX-RXM-PMREQ, waking on the UART RX | integration manual 3.7.4.2 |
 * | One UBX-NAV-PVT per epoch, UBX-NAV-SAT for the sky plot | interface description 3.15.11, 3.15.13 |
 * | L1 and L5 together, NavIC, and no CFG-PM at all | F10 SPG 6.00 UBX-23002975 R02, 4.9.20 |
 *
 * The receiver may miss a message while it duty cycles in LEAP (integration
 * manual 3.7.2): every configuration goes out with retries, and the driver
 * takes the receiver to full power before a batch of them. On the F10 there
 * is no LEAP to leave: the part tracks at full power and the energy comes
 * back from the standby, which the GNSS service drives.
 */

#define DT_DRV_COMPAT u_blox_max_m10

#include "drivers/gnss/ublox_m10.h"
#include "drivers/gnss/ubx_m10.h"

#include <zephyr/drivers/gnss.h>
#include <zephyr/drivers/gnss/gnss_publish.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/modem/backend/uart.h>
#include <zephyr/modem/pipe.h>
#include <zephyr/modem/ubx.h>

LOG_MODULE_REGISTER(ublox_m10, CONFIG_GNSS_LOG_LEVEL);

/* A UBX-NAV-SAT with 32 satellites takes 8 + 32 x 12 + 8 = 400 bytes */
#define M10_RX_BUF_SIZE     512
#define M10_TX_BUF_SIZE     128
/* The largest request is the signal batch of the F10: seven keys of one byte
 * in one UBX-CFG-VALSET, 47 bytes. ubx_m10_valset_many() refuses to build a
 * frame that does not fit, so a batch that grows past this fails loudly. */
#define M10_REQ_BUF_SIZE    96
#define M10_RSP_BUF_SIZE    128

/* modem_ubx splits the timeout across the attempts: 4 x 250 ms here */
#define M10_SCRIPT_TIMEOUT  K_SECONDS(1)
#define M10_SCRIPT_RETRIES  3
#define M10_LOCK_TIMEOUT    K_SECONDS(2)
/** Not measured on the board: time for the receiver to boot after a wake-up */
#define M10_WAKEUP_MS       100
/** RESET_N is active low for at least 1 ms (integration manual, 4.2) */
#define M10_RESET_LOW_MS    2
/**
 * A change in the CFG-SIGNAL group resets the GNSS subsystem: "wait first for
 * the acknowledgement from the receiver and then 0.5 seconds before sending
 * the next command" (F10 SPG 6.00 interface description, 4.9.20).
 */
#define M10_SIGNAL_RESET_MS 500

struct m10_config {
    const struct device *uart;
    const struct gpio_dt_spec reset;
    const struct device *vcc;
    gnss_systems_t systems;     /**< what the part can receive */
    uint16_t fix_rate_ms;
    uint8_t dyn_model;
    uint8_t power_mode;
    uint8_t sat_rate;
    bool timepulse;
    bool dual_band;             /**< F10: L1 and L5 together, and no CFG-PM */
    bool navic;
};

struct m10_data {
    const struct device *dev;
    struct {
        struct modem_pipe *pipe;
        struct modem_backend_uart uart_backend;
        uint8_t receive_buf[M10_RX_BUF_SIZE];
        uint8_t transmit_buf[M10_TX_BUF_SIZE];
    } backend;
    struct {
        struct modem_ubx inst;
        uint8_t receive_buf[M10_RX_BUF_SIZE];
    } ubx;
    struct {
        struct modem_ubx_script inst;
        uint8_t request_buf[M10_REQ_BUF_SIZE];
        uint8_t response_buf[M10_RSP_BUF_SIZE];
        struct k_sem lock;
    } script;
    uint8_t power_mode;     /**< enum ublox_m10_power_mode, what was applied */
    uint8_t psm_state;      /**< psmState of the last epoch */
    bool standby;
#if defined(CONFIG_GNSS_SATELLITES)
    struct gnss_satellite satellites[CONFIG_GNSS_UBLOX_M10_SATELLITES];
    struct ubx_m10_sat parsed[CONFIG_GNSS_UBLOX_M10_SATELLITES];
#endif
};

/* Messages that arrive on their own */

static void pvt_handler(struct modem_ubx *ubx, const struct ubx_frame *frame, size_t len,
                        void *user_data)
{
    ARG_UNUSED(ubx);

    struct m10_data *data = user_data;
    struct ubx_m10_pvt pvt;

    if (len < UBX_M10_FRAME_LEN(UBX_M10_NAV_PVT_LEN)) {
        return;
    }
    if (!ubx_m10_parse_pvt(frame->payload_and_checksum, len - UBX_M10_OVERHEAD, &pvt)) {
        return;
    }

    data->psm_state = pvt.psm_state;

    struct gnss_data out = {0};

    if (pvt.fix_ok && !pvt.llh_invalid) {
        switch (pvt.fix_type) {
        case UBX_M10_FIX_2D:
        case UBX_M10_FIX_3D:
            out.info.fix_status = GNSS_FIX_STATUS_GNSS_FIX;
            out.info.fix_quality = GNSS_FIX_QUALITY_GNSS_SPS;
            break;
        case UBX_M10_FIX_DEAD_RECKONING:
        case UBX_M10_FIX_GNSS_DR:
            out.info.fix_status = GNSS_FIX_STATUS_ESTIMATED_FIX;
            out.info.fix_quality = GNSS_FIX_QUALITY_ESTIMATED;
            break;
        default:
            break;
        }
    }

    /* the receiver gives pDOP; the horizontal alone would need UBX-NAV-DOP */
    out.info.satellites_cnt = pvt.num_sv;
    out.info.hdop = (uint32_t)pvt.pdop_e2 * 10U;
    out.info.geoid_separation = pvt.height_mm - pvt.hmsl_mm;

    int32_t bearing = pvt.head_motion_e5 / 100; /* 1e-5 degree to millidegree */

    if (bearing < 0) {
        bearing += 360000;
    }
    out.nav_data.latitude = (int64_t)pvt.lat_e7 * 100;   /* 1e-7 degree to nanodegree */
    out.nav_data.longitude = (int64_t)pvt.lon_e7 * 100;
    out.nav_data.bearing = (uint32_t)bearing;
    out.nav_data.speed = (pvt.ground_speed_mms > 0) ? (uint32_t)pvt.ground_speed_mms : 0U;
    out.nav_data.altitude = pvt.hmsl_mm;

    if (pvt.date_valid && pvt.time_valid) {
        int32_t ms = ((int32_t)pvt.second * 1000) + (pvt.nano / 1000000);

        if (ms < 0) {
            ms = 0;
        } else if (ms > 60999) {
            ms = 60999;
        } else {
            /* inside the range of struct gnss_time */
        }
        out.utc.hour = pvt.hour;
        out.utc.minute = pvt.minute;
        out.utc.millisecond = (uint16_t)ms;
        out.utc.month_day = pvt.day;
        out.utc.month = pvt.month;
        out.utc.century_year = (uint8_t)(pvt.year % 100U);
    }

    gnss_publish_data(data->dev, &out);
}

#if defined(CONFIG_GNSS_SATELLITES)

static enum gnss_system system_of(uint8_t gnss_id)
{
    switch (gnss_id) {
    case UBX_M10_GNSS_SBAS:
        return GNSS_SYSTEM_SBAS;
    case UBX_M10_GNSS_GALILEO:
        return GNSS_SYSTEM_GALILEO;
    case UBX_M10_GNSS_BEIDOU:
        return GNSS_SYSTEM_BEIDOU;
    case UBX_M10_GNSS_QZSS:
        return GNSS_SYSTEM_QZSS;
    case UBX_M10_GNSS_GLONASS:
        return GNSS_SYSTEM_GLONASS;
    case UBX_M10_GNSS_NAVIC:
        return GNSS_SYSTEM_IRNSS;
    default:
        return GNSS_SYSTEM_GPS;
    }
}

static void sat_handler(struct modem_ubx *ubx, const struct ubx_frame *frame, size_t len,
                        void *user_data)
{
    ARG_UNUSED(ubx);

    struct m10_data *data = user_data;

    if (len < UBX_M10_FRAME_LEN(UBX_M10_NAV_SAT_HDR)) {
        return;
    }

    int n = ubx_m10_parse_sat(frame->payload_and_checksum, len - UBX_M10_OVERHEAD, data->parsed,
                              (int)ARRAY_SIZE(data->parsed));

    if (n <= 0) {
        return;
    }

    for (int i = 0; i < n; i++) {
        int16_t azimuth = data->parsed[i].azim_deg;
        int8_t elevation = data->parsed[i].elev_deg;

        while (azimuth < 0) {
            azimuth += 360;
        }
        data->satellites[i] = (struct gnss_satellite){
            .prn = data->parsed[i].sv_id,
            .snr = data->parsed[i].cno_dbhz,
            .elevation = (elevation > 0) ? (uint8_t)elevation : 0U,
            .azimuth = (uint16_t)(azimuth % 360),
            .system = system_of(data->parsed[i].gnss_id),
            .is_tracked = data->parsed[i].used ? 1U : 0U,
        };
    }

    gnss_publish_satellites(data->dev, data->satellites, (uint16_t)n);
}

#endif /* CONFIG_GNSS_SATELLITES */

MODEM_UBX_MATCH_ARRAY_DEFINE(m10_unsol_messages,
    MODEM_UBX_MATCH_DEFINE(UBX_M10_CLASS_NAV, UBX_M10_NAV_PVT, pvt_handler),
#if defined(CONFIG_GNSS_SATELLITES)
    MODEM_UBX_MATCH_DEFINE(UBX_M10_CLASS_NAV, UBX_M10_NAV_SAT, sat_handler),
#endif
);

/* Frames out */

/**
 * Send a frame already in data->script.request_buf and wait for the answer
 * the caller asks for. Never call from the system work queue: the answer
 * arrives in a work item of that queue.
 */
static int run_script(const struct device *dev, size_t len, uint8_t match_class, uint8_t match_id,
                      uint16_t retries)
{
    struct m10_data *data = dev->data;
    int err;

    if (data->standby) {
        return -EAGAIN;
    }

    err = k_sem_take(&data->script.lock, M10_LOCK_TIMEOUT);
    if (err != 0) {
        return err;
    }

    data->script.inst.request.buf = data->script.request_buf;
    data->script.inst.request.len = (uint16_t)len;
    data->script.inst.response.buf = data->script.response_buf;
    data->script.inst.response.buf_len = sizeof(data->script.response_buf);
    data->script.inst.response.received_len = 0U;
    data->script.inst.match.filter.class = match_class;
    data->script.inst.match.filter.id = match_id;
    data->script.inst.match.filter.payload.buf = NULL;
    data->script.inst.match.filter.payload.len = 0U;
    data->script.inst.match.handler = NULL;
    data->script.inst.retry_count = retries;
    data->script.inst.timeout = M10_SCRIPT_TIMEOUT;

    err = modem_ubx_run_script(&data->ubx.inst, &data->script.inst);

    k_sem_give(&data->script.lock);

    return err;
}

/** Write one configuration key in the RAM and BBR layers and wait for the ack */
static int valset(const struct device *dev, uint32_t key, uint64_t value)
{
    struct m10_data *data = dev->data;
    size_t len = ubx_m10_valset(data->script.request_buf, sizeof(data->script.request_buf), key,
                                value, UBX_M10_LAYER_RAM | UBX_M10_LAYER_BBR);

    if (len == 0U) {
        return -EINVAL;
    }

    int err = run_script(dev, len, UBX_M10_CLASS_ACK, UBX_M10_ACK_ACK, M10_SCRIPT_RETRIES);

    if (err != 0) {
        LOG_ERR("key 0x%08x not acknowledged: %d", key, err);
    }

    return err;
}

/** Write several keys as one message, which the receiver applies together */
static int valset_many(const struct device *dev, const struct ubx_m10_kv *items, size_t count)
{
    struct m10_data *data = dev->data;
    size_t len = ubx_m10_valset_many(data->script.request_buf, sizeof(data->script.request_buf),
                                     items, count, UBX_M10_LAYER_RAM | UBX_M10_LAYER_BBR);

    if (len == 0U) {
        return -EINVAL;
    }

    int err = run_script(dev, len, UBX_M10_CLASS_ACK, UBX_M10_ACK_ACK, M10_SCRIPT_RETRIES);

    if (err != 0) {
        LOG_ERR("batch of %u keys not acknowledged: %d", (unsigned int)count, err);
    }

    return err;
}

/** Read one configuration key of the layer in use */
static int valget(const struct device *dev, uint32_t key, uint64_t *value)
{
    struct m10_data *data = dev->data;
    size_t len = ubx_m10_valget(data->script.request_buf, sizeof(data->script.request_buf), key);

    if (len == 0U) {
        return -EINVAL;
    }

    int err = run_script(dev, len, UBX_M10_CLASS_CFG, UBX_M10_CFG_VALGET, M10_SCRIPT_RETRIES);

    if (err != 0) {
        return err;
    }

    const struct ubx_frame *rsp = (const struct ubx_frame *)data->script.response_buf;
    size_t rsp_len = data->script.inst.response.received_len;

    if (rsp_len < UBX_M10_FRAME_LEN(UBX_M10_VALSET_HDR + 4U)) {
        return -EIO;
    }
    if (!ubx_m10_valget_parse(rsp->payload_and_checksum, rsp_len - UBX_M10_OVERHEAD, key, value)) {
        return -EIO;
    }

    return 0;
}

/* Zephyr GNSS API */

static int m10_set_fix_rate(const struct device *dev, uint32_t fix_interval_ms)
{
    /* the receiver takes 25 ms to 65535 ms; LEAP goes down to 2 Hz */
    if ((fix_interval_ms < 25U) || (fix_interval_ms > 65535U)) {
        return -EINVAL;
    }

    return valset(dev, UBX_M10_KEY_RATE_MEAS, fix_interval_ms);
}

static int m10_get_fix_rate(const struct device *dev, uint32_t *fix_interval_ms)
{
    uint64_t value = 0U;
    int err = valget(dev, UBX_M10_KEY_RATE_MEAS, &value);

    if (err == 0) {
        *fix_interval_ms = (uint32_t)value;
    }

    return err;
}

static uint8_t dyn_model_of(enum gnss_navigation_mode mode)
{
    switch (mode) {
    case GNSS_NAVIGATION_MODE_ZERO_DYNAMICS:
        return UBX_M10_DYN_STATIONARY;
    case GNSS_NAVIGATION_MODE_LOW_DYNAMICS:
        return UBX_M10_DYN_PEDESTRIAN;
    case GNSS_NAVIGATION_MODE_HIGH_DYNAMICS:
        return UBX_M10_DYN_AUTOMOTIVE;
    default:
        return UBX_M10_DYN_PORTABLE;
    }
}

static int m10_set_navigation_mode(const struct device *dev, enum gnss_navigation_mode mode)
{
    return valset(dev, UBX_M10_KEY_NAVSPG_DYNMODEL, dyn_model_of(mode));
}

static int m10_get_navigation_mode(const struct device *dev, enum gnss_navigation_mode *mode)
{
    uint64_t value = 0U;
    int err = valget(dev, UBX_M10_KEY_NAVSPG_DYNMODEL, &value);

    if (err != 0) {
        return err;
    }

    switch ((uint8_t)value) {
    case UBX_M10_DYN_STATIONARY:
        *mode = GNSS_NAVIGATION_MODE_ZERO_DYNAMICS;
        break;
    case UBX_M10_DYN_PEDESTRIAN:
    case UBX_M10_DYN_WRIST:
        *mode = GNSS_NAVIGATION_MODE_LOW_DYNAMICS;
        break;
    case UBX_M10_DYN_PORTABLE:
        *mode = GNSS_NAVIGATION_MODE_BALANCED_DYNAMICS;
        break;
    default:
        *mode = GNSS_NAVIGATION_MODE_HIGH_DYNAMICS;
        break;
    }

    return 0;
}

/** Both parts receive GPS, Galileo and BeiDou at the same time, with QZSS and SBAS */
#define M10_SYSTEMS                                                                                \
    (GNSS_SYSTEM_GPS | GNSS_SYSTEM_GALILEO | GNSS_SYSTEM_BEIDOU | GNSS_SYSTEM_QZSS |               \
     GNSS_SYSTEM_SBAS)
/** The F10 adds NavIC, on L5 (F10 interface description, table 46) */
#define F10_SYSTEMS (M10_SYSTEMS | GNSS_SYSTEM_IRNSS)

static const struct {
    gnss_systems_t system;
    uint32_t key;
} m10_system_keys[] = {
    {GNSS_SYSTEM_GPS, UBX_M10_KEY_SIGNAL_GPS_ENA},
    {GNSS_SYSTEM_SBAS, UBX_M10_KEY_SIGNAL_SBAS_ENA},
    {GNSS_SYSTEM_GALILEO, UBX_M10_KEY_SIGNAL_GAL_ENA},
    {GNSS_SYSTEM_BEIDOU, UBX_M10_KEY_SIGNAL_BDS_ENA},
    {GNSS_SYSTEM_QZSS, UBX_M10_KEY_SIGNAL_QZSS_ENA},
    {GNSS_SYSTEM_IRNSS, UBX_M10_KEY_SIGNAL_NAVIC_ENA},
};

/**
 * Write the constellations the caller asked for, in one message. A key that
 * the running firmware does not have makes the receiver NAK the whole message
 * and apply nothing (interface description 3.10.5), so NavIC only goes in the
 * batch on a dual-band part.
 */
static int m10_write_systems(const struct device *dev, gnss_systems_t systems)
{
    const struct m10_config *cfg = dev->config;
    struct ubx_m10_kv items[ARRAY_SIZE(m10_system_keys)];
    size_t count = 0U;
    int err;

    for (size_t i = 0U; i < ARRAY_SIZE(m10_system_keys); i++) {
        if ((m10_system_keys[i].system & cfg->systems) == 0U) {
            continue;
        }

        items[count].key = m10_system_keys[i].key;
        items[count].value = ((systems & m10_system_keys[i].system) != 0U) ? 1U : 0U;
        count++;
    }

    err = valset_many(dev, items, count);
    if (err != 0) {
        return err;
    }

    /* the group resets the GNSS subsystem (interface description 4.9.20) */
    k_msleep(M10_SIGNAL_RESET_MS);

    return 0;
}

static int m10_set_enabled_systems(const struct device *dev, gnss_systems_t systems)
{
    const struct m10_config *cfg = dev->config;

    if ((systems & ~cfg->systems) != 0U) {
        return -ENOTSUP; /* neither part has GLONASS */
    }

    return m10_write_systems(dev, systems);
}

static int m10_get_enabled_systems(const struct device *dev, gnss_systems_t *systems)
{
    const struct m10_config *cfg = dev->config;
    gnss_systems_t enabled = 0U;

    for (size_t i = 0U; i < ARRAY_SIZE(m10_system_keys); i++) {
        uint64_t value = 0U;
        int err;

        if ((m10_system_keys[i].system & cfg->systems) == 0U) {
            continue;
        }

        err = valget(dev, m10_system_keys[i].key, &value);
        if (err != 0) {
            return err;
        }
        if (value != 0U) {
            enabled |= m10_system_keys[i].system;
        }
    }
    *systems = enabled;

    return 0;
}

static int m10_get_supported_systems(const struct device *dev, gnss_systems_t *systems)
{
    const struct m10_config *cfg = dev->config;

    *systems = cfg->systems;

    return 0;
}

static DEVICE_API(gnss, m10_api) = {
    .set_fix_rate = m10_set_fix_rate,
    .get_fix_rate = m10_get_fix_rate,
    .set_navigation_mode = m10_set_navigation_mode,
    .get_navigation_mode = m10_get_navigation_mode,
    .set_enabled_systems = m10_set_enabled_systems,
    .get_enabled_systems = m10_get_enabled_systems,
    .get_supported_systems = m10_get_supported_systems,
};

/* What this driver adds */

int ublox_m10_set_power_mode(const struct device *dev, enum ublox_m10_power_mode mode)
{
    const struct m10_config *cfg = dev->config;
    struct m10_data *data = dev->data;
    int err;

    if (cfg->dual_band) {
        /*
         * The F10 firmware has no CFG-PM group at all (F10 SPG 6.00
         * interface description, 4.8): the key would come back NAKed. The
         * part only tracks at full power, so asking for that is not an
         * error; asking for LEAP is.
         */
        return (mode == UBLOX_M10_POWER_FULL) ? 0 : -ENOTSUP;
    }

    err = valset(dev, UBX_M10_KEY_PM_OPERATEMODE, (uint64_t)mode);
    if (err == 0) {
        data->power_mode = (uint8_t)mode;
    }

    return err;
}

enum ublox_m10_power_mode ublox_m10_get_power_mode(const struct device *dev)
{
    const struct m10_data *data = dev->data;

    return (enum ublox_m10_power_mode)data->power_mode;
}

uint8_t ublox_m10_psm_state(const struct device *dev)
{
    const struct m10_data *data = dev->data;

    return data->psm_state;
}

/**
 * Put the signals of a dual-band receiver where this board wants them.
 *
 * The receiver leaves the factory with L1 and L5 on for GPS, Galileo and
 * BeiDou (F10 SPG 6.00 interface description, Configuration defaults), but
 * the software standby clears the RAM and the driver writes every other
 * setting explicitly, so it writes these too: a receiver whose BBR was left
 * in another state comes back the same way every time.
 *
 * GPS L5 goes on because the firmware has it on; the satellites still
 * broadcast as unhealthy and stay out of the solution until the signal
 * becomes operational (MAX-F10S data sheet, 1.1). The gain today is Galileo
 * E5a and BeiDou B2a. There is nothing to choose: the part does not do one
 * band alone.
 */
static int m10_configure_signals(const struct device *dev)
{
    const struct m10_config *cfg = dev->config;
    const uint64_t navic = cfg->navic ? 1U : 0U;
    const struct ubx_m10_kv signals[] = {
        {UBX_M10_KEY_SIGNAL_GPS_L1CA_ENA, 1U},  {UBX_M10_KEY_SIGNAL_GPS_L5_ENA, 1U},
        {UBX_M10_KEY_SIGNAL_GAL_E1_ENA, 1U},    {UBX_M10_KEY_SIGNAL_GAL_E5A_ENA, 1U},
        {UBX_M10_KEY_SIGNAL_BDS_B1C_ENA, 1U},   {UBX_M10_KEY_SIGNAL_BDS_B2A_ENA, 1U},
        {UBX_M10_KEY_SIGNAL_SBAS_L1CA_ENA, 1U}, {UBX_M10_KEY_SIGNAL_NAVIC_ENA, navic},
        {UBX_M10_KEY_SIGNAL_NAVIC_L5_ENA, navic},
    };
    /* one message, so the subsystem resets once and not nine times */
    int err = valset_many(dev, signals, ARRAY_SIZE(signals));

    if (err != 0) {
        return err;
    }
    k_msleep(M10_SIGNAL_RESET_MS);

    /* the constellation enables stay where the receiver put them, as on the
     * M10: GPS, SBAS, Galileo and BeiDou on, QZSS off (Configuration
     * defaults). The rider changes them through the Zephyr GNSS API. */
    return 0;
}

int ublox_m10_configure(const struct device *dev)
{
    const struct m10_config *cfg = dev->config;
    struct m10_data *data = dev->data;
    static const uint32_t nmea_off[] = {
        UBX_M10_KEY_MSGOUT_NMEA_DTM_UART1, UBX_M10_KEY_MSGOUT_NMEA_GBS_UART1,
        UBX_M10_KEY_MSGOUT_NMEA_GGA_UART1, UBX_M10_KEY_MSGOUT_NMEA_GLL_UART1,
        UBX_M10_KEY_MSGOUT_NMEA_GNS_UART1, UBX_M10_KEY_MSGOUT_NMEA_GRS_UART1,
        UBX_M10_KEY_MSGOUT_NMEA_GSA_UART1, UBX_M10_KEY_MSGOUT_NMEA_GST_UART1,
        UBX_M10_KEY_MSGOUT_NMEA_GSV_UART1, UBX_M10_KEY_MSGOUT_NMEA_RLM_UART1,
        UBX_M10_KEY_MSGOUT_NMEA_RMC_UART1, UBX_M10_KEY_MSGOUT_NMEA_VLW_UART1,
        UBX_M10_KEY_MSGOUT_NMEA_VTG_UART1, UBX_M10_KEY_MSGOUT_NMEA_ZDA_UART1,
    };
    int err;

    if (data->standby) {
        return -EAGAIN;
    }

    /* full power first: in LEAP the receiver may miss what the host sends */
    err = ublox_m10_set_power_mode(dev, UBLOX_M10_POWER_FULL);
    if (err != 0) {
        return err;
    }

    err = valset(dev, UBX_M10_KEY_UART1INPROT_UBX, 1U);
    err = err ? err : valset(dev, UBX_M10_KEY_UART1OUTPROT_UBX, 1U);
    err = err ? err : valset(dev, UBX_M10_KEY_UART1INPROT_NMEA, 0U);
    err = err ? err : valset(dev, UBX_M10_KEY_UART1OUTPROT_NMEA, 0U);
    if (err != 0) {
        return err;
    }

    for (size_t i = 0U; i < ARRAY_SIZE(nmea_off); i++) {
        err = valset(dev, nmea_off[i], 0U);
        if (err != 0) {
            return err;
        }
    }

    err = valset(dev, UBX_M10_KEY_MSGOUT_NAV_PVT_UART1, 1U);
    err = err ? err : valset(dev, UBX_M10_KEY_RATE_MEAS, cfg->fix_rate_ms);
    err = err ? err : valset(dev, UBX_M10_KEY_RATE_NAV, 1U);
    err = err ? err : valset(dev, UBX_M10_KEY_NAVSPG_DYNMODEL, cfg->dyn_model);
    err = err ? err : valset(dev, UBX_M10_KEY_TP_TP1_ENA, cfg->timepulse ? 1U : 0U);
#if defined(CONFIG_GNSS_SATELLITES)
    err = err ? err : valset(dev, UBX_M10_KEY_MSGOUT_NAV_SAT_UART1, cfg->sat_rate);
#else
    err = err ? err : valset(dev, UBX_M10_KEY_MSGOUT_NAV_SAT_UART1, 0U);
#endif
    if (err != 0) {
        return err;
    }

    if (cfg->dual_band) {
        err = m10_configure_signals(dev);
        if (err != 0) {
            return err;
        }
    }

    if (cfg->power_mode != UBLOX_M10_POWER_FULL) {
        err = ublox_m10_set_power_mode(dev, (enum ublox_m10_power_mode)cfg->power_mode);
    }

    return err;
}

int ublox_m10_standby(const struct device *dev)
{
    const struct m10_config *cfg = dev->config;
    struct m10_data *data = dev->data;
    size_t len;
    int err;

    if (data->standby) {
        return 0;
    }

    /* the receiver does not answer this one: no match, no retry */
    len = ubx_m10_pmreq(data->script.request_buf, sizeof(data->script.request_buf), 0U, true, true,
                        UBX_M10_PMREQ_WAKE_UARTRX);
    if (len == 0U) {
        return -EINVAL;
    }

    err = run_script(dev, len, 0U, 0U, 0U);
    if (err != 0) {
        LOG_WRN("standby not sent: %d", err);
    }
    data->standby = true;

    if (cfg->vcc != NULL) {
        /* without the rail the receiver keeps only the backup domain */
        (void)regulator_disable(cfg->vcc);
    }

    return err;
}

int ublox_m10_wake(const struct device *dev)
{
    const struct m10_config *cfg = dev->config;
    struct m10_data *data = dev->data;
    const uint8_t wakeup[] = {0xFFU, 0xFFU}; /* not a UBX preamble: only the edge counts */

    if (!data->standby) {
        return 0;
    }

    if (cfg->vcc != NULL) {
        int err = regulator_enable(cfg->vcc);

        if (err != 0) {
            return err;
        }
    } else {
        (void)modem_pipe_transmit(data->backend.pipe, wakeup, sizeof(wakeup));
    }

    k_msleep(M10_WAKEUP_MS);
    data->standby = false;
    data->psm_state = UBX_M10_PSM_OFF;
    data->power_mode = UBLOX_M10_POWER_FULL; /* the standby cleared the RAM of the receiver */

    return 0;
}

bool ublox_m10_is_standby(const struct device *dev)
{
    const struct m10_data *data = dev->data;

    return data->standby;
}

int ublox_m10_restart(const struct device *dev, enum ublox_m10_restart kind)
{
    struct m10_data *data = dev->data;
    uint16_t bbr;
    size_t len;

    switch (kind) {
    case UBLOX_M10_RESTART_COLD:
        bbr = UBX_M10_RST_COLD;
        break;
    case UBLOX_M10_RESTART_WARM:
        bbr = UBX_M10_RST_WARM;
        break;
    default:
        bbr = UBX_M10_RST_HOT;
        break;
    }

    len = ubx_m10_cfg_rst(data->script.request_buf, sizeof(data->script.request_buf), bbr, 0x00U);
    if (len == 0U) {
        return -EINVAL;
    }

    /* UBX-CFG-RST is not acknowledged: the receiver resets */
    return run_script(dev, len, 0U, 0U, 0U);
}

int ublox_m10_hw_reset(const struct device *dev)
{
    const struct m10_config *cfg = dev->config;
    struct m10_data *data = dev->data;
    int err;

    if (cfg->reset.port == NULL) {
        return -ENOTSUP;
    }

    err = gpio_pin_set_dt(&cfg->reset, 1); /* active low in the devicetree */
    if (err != 0) {
        return err;
    }
    k_msleep(M10_RESET_LOW_MS);
    (void)gpio_pin_set_dt(&cfg->reset, 0);
    k_msleep(M10_WAKEUP_MS);

    data->standby = false;
    data->psm_state = UBX_M10_PSM_OFF;
    data->power_mode = UBLOX_M10_POWER_FULL;

    return 0;
}

/* Init */

static int m10_init(const struct device *dev)
{
    const struct m10_config *cfg = dev->config;
    struct m10_data *data = dev->data;
    int err;

    data->dev = dev;
    data->power_mode = UBLOX_M10_POWER_FULL;
    k_sem_init(&data->script.lock, 1, 1);

    if (cfg->reset.port != NULL) {
        err = gpio_pin_configure_dt(&cfg->reset, GPIO_OUTPUT_INACTIVE);
        if (err != 0) {
            LOG_ERR("reset pin: %d", err);
            return err;
        }
    }

    if (cfg->vcc != NULL) {
        err = regulator_enable(cfg->vcc);
        if (err != 0) {
            LOG_ERR("vcc: %d", err);
            return err;
        }
    }

    const struct modem_ubx_config ubx_config = {
        .user_data = data,
        .receive_buf = data->ubx.receive_buf,
        .receive_buf_size = sizeof(data->ubx.receive_buf),
        .unsol_matches = {
            .array = m10_unsol_messages,
            .size = ARRAY_SIZE(m10_unsol_messages),
        },
    };
    const struct modem_backend_uart_config uart_config = {
        .uart = cfg->uart,
        .receive_buf = data->backend.receive_buf,
        .receive_buf_size = sizeof(data->backend.receive_buf),
        .transmit_buf = data->backend.transmit_buf,
        .transmit_buf_size = sizeof(data->backend.transmit_buf),
    };

    err = modem_ubx_init(&data->ubx.inst, &ubx_config);
    if (err != 0) {
        return err;
    }

    /*
     * modem_ubx compares every frame with the filter of the running script,
     * without checking whether there is one: a receiver already configured in
     * the BBR layer answers before the first script. The empty filter below,
     * with class 0, never matches a frame.
     */
    data->script.inst.response.buf = data->script.response_buf;
    data->script.inst.response.buf_len = sizeof(data->script.response_buf);
    data->ubx.inst.script = &data->script.inst;

    data->backend.pipe = modem_backend_uart_init(&data->backend.uart_backend, &uart_config);
    err = modem_pipe_open(data->backend.pipe, K_SECONDS(1));
    if (err != 0) {
        LOG_ERR("pipe: %d", err);
        return err;
    }

    err = modem_ubx_attach(&data->ubx.inst, data->backend.pipe);
    if (err != 0) {
        LOG_ERR("attach: %d", err);
    }

    /* the configuration goes out from a thread: ublox_m10_configure() */
    return err;
}

#define M10_VCC(inst)                                                                                  COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, vcc_supply),                                                           (DEVICE_DT_GET(DT_INST_PHANDLE(inst, vcc_supply))), (NULL))

#define M10_DYN_MODEL(inst) DT_INST_STRING_UPPER_TOKEN(inst, dynamic_model)
#define M10_POWER_MODE(inst) DT_INST_STRING_UPPER_TOKEN(inst, power_mode)

/** What both parts share; the variant fills in bands, systems and power mode */
#define UBLOX_GNSS_COMMON(inst)                                                                    \
    .uart = DEVICE_DT_GET(DT_INST_BUS(inst)), .reset = GPIO_DT_SPEC_INST_GET_OR(inst,              \
                                                                               reset_gpios, {0}),  \
    .vcc = M10_VCC(inst), .fix_rate_ms = DT_INST_PROP(inst, fix_rate_ms),                          \
    .dyn_model = _CONCAT(UBX_M10_DYN_, M10_DYN_MODEL(inst)),                                       \
    .sat_rate = DT_INST_PROP(inst, satellites_rate),                                               \
    .timepulse = DT_INST_PROP(inst, timepulse_enable)

/* the initializer carries commas: it has to arrive as __VA_ARGS__ */
#define UBLOX_GNSS_DEVICE(inst, ...)                                                               \
    BUILD_ASSERT(DT_INST_PROP(inst, fix_rate_ms) >= 25 &&                                          \
                     DT_INST_PROP(inst, fix_rate_ms) <= 65535,                                     \
                 "fix-rate-ms outside the range of the receiver");                                 \
                                                                                                   \
    static const struct m10_config m10_cfg_##inst = __VA_ARGS__;                                   \
                                                                                                   \
    static struct m10_data m10_data_##inst;                                                        \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(inst, m10_init, NULL, &m10_data_##inst, &m10_cfg_##inst, POST_KERNEL,    \
                          CONFIG_GNSS_INIT_PRIORITY, &m10_api);

#define UBLOX_M10(inst)                                                                            \
    UBLOX_GNSS_DEVICE(inst, {                                                                      \
        UBLOX_GNSS_COMMON(inst),                                                                   \
        .systems = M10_SYSTEMS,                                                                    \
        .power_mode = _CONCAT(UBLOX_M10_POWER_, M10_POWER_MODE(inst)),                             \
        .dual_band = false,                                                                        \
        .navic = false,                                                                            \
    })

DT_INST_FOREACH_STATUS_OKAY(UBLOX_M10)

/*
 * The MAX-F10S: same frames and same driver, dual band, no CFG-PM group and
 * NavIC on top. The board takes either part in the same footprint, so both
 * compatibles live in this file.
 */
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT u_blox_max_f10

#define UBLOX_F10(inst)                                                                            \
    UBLOX_GNSS_DEVICE(inst, {                                                                      \
        UBLOX_GNSS_COMMON(inst),                                                                   \
        .systems = F10_SYSTEMS,                                                                    \
        .power_mode = UBLOX_M10_POWER_FULL,                                                        \
        .dual_band = true,                                                                         \
        .navic = DT_INST_PROP(inst, navic),                                                        \
    })

DT_INST_FOREACH_STATUS_OKAY(UBLOX_F10)
