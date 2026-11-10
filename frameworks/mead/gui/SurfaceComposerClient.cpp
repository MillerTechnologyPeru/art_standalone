/*
 * Mead's SurfaceComposerClient: the display Mead's bridge describes. See
 * gui/SurfaceComposerClient.h.
 */

#include <gui/SurfaceComposerClient.h>

#include <binder/Binder.h>
#include <mead/gui.h>

namespace android {

sp<IBinder> SurfaceComposerClient::getInternalDisplayToken() {
    static const sp<IBinder> token = new BBinder();
    return token;
}

status_t SurfaceComposerClient::getDisplayInfo(const sp<IBinder>& display, DisplayInfo* info) {
    if (display == nullptr || display != getInternalDisplayToken()) return NAME_NOT_FOUND;
    MeadDisplay described;
    mead_gui_get_display(&described);
    *info = DisplayInfo();
    info->w = described.width;
    info->h = described.height;
    info->xdpi = described.xdpi;
    info->ydpi = described.ydpi;
    info->fps = described.refreshRate;
    info->density = described.density;
    info->orientation = DISPLAY_ORIENTATION_0;
    info->secure = true;
    info->viewportW = described.width;
    info->viewportH = described.height;
    return NO_ERROR;
}

status_t SurfaceComposerClient::getDisplayConfigs(const sp<IBinder>& display,
                                                  Vector<DisplayInfo>* configs) {
    DisplayInfo info;
    status_t error = getDisplayInfo(display, &info);
    if (error != NO_ERROR) return error;
    configs->clear();
    configs->push_back(info);
    return NO_ERROR;
}

int SurfaceComposerClient::getActiveConfig(const sp<IBinder>& display) {
    return display != nullptr && display == getInternalDisplayToken() ? 0 : NAME_NOT_FOUND;
}

status_t SurfaceComposerClient::getCompositionPreference(
        ui::Dataspace* defaultDataspace, ui::PixelFormat* defaultPixelFormat,
        ui::Dataspace* wideColorGamutDataspace, ui::PixelFormat* wideColorGamutPixelFormat) {
    *defaultDataspace = ui::Dataspace::V0_SRGB;
    *defaultPixelFormat = ui::PixelFormat::RGBA_8888;
    *wideColorGamutDataspace = ui::Dataspace::V0_SRGB;
    *wideColorGamutPixelFormat = ui::PixelFormat::RGBA_8888;
    return NO_ERROR;
}

} // namespace android
