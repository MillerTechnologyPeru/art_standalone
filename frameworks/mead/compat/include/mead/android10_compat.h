/*
 * What Android 10's framework code expects of bionic, liblog and libcutils
 * that this tree's older libcutils and liblog and glibc don't provide.
 * Included ahead of every source file of the Android 10 framework modules
 * (with -include), so their sources stay as AOSP has them.
 */

#ifndef MEAD_ANDROID10_COMPAT_H
#define MEAD_ANDROID10_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* What bionic's headers bring in with others, and glibc's don't:
 * CLOCK_MONOTONIC, dprintf, malloc and free, open's flags. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef __cplusplus
/* What libc++'s headers bring in with others, and libstdc++'s don't. */
#include <algorithm>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* glibc has the kernel's capability header; this tree's libcutils carries a
 * copy for hosts without one, which would define the same types again. */
#include <linux/capability.h>
#define _SYSTEM_CORE_INCLUDE_PRIVATE_ANDROID_FILESYSTEM_CAPABILITY_H

/* liblog's largest message, which Android 10's liblog headers define. */
#ifndef LOGGER_ENTRY_MAX_PAYLOAD
#define LOGGER_ENTRY_MAX_PAYLOAD 4068
#endif

/* bionic's <limits.h>. */
#ifndef SIZE_T_MAX
#define SIZE_T_MAX SIZE_MAX
#endif

/* liblog's security event log: nothing collects it here. */
static inline int android_errorWriteLog(int tag, const char* subTag) {
    (void)tag;
    (void)subTag;
    return 0;
}

static inline int android_errorWriteWithInfoLog(int tag, const char* subTag, int32_t uid,
                                                const char* data, uint32_t dataLen) {
    (void)tag;
    (void)subTag;
    (void)uid;
    (void)data;
    (void)dataLen;
    return 0;
}

/* The NDK headers Android 10's code includes (<android/hardware_buffer.h>,
 * <android/sync.h>) declare their functions for the API level being built
 * for, with bionic's availability annotation: Android 10's, API 29. */
#ifndef __ANDROID_API__
#define __ANDROID_API__ 29
#endif
#ifndef __INTRODUCED_IN
#define __INTRODUCED_IN(api)
#endif

/* libui is built as in Android's vendor configuration, without BufferHub;
 * its headers declare BufferHub's members only without this. */
#ifndef LIBUI_IN_VNDK
#define LIBUI_IN_VNDK 1
#endif

/* libcutils' ashmem: there is no ashmem off Android, so no descriptor is one. */
static inline int ashmem_valid(int fd) {
    (void)fd;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
