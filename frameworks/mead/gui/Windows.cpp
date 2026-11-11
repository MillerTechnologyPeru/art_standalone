/*
 * Windows from Mead's display bridge, for SurfaceControl. See mead/gui.h.
 */

#include <mutex>

#include <mead/gui.h>

namespace {

std::mutex gLock;
MeadWindowCallbacks gCallbacks = {nullptr, nullptr, nullptr};

} // namespace

void mead_gui_set_window_callbacks(const MeadWindowCallbacks* callbacks) {
    std::lock_guard<std::mutex> lock(gLock);
    gCallbacks = *callbacks;
}

ANativeWindow* mead_gui_create_window(const char* name, uint32_t width, uint32_t height) {
    MeadWindowCallbacks callbacks;
    {
        std::lock_guard<std::mutex> lock(gLock);
        callbacks = gCallbacks;
    }
    if (callbacks.create == nullptr) return nullptr;
    return callbacks.create(callbacks.context, name, width, height);
}

void mead_gui_destroy_window(ANativeWindow* window) {
    MeadWindowCallbacks callbacks;
    {
        std::lock_guard<std::mutex> lock(gLock);
        callbacks = gCallbacks;
    }
    if (callbacks.destroy != nullptr) callbacks.destroy(callbacks.context, window);
}
