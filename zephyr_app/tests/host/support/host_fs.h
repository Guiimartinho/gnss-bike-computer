/**
 * @file host_fs.h
 * @brief Controls the in-memory file system fake used by host tests.
 *
 * One file at a time is enough for the modules under test: every path shares
 * the same content buffer, and host_fs_set_available(false) makes every call
 * fail with -ENOTSUP, like zephyr_app/src/utils/fs_stubs.c on the target.
 */

#ifndef HOST_FS_H
#define HOST_FS_H

#include <stdbool.h>
#include <stddef.h>

/** Empties the fake file and makes the file system available again. */
void host_fs_reset(void);

/** false: every fs_* call fails with -ENOTSUP (no card, no mount). */
void host_fs_set_available(bool available);

/** Bytes written so far (NUL-terminated copy of the fake file). */
const char *host_fs_content(void);

/** Number of fs_open() calls, successful or not. */
unsigned int host_fs_open_count(void);

/** Put a file with this content in the fake card; true on success. */
bool host_fs_add_file(const char *path, const char *content);

/** Put a file of raw bytes on the card (a `.RTE` has zeros inside) */
bool host_fs_add_bytes(const char *path, const void *bytes, size_t len);

/** Content of a file, or NULL when it is not there. */
const char *host_fs_file_content(const char *path);

/** How many files the fake card holds. */
unsigned int host_fs_file_count(void);

#endif /* HOST_FS_H */
