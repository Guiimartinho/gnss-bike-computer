/**
 * @file segment_file.h
 * @brief Segment files of the legacy: name, position and points
 *
 * A segment of the legacy is a text file in the root of the card whose
 * **name carries the position of its start** (`legacy/source/sd/sd_functions.cpp:279-333`
 * decides the type by the name, and `calculePos()` of
 * `libraries/utils/utils.c:201-227` reads the position):
 *
 * - twelve characters, `LLLLL#OO.OOO`, with `#` at position 5 and `.` at 8;
 * - `lat = base36(c0..c4) / 100000 - 90`;
 * - `lon = base36(c6 c7 c9 c10 c11) / 100000 - 180`.
 *
 * So the firmware decides whether to load a segment without opening it.
 * The content is text: a line `<Name>...</Name>` that the parser skips and
 * then one point per line, `lat ; lon ; rtime ; alt`, where `rtime` is a
 * time in seconds that the reader takes relative to the first point.
 *
 * Plain C, without Zephyr, so the host tests read the 140 real files of
 * `tools/TDD/DB/`.
 */

#ifndef MODEL_SEGMENT_FILE_H
#define MODEL_SEGMENT_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Length of the name of a segment of the legacy, without the directory */
#define SEGMENT_FILE_NAME_LEN   12U

/** Divider of the position in the name (FACTOR of the legacy) */
#define SEGMENT_FILE_FACTOR     100000.0f

/** One point of the file */
struct segment_file_point {
    float lat;
    float lon;
    float rtime;    /**< seconds, relative to the first point of the file */
    float alt;
};

/**
 * Is this the name of a segment of the legacy?
 *
 * Twelve characters, `#` at position 5, `.` at position 8 and the rest in
 * [0-9A-Z], as `sd_functions.cpp:279-333` checks before opening.
 */
bool segment_file_name_is_valid(const char *name);

/**
 * Position of the start of a segment, taken from its name.
 *
 * @return false when the name is not a segment of the legacy
 */
bool segment_file_position(const char *name, float *lat, float *lon);

/**
 * Read one line of a segment file.
 *
 * Skips the `<Name>` line and blank lines.
 *
 * @param line line without the line ending
 * @param out point read
 * @return true when the line carried a point
 */
bool segment_file_parse_line(const char *line, struct segment_file_point *out);

/**
 * Name of the file of a position, the other way round.
 *
 * Writes `SEGMENT_FILE_NAME_LEN + 1` characters in @p out, so a segment
 * recorded by the device lands on a name the legacy also understands.
 *
 * @return false when the buffer is too small or the position is out of range
 */
bool segment_file_name_of(char *out, size_t size, float lat, float lon);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_SEGMENT_FILE_H */
