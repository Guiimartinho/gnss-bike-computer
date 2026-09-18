/**
 * @file fs.h
 * @brief Host-test shim for <zephyr/fs/fs.h>.
 *
 * Same types, flags and signatures as Zephyr 4.3; the implementation is the
 * in-memory fake of support/host_fs.c (host_fs.h controls failures and reads
 * back what was written).
 */

#ifndef HOST_SHIM_ZEPHYR_FS_FS_H
#define HOST_SHIM_ZEPHYR_FS_FS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define MAX_FILE_NAME 255

#define FS_O_READ       0x01
#define FS_O_WRITE      0x02
#define FS_O_RDWR       (FS_O_READ | FS_O_WRITE)
#define FS_O_CREATE     0x10
#define FS_O_APPEND     0x20
#define FS_O_TRUNC      0x40

#define FS_SEEK_SET 0
#define FS_SEEK_CUR 1
#define FS_SEEK_END 2

typedef uint8_t fs_mode_t;

enum fs_dir_entry_type {
    FS_DIR_ENTRY_FILE = 0,
    FS_DIR_ENTRY_DIR
};

struct fs_file_t {
    int handle;
    fs_mode_t flags;
};

struct fs_dir_t {
    int handle;
};

struct fs_dirent {
    enum fs_dir_entry_type type;
    char name[MAX_FILE_NAME + 1];
    size_t size;
};

static inline void fs_file_t_init(struct fs_file_t *zfp)
{
    zfp->handle = -1;
    zfp->flags = 0U;
}

static inline void fs_dir_t_init(struct fs_dir_t *zdp)
{
    zdp->handle = -1;
}

int fs_open(struct fs_file_t *zfp, const char *file_name, fs_mode_t flags);
int fs_close(struct fs_file_t *zfp);
ssize_t fs_read(struct fs_file_t *zfp, void *ptr, size_t size);
ssize_t fs_write(struct fs_file_t *zfp, const void *ptr, size_t size);
int fs_seek(struct fs_file_t *zfp, off_t offset, int whence);
int fs_stat(const char *path, struct fs_dirent *entry);
int fs_mkdir(const char *path);
int fs_unlink(const char *path);
int fs_opendir(struct fs_dir_t *zdp, const char *path);
int fs_readdir(struct fs_dir_t *zdp, struct fs_dirent *entry);
int fs_closedir(struct fs_dir_t *zdp);

#endif /* HOST_SHIM_ZEPHYR_FS_FS_H */
