/*
 * android.view.SurfaceControl's and SurfaceSession's natives for Mead, in
 * place of Android 10's, which talk to SurfaceFlinger: a SurfaceControl is a
 * window that Mead's display bridge makes (mead_gui_create_window), with the
 * surface drawing to it. There is no composition: transactions, which
 * position, size and stack layers, are accepted and do nothing.
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

namespace android {

static const char* const kUnsupported = "java/lang/UnsupportedOperationException";

// The owner passed to incStrong and decStrong for Java's references.
static const void* sRefBaseOwner;

static jlong nativeCreate(JNIEnv* env, jclass, jobject, jstring nameStr, jint w, jint h, jint,
                          jint, jlong, jobject) {
    ScopedUtfChars name(env, nameStr);
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
    if (control->surface != nullptr) {
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

// Transactions: accepted, and nothing done.
struct MeadTransaction {};

static void destroyTransaction(MeadTransaction* transaction) {
    delete transaction;
}

static jlong nativeCreateTransaction(JNIEnv*, jclass) {
    return reinterpret_cast<jlong>(new MeadTransaction());
}

static jlong nativeGetNativeTransactionFinalizer(JNIEnv*, jclass) {
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(&destroyTransaction));
}

// For every transaction call: each takes the transaction and its own
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
        {"nativeApplyTransaction", "(JZ)V", (void*)transactionNoOp},
        {"nativeMergeTransaction", "(JJ)V", (void*)transactionNoOp},
        {"nativeSetAnimationTransaction", "(J)V", (void*)transactionNoOp},
        {"nativeSetEarlyWakeup", "(J)V", (void*)transactionNoOp},
        {"nativeSetLayer", "(JJI)V", (void*)transactionNoOp},
        {"nativeSetRelativeLayer", "(JJLandroid/os/IBinder;I)V", (void*)transactionNoOp},
        {"nativeSetPosition", "(JJFF)V", (void*)transactionNoOp},
        {"nativeSetGeometryAppliesWithResize", "(JJ)V", (void*)transactionNoOp},
        {"nativeSetSize", "(JJII)V", (void*)transactionNoOp},
        {"nativeSetTransparentRegionHint", "(JJLandroid/graphics/Region;)V", (void*)transactionNoOp},
        {"nativeSetAlpha", "(JJF)V", (void*)transactionNoOp},
        {"nativeSetColor", "(JJ[F)V", (void*)transactionNoOp},
        {"nativeSetMatrix", "(JJFFFF)V", (void*)transactionNoOp},
        {"nativeSetColorTransform", "(JJ[F[F)V", (void*)transactionNoOp},
        {"nativeSetColorSpaceAgnostic", "(JJZ)V", (void*)transactionNoOp},
        {"nativeSetFlags", "(JJII)V", (void*)transactionNoOp},
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
