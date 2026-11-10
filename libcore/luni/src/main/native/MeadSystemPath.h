/*
 * Android's /system is ANDROID_ROOT here: the framework, the core library and
 * apps read /system paths written into Android's code (/system/fonts,
 * /system/etc/fonts.xml, /system/etc/permissions...), and on Linux there is
 * no /system, or one that isn't Android's. The core library's file natives
 * take such paths as paths in ANDROID_ROOT; every other path is as it was.
 */

#ifndef MEAD_SYSTEM_PATH_H
#define MEAD_SYSTEM_PATH_H

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* path, or when it is /system or in it and ANDROID_ROOT is elsewhere, the
 * same path in ANDROID_ROOT, written to buffer. */
static inline const char* mead_system_path(const char* path, char* buffer, size_t size) {
    if (path == NULL || strncmp(path, "/system", 7) != 0 || (path[7] != '\0' && path[7] != '/')) {
        return path;
    }
    const char* root = getenv("ANDROID_ROOT");
    if (root == NULL || root[0] == '\0' || strcmp(root, "/system") == 0) {
        return path;
    }
    int written = snprintf(buffer, size, "%s%s", root, path + 7);
    return written < 0 || (size_t)written >= size ? path : buffer;
}

#ifdef __cplusplus

#include <nativehelper/ScopedUtfChars.h>

/* A Java path string as a C string, with mead_system_path's mapping. */
class ScopedPathChars {
public:
    ScopedPathChars(JNIEnv* env, jstring path) : chars_(env, path) {
        path_ = mead_system_path(chars_.c_str(), buffer_, sizeof(buffer_));
    }

    ScopedPathChars(const ScopedPathChars&) = delete;
    ScopedPathChars& operator=(const ScopedPathChars&) = delete;

    const char* c_str() const { return path_; }

private:
    ScopedUtfChars chars_;
    char buffer_[PATH_MAX];
    const char* path_;
};

#endif

#endif
