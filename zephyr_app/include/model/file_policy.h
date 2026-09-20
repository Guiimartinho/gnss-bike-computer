/**
 * @file file_policy.h
 * @brief What the phone may read and write on the storage of the device
 *
 * The routes come in over Bluetooth, by the file group of mcumgr, which is
 * the same channel the update uses (`docs/09-armazenamento-usb.md`). The
 * device is on a handlebar and anyone within radio range can talk to it, so
 * the firmware decides what a file transfer may touch instead of handing
 * over the whole card.
 *
 * Pure C: no Zephyr, no hardware.
 */

#ifndef MODEL_FILE_POLICY_H
#define MODEL_FILE_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Root of the storage, the mount point of the card or of the flash */
#define FILE_POLICY_ROOT    "/SD:/"

/** What the other side wants to do */
enum file_access {
    FILE_ACCESS_READ = 0,   /**< take a file from the device */
    FILE_ACCESS_WRITE,      /**< put a file on the device */
    FILE_ACCESS_STATUS,     /**< ask the size of a file */
    FILE_ACCESS_HASH,       /**< ask a checksum, to confirm a transfer */
};

/** What a name on the storage means to this firmware */
enum file_kind {
    FILE_KIND_UNKNOWN = 0,
    FILE_KIND_ROUTE,        /**< `.PAR` of the legacy or `.CRS` of the port */
    FILE_KIND_SEGMENT,      /**< the base-36 name of a segment */
    FILE_KIND_LOG,          /**< `@DDMMYY.txt`, an activity of the rider */
};

/**
 * @brief What the name is, by the rules of the legacy
 *
 * @param path Whole path, as the other side sent it
 */
enum file_kind file_policy_kind(const char *path);

/**
 * @brief Whether the transfer may go on
 *
 * The rules, which are of this port and not of the legacy (which had none):
 * everything lives on the root of the storage, nothing climbs out of it,
 * and only what the device knows how to read may be written — a route or a
 * segment. The activities go out but never come in, because the device is
 * what writes them, and a file the firmware does not understand has no
 * reason to take room.
 */
bool file_policy_allows(const char *path, enum file_access access);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_FILE_POLICY_H */
