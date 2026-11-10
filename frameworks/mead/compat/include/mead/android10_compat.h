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

/* libcutils' ashmem: there is no ashmem off Android, so no descriptor is one. */
static inline int ashmem_valid(int fd) {
    (void)fd;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
