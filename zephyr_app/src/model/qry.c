/**
 * @file qry.c
 * @brief The `$QRY` command: what is on the storage, and removing one of it
 *
 * The three questions, the refusal and the shape of the replies are in
 * model/qry.h.
 */

#include <stdio.h>
#include <string.h>

#include "model/file_policy.h"
#include "model/qry.h"

const char *qry_error_word(enum qry_error err)
{
    switch (err) {
    case QRY_ERR_NONE:
        return "OK";
    case QRY_ERR_NAME:
        return "NAME";
    case QRY_ERR_FORBIDDEN:
        return "FORBIDDEN";
    case QRY_ERR_NOTFOUND:
        return "NOTFOUND";
    case QRY_ERR_IO:
        return "IO";
    case QRY_ERR_USE_SMP:
        return "USESMP";
    case QRY_ERR_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

size_t qry_format_entry(char *out, size_t len, const char *name, uint32_t size)
{
    if ((out == NULL) || (len == 0U)) {
        return 0U;
    }

    int n = snprintf(out, len, "$QRY,1,%s,%u\r\n", (name != NULL) ? name : "",
                     (unsigned int)size);

    return ((n > 0) && ((size_t)n < len)) ? (size_t)n : 0U;
}

size_t qry_format_end(char *out, size_t len, uint32_t count)
{
    if ((out == NULL) || (len == 0U)) {
        return 0U;
    }

    int n = snprintf(out, len, "$QRY,1,END,%u\r\n", (unsigned int)count);

    return ((n > 0) && ((size_t)n < len)) ? (size_t)n : 0U;
}

size_t qry_format_ok(char *out, size_t len, uint8_t type)
{
    if ((out == NULL) || (len == 0U)) {
        return 0U;
    }

    int n = snprintf(out, len, "$QRY,%u,OK\r\n", (unsigned int)type);

    return ((n > 0) && ((size_t)n < len)) ? (size_t)n : 0U;
}

size_t qry_format_error(char *out, size_t len, uint8_t type, enum qry_error err)
{
    if ((out == NULL) || (len == 0U)) {
        return 0U;
    }

    int n = snprintf(out, len, "$QRY,%u,ERR,%s\r\n", (unsigned int)type,
                     qry_error_word(err));

    return ((n > 0) && ((size_t)n < len)) ? (size_t)n : 0U;
}

enum qry_error qry_check_erase(const char *name, char *path, size_t len)
{
    if ((name == NULL) || (path == NULL) || (name[0] == '\0')) {
        return QRY_ERR_NAME;
    }

    /*
     * A name and not a path: anything with a separator in it is refused
     * before it is built, so nothing can walk out of the root even if the
     * policy below were ever loosened.
     */
    if ((strchr(name, '/') != NULL) || (strchr(name, '\\') != NULL) ||
        (strstr(name, "..") != NULL)) {
        return QRY_ERR_NAME;
    }

    int n = snprintf(path, len, "%s%s", FILE_POLICY_ROOT, name);

    if ((n <= 0) || ((size_t)n >= len)) {
        return QRY_ERR_NAME;
    }

    /*
     * Erasing is a write. The policy lets a route or a segment be written
     * and never an activity, so a ride cannot be taken away from here.
     */
    if (!file_policy_allows(path, FILE_ACCESS_WRITE)) {
        return QRY_ERR_FORBIDDEN;
    }

    return QRY_ERR_NONE;
}
