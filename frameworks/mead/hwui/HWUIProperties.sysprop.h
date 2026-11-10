/*
 * hwui's system properties (HWUIProperties.sysprop), as Android's sysprop
 * compiler would generate their accessors: read-only properties, read
 * through libbase.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <android-base/parsebool.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>

namespace android::uirenderer {

inline std::optional<bool> use_vulkan() {
    std::string value = android::base::GetProperty("ro.hwui.use_vulkan", "");
    switch (android::base::ParseBool(value)) {
        case android::base::ParseBoolResult::kTrue:
            return true;
        case android::base::ParseBoolResult::kFalse:
            return false;
        default:
            return std::nullopt;
    }
}

inline std::optional<std::int32_t> render_ahead() {
    std::int32_t value;
    if (android::base::ParseInt(android::base::GetProperty("ro.hwui.render_ahead", ""), &value)) {
        return value;
    }
    return std::nullopt;
}

} // namespace android::uirenderer
