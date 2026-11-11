/*
 * Mead's SurfaceControl: a window the display bridge made, and the surface
 * drawing to it; or a layer in a window (a SurfaceView's), with the state
 * transactions give it. See android_view_SurfaceControl.cpp.
 */

#pragma once

#include <binder/Binder.h>
#include <gui/Surface.h>
#include <mead/gui.h>
#include <utils/RefBase.h>

namespace android {

struct MeadSurfaceControl : public RefBase {
    // The surface: a window's, a buffer layer's, or none (a color or
    // container layer, which Mead doesn't draw).
    sp<Surface> surface;
    // The handle Java's SurfaceControl.getHandle gives out.
    sp<BBinder> handle = new BBinder();
    // A layer's: made with a parent, its state as transactions set it, and
    // whether nativeDestroy took it down.
    bool layer = false;
    MeadLayerState state = {};
    bool destroyed = false;

    ~MeadSurfaceControl() override {
        // A layer whose last reference goes without nativeDestroy goes too.
        if (layer && !destroyed && surface != nullptr) mead_gui_destroy_window(surface.get());
    }
};

} // namespace android
