/**
 * @file host_fs.c
 * @brief In-memory fake of the Zephyr file system API for host tests.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/fs/fs.h>

#include "host_fs.h"

#define HOST_FS_SIZE 16384U

static char s_content[HOST_FS_SIZE + 1U];
static size_t s_size;
static size_t s_pos;
static bool s_available = true;
static unsigned int s_open_count;

void host_fs_reset(void)
{
    s_size = 0U;
    s_pos = 0U;
    s_content[0] = '\0';
    s_available = true;
    s_open_count = 0U;
}

void host_fs_set_available(bool available)
{
    s_available = available;
}

const char *host_fs_content(void)
{
    s_content[s_size] = '\0';
    return s_content;
}

unsigned int host_fs_open_count(void)
{
    return s_open_count;
}

int fs_open(struct fs_file_t *zfp, const char *file_name, fs_mode_t flags)
{
    (void)file_name;
    s_open_count++;
    if (!s_available) {
        return -ENOTSUP;
    }
    zfp->handle = 1;
    zfp->flags = flags;
    if ((flags & FS_O_TRUNC) != 0U) {
        s_size = 0U;
    }
    s_pos = ((flags & FS_O_APPEND) != 0U) ? s_size : 0U;
    return 0;
}

int fs_close(struct fs_file_t *zfp)
{
    zfp->handle = -1;
    return s_available ? 0 : -ENOTSUP;
}

ssize_t fs_read(struct fs_file_t *zfp, void *ptr, size_t size)
{
    (void)zfp;
    if (!s_available) {
        return -ENOTSUP;
    }
    size_t n = ((s_size - s_pos) < size) ? (s_size - s_pos) : size;
    memcpy(ptr, &s_content[s_pos], n);
    s_pos += n;
    return (ssize_t)n;
}

ssize_t fs_write(struct fs_file_t *zfp, const void *ptr, size_t size)
{
    (void)zfp;
    if (!s_available) {
        return -ENOTSUP;
    }
    if ((s_pos + size) > HOST_FS_SIZE) {
        return -ENOSPC;
    }
    memcpy(&s_content[s_pos], ptr, size);
    s_pos += size;
    if (s_pos > s_size) {
        s_size = s_pos;
    }
    return (ssize_t)size;
}

int fs_seek(struct fs_file_t *zfp, off_t offset, int whence)
{
    (void)zfp;
    if (!s_available) {
        return -ENOTSUP;
    }
    size_t base = (whence == FS_SEEK_END) ? s_size : ((whence == FS_SEEK_CUR) ? s_pos : 0U);
    s_pos = base + (size_t)offset;
    return 0;
}

int fs_stat(const char *path, struct fs_dirent *entry)
{
    if (!s_available) {
        return -ENOTSUP;
    }
    (void)strncpy(entry->name, path, MAX_FILE_NAME);
    entry->name[MAX_FILE_NAME] = '\0';
    entry->type = FS_DIR_ENTRY_FILE;
    entry->size = s_size;
    return 0;
}

int fs_mkdir(const char *path)
{
    (void)path;
    return s_available ? 0 : -ENOTSUP;
}

int fs_unlink(const char *path)
{
    (void)path;
    if (!s_available) {
        return -ENOTSUP;
    }
    s_size = 0U;
    s_pos = 0U;
    return 0;
}

int fs_opendir(struct fs_dir_t *zdp, const char *path)
{
    (void)path;
    zdp->handle = 1;
    return s_available ? 0 : -ENOTSUP;
}

int fs_readdir(struct fs_dir_t *zdp, struct fs_dirent *entry)
{
    (void)zdp;
    entry->name[0] = '\0';  /* end of directory */
    return s_available ? 0 : -ENOTSUP;
}

int fs_closedir(struct fs_dir_t *zdp)
{
    zdp->handle = -1;
    return 0;
}
