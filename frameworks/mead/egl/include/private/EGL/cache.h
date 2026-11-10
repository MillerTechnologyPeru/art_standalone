/*
 * Android's libEGL's private blob cache file setting, which the framework
 * calls; Mead's stand-in is in egl_extensions.cpp.
 */

#pragma once

namespace android {

void egl_set_cache_filename(const char* filename);

} // namespace android
