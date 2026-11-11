/*
 * Mead's SurfaceControl: a window the display bridge made, and the surface
 * drawing to it. See android_view_SurfaceControl.cpp.
 */

#pragma once

#include <binder/Binder.h>
#include <gui/Surface.h>
#include <utils/RefBase.h>

namespace android {

struct MeadSurfaceControl : public RefBase {
    sp<Surface> surface;
    // The handle Java's SurfaceControl.getHandle gives out.
    sp<BBinder> handle = new BBinder();
};

} // namespace android
