/*
 * Mead's ISurfaceComposer: there is no SurfaceFlinger to talk to, so only
 * the types that the rest of Mead's libgui uses in its signatures.
 */

#pragma once

namespace android {

class ISurfaceComposer {
public:
    enum VsyncSource { eVsyncSourceApp = 0, eVsyncSourceSurfaceFlinger = 1 };

private:
    ISurfaceComposer() = delete;
};

} // namespace android
