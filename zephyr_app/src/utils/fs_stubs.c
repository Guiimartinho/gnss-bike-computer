/**
 * @file fs_stubs.c
 * @brief Stub implementations for filesystem functions when CONFIG_FILE_SYSTEM is disabled
 *
 * These stubs allow the code to compile without the filesystem subsystem.
 * All operations return error or no-op.
 */

#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <errno.h>

#ifndef CONFIG_FILE_SYSTEM

int fs_open(struct fs_file_t *zfp, const char *file_name, fs_mode_t flags)
{
    ARG_UNUSED(zfp);
    ARG_UNUSED(file_name);
    ARG_UNUSED(flags);
    return -ENOTSUP;
}

int fs_close(struct fs_file_t *zfp)
{
    ARG_UNUSED(zfp);
    return -ENOTSUP;
}

ssize_t fs_read(struct fs_file_t *zfp, void *ptr, size_t size)
{
    ARG_UNUSED(zfp);
    ARG_UNUSED(ptr);
    ARG_UNUSED(size);
    return -ENOTSUP;
}

ssize_t fs_write(struct fs_file_t *zfp, const void *ptr, size_t size)
{
    ARG_UNUSED(zfp);
    ARG_UNUSED(ptr);
    ARG_UNUSED(size);
    return -ENOTSUP;
}

int fs_stat(const char *path, struct fs_dirent *entry)
{
    ARG_UNUSED(path);
    ARG_UNUSED(entry);
    return -ENOTSUP;
}

int fs_mkdir(const char *path)
{
    ARG_UNUSED(path);
    return -ENOTSUP;
}

int fs_opendir(struct fs_dir_t *zdp, const char *path)
{
    ARG_UNUSED(zdp);
    ARG_UNUSED(path);
    return -ENOTSUP;
}

int fs_readdir(struct fs_dir_t *zdp, struct fs_dirent *entry)
{
    ARG_UNUSED(zdp);
    if (entry != NULL) {
        entry->name[0] = '\0';  /* Signal end of directory */
    }
    return 0;
}

int fs_closedir(struct fs_dir_t *zdp)
{
    ARG_UNUSED(zdp);
    return 0;
}

int fs_seek(struct fs_file_t *zfp, off_t offset, int whence)
{
    ARG_UNUSED(zfp);
    ARG_UNUSED(offset);
    ARG_UNUSED(whence);
    return -ENOTSUP;
}

off_t fs_tell(struct fs_file_t *zfp)
{
    ARG_UNUSED(zfp);
    return (off_t)-ENOTSUP;
}

int fs_truncate(struct fs_file_t *zfp, off_t length)
{
    ARG_UNUSED(zfp);
    ARG_UNUSED(length);
    return -ENOTSUP;
}

int fs_sync(struct fs_file_t *zfp)
{
    ARG_UNUSED(zfp);
    return -ENOTSUP;
}

int fs_unlink(const char *path)
{
    ARG_UNUSED(path);
    return -ENOTSUP;
}

int fs_rename(const char *from, const char *to)
{
    ARG_UNUSED(from);
    ARG_UNUSED(to);
    return -ENOTSUP;
}

#endif /* !CONFIG_FILE_SYSTEM */
