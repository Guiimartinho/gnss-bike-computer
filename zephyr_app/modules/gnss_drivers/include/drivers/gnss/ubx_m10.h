/**
 * @file ubx_m10.h
 * @brief UBX frames and messages of the u-blox M10 and F10, in plain C
 *
 * Plain C, no Zephyr, so that the host tests check it byte by byte against
 * the documents. Everything here comes from "u-blox M10 SPG 5.30 Interface
 * description" (UBXDOC-304424225-20395, R01), the firmware of the
 * MAX-M10N-10B, from "MAX-M10N Integration manual"
 * (UBXDOC-304424225-19802, R03), and from "u-blox F10 SPG 6.00 Interface
 * description" (UBX-23002975, R02), the firmware of the MAX-F10S.
 *
 * The board takes either module in the same footprint. The frames, the
 * configuration keys of the UART, of the rate, of the dynamic model and of
 * the message output, and the five constellation enables are the same on the
 * two firmwares; what changes is the L5 signals and NavIC, which only the F10
 * has, and the CFG-PM group, which the F10 does not have at all.
 *
 * | Item | Where |
 * |---|---|
 * | Frame: 0xB5 0x62, class, id, length (U2), payload, CK_A, CK_B | 3.2 UBX frame structure |
 * | Checksum: 8-bit Fletcher over class, id, length and payload | 3.4 UBX checksum |
 * | Configuration key ID: bits 30..28 give the storage size | 5.2 Configuration items |
 * | UBX-CFG-VALSET (0x06 0x8A): version, layers, reserved, key-value pairs | 3.10.5 |
 * | UBX-RXM-PMREQ (0x02 0x41): version, reserved, duration, flags, wakeupSources | 3.16.7 |
 * | UBX-NAV-PVT (0x01 0x07), 92 bytes | 3.15.11 |
 * | UBX-NAV-SAT (0x01 0x35), 8 + 12 per satellite | 3.15.13 |
 * | LEAP is CFG-PM-OPERATEMODE = 2, full power is 0 | Integration manual 3.7.2.3 |
 */

#ifndef UBX_M10_H
#define UBX_M10_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Frame (3.2) */
#define UBX_M10_SYNC1           0xB5U
#define UBX_M10_SYNC2           0x62U
#define UBX_M10_HEADER_LEN      6U      /**< sync, sync, class, id, length */
#define UBX_M10_CHECKSUM_LEN    2U
#define UBX_M10_OVERHEAD        (UBX_M10_HEADER_LEN + UBX_M10_CHECKSUM_LEN)
#define UBX_M10_FRAME_LEN(payload_len)  ((payload_len) + UBX_M10_OVERHEAD)

/* Classes and messages used by the driver */
#define UBX_M10_CLASS_NAV       0x01U
#define UBX_M10_NAV_PVT         0x07U
#define UBX_M10_NAV_SAT         0x35U
#define UBX_M10_CLASS_RXM       0x02U
#define UBX_M10_RXM_PMREQ       0x41U
#define UBX_M10_CLASS_ACK       0x05U
#define UBX_M10_ACK_NAK         0x00U
#define UBX_M10_ACK_ACK         0x01U
#define UBX_M10_CLASS_CFG       0x06U
#define UBX_M10_CFG_RST         0x04U
#define UBX_M10_CFG_VALSET      0x8AU
#define UBX_M10_CFG_VALGET      0x8BU
#define UBX_M10_CLASS_MON       0x0AU
#define UBX_M10_MON_VER         0x04U
#define UBX_M10_MON_GNSS        0x28U

/* Payload sizes */
#define UBX_M10_NAV_PVT_LEN     92U
#define UBX_M10_NAV_SAT_HDR     8U
#define UBX_M10_NAV_SAT_ITEM    12U
#define UBX_M10_PMREQ_LEN       16U
#define UBX_M10_VALSET_HDR      4U
#define UBX_M10_VALSET_MAX      (UBX_M10_VALSET_HDR + 4U + 8U)
/** Keys ubx_m10_valset_many() takes in one message; the receiver allows 64 */
#define UBX_M10_VALSET_KEYS_MAX 12U
/** Payload of the largest batch, with every key of one byte */
#define UBX_M10_VALSET_BATCH    (UBX_M10_VALSET_HDR + (UBX_M10_VALSET_KEYS_MAX * (4U + 8U)))

/* Configuration keys (5.9), with the storage size in bits 30..28 */
#define UBX_M10_KEY_UART1_BAUDRATE          0x40520001U /**< U4, default 38400 */
#define UBX_M10_KEY_UART1INPROT_UBX         0x10730001U /**< L */
#define UBX_M10_KEY_UART1INPROT_NMEA        0x10730002U /**< L */
#define UBX_M10_KEY_UART1OUTPROT_UBX        0x10740001U /**< L */
#define UBX_M10_KEY_UART1OUTPROT_NMEA       0x10740002U /**< L */
#define UBX_M10_KEY_RATE_MEAS               0x30210001U /**< U2, ms */
#define UBX_M10_KEY_RATE_NAV                0x30210002U /**< U2, measurements per epoch */
#define UBX_M10_KEY_NAVSPG_FIXMODE          0x20110011U /**< E1 */
#define UBX_M10_KEY_NAVSPG_DYNMODEL         0x20110021U /**< E1 */
#define UBX_M10_KEY_PM_OPERATEMODE          0x20D00001U /**< E1: 0 full, 1 PSMOO, 2 LEAP */
#define UBX_M10_KEY_PM_UPDATEEPH            0x10D0000AU /**< L */
#define UBX_M10_KEY_TP_TP1_ENA              0x10050007U /**< L, time pulse 1 */
#define UBX_M10_KEY_MSGOUT_NAV_PVT_UART1    0x20910007U /**< U1, epochs between messages */
#define UBX_M10_KEY_MSGOUT_NAV_SAT_UART1    0x20910016U /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_DTM_UART1   0x209100A7U /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_GBS_UART1   0x209100DEU /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_GGA_UART1   0x209100BBU /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_GLL_UART1   0x209100CAU /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_GNS_UART1   0x209100B6U /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_GRS_UART1   0x209100CFU /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_GSA_UART1   0x209100C0U /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_GST_UART1   0x209100D4U /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_GSV_UART1   0x209100C5U /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_RLM_UART1   0x20910401U /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_RMC_UART1   0x209100ACU /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_VLW_UART1   0x209100E8U /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_VTG_UART1   0x209100B1U /**< U1 */
#define UBX_M10_KEY_MSGOUT_NMEA_ZDA_UART1   0x209100D9U /**< U1 */
#define UBX_M10_KEY_SIGNAL_GPS_ENA          0x1031001FU /**< L */
#define UBX_M10_KEY_SIGNAL_SBAS_ENA         0x10310020U /**< L */
#define UBX_M10_KEY_SIGNAL_GAL_ENA          0x10310021U /**< L */
#define UBX_M10_KEY_SIGNAL_BDS_ENA          0x10310022U /**< L */
#define UBX_M10_KEY_SIGNAL_QZSS_ENA         0x10310024U /**< L */
#define UBX_M10_KEY_SIGNAL_NAVIC_ENA        0x10310026U /**< L, F10 only */

/*
 * Signal keys of one constellation. The M10 has only the L1 ones; the F10
 * adds L5 and NavIC. The five constellation keys above hold the same ID on
 * both, which is why one driver serves the two parts (F10 SPG 6.00 interface
 * description UBX-23002975 R02, 4.9.20, table 46).
 */
#define UBX_M10_KEY_SIGNAL_GPS_L1CA_ENA     0x10310001U /**< L */
#define UBX_M10_KEY_SIGNAL_GPS_L5_ENA       0x10310004U /**< L, F10 only */
#define UBX_M10_KEY_SIGNAL_SBAS_L1CA_ENA    0x10310005U /**< L */
#define UBX_M10_KEY_SIGNAL_GAL_E1_ENA       0x10310007U /**< L */
#define UBX_M10_KEY_SIGNAL_GAL_E5A_ENA      0x10310009U /**< L, F10 only */
#define UBX_M10_KEY_SIGNAL_BDS_B1_ENA       0x1031000DU /**< L, B1I */
#define UBX_M10_KEY_SIGNAL_BDS_B1C_ENA      0x1031000FU /**< L */
#define UBX_M10_KEY_SIGNAL_BDS_B2A_ENA      0x10310028U /**< L, F10 only */
#define UBX_M10_KEY_SIGNAL_QZSS_L1CA_ENA    0x10310012U /**< L */
#define UBX_M10_KEY_SIGNAL_QZSS_L1S_ENA     0x10310014U /**< L */
#define UBX_M10_KEY_SIGNAL_QZSS_L5_ENA      0x10310017U /**< L, F10 only */
#define UBX_M10_KEY_SIGNAL_NAVIC_L5_ENA     0x1031001DU /**< L, F10 only */

/**
 * Layers of UBX-CFG-VALSET (3.10.5). The driver writes RAM and BBR: the
 * software standby "clears the RAM memory including the FW image, and the
 * receiver configuration" (integration manual 3.7.4.2), and so does the
 * inactive state of the cyclic tracking that LEAP uses (table 3); with the
 * value in the battery-backed RAM, which V_BCKP holds, the receiver comes
 * back configured.
 */
#define UBX_M10_LAYER_RAM       0x01U
#define UBX_M10_LAYER_BBR       0x02U
#define UBX_M10_LAYER_FLASH     0x04U

/** CFG-PM-OPERATEMODE (table 37 and integration manual 3.7.2.3) */
enum ubx_m10_operate_mode {
    UBX_M10_OPERATE_FULL = 0,   /**< full power, the default of the receiver */
    UBX_M10_OPERATE_PSMOO = 1,  /**< ON/OFF power save, not used here */
    UBX_M10_OPERATE_LEAP = 2    /**< low energy accurate positioning */
};

/** CFG-NAVSPG-DYNMODEL (table 26); BIKE is a motorbike and is not for us */
enum ubx_m10_dyn_model {
    UBX_M10_DYN_PORTABLE = 0,
    UBX_M10_DYN_STATIONARY = 2,
    UBX_M10_DYN_PEDESTRIAN = 3,
    UBX_M10_DYN_AUTOMOTIVE = 4,
    UBX_M10_DYN_SEA = 5,
    UBX_M10_DYN_AIRBORNE_1G = 6,
    UBX_M10_DYN_AIRBORNE_2G = 7,
    UBX_M10_DYN_AIRBORNE_4G = 8,
    UBX_M10_DYN_WRIST = 9,
    UBX_M10_DYN_BIKE = 10
};

/** UBX-NAV-PVT fixType (3.15.11) */
enum ubx_m10_fix_type {
    UBX_M10_FIX_NONE = 0,
    UBX_M10_FIX_DEAD_RECKONING = 1,
    UBX_M10_FIX_2D = 2,
    UBX_M10_FIX_3D = 3,
    UBX_M10_FIX_GNSS_DR = 4,
    UBX_M10_FIX_TIME_ONLY = 5
};

/** psmState of UBX-NAV-PVT flags, bits 4..2 (3.15.11) */
enum ubx_m10_psm_state {
    UBX_M10_PSM_OFF = 0,        /**< power save not active: full power */
    UBX_M10_PSM_ENABLED = 1,
    UBX_M10_PSM_ACQUISITION = 2,
    UBX_M10_PSM_TRACKING = 3,
    UBX_M10_PSM_POT = 4,        /**< power optimized tracking, the LEAP state */
    UBX_M10_PSM_INACTIVE = 5
};

/** GNSS identifiers of UBX-NAV-SAT (Satellite numbering) */
enum ubx_m10_gnss_id {
    UBX_M10_GNSS_GPS = 0,
    UBX_M10_GNSS_SBAS = 1,
    UBX_M10_GNSS_GALILEO = 2,
    UBX_M10_GNSS_BEIDOU = 3,
    UBX_M10_GNSS_QZSS = 5,
    UBX_M10_GNSS_GLONASS = 6,
    UBX_M10_GNSS_NAVIC = 7      /**< F10 only (F10 interface description, table 1) */
};

/** Wake-up sources of UBX-RXM-PMREQ (3.16.7) */
#define UBX_M10_PMREQ_WAKE_UARTRX   0x08U  /**< bit 3 */
#define UBX_M10_PMREQ_WAKE_EXTINT0  0x20U  /**< bit 5 */
#define UBX_M10_PMREQ_WAKE_EXTINT1  0x40U  /**< bit 6 */

/** One navigation epoch, from UBX-NAV-PVT */
struct ubx_m10_pvt {
    uint32_t itow_ms;           /**< GPS time of week */
    uint16_t year;
    uint8_t month;              /**< 1 to 12 */
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool date_valid;            /**< valid bit 0 */
    bool time_valid;            /**< valid bit 1 */
    bool fully_resolved;        /**< valid bit 2 */
    int32_t nano;               /**< fraction of second, -1e9 to 1e9 */
    uint8_t fix_type;           /**< enum ubx_m10_fix_type */
    bool fix_ok;                /**< flags bit 0: inside the DOP and accuracy masks */
    uint8_t psm_state;          /**< enum ubx_m10_psm_state */
    bool llh_invalid;           /**< flags3 bit 0 */
    uint8_t num_sv;             /**< satellites used in the solution */
    int32_t lon_e7;             /**< 1e-7 degree */
    int32_t lat_e7;             /**< 1e-7 degree */
    int32_t height_mm;          /**< above the ellipsoid */
    int32_t hmsl_mm;            /**< above mean sea level */
    uint32_t hacc_mm;
    uint32_t vacc_mm;
    int32_t ground_speed_mms;   /**< 2D speed */
    int32_t head_motion_e5;     /**< course over ground, 1e-5 degree */
    uint32_t speed_acc_mms;
    uint32_t head_acc_e5;
    uint16_t pdop_e2;           /**< position dilution of precision, 0.01 */
};

/** One satellite of UBX-NAV-SAT */
struct ubx_m10_sat {
    uint8_t gnss_id;            /**< enum ubx_m10_gnss_id */
    uint8_t sv_id;
    uint8_t cno_dbhz;
    int8_t elev_deg;            /**< -90 to 90 */
    int16_t azim_deg;           /**< 0 to 360 */
    bool used;                  /**< flags bit 3, svUsed */
};

/**
 * Storage size of a configuration value, from bits 30..28 of the key (5.2):
 * 1 bit and 1 byte both take one byte.
 *
 * @return 1, 2, 4 or 8 bytes, or 0 if the key has no valid size
 */
size_t ubx_m10_key_size(uint32_t key);

/**
 * Build a UBX frame.
 *
 * @param buf output buffer
 * @param cap size of @p buf
 * @param cls message class
 * @param id message id
 * @param payload payload, may be NULL when @p len is 0
 * @param len payload length
 * @return frame length, or 0 if it does not fit
 */
size_t ubx_m10_frame(uint8_t *buf, size_t cap, uint8_t cls, uint8_t id, const uint8_t *payload,
                     size_t len);

/**
 * Build a UBX-CFG-VALSET frame with one key.
 *
 * @param value value, little-endian, truncated to the size of the key
 * @param layers where to write: UBX_M10_LAYER_RAM, _BBR, _FLASH, or their or
 * @return frame length, or 0 if the key or the layers are invalid, or it does not fit
 */
size_t ubx_m10_valset(uint8_t *buf, size_t cap, uint32_t key, uint64_t value, uint8_t layers);

/** One key and its value, for ubx_m10_valset_many() */
struct ubx_m10_kv {
    uint32_t key;
    uint64_t value;
};

/**
 * Build a UBX-CFG-VALSET frame with several keys, which the receiver applies
 * as one message. This is what the CFG-SIGNAL group needs: every change in it
 * resets the GNSS subsystem (F10 interface description 4.9.20), so a batch in
 * one frame costs one reset instead of one per key.
 *
 * @param items keys and values, in the order they go in the payload
 * @param count how many, at least one
 * @param layers where to write: UBX_M10_LAYER_RAM, _BBR, _FLASH, or their or
 * @return frame length, or 0 if a key or the layers are invalid, or it does not fit
 */
size_t ubx_m10_valset_many(uint8_t *buf, size_t cap, const struct ubx_m10_kv *items, size_t count,
                           uint8_t layers);

/** Build a UBX-CFG-VALGET frame that polls one key of the RAM layer */
size_t ubx_m10_valget(uint8_t *buf, size_t cap, uint32_t key);

/**
 * Read the value of a UBX-CFG-VALGET answer with one key.
 *
 * @param payload payload of the answer, without header or checksum
 * @param len payload length
 * @param key key the answer must carry
 * @param value where the value goes, zero extended
 * @return true if the answer carries that key with its full value
 */
bool ubx_m10_valget_parse(const uint8_t *payload, size_t len, uint32_t key, uint64_t *value);

/**
 * Build a UBX-RXM-PMREQ frame (software standby).
 *
 * @param duration_ms task duration; 0 waits for a wake-up source
 * @param backup true to enter backup mode
 * @param force true for the minimum consumption
 * @param wakeup_sources UBX_M10_PMREQ_WAKE_*
 */
size_t ubx_m10_pmreq(uint8_t *buf, size_t cap, uint32_t duration_ms, bool backup, bool force,
                     uint32_t wakeup_sources);

/** Start modes of UBX-CFG-RST (3.10.3) */
#define UBX_M10_RST_HOT     0x0000U
#define UBX_M10_RST_WARM    0x0001U
#define UBX_M10_RST_COLD    0xFFFFU

/** Build a UBX-CFG-RST frame; reset_mode 0x00 is a hardware reset */
size_t ubx_m10_cfg_rst(uint8_t *buf, size_t cap, uint16_t nav_bbr_mask, uint8_t reset_mode);

/**
 * Parse a UBX-NAV-PVT payload.
 *
 * @return true if the payload has the 92 bytes of the message
 */
bool ubx_m10_parse_pvt(const uint8_t *payload, size_t len, struct ubx_m10_pvt *out);

/**
 * Parse a UBX-NAV-SAT payload.
 *
 * @param out array of at least @p max satellites
 * @param max how many fit in @p out
 * @return how many satellites were written, or -1 if the payload is short
 */
int ubx_m10_parse_sat(const uint8_t *payload, size_t len, struct ubx_m10_sat *out, int max);

#ifdef __cplusplus
}
#endif

#endif /* UBX_M10_H */
