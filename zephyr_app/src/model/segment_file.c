/**
 * @file segment_file.c
 * @brief Segment files of the legacy: name, position and points
 *
 * Rules and origin in model/segment_file.h.
 */

#include "model/segment_file.h"

#include <stdlib.h>
#include <string.h>

/** Value of a character in base 36, or -1 */
static int base36_digit(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    }
    if ((c >= 'A') && (c <= 'Z')) {
        return (c - 'A') + 10;
    }

    return -1;
}

/** Five characters in base 36, as `toBase10()` of the legacy reads them */
static bool base36_five(const char *chars, uint32_t *value)
{
    uint32_t acc = 0U;

    for (uint8_t i = 0U; i < 5U; i++) {
        int digit = base36_digit(chars[i]);

        if (digit < 0) {
            return false;
        }
        acc = (acc * 36U) + (uint32_t)digit;
    }
    *value = acc;

    return true;
}

bool segment_file_name_is_valid(const char *name)
{
    if (name == NULL) {
        return false;
    }
    if (strlen(name) != SEGMENT_FILE_NAME_LEN) {
        return false;
    }
    if ((name[5] != '#') || (name[8] != '.')) {
        return false;
    }

    for (uint8_t i = 0U; i < SEGMENT_FILE_NAME_LEN; i++) {
        if ((i == 5U) || (i == 8U)) {
            continue;
        }
        if (base36_digit(name[i]) < 0) {
            return false;
        }
    }

    return true;
}

bool segment_file_position(const char *name, float *lat, float *lon)
{
    if (!segment_file_name_is_valid(name)) {
        return false;
    }

    char buf[6];
    uint32_t value;

    /* latitude: the first five characters */
    if (!base36_five(name, &value)) {
        return false;
    }
    if (lat != NULL) {
        /*
         * In double: the legacy divides the integer as float
         * (`utils.c:214`), and a longitude of 34.696.890 does not fit in
         * the 24 bits of a float32, which costs about 4 m. The value below
         * is the same the legacy meant, with the rounding of one float.
         */
        *lat = (float)(((double)value / (double)SEGMENT_FILE_FACTOR) - 90.0);
    }

    /* longitude: two before the dot and three after, as calculePos() reads */
    buf[0] = name[6];
    buf[1] = name[7];
    buf[2] = name[9];
    buf[3] = name[10];
    buf[4] = name[11];
    buf[5] = '\0';
    if (!base36_five(buf, &value)) {
        return false;
    }
    if (lon != NULL) {
        *lon = (float)(((double)value / (double)SEGMENT_FILE_FACTOR) - 180.0);
    }

    return true;
}

bool segment_file_parse_line(const char *line, struct segment_file_point *out)
{
    if ((line == NULL) || (out == NULL)) {
        return false;
    }

    /* the name of the segment, which the legacy reads but does not store */
    if (strstr(line, "<Name>") != NULL) {
        return false;
    }

    const char *cursor = line;
    float values[4];

    for (uint8_t i = 0U; i < 4U; i++) {
        char *end = NULL;

        values[i] = strtof(cursor, &end);
        if ((end == NULL) || (end == cursor)) {
            return false;
        }
        cursor = end;
        while ((*cursor == ' ') || (*cursor == '\t')) {
            cursor++;
        }
        if (i < 3U) {
            if (*cursor != ';') {
                return false;
            }
            cursor++;
        }
    }

    out->lat = values[0];
    out->lon = values[1];
    out->rtime = values[2];
    out->alt = values[3];

    return true;
}

bool segment_file_name_of(char *out, size_t size, float lat, float lon)
{
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if ((out == NULL) || (size < (SEGMENT_FILE_NAME_LEN + 1U))) {
        return false;
    }
    if ((lat < -90.0f) || (lat > 90.0f) || (lon < -180.0f) || (lon > 180.0f)) {
        return false;
    }

    /* rounded, so a position that came out of a name goes back to it */
    uint32_t ilat = (uint32_t)((((double)lat + 90.0) * (double)SEGMENT_FILE_FACTOR) + 0.5);
    uint32_t ilon = (uint32_t)((((double)lon + 180.0) * (double)SEGMENT_FILE_FACTOR) + 0.5);
    char lat_chars[5];
    char lon_chars[5];

    for (int i = 4; i >= 0; i--) {
        lat_chars[i] = digits[ilat % 36U];
        ilat /= 36U;
        lon_chars[i] = digits[ilon % 36U];
        ilon /= 36U;
    }
    if ((ilat != 0U) || (ilon != 0U)) {
        return false; /* it does not fit in five characters of base 36 */
    }

    (void)memcpy(&out[0], lat_chars, 5U);
    out[5] = '#';
    out[6] = lon_chars[0];
    out[7] = lon_chars[1];
    out[8] = '.';
    out[9] = lon_chars[2];
    out[10] = lon_chars[3];
    out[11] = lon_chars[4];
    out[12] = '\0';

    return true;
}
