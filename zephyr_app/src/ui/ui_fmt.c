/**
 * @file ui_fmt.c
 * @brief Number and time formatting of the legacy screens
 *
 * Port of _fmkstr(), _imkstr() and _secjmkstr() from
 * legacy/source/vue/Screenutils.cpp. The float arithmetic is kept as in the
 * legacy, including its truncation: 0.21f prints "0.20" with 2 digits,
 * because 0.21f is 0.2099999...
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui/ui_fmt.h"

/** Above this absolute value the legacy prints "---" (_fmkstr) */
#define UI_FMT_MAX_ABS      100000.0f

char *ui_fmt_float(char *buf, size_t size, float value, unsigned int nb_digits)
{
    size_t len = 0U;
    int ent_val;

    if ((buf == NULL) || (size == 0U)) {
        return buf;
    }
    buf[0] = '\0';

    if (!(fabsf(value) <= UI_FMT_MAX_ABS)) {
        (void)snprintf(buf, size, "---");
        return buf;
    }

    ent_val = (int)value;
    if ((ent_val == 0) && (value < 0.0f)) {
        len += (size_t)snprintf(&buf[len], size - len, "-");
    }
    if (len < size) {
        len += (size_t)snprintf(&buf[len], size - len, "%d", ent_val);
    }

    if ((nb_digits > 0U) && (len < size)) {
        len += (size_t)snprintf(&buf[len], size - len, ".");
        for (unsigned int i = 0U; (i < nb_digits) && (len < size); i++) {
            /* Screenutils.cpp: value = fabsf(value - ent); value *= 10; digit = (int)value */
            value = fabsf(value - (float)ent_val);
            value *= 10.0f;
            ent_val = (int)value;
            len += (size_t)snprintf(&buf[len], size - len, "%u", (unsigned int)value);
        }
    }
    return buf;
}

char *ui_fmt_int(char *buf, size_t size, int32_t value)
{
    if ((buf != NULL) && (size > 0U)) {
        (void)snprintf(buf, size, "%ld", (long)value);
    }
    return buf;
}

char *ui_fmt_hms(char *buf, size_t size, uint32_t seconds, char sep)
{
    if ((buf == NULL) || (size == 0U)) {
        return buf;
    }
    if (seconds >= 86400U) {
        (void)snprintf(buf, size, "--%c--%c--", sep, sep);
        return buf;
    }
    unsigned int hours = seconds / 3600U;
    unsigned int minutes = (seconds % 3600U) / 60U;
    unsigned int secs = seconds % 60U;

    (void)snprintf(buf, size, "%02u%c%02u%c%02u", hours, sep, minutes, sep, secs);
    return buf;
}

char *ui_fmt_hm(char *buf, size_t size, uint32_t seconds)
{
    if ((buf == NULL) || (size == 0U)) {
        return buf;
    }
    if (seconds >= 86400U) {
        (void)snprintf(buf, size, "--:--");
        return buf;
    }
    (void)snprintf(buf, size, "%02u:%02u", (unsigned int)(seconds / 3600U),
                   (unsigned int)((seconds % 3600U) / 60U));
    return buf;
}

const char *ui_fmt_cadran(const char *text, size_t max_len)
{
    if (text == NULL) {
        return "";
    }
    if (strlen(text) > max_len) {
        /* Vue::cadran() prints "---" past 6 characters, cadranH() "-----" past 9 */
        return (max_len > 6U) ? "-----" : "---";
    }
    return text;
}

char *ui_fmt_signed(char *buf, size_t size, float value, unsigned int nb_digits, const char *unit)
{
    char num[16] = "";

    if ((buf == NULL) || (size == 0U)) {
        return buf;
    }
    (void)ui_fmt_float(num, sizeof(num), fabsf(value), nb_digits);
    if (strcmp(num, "---") == 0) {
        (void)snprintf(buf, size, "---");
        return buf;
    }
    (void)snprintf(buf, size, "%s%s%s%s", (value < 0.0f) ? "-" : "+", num,
                   ((unit != NULL) && (unit[0] != '\0')) ? " " : "",
                   (unit != NULL) ? unit : "");
    return buf;
}
