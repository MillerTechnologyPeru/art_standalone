/*
 * statsd's generated logging header, as hwui's jank tracker uses it. Mead has
 * no statsd, so the events go nowhere.
 */

#pragma once

#include <stdint.h>

namespace android::util {

enum { DAVEY_OCCURRED = 58 };

template <typename... Fields>
inline int stats_write(int32_t /*code*/, Fields... /*fields*/) {
    return 0;
}

} // namespace android::util
