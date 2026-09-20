/**
 * @file host_fs.c
 * @brief In-memory fake of the Zephyr file system API for host tests.
 *
 * A handful of files, each with a name and a content buffer, is enough for
 * what the firmware does with the card: append to the activity log, list
 * the root looking for segments and routes, and read a file line by line.
 *
 * host_fs_content() keeps answering with the file the test wrote last, so
 * the tests written before this table still hold.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/fs/fs.h>

#include "host_fs.h"

#define HOST_FS_FILES       12U
#define HOST_FS_FILE_SIZE   65536U

struct host_file {
    char name[MAX_FILE_NAME + 1U];
    char content[HOST_FS_FILE_SIZE + 1U];
    size_t size;
    bool used;
};

static struct host_file s_files[HOST_FS_FILES];
static bool s_available = true;
static unsigned int s_open_count;
static int s_last_written = -1;

/** Open file, as the handle of struct fs_file_t points at it */
struct host_open {
    int index;
    size_t pos;
};

static struct host_open s_open[HOST_FS_FILES];
static unsigned int s_open_slots;

/** Directory walk: index of the next file to hand out */
static unsigned int s_dir_pos;

/** Open file behind a handle of the shim, which is an index */
static struct host_open *slot_of(const struct fs_file_t *zfp)
{
    if ((zfp->handle < 0) || ((unsigned int)zfp->handle >= HOST_FS_FILES)) {
        return NULL;
    }

    return &s_open[zfp->handle];
}

static int find_file(const char *path)
{
    for (unsigned int i = 0U; i < HOST_FS_FILES; i++) {
        if (s_files[i].used && (strcmp(s_files[i].name, path) == 0)) {
            return (int)i;
        }
    }

    return -1;
}

static int create_file(const char *path)
{
    for (unsigned int i = 0U; i < HOST_FS_FILES; i++) {
        if (!s_files[i].used) {
            s_files[i].used = true;
            (void)strncpy(s_files[i].name, path, MAX_FILE_NAME);
            s_files[i].name[MAX_FILE_NAME] = '\0';
            s_files[i].size = 0U;
            s_files[i].content[0] = '\0';

            return (int)i;
        }
    }

    return -1;
}

void host_fs_reset(void)
{
    (void)memset(s_files, 0, sizeof(s_files));
    (void)memset(s_open, 0, sizeof(s_open));
    s_available = true;
    s_open_count = 0U;
    s_open_slots = 0U;
    s_dir_pos = 0U;
    s_last_written = -1;
}

void host_fs_set_available(bool available)
{
    s_available = available;
}

const char *host_fs_content(void)
{
    static const char empty[] = "";

    if (s_last_written < 0) {
        return empty;
    }
    s_files[s_last_written].content[s_files[s_last_written].size] = '\0';

    return s_files[s_last_written].content;
}

unsigned int host_fs_open_count(void)
{
    return s_open_count;
}

bool host_fs_add_file(const char *path, const char *content)
{
    int idx = find_file(path);

    if (idx < 0) {
        idx = create_file(path);
    }
    if (idx < 0) {
        return false;
    }

    size_t len = strlen(content);

    if (len > HOST_FS_FILE_SIZE) {
        return false;
    }
    (void)memcpy(s_files[idx].content, content, len);
    s_files[idx].size = len;
    s_files[idx].content[len] = '\0';

    return true;
}

const char *host_fs_file_content(const char *path)
{
    int idx = find_file(path);

    if (idx < 0) {
        return NULL;
    }
    s_files[idx].content[s_files[idx].size] = '\0';

    return s_files[idx].content;
}

unsigned int host_fs_file_count(void)
{
    unsigned int n = 0U;

    for (unsigned int i = 0U; i < HOST_FS_FILES; i++) {
        if (s_files[i].used) {
            n++;
        }
    }

    return n;
}

int fs_open(struct fs_file_t *zfp, const char *file_name, fs_mode_t flags)
{
    s_open_count++;
    if (!s_available) {
        return -ENOTSUP;
    }

    int idx = find_file(file_name);

    if (idx < 0) {
        if ((flags & FS_O_CREATE) == 0U) {
            return -ENOENT;
        }
        idx = create_file(file_name);
        if (idx < 0) {
            return -ENOSPC;
        }
    }

    if (s_open_slots >= HOST_FS_FILES) {
        return -ENFILE;
    }

    unsigned int slot = s_open_slots;

    s_open_slots++;
    s_open[slot].index = idx;
    if ((flags & FS_O_TRUNC) != 0U) {
        s_files[idx].size = 0U;
    }
    s_open[slot].pos = ((flags & FS_O_APPEND) != 0U) ? s_files[idx].size : 0U;

    zfp->handle = (int)slot;
    zfp->flags = flags;

    return 0;
}

int fs_close(struct fs_file_t *zfp)
{
    zfp->handle = -1;
    if (s_open_slots > 0U) {
        s_open_slots--;
    }

    return s_available ? 0 : -ENOTSUP;
}

ssize_t fs_read(struct fs_file_t *zfp, void *ptr, size_t size)
{
    if (!s_available) {
        return -ENOTSUP;
    }

    struct host_open *open = slot_of(zfp);

    if (open == NULL) {
        return -EBADF;
    }

    struct host_file *f = &s_files[open->index];
    size_t n = ((f->size - open->pos) < size) ? (f->size - open->pos) : size;

    (void)memcpy(ptr, &f->content[open->pos], n);
    open->pos += n;

    return (ssize_t)n;
}

ssize_t fs_write(struct fs_file_t *zfp, const void *ptr, size_t size)
{
    if (!s_available) {
        return -ENOTSUP;
    }

    struct host_open *open = slot_of(zfp);

    if (open == NULL) {
        return -EBADF;
    }

    struct host_file *f = &s_files[open->index];

    if ((open->pos + size) > HOST_FS_FILE_SIZE) {
        return -ENOSPC;
    }
    (void)memcpy(&f->content[open->pos], ptr, size);
    open->pos += size;
    if (open->pos > f->size) {
        f->size = open->pos;
    }
    s_last_written = open->index;

    return (ssize_t)size;
}

int fs_seek(struct fs_file_t *zfp, off_t offset, int whence)
{
    if (!s_available) {
        return -ENOTSUP;
    }

    struct host_open *open = slot_of(zfp);

    if (open == NULL) {
        return -EBADF;
    }

    const struct host_file *f = &s_files[open->index];
    size_t base = (whence == FS_SEEK_END) ? f->size
                                          : ((whence == FS_SEEK_CUR) ? open->pos : 0U);

    open->pos = base + (size_t)offset;

    return 0;
}

int fs_stat(const char *path, struct fs_dirent *entry)
{
    if (!s_available) {
        return -ENOTSUP;
    }

    int idx = find_file(path);

    if (idx < 0) {
        return -ENOENT;
    }
    (void)strncpy(entry->name, s_files[idx].name, MAX_FILE_NAME);
    entry->name[MAX_FILE_NAME] = '\0';
    entry->type = FS_DIR_ENTRY_FILE;
    entry->size = s_files[idx].size;

    return 0;
}

int fs_mkdir(const char *path)
{
    (void)path;

    return s_available ? 0 : -ENOTSUP;
}

int fs_unlink(const char *path)
{
    if (!s_available) {
        return -ENOTSUP;
    }

    int idx = find_file(path);

    if (idx < 0) {
        return -ENOENT;
    }
    s_files[idx].used = false;

    return 0;
}

int fs_opendir(struct fs_dir_t *zdp, const char *path)
{
    (void)path;
    if (!s_available) {
        return -ENOTSUP;
    }
    s_dir_pos = 0U;
    zdp->handle = 1;

    return 0;
}

int fs_readdir(struct fs_dir_t *zdp, struct fs_dirent *entry)
{
    (void)zdp;
    if (!s_available) {
        return -ENOTSUP;
    }

    while (s_dir_pos < HOST_FS_FILES) {
        const struct host_file *f = &s_files[s_dir_pos];

        s_dir_pos++;
        if (!f->used) {
            continue;
        }

        /* the firmware lists a directory and gets names without the path */
        const char *name = strrchr(f->name, '/');

        name = (name != NULL) ? (name + 1) : f->name;
        (void)strncpy(entry->name, name, MAX_FILE_NAME);
        entry->name[MAX_FILE_NAME] = '\0';
        entry->type = FS_DIR_ENTRY_FILE;
        entry->size = f->size;

        return 0;
    }

    entry->name[0] = '\0'; /* end of directory */

    return 0;
}

int fs_closedir(struct fs_dir_t *zdp)
{
    zdp->handle = -1;

    return 0;
}
