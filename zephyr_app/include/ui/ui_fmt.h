/**
 * @file ui_fmt.h
 * @brief Number and time formatting of the legacy screens
 *
 * Pure C, tested on the host. The rules come from
 * legacy/source/vue/Screenutils.cpp: decimals are truncated, not rounded
 * (_fmkstr), values above 100000 print "---", and times print HH:MM:SS or
 * "--:--:--" past one day (_secjmkstr).
 */

#ifndef UI_FMT_H
#define UI_FMT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Float with nb_digits truncated decimals (legacy _fmkstr)
 *
 * @return buf
 */
char *ui_fmt_float(char *buf, size_t size, float value, unsigned int nb_digits);

/** Integer (legacy _imkstr) */
char *ui_fmt_int(char *buf, size_t size, int32_t value);

/** Seconds of the day as HH:MM:SS, or "--:--:--" (legacy _secjmkstr) */
char *ui_fmt_hms(char *buf, size_t size, uint32_t seconds, char sep);

/** Seconds of the day as HH:MM, or "--:--" */
char *ui_fmt_hm(char *buf, size_t size, uint32_t seconds);

/**
 * @brief Value of a legacy cadran: more than max_len characters prints "---"
 *
 * Vue::cadran() replaces texts longer than 6 characters with "---", and
 * Vue::cadranH() those longer than 9 with "-----".
 */
const char *ui_fmt_cadran(const char *text, size_t max_len);

/** Signed value with one decimal and a unit: "+12.4 s", "-3.0 s" */
char *ui_fmt_signed(char *buf, size_t size, float value, unsigned int nb_digits, const char *unit);

#ifdef __cplusplus
}
#endif

#endif /* UI_FMT_H */
