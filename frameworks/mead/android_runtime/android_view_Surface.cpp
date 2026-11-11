/*
 * android.view.Surface's natives over Mead's Surface (frameworks/mead/gui),
 * in place of Android 10's android_view_Surface.cpp, which talks to
 * SurfaceFlinger's buffer queues. A Surface here comes from Mead's display
 * bridge, directly or through a SurfaceControl (Mead's: a window); one
 * can't be made from a SurfaceTexture or read from a Parcel yet.
 */

#define LOG_TAG "Surface"

#include "jni.h"
#include <nativehelper/JNIHelp.h>

#include "android/graphics/GraphicsJNI.h"
#include "core_jni_helpers.h"
#include <android_runtime/android_view_Surface.h>

#include <gui/Surface.h>

#include "MeadSurfaceControl.h"
#include <ui/PixelFormat.h>
#include <ui/PublicFormat.h>

#include <SkBitmap.h>
#include <SkCanvas.h>

#include <AnimationContext.h>
#include <FrameInfo.h>
#include <RenderNode.h>
#include <hwui/Canvas.h>
#include <renderthread/RenderProxy.h>
#include <utils/Log.h>

namespace android {

static const char* const kOutOfResourcesException = "android/view/Surface$OutOfResourcesException";
static const char* const kUnsupported = "java/lang/UnsupportedOperationException";

static struct {
    jclass clazz;
    jfieldID mNativeObject;
    jfieldID mLock;
    jmethodID ctor;
} gSurfaceClassInfo;

static struct {
    jfieldID left;
    jfieldID top;
    jfieldID right;
    jfieldID bottom;
} gRectClassInfo;

// The owner passed to incStrong and decStrong for Java's references.
static const void* sRefBaseOwner;

bool android_view_Surface_isInstanceOf(JNIEnv* env, jobject obj) {
    return env->IsInstanceOf(obj, gSurfaceClassInfo.clazz);
}

sp<ANativeWindow> android_view_Surface_getNativeWindow(JNIEnv* env, jobject surfaceObj) {
    return android_view_Surface_getSurface(env, surfaceObj);
}

sp<Surface> android_view_Surface_getSurface(JNIEnv* env, jobject surfaceObj) {
    sp<Surface> surface;
    jobject lock = env->GetObjectField(surfaceObj, gSurfaceClassInfo.mLock);
    if (env->MonitorEnter(lock) == JNI_OK) {
        surface = reinterpret_cast<Surface*>(
                env->GetLongField(surfaceObj, gSurfaceClassInfo.mNativeObject));
        env->MonitorExit(lock);
    }
    env->DeleteLocalRef(lock);
    return surface;
}

jobject android_view_Surface_createFromSurface(JNIEnv* env, const sp<Surface>& surface) {
    jobject surfaceObj =
            env->NewObject(gSurfaceClassInfo.clazz, gSurfaceClassInfo.ctor, (jlong)surface.get());
    if (surfaceObj == nullptr) {
        if (env->ExceptionCheck()) {
            ALOGE("Could not create a Surface for a native surface.");
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return nullptr;
    }
    surface->incStrong(&sRefBaseOwner);
    return surfaceObj;
}

jobject android_view_Surface_createFromIGraphicBufferProducer(JNIEnv*,
                                                              const sp<IGraphicBufferProducer>&) {
    // Mead's libgui has no buffer queues.
    return nullptr;
}

int android_view_Surface_mapPublicFormatToHalFormat(PublicFormat f) {
    return mapPublicFormatToHalFormat(f);
}

android_dataspace android_view_Surface_mapPublicFormatToHalDataspace(PublicFormat f) {
    return mapPublicFormatToHalDataspace(f);
}

PublicFormat android_view_Surface_mapHalFormatDataspaceToPublicFormat(int format,
                                                                      android_dataspace dataSpace) {
    return mapHalFormatDataspaceToPublicFormat(format, dataSpace);
}

static jlong nativeCreateFromSurfaceTexture(JNIEnv* env, jclass, jobject) {
    jniThrowException(env, kUnsupported, "Mead has no SurfaceTexture yet");
    return 0;
}

static void nativeRelease(JNIEnv*, jclass, jlong nativeObject) {
    reinterpret_cast<Surface*>(nativeObject)->decStrong(&sRefBaseOwner);
}

static jboolean nativeIsValid(JNIEnv*, jclass, jlong nativeObject) {
    return nativeObject != 0 ? JNI_TRUE : JNI_FALSE;
}

static jboolean nativeIsConsumerRunningBehind(JNIEnv* env, jclass, jlong nativeObject) {
    Surface* surface = reinterpret_cast<Surface*>(nativeObject);
    if (surface == nullptr) {
        doThrowIAE(env);
        return JNI_FALSE;
    }
    int value = 0;
    surface->query(NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND, &value);
    return value ? JNI_TRUE : JNI_FALSE;
}

static SkColorType convertPixelFormat(PixelFormat format) {
    switch (format) {
        case PIXEL_FORMAT_RGBX_8888:
        case PIXEL_FORMAT_RGBA_8888:
            return kN32_SkColorType;
        case PIXEL_FORMAT_RGBA_FP16:
            return kRGBA_F16_SkColorType;
        case PIXEL_FORMAT_RGB_565:
            return kRGB_565_SkColorType;
        default:
            return kUnknown_SkColorType;
    }
}

static jlong nativeLockCanvas(JNIEnv* env, jclass, jlong nativeObject, jobject canvasObj,
                              jobject dirtyRectObj) {
    sp<Surface> surface(reinterpret_cast<Surface*>(nativeObject));
    if (surface == nullptr) {
        doThrowIAE(env);
        return 0;
    }
    if (convertPixelFormat(ANativeWindow_getFormat(surface.get())) == kUnknown_SkColorType) {
        native_window_set_buffers_format(surface.get(), PIXEL_FORMAT_RGBA_8888);
    }

    ARect dirty = {0, 0, 0, 0};
    ANativeWindow_Buffer outBuffer;
    status_t err = surface->lock(&outBuffer, &dirty);
    if (err < 0) {
        jniThrowException(env,
                          err == NO_MEMORY ? kOutOfResourcesException
                                           : "java/lang/IllegalArgumentException",
                          nullptr);
        return 0;
    }

    SkImageInfo info = SkImageInfo::Make(outBuffer.width, outBuffer.height,
                                         convertPixelFormat(outBuffer.format),
                                         outBuffer.format == PIXEL_FORMAT_RGBX_8888
                                                 ? kOpaque_SkAlphaType
                                                 : kPremul_SkAlphaType);
    SkBitmap bitmap;
    bitmap.setInfo(info, static_cast<size_t>(outBuffer.stride) * bytesPerPixel(outBuffer.format));
    bitmap.setPixels(outBuffer.width > 0 && outBuffer.height > 0 ? outBuffer.bits : nullptr);

    Canvas* nativeCanvas = GraphicsJNI::getNativeCanvas(env, canvasObj);
    nativeCanvas->setBitmap(bitmap);

    // The whole buffer is to be drawn: frames' earlier contents aren't kept.
    if (dirtyRectObj) {
        env->SetIntField(dirtyRectObj, gRectClassInfo.left, dirty.left);
        env->SetIntField(dirtyRectObj, gRectClassInfo.top, dirty.top);
        env->SetIntField(dirtyRectObj, gRectClassInfo.right, dirty.right);
        env->SetIntField(dirtyRectObj, gRectClassInfo.bottom, dirty.bottom);
    }

    // Another reference, which nativeUnlockCanvasAndPost is given.
    surface->incStrong(&sRefBaseOwner);
    return (jlong)surface.get();
}

static void nativeUnlockCanvasAndPost(JNIEnv* env, jclass, jlong nativeObject,
                                      jobject canvasObj) {
    sp<Surface> surface(reinterpret_cast<Surface*>(nativeObject));
    if (surface == nullptr) return;
    Canvas* nativeCanvas = GraphicsJNI::getNativeCanvas(env, canvasObj);
    nativeCanvas->setBitmap(SkBitmap());
    if (surface->unlockAndPost() < 0) {
        doThrowIAE(env);
    }
}

static void nativeAllocateBuffers(JNIEnv*, jclass, jlong nativeObject) {
    Surface* surface = reinterpret_cast<Surface*>(nativeObject);
    if (surface != nullptr) surface->allocateBuffers();
}

// The surface of a SurfaceControl, Mead's: the one drawing to its window.
static jlong nativeCreateFromSurfaceControl(JNIEnv*, jclass, jlong surfaceControlNativeObj) {
    auto control = reinterpret_cast<MeadSurfaceControl*>(surfaceControlNativeObj);
    if (control == nullptr || control->surface == nullptr) return 0;
    control->surface->incStrong(&sRefBaseOwner);
    return reinterpret_cast<jlong>(control->surface.get());
}

static jlong nativeGetFromSurfaceControl(JNIEnv* env, jclass clazz, jlong nativeObject,
                                         jlong surfaceControlNativeObj) {
    auto control = reinterpret_cast<MeadSurfaceControl*>(surfaceControlNativeObj);
    if (control != nullptr && reinterpret_cast<Surface*>(nativeObject) == control->surface.get()) {
        return nativeObject;
    }
    return nativeCreateFromSurfaceControl(env, clazz, surfaceControlNativeObj);
}

static jlong nativeReadFromParcel(JNIEnv* env, jclass, jlong, jobject) {
    jniThrowException(env, kUnsupported, "Mead's surfaces don't cross processes yet");
    return 0;
}

static void nativeWriteToParcel(JNIEnv* env, jclass, jlong, jobject) {
    jniThrowException(env, kUnsupported, "Mead's surfaces don't cross processes yet");
}

static jint nativeGetWidth(JNIEnv*, jclass, jlong nativeObject) {
    int value = 0;
    reinterpret_cast<Surface*>(nativeObject)->query(NATIVE_WINDOW_WIDTH, &value);
    return value;
}

static jint nativeGetHeight(JNIEnv*, jclass, jlong nativeObject) {
    int value = 0;
    reinterpret_cast<Surface*>(nativeObject)->query(NATIVE_WINDOW_HEIGHT, &value);
    return value;
}

static jlong nativeGetNextFrameNumber(JNIEnv*, jclass, jlong nativeObject) {
    return static_cast<jlong>(reinterpret_cast<Surface*>(nativeObject)->getNextFrameNumber());
}

static jint nativeSetScalingMode(JNIEnv*, jclass, jlong nativeObject, jint scalingMode) {
    return reinterpret_cast<Surface*>(nativeObject)->setScalingMode(scalingMode);
}

static jint nativeForceScopedDisconnect(JNIEnv*, jclass, jlong) {
    return NO_ERROR;
}

static jint nativeAttachAndQueueBuffer(JNIEnv*, jclass, jlong, jobject) {
    return INVALID_OPERATION;
}

static jint nativeSetSharedBufferModeEnabled(JNIEnv*, jclass, jlong nativeObject,
                                             jboolean enabled) {
    ANativeWindow* window = reinterpret_cast<Surface*>(nativeObject);
    return native_window_set_shared_buffer_mode(window, enabled);
}

static jint nativeSetAutoRefreshEnabled(JNIEnv*, jclass, jlong nativeObject, jboolean enabled) {
    ANativeWindow* window = reinterpret_cast<Surface*>(nativeObject);
    return native_window_set_auto_refresh(window, enabled);
}

// Surface's hwui context, for lockHardwareCanvas: Android's.
namespace uirenderer {

using namespace android::uirenderer::renderthread;

class ContextFactory : public IContextFactory {
public:
    virtual AnimationContext* createAnimationContext(renderthread::TimeLord& clock) {
        return new AnimationContext(clock);
    }
};

static jlong create(JNIEnv*, jclass, jlong rootNodePtr, jlong surfacePtr,
                    jboolean isWideColorGamut) {
    RenderNode* rootNode = reinterpret_cast<RenderNode*>(rootNodePtr);
    sp<Surface> surface(reinterpret_cast<Surface*>(surfacePtr));
    ContextFactory factory;
    RenderProxy* proxy = new RenderProxy(false, rootNode, &factory);
    proxy->loadSystemProperties();
    if (isWideColorGamut) {
        proxy->setWideGamut(true);
    }
    proxy->setSwapBehavior(SwapBehavior::kSwap_discardBuffer);
    proxy->setSurface(surface);
    // Shadows can't be used via this interface, so just set the light source
    // to all 0s.
    proxy->setLightAlpha(0, 0);
    proxy->setLightGeometry((Vector3){0, 0, 0}, 0);
    return (jlong)proxy;
}

static void setSurface(JNIEnv*, jclass, jlong rendererPtr, jlong surfacePtr) {
    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(rendererPtr);
    sp<Surface> surface(reinterpret_cast<Surface*>(surfacePtr));
    proxy->setSurface(surface);
}

static void draw(JNIEnv*, jclass, jlong rendererPtr) {
    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(rendererPtr);
    nsecs_t vsync = systemTime(CLOCK_MONOTONIC);
    UiFrameInfoBuilder(proxy->frameInfo())
            .setVsync(vsync, vsync)
            .addFlag(FrameInfoFlags::SurfaceCanvas);
    proxy->syncAndDrawFrame();
}

static void destroy(JNIEnv*, jclass, jlong rendererPtr) {
    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(rendererPtr);
    delete proxy;
}

} // namespace uirenderer

namespace hwui = android::uirenderer;

static const JNINativeMethod gSurfaceMethods[] = {
        {"nativeCreateFromSurfaceTexture", "(Landroid/graphics/SurfaceTexture;)J",
         (void*)nativeCreateFromSurfaceTexture},
        {"nativeRelease", "(J)V", (void*)nativeRelease},
        {"nativeIsValid", "(J)Z", (void*)nativeIsValid},
        {"nativeIsConsumerRunningBehind", "(J)Z", (void*)nativeIsConsumerRunningBehind},
        {"nativeLockCanvas", "(JLandroid/graphics/Canvas;Landroid/graphics/Rect;)J",
         (void*)nativeLockCanvas},
        {"nativeUnlockCanvasAndPost", "(JLandroid/graphics/Canvas;)V",
         (void*)nativeUnlockCanvasAndPost},
        {"nativeAllocateBuffers", "(J)V", (void*)nativeAllocateBuffers},
        {"nativeCreateFromSurfaceControl", "(J)J", (void*)nativeCreateFromSurfaceControl},
        {"nativeGetFromSurfaceControl", "(JJ)J", (void*)nativeGetFromSurfaceControl},
        {"nativeReadFromParcel", "(JLandroid/os/Parcel;)J", (void*)nativeReadFromParcel},
        {"nativeWriteToParcel", "(JLandroid/os/Parcel;)V", (void*)nativeWriteToParcel},
        {"nativeGetWidth", "(J)I", (void*)nativeGetWidth},
        {"nativeGetHeight", "(J)I", (void*)nativeGetHeight},
        {"nativeGetNextFrameNumber", "(J)J", (void*)nativeGetNextFrameNumber},
        {"nativeSetScalingMode", "(JI)I", (void*)nativeSetScalingMode},
        {"nativeForceScopedDisconnect", "(J)I", (void*)nativeForceScopedDisconnect},
        {"nativeAttachAndQueueBuffer", "(JLandroid/graphics/GraphicBuffer;)I",
         (void*)nativeAttachAndQueueBuffer},
        {"nativeSetSharedBufferModeEnabled", "(JZ)I", (void*)nativeSetSharedBufferModeEnabled},
        {"nativeSetAutoRefreshEnabled", "(JZ)I", (void*)nativeSetAutoRefreshEnabled},
        {"nHwuiCreate", "(JJZ)J", (void*)hwui::create},
        {"nHwuiSetSurface", "(JJ)V", (void*)hwui::setSurface},
        {"nHwuiDraw", "(J)V", (void*)hwui::draw},
        {"nHwuiDestroy", "(J)V", (void*)hwui::destroy},
};

int register_android_view_Surface(JNIEnv* env) {
    int err = RegisterMethodsOrDie(env, "android/view/Surface", gSurfaceMethods,
                                   NELEM(gSurfaceMethods));

    jclass clazz = FindClassOrDie(env, "android/view/Surface");
    gSurfaceClassInfo.clazz = MakeGlobalRefOrDie(env, clazz);
    gSurfaceClassInfo.mNativeObject =
            GetFieldIDOrDie(env, gSurfaceClassInfo.clazz, "mNativeObject", "J");
    gSurfaceClassInfo.mLock =
            GetFieldIDOrDie(env, gSurfaceClassInfo.clazz, "mLock", "Ljava/lang/Object;");
    gSurfaceClassInfo.ctor = GetMethodIDOrDie(env, gSurfaceClassInfo.clazz, "<init>", "(J)V");

    clazz = FindClassOrDie(env, "android/graphics/Rect");
    gRectClassInfo.left = GetFieldIDOrDie(env, clazz, "left", "I");
    gRectClassInfo.top = GetFieldIDOrDie(env, clazz, "top", "I");
    gRectClassInfo.right = GetFieldIDOrDie(env, clazz, "right", "I");
    gRectClassInfo.bottom = GetFieldIDOrDie(env, clazz, "bottom", "I");
    return err;
}

} // namespace android

// For Mead's display bridge: the surface an android.view.Surface draws to,
// as an ANativeWindow, without a reference; NULL for none.
extern "C" ANativeWindow* mead_surface_from_java(JNIEnv* env, jobject surface) {
    if (surface == nullptr || !android::android_view_Surface_isInstanceOf(env, surface)) {
        return nullptr;
    }
    return android::android_view_Surface_getNativeWindow(env, surface).get();
}

// For Mead's display bridge: an android.view.Surface for a surface from
// mead_surface_create, holding its own reference.
extern "C" jobject mead_surface_to_java(JNIEnv* env, ANativeWindow* window) {
    return android::android_view_Surface_createFromSurface(
            env, static_cast<android::Surface*>(window));
}
