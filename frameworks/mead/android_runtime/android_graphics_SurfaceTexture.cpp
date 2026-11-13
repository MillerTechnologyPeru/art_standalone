/*
 * android.graphics.SurfaceTexture's natives for Mead, in place of Android
 * 10's, which consume a BufferQueue through GLConsumer. Mead's libgui has no
 * buffer queues: a SurfaceTexture's producer is a Mead surface whose queued
 * frames (CPU pixels: a decoder's, a canvas's) come here, and
 * updateTexImage puts the newest into the app's GL_TEXTURE_EXTERNAL_OES
 * texture, on the calling thread's GL context, through an EGLImage of a 2D
 * texture holding it (EGL_KHR_gl_texture_2D_image).
 */

#define LOG_TAG "SurfaceTexture"

#include "jni.h"
#include <nativehelper/JNIHelp.h>

#include "core_jni_helpers.h"
#include <android_runtime/AndroidRuntime.h>

#include <gui/Surface.h>
#include <mead/gui.h>
#include <system/graphics.h>
#include <utils/Log.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <mutex>
#include <string.h>
#include <time.h>
#include <vector>

#include "MeadSurfaceTexture.h"

namespace android {

static const char* const kSurfaceTextureClass = "android/graphics/SurfaceTexture";

static struct {
    jfieldID surfaceTexture;
    jmethodID postEvent;
} gFields;

/* A SurfaceTexture: its producer surface, the newest frame queued to it, and
 * what the app's texture shows. */
struct MeadSurfaceTexture {
    std::mutex lock;
    GLuint texName = 0;
    uint32_t defaultWidth = 1;
    uint32_t defaultHeight = 1;
    sp<Surface> producer;
    jobject weakSelf = nullptr;  // a global ref to Java's WeakReference
    bool released = false;

    // The newest frame, RGBA rows of width pixels.
    std::vector<uint8_t> pending;
    uint32_t pendingWidth = 0, pendingHeight = 0;
    int64_t pendingTimestamp = 0;
    bool hasPending = false;

    // What the texture shows; the 2D texture and image behind it belong to
    // the context updateTexImage last ran on.
    int64_t timestamp = 0;
    GLuint storage = 0;
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    EGLDisplay imageDisplay = EGL_NO_DISPLAY;
    uint32_t storageWidth = 0, storageHeight = 0;
};

static MeadSurfaceTexture* get(JNIEnv* env, jobject thiz) {
    return reinterpret_cast<MeadSurfaceTexture*>(env->GetLongField(thiz, gFields.surfaceTexture));
}

static int64_t now() {
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return int64_t(time.tv_sec) * 1000000000LL + time.tv_nsec;
}

/* Java's frame-available listener, on whichever thread queued the frame. */
static void postFrameAvailable(jobject weakSelf) {
    JavaVM* vm = AndroidRuntime::getJavaVM();
    JNIEnv* env = nullptr;
    bool attached = false;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        JavaVMAttachArgs args = {JNI_VERSION_1_6, "SurfaceTexture", nullptr};
        if (vm->AttachCurrentThread(&env, &args) != JNI_OK) return;
        attached = true;
    }
    jclass clazz = env->FindClass(kSurfaceTextureClass);
    if (clazz != nullptr) {
        env->CallStaticVoidMethod(clazz, gFields.postEvent, weakSelf);
        env->DeleteLocalRef(clazz);
    }
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    if (attached) vm->DetachCurrentThread();
}

/* A frame queued to the producer: kept as RGBA for updateTexImage. */
static void present(void* context, const void* pixels, uint32_t width, uint32_t height,
                    uint32_t stride, int32_t format) {
    MeadSurfaceTexture* texture = static_cast<MeadSurfaceTexture*>(context);
    jobject weakSelf;
    {
        std::lock_guard<std::mutex> guard(texture->lock);
        if (texture->released) return;
        texture->pending.resize(size_t(width) * height * 4);
        const uint8_t* source = static_cast<const uint8_t*>(pixels);
        if (format == HAL_PIXEL_FORMAT_RGB_565) {
            for (uint32_t y = 0; y < height; y++) {
                const uint16_t* row = reinterpret_cast<const uint16_t*>(source) + size_t(y) * stride;
                uint8_t* out = &texture->pending[size_t(y) * width * 4];
                for (uint32_t x = 0; x < width; x++) {
                    uint16_t p = row[x];
                    out[x * 4 + 0] = uint8_t(((p >> 11) & 0x1f) * 255 / 31);
                    out[x * 4 + 1] = uint8_t(((p >> 5) & 0x3f) * 255 / 63);
                    out[x * 4 + 2] = uint8_t((p & 0x1f) * 255 / 31);
                    out[x * 4 + 3] = 255;
                }
            }
        } else {
            // RGBA_8888, RGBX_8888 (alpha set) and BGRA_8888 (swapped).
            for (uint32_t y = 0; y < height; y++) {
                const uint8_t* row = source + size_t(y) * stride * 4;
                uint8_t* out = &texture->pending[size_t(y) * width * 4];
                if (format == HAL_PIXEL_FORMAT_BGRA_8888) {
                    for (uint32_t x = 0; x < width; x++) {
                        out[x * 4 + 0] = row[x * 4 + 2];
                        out[x * 4 + 1] = row[x * 4 + 1];
                        out[x * 4 + 2] = row[x * 4 + 0];
                        out[x * 4 + 3] = row[x * 4 + 3];
                    }
                } else {
                    memcpy(out, row, size_t(width) * 4);
                    if (format == HAL_PIXEL_FORMAT_RGBX_8888) {
                        for (uint32_t x = 0; x < width; x++) out[x * 4 + 3] = 255;
                    }
                }
            }
        }
        texture->pendingWidth = width;
        texture->pendingHeight = height;
        texture->pendingTimestamp = now();
        texture->hasPending = true;
        weakSelf = texture->weakSelf;
    }
    if (weakSelf != nullptr) postFrameAvailable(weakSelf);
}

sp<Surface> mead_SurfaceTexture_getProducer(JNIEnv* env, jobject thiz) {
    MeadSurfaceTexture* texture = get(env, thiz);
    if (texture == nullptr) return nullptr;
    std::lock_guard<std::mutex> guard(texture->lock);
    return texture->producer;
}

static void SurfaceTexture_init(JNIEnv* env, jobject thiz, jboolean isDetached, jint texName,
                                jboolean singleBufferMode, jobject weakThiz) {
    MeadSurfaceTexture* texture = new MeadSurfaceTexture();
    texture->texName = isDetached ? 0 : GLuint(texName);
    texture->weakSelf = env->NewGlobalRef(weakThiz);
    MeadSurfaceCallbacks callbacks = {};
    callbacks.context = texture;
    callbacks.present = present;
    ANativeWindow* window = mead_surface_create(texture->defaultWidth, texture->defaultHeight,
                                                HAL_PIXEL_FORMAT_RGBA_8888, &callbacks);
    if (window == nullptr) {
        env->DeleteGlobalRef(texture->weakSelf);
        delete texture;
        jniThrowException(env, "android/view/Surface$OutOfResourcesException",
                          "Unable to create a surface for the SurfaceTexture");
        return;
    }
    texture->producer = static_cast<Surface*>(window);
    ANativeWindow_release(window);  // producer holds it now
    env->SetLongField(thiz, gFields.surfaceTexture, reinterpret_cast<jlong>(texture));
}

/* Drops the 2D texture and image, on a thread whose context owns them (or
 * leaks them, when it isn't current: their context's teardown frees them). */
static void dropStorage(MeadSurfaceTexture* texture) {
    if (texture->image != EGL_NO_IMAGE_KHR) {
        auto destroy = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
                eglGetProcAddress("eglDestroyImageKHR"));
        if (destroy != nullptr) destroy(texture->imageDisplay, texture->image);
        texture->image = EGL_NO_IMAGE_KHR;
    }
    if (texture->storage != 0 && eglGetCurrentContext() != EGL_NO_CONTEXT) {
        glDeleteTextures(1, &texture->storage);
    }
    texture->storage = 0;
    texture->storageWidth = texture->storageHeight = 0;
}

static void SurfaceTexture_release(JNIEnv* env, jobject thiz) {
    MeadSurfaceTexture* texture = get(env, thiz);
    if (texture == nullptr) return;
    std::lock_guard<std::mutex> guard(texture->lock);
    texture->released = true;
    texture->producer.clear();
}

static void SurfaceTexture_finalize(JNIEnv* env, jobject thiz) {
    MeadSurfaceTexture* texture = get(env, thiz);
    if (texture == nullptr) return;
    jobject weakSelf;
    {
        std::lock_guard<std::mutex> guard(texture->lock);
        texture->released = true;
        texture->producer.clear();
        std::vector<uint8_t>().swap(texture->pending);
        weakSelf = texture->weakSelf;
        texture->weakSelf = nullptr;
    }
    if (weakSelf != nullptr) env->DeleteGlobalRef(weakSelf);
    env->SetLongField(thiz, gFields.surfaceTexture, 0);
    // The struct itself stays: a producer someone else still holds (a
    // decoder's) calls present with it, which then does nothing. The GL
    // objects belong to a context that may be gone.
}

static void SurfaceTexture_setDefaultBufferSize(JNIEnv* env, jobject thiz, jint width, jint height) {
    MeadSurfaceTexture* texture = get(env, thiz);
    if (texture == nullptr) return;
    std::lock_guard<std::mutex> guard(texture->lock);
    texture->defaultWidth = width > 0 ? width : 1;
    texture->defaultHeight = height > 0 ? height : 1;
    if (texture->producer != nullptr) {
        native_window_set_buffers_dimensions(texture->producer.get(), int(texture->defaultWidth),
                                             int(texture->defaultHeight));
    }
}

static void SurfaceTexture_updateTexImage(JNIEnv* env, jobject thiz) {
    MeadSurfaceTexture* texture = get(env, thiz);
    if (texture == nullptr) return;
    std::lock_guard<std::mutex> guard(texture->lock);
    if (!texture->hasPending) return;
    texture->hasPending = false;
    EGLDisplay display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY || eglGetCurrentContext() == EGL_NO_CONTEXT) {
        jniThrowException(env, "java/lang/IllegalStateException",
                          "updateTexImage needs a current GL context");
        return;
    }
    auto createImage = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    auto targetTexture = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
            eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (createImage == nullptr || targetTexture == nullptr) {
        jniThrowException(env, "java/lang/RuntimeException",
                          "EGL has no images to put frames in an external texture");
        return;
    }
    GLint bound2D = 0, boundExternal = 0, alignment = 4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound2D);
    glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &boundExternal);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    bool resized = texture->pendingWidth != texture->storageWidth ||
                   texture->pendingHeight != texture->storageHeight ||
                   texture->imageDisplay != display;
    if (resized) {
        dropStorage(texture);
        glGenTextures(1, &texture->storage);
        glBindTexture(GL_TEXTURE_2D, texture->storage);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->pendingWidth, texture->pendingHeight, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, texture->pending.data());
        const EGLint attributes[] = {EGL_GL_TEXTURE_LEVEL_KHR, 0, EGL_IMAGE_PRESERVED_KHR,
                                     EGL_TRUE, EGL_NONE};
        texture->image = createImage(display, eglGetCurrentContext(), EGL_GL_TEXTURE_2D_KHR,
                                     reinterpret_cast<EGLClientBuffer>(uintptr_t(texture->storage)),
                                     attributes);
        texture->imageDisplay = display;
        texture->storageWidth = texture->pendingWidth;
        texture->storageHeight = texture->pendingHeight;
        if (texture->image == EGL_NO_IMAGE_KHR) {
            ALOGE("eglCreateImageKHR failed: 0x%x", eglGetError());
        } else {
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture->texName);
            targetTexture(GL_TEXTURE_EXTERNAL_OES, texture->image);
        }
    } else {
        // The image shares the 2D texture's storage, so the external texture
        // sees the new pixels.
        glBindTexture(GL_TEXTURE_2D, texture->storage);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->pendingWidth, texture->pendingHeight,
                        GL_RGBA, GL_UNSIGNED_BYTE, texture->pending.data());
    }
    glBindTexture(GL_TEXTURE_2D, GLuint(bound2D));
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, GLuint(boundExternal));
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    texture->timestamp = texture->pendingTimestamp;
}

static void SurfaceTexture_releaseTexImage(JNIEnv*, jobject) {}

/* As GLConsumer's for a buffer without a transform: flipped vertically, as
 * the frame's first row is its top and a texture's first row its bottom. */
static void SurfaceTexture_getTransformMatrix(JNIEnv* env, jobject, jfloatArray matrix) {
    static const jfloat flip[16] = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1};
    env->SetFloatArrayRegion(matrix, 0, 16, flip);
}

static jlong SurfaceTexture_getTimestamp(JNIEnv* env, jobject thiz) {
    MeadSurfaceTexture* texture = get(env, thiz);
    if (texture == nullptr) return 0;
    std::lock_guard<std::mutex> guard(texture->lock);
    return texture->timestamp;
}

static jint SurfaceTexture_detachFromGLContext(JNIEnv* env, jobject thiz) {
    MeadSurfaceTexture* texture = get(env, thiz);
    if (texture == nullptr) return -EINVAL;
    std::lock_guard<std::mutex> guard(texture->lock);
    dropStorage(texture);
    texture->texName = 0;
    return 0;
}

static jint SurfaceTexture_attachToGLContext(JNIEnv* env, jobject thiz, jint texName) {
    MeadSurfaceTexture* texture = get(env, thiz);
    if (texture == nullptr) return -EINVAL;
    std::lock_guard<std::mutex> guard(texture->lock);
    texture->texName = GLuint(texName);
    texture->storageWidth = texture->storageHeight = 0;  // made again on the new context
    texture->image = EGL_NO_IMAGE_KHR;
    texture->storage = 0;
    return 0;
}

static jboolean SurfaceTexture_isReleased(JNIEnv* env, jobject thiz) {
    MeadSurfaceTexture* texture = get(env, thiz);
    if (texture == nullptr) return JNI_TRUE;
    std::lock_guard<std::mutex> guard(texture->lock);
    return texture->released ? JNI_TRUE : JNI_FALSE;
}

static const JNINativeMethod gSurfaceTextureMethods[] = {
        {"nativeInit", "(ZIZLjava/lang/ref/WeakReference;)V", (void*)SurfaceTexture_init},
        {"nativeFinalize", "()V", (void*)SurfaceTexture_finalize},
        {"nativeSetDefaultBufferSize", "(II)V", (void*)SurfaceTexture_setDefaultBufferSize},
        {"nativeUpdateTexImage", "()V", (void*)SurfaceTexture_updateTexImage},
        {"nativeReleaseTexImage", "()V", (void*)SurfaceTexture_releaseTexImage},
        {"nativeDetachFromGLContext", "()I", (void*)SurfaceTexture_detachFromGLContext},
        {"nativeAttachToGLContext", "(I)I", (void*)SurfaceTexture_attachToGLContext},
        {"nativeGetTransformMatrix", "([F)V", (void*)SurfaceTexture_getTransformMatrix},
        {"nativeGetTimestamp", "()J", (void*)SurfaceTexture_getTimestamp},
        {"nativeRelease", "()V", (void*)SurfaceTexture_release},
        {"nativeIsReleased", "()Z", (void*)SurfaceTexture_isReleased},
};

int register_android_graphics_SurfaceTexture(JNIEnv* env) {
    jclass clazz = FindClassOrDie(env, kSurfaceTextureClass);
    gFields.surfaceTexture = GetFieldIDOrDie(env, clazz, "mSurfaceTexture", "J");
    gFields.postEvent = GetStaticMethodIDOrDie(env, clazz, "postEventFromNative",
                                               "(Ljava/lang/ref/WeakReference;)V");
    return RegisterMethodsOrDie(env, kSurfaceTextureClass, gSurfaceTextureMethods,
                                NELEM(gSurfaceTextureMethods));
}

}  // namespace android
