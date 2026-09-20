/**
 * @file gpx_scan.c
 * @brief Points out of a GPX, read as the bytes arrive
 */

#include <stdlib.h>
#include <string.h>

#include "model/gpx_scan.h"

/** The element carries a point of the course */
static bool is_point_tag(const char *name)
{
    /* `trkpt` of a track and `rtept` of a route, with or without namespace */
    const char *colon = strchr(name, ':');
    const char *bare = (colon != NULL) ? (colon + 1) : name;

    return (strcmp(bare, "trkpt") == 0) || (strcmp(bare, "rtept") == 0);
}

/** The element carries an altitude */
static bool is_ele_tag(const char *name)
{
    const char *colon = strchr(name, ':');
    const char *bare = (colon != NULL) ? (colon + 1) : name;

    return (strcmp(bare, "ele") == 0);
}

/** The value of an attribute of the element being read, or NULL */
static bool attr_value(const char *attrs, const char *name, float *out)
{
    const char *at = strstr(attrs, name);

    if (at == NULL) {
        return false;
    }

    const char *cursor = at + strlen(name);

    while ((*cursor == ' ') || (*cursor == '\t')) {
        cursor++;
    }
    if (*cursor != '=') {
        return false;
    }
    cursor++;
    while ((*cursor == ' ') || (*cursor == '\t')) {
        cursor++;
    }
    if ((*cursor == '"') || (*cursor == '\'')) {
        cursor++;
    }

    char *end = NULL;
    float value = strtof(cursor, &end);

    if ((end == NULL) || (end == cursor)) {
        return false;
    }
    *out = value;

    return true;
}

/** The point that was being read is complete: hand it over */
static void emit(struct gpx_scan *scan, gpx_point_fn cb, void *user)
{
    if (!scan->have_point) {
        return;
    }

    if (cb != NULL) {
        cb(scan->lat, scan->lon, scan->have_alt ? scan->alt : 0.0f, user);
    }
    scan->points++;
    scan->have_point = false;
    scan->have_alt = false;
}

static void push(struct gpx_scan *scan, char c)
{
    if (scan->len < (GPX_SCAN_BUF - 1U)) {
        scan->buf[scan->len] = c;
        scan->len++;
    }
}

void gpx_scan_init(struct gpx_scan *scan)
{
    if (scan == NULL) {
        return;
    }

    (void)memset(scan, 0, sizeof(*scan));
    scan->state = GPX_OUTSIDE;
}

void gpx_scan_feed(struct gpx_scan *scan, const char *data, size_t len, gpx_point_fn cb,
                   void *user)
{
    if ((scan == NULL) || (data == NULL)) {
        return;
    }

    for (size_t i = 0U; i < len; i++) {
        char c = data[i];

        switch (scan->state) {
        case GPX_OUTSIDE:
            if (c == '<') {
                scan->len = 0U;
                scan->state = GPX_TAG_NAME;
            }
            break;

        case GPX_TAG_NAME:
            if ((c == ' ') || (c == '\t') || (c == '>') || (c == '/') || (c == '\r') ||
                (c == '\n')) {
                scan->buf[scan->len] = '\0';

                bool point = is_point_tag(scan->buf);
                bool ele = is_ele_tag(scan->buf);

                /* a new point closes whatever was open before it */
                if (point) {
                    emit(scan, cb, user);
                }

                if (point && (c != '>')) {
                    scan->len = 0U;
                    scan->state = GPX_TAG_ATTR;
                } else if (ele && (c == '>')) {
                    scan->len = 0U;
                    scan->state = GPX_ELE;
                } else if (c == '>') {
                    scan->state = scan->have_point ? GPX_POINT_BODY : GPX_OUTSIDE;
                } else {
                    /* another element: skip to its end */
                    scan->state = GPX_OUTSIDE;
                    scan->len = 0U;
                    while ((i + 1U) < len) {
                        i++;
                        if (data[i] == '>') {
                            scan->state = scan->have_point ? GPX_POINT_BODY : GPX_OUTSIDE;
                            break;
                        }
                    }
                }
            } else {
                push(scan, c);
            }
            break;

        case GPX_TAG_ATTR:
            if (c == '>') {
                scan->buf[scan->len] = '\0';

                float lat;
                float lon;

                if (attr_value(scan->buf, "lat", &lat) && attr_value(scan->buf, "lon", &lon)) {
                    scan->lat = lat;
                    scan->lon = lon;
                    scan->have_point = true;
                    scan->have_alt = false;
                }
                scan->len = 0U;
                /* `<trkpt .../>` has no body and no altitude */
                scan->state = GPX_POINT_BODY;
            } else {
                push(scan, c);
            }
            break;

        case GPX_POINT_BODY:
            if (c == '<') {
                scan->len = 0U;
                scan->state = GPX_TAG_NAME;
            }
            break;

        case GPX_ELE:
            if (c == '<') {
                scan->buf[scan->len] = '\0';

                char *end = NULL;
                float value = strtof(scan->buf, &end);

                if ((end != NULL) && (end != scan->buf)) {
                    scan->alt = value;
                    scan->have_alt = true;
                }
                scan->len = 0U;
                scan->state = GPX_TAG_NAME;
            } else {
                push(scan, c);
            }
            break;

        default:
            scan->state = GPX_OUTSIDE;
            break;
        }
    }
}

void gpx_scan_end(struct gpx_scan *scan, gpx_point_fn cb, void *user)
{
    if (scan == NULL) {
        return;
    }
    emit(scan, cb, user);
}

bool gpx_scan_looks_like_gpx(const char *data, size_t len)
{
    if ((data == NULL) || (len < 5U)) {
        return false;
    }

    /* `<?xml` or `<gpx` somewhere in what came, which is the head of the file */
    size_t limit = (len > 256U) ? 256U : len;

    for (size_t i = 0U; (i + 4U) < limit; i++) {
        if ((data[i] == '<') &&
            (((data[i + 1] == '?') && (data[i + 2] == 'x') && (data[i + 3] == 'm') &&
              (data[i + 4] == 'l')) ||
             ((data[i + 1] == 'g') && (data[i + 2] == 'p') && (data[i + 3] == 'x')))) {
            return true;
        }
    }

    return false;
}
