/**
 * @file cmd_parser.h
 * @brief The command sentences of the legacy (VParser)
 *
 * The stravaV10 takes NMEA-like sentences over the Nordic UART Service and
 * over the USB serial: `$` , a type of three or four letters, terms
 * separated by commas and a line end
 * (`libraries/VParser/VParser.cpp:59-100`). The PC tools of `tools/zpm` and
 * the stravaAP dongle speak this.
 *
 * Pure C: feed it the characters as they arrive and it says when a whole
 * sentence came in. No Zephyr, no hardware.
 */

#ifndef MODEL_CMD_PARSER_H
#define MODEL_CMD_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Longest sentence taken, as MAX_SIZE of the legacy */
#define CMD_LINE_MAX        150U
/** Longest name of a file in a `$QRY` */
#define CMD_NAME_MAX        24U
/** Longest text of a notification (`$ANCS`) */
#define CMD_TEXT_MAX        32U

/** What came in */
enum cmd_kind {
    CMD_NONE = 0,       /**< nothing complete yet */
    CMD_OTHER,          /**< a sentence of another kind */
    CMD_LOC,            /**< simulated position */
    CMD_HRM,            /**< heart rate from the PC */
    CMD_CAD,            /**< cadence and speed from the PC */
    CMD_ANCS,           /**< notification of the phone */
    CMD_DWN,            /**< order to the device */
    CMD_QRY,            /**< question about the files */
    CMD_BTN,            /**< a key pressed from the PC */
    CMD_DBG,            /**< a message for the debug screen */
};

/** Orders of `$DWN` (`docs/07-radio-ant-ble.md`, stravaAP e comandos) */
enum cmd_dwn_code {
    CMD_DWN_HARDFAULT = 12,
    CMD_DWN_FORMAT = 13,
    CMD_DWN_MEMORY = 14,
    CMD_DWN_MKFS = 15,
    CMD_DWN_MSC = 16,
    CMD_DWN_DFU = 17,
    CMD_DWN_CALIB_MAG = 18,
};

/** Questions of `$QRY` */
enum cmd_qry_type {
    CMD_QRY_LIST = 1,   /**< list the activity logs */
    CMD_QRY_SEND = 2,   /**< send a file */
    CMD_QRY_ERASE = 3,  /**< erase a file */
};

/** What a sentence carried */
struct cmd_data {
    enum cmd_kind kind;
    /** `$LOC`: seconds of the day, position in 1e-7 degrees, altitude in cm */
    uint32_t secj;
    int32_t lat_e7;
    int32_t lon_e7;
    int32_t ele_cm;
    int32_t speed_cms;
    /** `$HRM`, `$CAD` */
    uint16_t bpm;
    uint16_t rr_ms;
    uint16_t rpm;
    uint16_t cad_speed;
    /** `$DWN`, `$QRY`, `$BTN`, `$DBG` */
    uint8_t code;
    uint8_t qry_type;
    char name[CMD_NAME_MAX];
    /** `$ANCS`, `$DBG` */
    char title[CMD_TEXT_MAX];
    char text[CMD_TEXT_MAX];
};

/** State of the reader; the caller keeps one per channel */
struct cmd_parser {
    char term[CMD_LINE_MAX];
    uint8_t term_len;
    uint8_t term_number;
    bool started;
    enum cmd_kind kind;
    struct cmd_data data;
};

/** @brief Start over, with nothing read */
void cmd_parser_init(struct cmd_parser *p);

/**
 * @brief Take one character
 *
 * @return The kind of the sentence that just ended, or CMD_NONE
 */
enum cmd_kind cmd_parser_feed(struct cmd_parser *p, char c);

/**
 * @brief Take a whole line, giving the kind of the sentence it held
 */
enum cmd_kind cmd_parser_line(struct cmd_parser *p, const char *line);

/** @brief What the last complete sentence carried */
const struct cmd_data *cmd_parser_data(const struct cmd_parser *p);

/**
 * @brief Whether an order of `$DWN` may run without someone at the device
 *
 * The legacy took any of them from anyone that called itself stravaAP,
 * including the one that formats the card. In the port the orders that
 * destroy data or crash the device on purpose only come from the menu,
 * where the rider confirms them on the screen.
 */
bool cmd_dwn_allowed(uint8_t code);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_CMD_PARSER_H */
