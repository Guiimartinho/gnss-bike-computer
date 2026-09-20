/**
 * @file file_policy.c
 * @brief What the phone may read and write on the storage of the device
 */

#include <string.h>

#include "model/file_policy.h"
#include "model/segment_file.h"

/** Longest name FatFs takes here, 8.3 of the legacy plus room to spare */
#define FILE_POLICY_NAME_MAX    20U

/** The name inside the root, or NULL when the path is not one of ours */
static const char *name_of(const char *path)
{
    if (path == NULL) {
        return NULL;
    }

    size_t root = strlen(FILE_POLICY_ROOT);

    if (strncmp(path, FILE_POLICY_ROOT, root) != 0) {
        return NULL;
    }

    const char *name = &path[root];

    /* the root and nothing else: no directories, nothing climbing out */
    if ((name[0] == '\0') || (strchr(name, '/') != NULL) || (strstr(name, "..") != NULL)) {
        return NULL;
    }
    if (strlen(name) > FILE_POLICY_NAME_MAX) {
        return NULL;
    }

    return name;
}

/** Whether the name ends with @p ext, in either case */
static bool ends_with(const char *name, const char *ext)
{
    size_t n = strlen(name);
    size_t e = strlen(ext);

    if (n <= e) {
        return false;
    }

    const char *tail = &name[n - e];

    for (size_t i = 0U; i < e; i++) {
        char a = tail[i];
        char b = ext[i];

        if ((a >= 'a') && (a <= 'z')) {
            a = (char)(a - ('a' - 'A'));
        }
        if ((b >= 'a') && (b <= 'z')) {
            b = (char)(b - ('a' - 'A'));
        }
        if (a != b) {
            return false;
        }
    }

    return true;
}

enum file_kind file_policy_kind(const char *path)
{
    const char *name = name_of(path);

    if (name == NULL) {
        return FILE_KIND_UNKNOWN;
    }

    if (ends_with(name, ".PAR") || ends_with(name, ".CRS")) {
        return FILE_KIND_ROUTE;
    }

    /* `@DDMMYY.txt` of the legacy (`sd_functions.cpp`) */
    if ((name[0] == '@') && ends_with(name, ".TXT")) {
        return FILE_KIND_LOG;
    }

    if (segment_file_name_is_valid(name)) {
        return FILE_KIND_SEGMENT;
    }

    return FILE_KIND_UNKNOWN;
}

bool file_policy_allows(const char *path, enum file_access access)
{
    enum file_kind kind = file_policy_kind(path);

    if (kind == FILE_KIND_UNKNOWN) {
        return false;
    }

    switch (access) {
    case FILE_ACCESS_WRITE:
        /* the rider sends routes and segments; the activities are written
         * by the device itself and are not taken from outside */
        return (kind == FILE_KIND_ROUTE) || (kind == FILE_KIND_SEGMENT);

    case FILE_ACCESS_READ:
    case FILE_ACCESS_STATUS:
    case FILE_ACCESS_HASH:
        return true;

    default:
        return false;
    }
}
