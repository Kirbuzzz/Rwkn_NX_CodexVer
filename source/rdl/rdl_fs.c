#include "rdl_fs.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <sys/statvfs.h>
#endif

bool rdl_fs_copy_str(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    if (n >= cap) return false;
    memcpy(dst, src, n + 1);
    return true;
}

bool rdl_fs_join(char *out, size_t cap, const char *dir, const char *name)
{
    size_t dl = strlen(dir);
    int n = snprintf(out, cap, "%s%s%s", dir, (dl && dir[dl - 1] == '/') ? "" : "/", name);
    return n > 0 && (size_t)n < cap;
}

bool rdl_fs_mkdir_p(const char *path)
{
    char tmp[512];
    if (!rdl_fs_copy_str(tmp, sizeof tmp, path)) return false;
    size_t len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') tmp[--len] = 0;

    char *p = tmp;
    char *colon = strchr(tmp, ':');
    if (colon && colon[1] == '/') p = colon + 2;     /* "sdmc:/" */
    else if (*p == '/') p++;

    for (; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0777) != 0 && errno != EEXIST) return false;
            *p = '/';
        }
    }
    return mkdir(tmp, 0777) == 0 || errno == EEXIST;
}

void rdl_fs_ensure_parent(const char *file_path)
{
    char tmp[512];
    if (!rdl_fs_copy_str(tmp, sizeof tmp, file_path)) return;
    char *slash = strrchr(tmp, '/');
    if (slash && slash != tmp) { *slash = 0; rdl_fs_mkdir_p(tmp); }
}

bool rdl_fs_size(const char *path, uint64_t *out)
{
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return false;
    *out = (uint64_t)st.st_size;
    return true;
}

bool rdl_fs_free_space(const char *path, uint64_t *out)
{
#ifdef __SWITCH__
    (void)path;
    FsFileSystem *fs = fsdevGetDeviceFileSystem("sdmc");
    s64 fr = 0;
    if (!fs || R_FAILED(fsFsGetFreeSpace(fs, "/", &fr))) return false;
    *out = (uint64_t)fr;
    return true;
#else
    struct statvfs sv;
    if (statvfs(path, &sv) != 0) return false;
    *out = (uint64_t)sv.f_bavail * (uint64_t)sv.f_frsize;
    return true;
#endif
}
