/*
 * android.view.SurfaceControl's and SurfaceSession's natives for Mead, in
 * place of Android 10's, which talk to SurfaceFlinger: a SurfaceControl is a
 * window that Mead's display bridge makes (mead_gui_create_window), with the
 * surface drawing to it, or, made with a parent, a layer in that window (a
 * SurfaceView's, mead_gui_create_layer). Transactions position, size, stack,
 * show and hide layers; the rest of what they set (crops, matrices, alpha,
 * colors) isn't shown yet.
 */

#define LOG_TAG "SurfaceControl"

#include "jni.h"
#include <nativehelper/JNIHelp.h>
#include <nativehelper/ScopedUtfChars.h>

#include "core_jni_helpers.h"
#include "android_util_Binder.h"

#include <mead/gui.h>
#include <utils/Log.h>

#include "MeadSurfaceControl.h"

#include <map>

namespace android {

static const char* const kUnsupported = "java/lang/UnsupportedOperationException";

// The owner passed to incStrong and decStrong for Java's references.
static const void* sRefBaseOwner;

// SurfaceControl's flags: made hidden, and the kinds of layer with no
// buffer of their own.
static const jint kHidden = 0x00000004;
static const jint kKindMask = 0x000F0000;
// A transaction's flags: hidden.
static const jint kSurfaceHidden = 0x01;

// A layer, in the window its parent is in; its own surface if it has a
// buffer.
static jlong createLayer(JNIEnv* env, const char* name, jint w, jint h, jint flags,
                         MeadSurfaceControl* parent) {
    sp<MeadSurfaceControl> control = new MeadSurfaceControl();
    control->layer = true;
    control->state.width = static_cast<uint32_t>(w);
    control->state.height = static_cast<uint32_t>(h);
    control->state.shown = (flags & kHidden) == 0;
    control->state.scale_x = 1;
    control->state.scale_y = 1;
    if ((flags & kKindMask) == 0 && parent->surface != nullptr) {
        ANativeWindow* window = mead_gui_create_layer(parent->surface.get(), name,
                                                      static_cast<uint32_t>(w),
                                                      static_cast<uint32_t>(h));
        if (window == nullptr) {
            jniThrowException(env, "android/view/Surface$OutOfResourcesException",
                              "the display bridge made no layer");
            return 0;
        }
        control->surface = static_cast<Surface*>(window);
        // mead_gui_create_layer's reference, now control->surface's.
        control->surface->decStrong(nullptr);
        mead_gui_update_layer(window, &control->state);
    }
    control->incStrong(&sRefBaseOwner);
    return reinterpret_cast<jlong>(control.get());
}

static jlong nativeCreate(JNIEnv* env, jclass, jobject, jstring nameStr, jint w, jint h, jint,
                          jint flags, jlong parentObject, jobject) {
    ScopedUtfChars name(env, nameStr);
    if (parentObject != 0) {
        return createLayer(env, name.c_str(), w, h, flags,
                           reinterpret_cast<MeadSurfaceControl*>(parentObject));
    }
    ANativeWindow* window = mead_gui_create_window(name.c_str(), static_cast<uint32_t>(w),
                                                   static_cast<uint32_t>(h));
    if (window == nullptr) {
        jniThrowException(env, "android/view/Surface$OutOfResourcesException",
                          "the display bridge made no window");
        return 0;
    }
    sp<MeadSurfaceControl> control = new MeadSurfaceControl();
    control->surface = static_cast<Surface*>(window);
    // mead_gui_create_window's reference, now control->surface's.
    control->surface->decStrong(nullptr);
    control->incStrong(&sRefBaseOwner);
    return reinterpret_cast<jlong>(control.get());
}

static jlong nativeReadFromParcel(JNIEnv* env, jclass, jobject) {
    jniThrowException(env, kUnsupported, "Mead's surfaces don't cross processes yet");
    return 0;
}

static void nativeWriteToParcel(JNIEnv* env, jclass, jlong, jobject) {
    jniThrowException(env, kUnsupported, "Mead's surfaces don't cross processes yet");
}

static jlong nativeCopyFromSurfaceControl(JNIEnv*, jclass, jlong nativeObject) {
    auto control = reinterpret_cast<MeadSurfaceControl*>(nativeObject);
    if (control == nullptr) return 0;
    control->incStrong(&sRefBaseOwner);
    return nativeObject;
}

static void nativeRelease(JNIEnv*, jclass, jlong nativeObject) {
    reinterpret_cast<MeadSurfaceControl*>(nativeObject)->decStrong(&sRefBaseOwner);
}

static void nativeDestroy(JNIEnv*, jclass, jlong nativeObject) {
    auto control = reinterpret_cast<MeadSurfaceControl*>(nativeObject);
    if (control->surface != nullptr && !control->destroyed) {
        control->destroyed = true;
        mead_gui_destroy_window(control->surface.get());
    }
    control->decStrong(&sRefBaseOwner);
}

static void nativeDisconnect(JNIEnv*, jclass, jlong) {}

static jobject nativeGetHandle(JNIEnv* env, jclass, jlong nativeObject) {
    return javaObjectForIBinder(env, reinterpret_cast<MeadSurfaceControl*>(nativeObject)->handle);
}

static jboolean nativeGetTransformToDisplayInverse(JNIEnv*, jclass, jlong) {
    return JNI_FALSE;
}

static jboolean nativeGetProtectedContentSupport(JNIEnv*, jclass) {
    return JNI_FALSE;
}

// Transactions: each layer's changes, applied together. Changes to windows
// (the window manager's own) are the window manager's to make, and dropped.
struct MeadLayerChange {
    enum : uint32_t { kPosition = 1, kSize = 2, kZ = 4, kFlags = 8, kMatrix = 16 };
    uint32_t what = 0;
    int32_t x = 0, y = 0;
    uint32_t width = 0, height = 0;
    int32_t z = 0;
    int32_t flags = 0, mask = 0;
    float scaleX = 1, scaleY = 1;

    void merge(const MeadLayerChange& other) {
        if (other.what & kPosition) { x = other.x; y = other.y; }
        if (other.what & kSize) { width = other.width; height = other.height; }
        if (other.what & kZ) z = other.z;
        if (other.what & kMatrix) { scaleX = other.scaleX; scaleY = other.scaleY; }
        if (other.what & kFlags) { flags = (flags & ~other.mask) | (other.flags & other.mask); mask |= other.mask; }
        what |= other.what;
    }
};

struct MeadTransaction {
    std::map<sp<MeadSurfaceControl>, MeadLayerChange> changes;
};

static MeadLayerChange* changeFor(jlong transactionObj, jlong nativeObject) {
    auto transaction = reinterpret_cast<MeadTransaction*>(transactionObj);
    auto control = reinterpret_cast<MeadSurfaceControl*>(nativeObject);
    if (transaction == nullptr || control == nullptr || !control->layer) return nullptr;
    return &transaction->changes[control];
}

static void destroyTransaction(MeadTransaction* transaction) {
    delete transaction;
}

static jlong nativeCreateTransaction(JNIEnv*, jclass) {
    return reinterpret_cast<jlong>(new MeadTransaction());
}

static jlong nativeGetNativeTransactionFinalizer(JNIEnv*, jclass) {
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(&destroyTransaction));
}

static void nativeApplyTransaction(JNIEnv*, jclass, jlong transactionObj, jboolean) {
    auto transaction = reinterpret_cast<MeadTransaction*>(transactionObj);
    for (auto& entry : transaction->changes) {
        const sp<MeadSurfaceControl>& control = entry.first;
        const MeadLayerChange& change = entry.second;
        MeadLayerState& state = control->state;
        if (change.what & MeadLayerChange::kPosition) { state.x = change.x; state.y = change.y; }
        if (change.what & MeadLayerChange::kSize) { state.width = change.width; state.height = change.height; }
        if (change.what & MeadLayerChange::kZ) state.z = change.z;
        if (change.what & MeadLayerChange::kMatrix) {
            state.scale_x = change.scaleX;
            state.scale_y = change.scaleY;
        }
        if ((change.what & MeadLayerChange::kFlags) && (change.mask & kSurfaceHidden)) {
            state.shown = (change.flags & kSurfaceHidden) == 0;
        }
        if (control->surface != nullptr && !control->destroyed) {
            mead_gui_update_layer(control->surface.get(), &state);
        }
    }
    transaction->changes.clear();
}

static void nativeMergeTransaction(JNIEnv*, jclass, jlong transactionObj, jlong otherObj) {
    auto transaction = reinterpret_cast<MeadTransaction*>(transactionObj);
    auto other = reinterpret_cast<MeadTransaction*>(otherObj);
    for (auto& entry : other->changes) transaction->changes[entry.first].merge(entry.second);
    other->changes.clear();
}

static void nativeSetPosition(JNIEnv*, jclass, jlong transactionObj, jlong nativeObject, jfloat x,
                              jfloat y) {
    if (MeadLayerChange* change = changeFor(transactionObj, nativeObject)) {
        change->what |= MeadLayerChange::kPosition;
        change->x = static_cast<int32_t>(x);
        change->y = static_cast<int32_t>(y);
    }
}

// A layer's matrix: its scale, which is all SurfaceView sets; a rotation or
// skew's other terms are dropped.
static void nativeSetMatrix(JNIEnv*, jclass, jlong transactionObj, jlong nativeObject,
                            jfloat dsdx, jfloat, jfloat, jfloat dsdy) {
    if (MeadLayerChange* change = changeFor(transactionObj, nativeObject)) {
        change->what |= MeadLayerChange::kMatrix;
        change->scaleX = dsdx;
        change->scaleY = dsdy;
    }
}

static void nativeSetSize(JNIEnv*, jclass, jlong transactionObj, jlong nativeObject, jint w,
                          jint h) {
    if (MeadLayerChange* change = changeFor(transactionObj, nativeObject)) {
        change->what |= MeadLayerChange::kSize;
        change->width = static_cast<uint32_t>(w);
        change->height = static_cast<uint32_t>(h);
    }
}

static void nativeSetLayer(JNIEnv*, jclass, jlong transactionObj, jlong nativeObject, jint z) {
    if (MeadLayerChange* change = changeFor(transactionObj, nativeObject)) {
        change->what |= MeadLayerChange::kZ;
        change->z = z;
    }
}

// Relative to another layer: Mead orders layers against their window only.
static void nativeSetRelativeLayer(JNIEnv* env, jclass clazz, jlong transactionObj,
                                   jlong nativeObject, jobject, jint z) {
    nativeSetLayer(env, clazz, transactionObj, nativeObject, z);
}

static void nativeSetFlags(JNIEnv*, jclass, jlong transactionObj, jlong nativeObject, jint flags,
                           jint mask) {
    if (MeadLayerChange* change = changeFor(transactionObj, nativeObject)) {
        MeadLayerChange flagChange;
        flagChange.what = MeadLayerChange::kFlags;
        flagChange.flags = flags;
        flagChange.mask = mask;
        change->merge(flagChange);
    }
}

// For the other transaction calls: each takes the transaction and its own
// arguments, which a function doing nothing never reads.
static void transactionNoOp(JNIEnv*, jclass) {}

static const JNINativeMethod sSurfaceControlMethods[] = {
        {"nativeCreate", "(Landroid/view/SurfaceSession;Ljava/lang/String;IIIIJLandroid/os/Parcel;)J",
         (void*)nativeCreate},
        {"nativeReadFromParcel", "(Landroid/os/Parcel;)J", (void*)nativeReadFromParcel},
        {"nativeCopyFromSurfaceControl", "(J)J", (void*)nativeCopyFromSurfaceControl},
        {"nativeWriteToParcel", "(JLandroid/os/Parcel;)V", (void*)nativeWriteToParcel},
        {"nativeRelease", "(J)V", (void*)nativeRelease},
        {"nativeDestroy", "(J)V", (void*)nativeDestroy},
        {"nativeDisconnect", "(J)V", (void*)nativeDisconnect},
        {"nativeGetHandle", "(J)Landroid/os/IBinder;", (void*)nativeGetHandle},
        {"nativeGetTransformToDisplayInverse", "(J)Z", (void*)nativeGetTransformToDisplayInverse},
        {"nativeGetProtectedContentSupport", "()Z", (void*)nativeGetProtectedContentSupport},
        {"nativeCreateTransaction", "()J", (void*)nativeCreateTransaction},
        {"nativeGetNativeTransactionFinalizer", "()J", (void*)nativeGetNativeTransactionFinalizer},
        {"nativeApplyTransaction", "(JZ)V", (void*)nativeApplyTransaction},
        {"nativeMergeTransaction", "(JJ)V", (void*)nativeMergeTransaction},
        {"nativeSetAnimationTransaction", "(J)V", (void*)transactionNoOp},
        {"nativeSetEarlyWakeup", "(J)V", (void*)transactionNoOp},
        {"nativeSetLayer", "(JJI)V", (void*)nativeSetLayer},
        {"nativeSetRelativeLayer", "(JJLandroid/os/IBinder;I)V", (void*)nativeSetRelativeLayer},
        {"nativeSetPosition", "(JJFF)V", (void*)nativeSetPosition},
        {"nativeSetGeometryAppliesWithResize", "(JJ)V", (void*)transactionNoOp},
        {"nativeSetSize", "(JJII)V", (void*)nativeSetSize},
        {"nativeSetTransparentRegionHint", "(JJLandroid/graphics/Region;)V", (void*)transactionNoOp},
        {"nativeSetAlpha", "(JJF)V", (void*)transactionNoOp},
        {"nativeSetColor", "(JJ[F)V", (void*)transactionNoOp},
        {"nativeSetMatrix", "(JJFFFF)V", (void*)nativeSetMatrix},
        {"nativeSetColorTransform", "(JJ[F[F)V", (void*)transactionNoOp},
        {"nativeSetColorSpaceAgnostic", "(JJZ)V", (void*)transactionNoOp},
        {"nativeSetFlags", "(JJII)V", (void*)nativeSetFlags},
        {"nativeSetWindowCrop", "(JJIIII)V", (void*)transactionNoOp},
        {"nativeSetCornerRadius", "(JJF)V", (void*)transactionNoOp},
        {"nativeSetLayerStack", "(JJI)V", (void*)transactionNoOp},
        {"nativeDeferTransactionUntil", "(JJLandroid/os/IBinder;J)V", (void*)transactionNoOp},
        {"nativeDeferTransactionUntilSurface", "(JJJJ)V", (void*)transactionNoOp},
        {"nativeReparentChildren", "(JJLandroid/os/IBinder;)V", (void*)transactionNoOp},
        {"nativeReparent", "(JJJ)V", (void*)transactionNoOp},
        {"nativeSeverChildren", "(JJ)V", (void*)transactionNoOp},
        {"nativeSetOverrideScalingMode", "(JJI)V", (void*)transactionNoOp},
        {"nativeSetInputWindowInfo", "(JJLandroid/view/InputWindowHandle;)V", (void*)transactionNoOp},
        {"nativeSetMetadata", "(JJILandroid/os/Parcel;)V", (void*)transactionNoOp},
        {"nativeSetGeometry", "(JJLandroid/graphics/Rect;Landroid/graphics/Rect;J)V",
         (void*)transactionNoOp},
        {"nativeSyncInputWindows", "(J)V", (void*)transactionNoOp},
};

// SurfaceSession: the connection to SurfaceFlinger, which there isn't.
static jlong sessionCreate(JNIEnv*, jclass) {
    return 1;
}

static void sessionDestroy(JNIEnv*, jclass, jlong) {}
static void sessionKill(JNIEnv*, jclass, jlong) {}

static const JNINativeMethod sSurfaceSessionMethods[] = {
        {"nativeCreate", "()J", (void*)sessionCreate},
        {"nativeDestroy", "(J)V", (void*)sessionDestroy},
        {"nativeKill", "(J)V", (void*)sessionKill},
};

int register_android_view_SurfaceControl(JNIEnv* env) {
    return RegisterMethodsOrDie(env, "android/view/SurfaceControl", sSurfaceControlMethods,
                                NELEM(sSurfaceControlMethods));
}

int register_android_view_SurfaceSession(JNIEnv* env) {
    return RegisterMethodsOrDie(env, "android/view/SurfaceSession", sSurfaceSessionMethods,
                                NELEM(sSurfaceSessionMethods));
}

} // namespace android

// For Mead's window manager: a SurfaceControl's native object for a window
// the display bridge made, taking the surface's reference; for
// SurfaceControl's mNativeObject.
extern "C" jlong mead_surface_control_create(ANativeWindow* window) {
    android::sp<android::MeadSurfaceControl> control = new android::MeadSurfaceControl();
    control->surface = static_cast<android::Surface*>(window);
    control->surface->decStrong(nullptr);
    control->incStrong(&android::sRefBaseOwner);
    return reinterpret_cast<jlong>(control.get());
}
