/**
 * @file qry.h
 * @brief The `$QRY` command: what is on the storage, and removing one of it
 *
 * `$QRY` is of this port and not of the legacy — the stravaV10 has no such
 * sentence. It was left as a stub that answered `$QRY,0` and logged "not
 * answered yet"; this is the answer.
 *
 * Three questions, and a deliberate refusal:
 *
 * | Sentence | Meaning |
 * |---|---|
 * | `$QRY,1` | list what is on the storage |
 * | `$QRY,2,<name>` | **refused**: see below |
 * | `$QRY,3,<name>` | erase one file |
 *
 * ## Why sending a file is refused
 *
 * The device already hands files over properly, through the file group of
 * mcumgr on the same SMP link that carries the firmware update
 * (`rf/file_xfer.c`): with offsets, a checksum and a transfer that can be
 * resumed. Sending a ride of a few hundred kilobytes over a serial line in
 * twenty-byte notifications would be slower, would have none of that, and
 * would be a second way of doing one thing. `$QRY,2` answers `ERR` and the
 * caller is expected to use SMP.
 *
 * ## The replies
 *
 * One sentence per line, `\r\n` at the end, in the shape of the commands
 * the legacy already speaks:
 *
 * ```
 * $QRY,1,<name>,<bytes>      one per file
 * $QRY,1,END,<count>         the listing ended
 * $QRY,3,OK                  the file went
 * $QRY,3,ERR,<why>           it did not
 * ```
 *
 * `<why>` is a word and not a number, because whoever reads this is a
 * person at a terminal or a script, and both read `NOTFOUND` faster than
 * they read `-2`.
 *
 * ## What may be touched
 *
 * The rules are the ones `model/file_policy.h` already holds for transfers
 * over the air: everything lives on the root of the storage, nothing
 * climbs out of it, and an activity may be read but never written. Erasing
 * is a write, so **an activity cannot be erased over the wire** — the
 * rider's ride is not something a stray command should be able to take.
 */

#ifndef MODEL_QRY_H
#define MODEL_QRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Longest reply, with the longest name the storage holds */
#define QRY_REPLY_LEN   64U

/** Why a question could not be answered */
enum qry_error {
    QRY_ERR_NONE = 0,
    QRY_ERR_UNKNOWN,        /**< no such question */
    QRY_ERR_NAME,           /**< the name is missing or climbs out of the root */
    QRY_ERR_FORBIDDEN,      /**< the policy of `model/file_policy.h` says no */
    QRY_ERR_NOTFOUND,       /**< there is no such file */
    QRY_ERR_IO,             /**< the storage refused */
    QRY_ERR_USE_SMP,        /**< sending a file goes over SMP, not over here */
};

/** The word that goes on the wire for each */
const char *qry_error_word(enum qry_error err);

/**
 * @brief One line of a listing
 *
 * @param out where to write, at least QRY_REPLY_LEN
 * @param name of the file, without a path
 * @param size in bytes
 * @return length written
 */
size_t qry_format_entry(char *out, size_t len, const char *name, uint32_t size);

/**
 * @brief The line that closes a listing
 * @param count how many files it named
 */
size_t qry_format_end(char *out, size_t len, uint32_t count);

/** `$QRY,3,OK` */
size_t qry_format_ok(char *out, size_t len, uint8_t type);

/** `$QRY,<type>,ERR,<why>` */
size_t qry_format_error(char *out, size_t len, uint8_t type, enum qry_error err);

/**
 * @brief Whether a name may be erased over the wire
 *
 * Builds the whole path under the root and asks `model/file_policy.h`
 * whether a write is allowed there. An activity is readable and not
 * writable, so it comes back false: the rider's ride is not something a
 * stray command takes away.
 *
 * @param path filled with the whole path when the answer is true
 * @return QRY_ERR_NONE when it may
 */
enum qry_error qry_check_erase(const char *name, char *path, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_QRY_H */
