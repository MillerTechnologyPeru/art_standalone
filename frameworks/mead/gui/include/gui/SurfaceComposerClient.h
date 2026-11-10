/*
 * Mead's SurfaceComposerClient: the display queries the framework's
 * graphics make, answered from the display Mead's bridge describes
 * (mead_gui_set_display). There is one display and no composition.
 */

#pragma once

#include <binder/IBinder.h>
#include <ui/DisplayInfo.h>
#include <ui/GraphicTypes.h>
#include <utils/Errors.h>
#include <utils/StrongPointer.h>
#include <utils/Vector.h>

namespace android {

class SurfaceComposerClient {
public:
    // The one display's token.
    static sp<IBinder> getInternalDisplayToken();

    static status_t getDisplayInfo(const sp<IBinder>& display, DisplayInfo* info);

    // The display's configurations: the one it has.
    static status_t getDisplayConfigs(const sp<IBinder>& display, Vector<DisplayInfo>* configs);

    static int getActiveConfig(const sp<IBinder>& display);

    // sRGB in RGBA 8888, without wide color.
    static status_t getCompositionPreference(ui::Dataspace* defaultDataspace,
                                             ui::PixelFormat* defaultPixelFormat,
                                             ui::Dataspace* wideColorGamutDataspace,
                                             ui::PixelFormat* wideColorGamutPixelFormat);

private:
    SurfaceComposerClient() = delete;
};

} // namespace android
