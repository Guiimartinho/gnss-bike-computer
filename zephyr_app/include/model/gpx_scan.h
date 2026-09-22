/**
 * @file gpx_scan.h
 * @brief Points out of a GPX, read as the bytes arrive
 *
 * The rider should not have to convert anything: a course downloaded from
 * Strava, Komoot or RideWithGPS is a GPX, and the device takes it as it is.
 * A GPX is XML, but a course only needs three things out of it — the
 * latitude and the longitude of each `<trkpt>` and its `<ele>` — so this is
 * a scanner with a small state machine, not an XML parser: it never holds
 * the file, only the element it is inside of, and it survives namespaces,
 * attributes in any order, quotes of either kind and lines of any length.
 *
 * A converted `.RTE` is still worth it (a tenth of the size, a checksum and
 * the turns), and `tools/route_convert.py` makes one; this is the way in
 * when nothing converted anything.
 *
 * Pure C: no Zephyr, no hardware.
 */

#ifndef MODEL_GPX_SCAN_H
#define MODEL_GPX_SCAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Longest piece of an element this keeps while reading it */
#define GPX_SCAN_BUF        128U

/** What the scanner is in the middle of */
enum gpx_state {
    GPX_OUTSIDE = 0,    /**< between elements */
    GPX_TAG_NAME,       /**< reading the name of an element */
    GPX_TAG_ATTR,       /**< inside `<trkpt ...>`, reading the attributes */
    GPX_POINT_BODY,     /**< inside the element, looking for the altitude */
    GPX_ELE,            /**< reading the number of `<ele>` */
};

/** State of the reading; the caller keeps one per file */
struct gpx_scan {
    enum gpx_state state;
    char buf[GPX_SCAN_BUF];
    uint16_t len;
    float lat;
    float lon;
    float alt;
    bool have_point;    /**< the latitude and the longitude came */
    bool have_alt;
    uint32_t points;    /**< points given to the caller so far */
};

/**
 * @brief One point of the course
 *
 * @param lat Latitude in degrees
 * @param lon Longitude in degrees
 * @param alt Altitude in metres, zero when the file has none
 * @param user What the caller gave to gpx_scan_feed()
 */
typedef void (*gpx_point_fn)(float lat, float lon, float alt, void *user);

/** @brief Start over, with nothing read */
void gpx_scan_init(struct gpx_scan *scan);

/**
 * @brief Take the next bytes of the file
 *
 * Calls @p cb once for each point that ends inside these bytes.
 */
void gpx_scan_feed(struct gpx_scan *scan, const char *data, size_t len, gpx_point_fn cb,
                   void *user);

/**
 * @brief The file ended: give the point that was still open, if any
 */
void gpx_scan_end(struct gpx_scan *scan, gpx_point_fn cb, void *user);

/**
 * @brief Whether these first bytes look like a GPX
 *
 * What the loader uses to tell a GPX from the text of the legacy.
 */
bool gpx_scan_looks_like_gpx(const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_GPX_SCAN_H */
