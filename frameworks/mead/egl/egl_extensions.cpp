/*
 * The EGL and GLES extension functions Android's libEGL and libGLESv2 export,
 * looked up with eglGetProcAddress on first use. Where the EGL
 * implementation lacks an extension, its functions return their documented
 * failure value, and GL's do nothing.
 */

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace {

template <typename Function>
Function lookup(const char* name) {
    return reinterpret_cast<Function>(eglGetProcAddress(name));
}

} // namespace

#define FORWARD(Return, name, failure, Params, Args)                    \
    extern "C" Return name Params {                                     \
        static const auto function = lookup<Return (*) Params>(#name); \
        if (function == nullptr) return failure;                        \
        return function Args;                                           \
    }

FORWARD(EGLImageKHR, eglCreateImageKHR, EGL_NO_IMAGE_KHR,
        (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer,
         const EGLint* attribs),
        (dpy, ctx, target, buffer, attribs))
FORWARD(EGLBoolean, eglDestroyImageKHR, EGL_FALSE, (EGLDisplay dpy, EGLImageKHR image),
        (dpy, image))
FORWARD(EGLSyncKHR, eglCreateSyncKHR, EGL_NO_SYNC_KHR,
        (EGLDisplay dpy, EGLenum type, const EGLint* attribs), (dpy, type, attribs))
FORWARD(EGLBoolean, eglDestroySyncKHR, EGL_FALSE, (EGLDisplay dpy, EGLSyncKHR sync),
        (dpy, sync))
FORWARD(EGLint, eglClientWaitSyncKHR, EGL_FALSE,
        (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout),
        (dpy, sync, flags, timeout))
FORWARD(EGLint, eglWaitSyncKHR, EGL_FALSE, (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags),
        (dpy, sync, flags))
FORWARD(EGLint, eglDupNativeFenceFDANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID,
        (EGLDisplay dpy, EGLSyncKHR sync), (dpy, sync))
FORWARD(EGLClientBuffer, eglGetNativeClientBufferANDROID, nullptr,
        (const struct AHardwareBuffer* buffer), (buffer))
FORWARD(EGLBoolean, eglSetDamageRegionKHR, EGL_FALSE,
        (EGLDisplay dpy, EGLSurface surface, EGLint* rects, EGLint n_rects),
        (dpy, surface, rects, n_rects))

// Without the extension, the whole surface is swapped.
extern "C" EGLBoolean eglSwapBuffersWithDamageKHR(EGLDisplay dpy, EGLSurface surface,
                                                  const EGLint* rects, EGLint n_rects) {
    static const auto function =
            lookup<EGLBoolean (*)(EGLDisplay, EGLSurface, const EGLint*, EGLint)>(
                    "eglSwapBuffersWithDamageKHR");
    if (function == nullptr) return eglSwapBuffers(dpy, surface);
    return function(dpy, surface, rects, n_rects);
}

extern "C" void glEGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image) {
    static const auto function =
            lookup<void (*)(GLenum, GLeglImageOES)>("glEGLImageTargetTexture2DOES");
    if (function != nullptr) function(target, image);
}

// Android's libEGL's private frame marker, which hwui declares itself (with
// C++ linkage); there is no frame to mark here.
void eglBeginFrame(EGLDisplay, EGLSurface) {}
