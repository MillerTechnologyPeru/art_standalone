/*
 * Android's /system is ANDROID_ROOT here, and its /data ANDROID_DATA: the
 * framework, the core library and apps read paths written into Android's
 * code (/system/fonts, /system/etc/fonts.xml, /data for SQLite's page
 * size...), and on Linux there is no /system or /data, or ones that aren't
 * Android's. The core library's file natives take such paths as paths in
 * ANDROID_ROOT and ANDROID_DATA; every other path is as it was.
 */

#ifndef MEAD_SYSTEM_PATH_H
#define MEAD_SYSTEM_PATH_H

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* path, or when it is in Android's top-level directory android (as
 * "/system", of length length) and variable names somewhere else, the same
 * path there, written to buffer; NULL when it isn't in android. */
static inline const char* mead_android_path(const char* path, const char* android, size_t length,
                                            const char* variable, char* buffer, size_t size) {
    if (strncmp(path, android, length) != 0 || (path[length] != '\0' && path[length] != '/')) {
        return NULL;
    }
    const char* root = getenv(variable);
    if (root == NULL || root[0] == '\0' || strcmp(root, android) == 0) {
        return path;
    }
    int written = snprintf(buffer, size, "%s%s", root, path + length);
    return written < 0 || (size_t)written >= size ? path : buffer;
}

/* path, or when it is /system or /data or in them and ANDROID_ROOT or
 * ANDROID_DATA is elsewhere, the same path there, written to buffer. */
static inline const char* mead_system_path(const char* path, char* buffer, size_t size) {
    if (path == NULL) return path;
    const char* mapped = mead_android_path(path, "/system", 7, "ANDROID_ROOT", buffer, size);
    if (mapped == NULL) mapped = mead_android_path(path, "/data", 5, "ANDROID_DATA", buffer, size);
    return mapped != NULL ? mapped : path;
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
