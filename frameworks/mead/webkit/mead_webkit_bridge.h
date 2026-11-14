/*
 * The bridge between Mead and the WPE web processes: a page's JavaScript
 * interfaces, called synchronously over a Unix socket, as Android's are.
 *
 * Each message is a list of strings: its length, then each string's length
 * and bytes (32-bit counts, native order: both ends are on one machine).
 *
 *   ["interfaces", page id]               -> ["name:method,method", ...]
 *   ["call", page id, object, method, argument JSON...] -> [result JSON]
 *
 * A result of "undefined" is JavaScript's undefined.
 */

#ifndef MEAD_WEBKIT_BRIDGE_H
#define MEAD_WEBKIT_BRIDGE_H

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline int mead_bridge_write_all(int fd, const void* data, size_t length) {
    const char* bytes = data;
    while (length > 0) {
        ssize_t written = write(fd, bytes, length);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return -1;
        bytes += written;
        length -= (size_t)written;
    }
    return 0;
}

static inline int mead_bridge_read_all(int fd, void* data, size_t length) {
    char* bytes = data;
    while (length > 0) {
        ssize_t got = read(fd, bytes, length);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) return -1;
        bytes += got;
        length -= (size_t)got;
    }
    return 0;
}

/* Sends a list of strings; 0 on success. */
static inline int mead_bridge_send(int fd, const char* const* strings, uint32_t count) {
    if (mead_bridge_write_all(fd, &count, sizeof(count)) != 0) return -1;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t length = strings[i] ? (uint32_t)strlen(strings[i]) : 0;
        if (mead_bridge_write_all(fd, &length, sizeof(length)) != 0) return -1;
        if (length && mead_bridge_write_all(fd, strings[i], length) != 0) return -1;
    }
    return 0;
}

/* Receives a list of strings, each and the list malloc'd; NULL on failure. */
static inline char** mead_bridge_receive(int fd, uint32_t* count) {
    if (mead_bridge_read_all(fd, count, sizeof(*count)) != 0 || *count > 4096) return NULL;
    char** strings = calloc(*count + 1, sizeof(char*));
    for (uint32_t i = 0; i < *count; i++) {
        uint32_t length;
        if (mead_bridge_read_all(fd, &length, sizeof(length)) != 0 || length > (64u << 20)) goto fail;
        strings[i] = malloc(length + 1);
        if (length && mead_bridge_read_all(fd, strings[i], length) != 0) goto fail;
        strings[i][length] = 0;
    }
    return strings;
fail:
    for (uint32_t i = 0; i < *count; i++) free(strings[i]);
    free(strings);
    return NULL;
}

static inline void mead_bridge_free(char** strings, uint32_t count) {
    if (!strings) return;
    for (uint32_t i = 0; i < count; i++) free(strings[i]);
    free(strings);
}

#endif
