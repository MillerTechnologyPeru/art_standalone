/*
 * libcutils' property functions, which this tree's libcutils builds only for
 * Android, written over bionic's __system_property_get and _set (libbase's,
 * in-process, or Mead's bionic provider's).
 */

#include <cutils/properties.h>

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <limits>

extern "C" {

int property_get(const char* key, char* value, const char* default_value) {
    int length = __system_property_get(key, value);
    if (length > 0) {
        return length;
    }
    if (default_value == nullptr) {
        value[0] = '\0';
        return 0;
    }
    length = static_cast<int>(strnlen(default_value, PROPERTY_VALUE_MAX - 1));
    memcpy(value, default_value, length);
    value[length] = '\0';
    return length;
}

int property_set(const char* key, const char* value) {
    return __system_property_set(key, value);
}

int8_t property_get_bool(const char* key, int8_t default_value) {
    char value[PROPERTY_VALUE_MAX];
    if (property_get(key, value, nullptr) == 0) {
        return default_value;
    }
    if (!strcmp(value, "1") || !strcmp(value, "y") || !strcmp(value, "yes") ||
        !strcmp(value, "on") || !strcmp(value, "true")) {
        return 1;
    }
    if (!strcmp(value, "0") || !strcmp(value, "n") || !strcmp(value, "no") ||
        !strcmp(value, "off") || !strcmp(value, "false")) {
        return 0;
    }
    return default_value;
}

static intmax_t property_get_integer(const char* key, intmax_t default_value, intmax_t lower,
                                     intmax_t upper) {
    char value[PROPERTY_VALUE_MAX];
    if (property_get(key, value, nullptr) == 0) {
        return default_value;
    }
    char* end;
    errno = 0;
    intmax_t result = strtoimax(value, &end, 0);
    if (errno != 0 || end == value || *end != '\0' || result < lower || result > upper) {
        return default_value;
    }
    return result;
}

int64_t property_get_int64(const char* key, int64_t default_value) {
    return property_get_integer(key, default_value, std::numeric_limits<int64_t>::min(),
                                std::numeric_limits<int64_t>::max());
}

int32_t property_get_int32(const char* key, int32_t default_value) {
    return property_get_integer(key, default_value, std::numeric_limits<int32_t>::min(),
                                std::numeric_limits<int32_t>::max());
}

}
