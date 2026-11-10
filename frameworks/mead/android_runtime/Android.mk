#
# libandroid_runtime for the host: Android 10's framework JNI
# (frameworks/base/core/jni), as much of it as runs here so far (now with
# the graphics JNI over hwui, without Movie, SurfaceTexture and PDF), registered
# from JNI_OnLoad by android_runtime_mead.cpp instead of by app_process.
#

LOCAL_PATH := $(call my-dir)

android_runtime_jni := frameworks/base/core/jni

include $(CLEAR_VARS)
LOCAL_MODULE := libandroid_runtime
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    android_runtime_mead.cpp \
    ../../base/core/jni/android_util_Log.cpp \
    ../../base/core/jni/android_os_SystemClock.cpp \
    ../../base/core/jni/android_os_SystemProperties.cpp \
    ../../base/core/jni/android_os_Trace.cpp \
    ../../base/core/jni/android_os_Parcel.cpp \
    ../../base/core/jni/android_util_Binder.cpp \
    ../../base/core/jni/android_util_AssetManager.cpp \
    ../../base/core/jni/android_content_res_ApkAssets.cpp \
    ../../base/core/jni/android_util_StringBlock.cpp \
    ../../base/core/jni/android_util_XmlBlock.cpp \
    ../../base/core/jni/android_hardware_HardwareBuffer.cpp \
    ../../base/core/jni/android_nio_utils.cpp \
    ../../base/core/jni/android_graphics_Canvas.cpp \
    ../../base/core/jni/android_graphics_ColorSpace.cpp \
    ../../base/core/jni/android_graphics_Picture.cpp \
    ../../base/core/jni/android_graphics_drawable_AnimatedVectorDrawable.cpp \
    ../../base/core/jni/android_graphics_drawable_VectorDrawable.cpp \
    ../../base/core/jni/android/graphics/AnimatedImageDrawable.cpp \
    ../../base/core/jni/android/graphics/Bitmap.cpp \
    ../../base/core/jni/android/graphics/BitmapFactory.cpp \
    ../../base/core/jni/android/graphics/BitmapRegionDecoder.cpp \
    ../../base/core/jni/android/graphics/ByteBufferStreamAdaptor.cpp \
    ../../base/core/jni/android/graphics/Camera.cpp \
    ../../base/core/jni/android/graphics/CanvasProperty.cpp \
    ../../base/core/jni/android/graphics/ColorFilter.cpp \
    ../../base/core/jni/android/graphics/CreateJavaOutputStreamAdaptor.cpp \
    ../../base/core/jni/android/graphics/FontFamily.cpp \
    ../../base/core/jni/android/graphics/FontUtils.cpp \
    ../../base/core/jni/android/graphics/GraphicBuffer.cpp \
    ../../base/core/jni/android/graphics/Graphics.cpp \
    ../../base/core/jni/android/graphics/ImageDecoder.cpp \
    ../../base/core/jni/android/graphics/Interpolator.cpp \
    ../../base/core/jni/android/graphics/MaskFilter.cpp \
    ../../base/core/jni/android/graphics/Matrix.cpp \
    ../../base/core/jni/android/graphics/NinePatch.cpp \
    ../../base/core/jni/android/graphics/NinePatchPeeker.cpp \
    ../../base/core/jni/android/graphics/Paint.cpp \
    ../../base/core/jni/android/graphics/PaintFilter.cpp \
    ../../base/core/jni/android/graphics/Path.cpp \
    ../../base/core/jni/android/graphics/PathEffect.cpp \
    ../../base/core/jni/android/graphics/PathMeasure.cpp \
    ../../base/core/jni/android/graphics/Picture.cpp \
    ../../base/core/jni/android/graphics/Region.cpp \
    ../../base/core/jni/android/graphics/Shader.cpp \
    ../../base/core/jni/android/graphics/Typeface.cpp \
    ../../base/core/jni/android/graphics/Utils.cpp \
    ../../base/core/jni/android/graphics/YuvToJpegEncoder.cpp \
    ../../base/core/jni/android/graphics/fonts/Font.cpp \
    ../../base/core/jni/android/graphics/fonts/FontFamily.cpp \
    ../../base/core/jni/android/graphics/text/LineBreaker.cpp \
    ../../base/core/jni/android/graphics/text/MeasuredText.cpp
LOCAL_C_INCLUDES := \
    $(android_runtime_jni) \
    $(android_runtime_jni)/include \
    $(android_runtime_jni)/android/graphics \
    external/libjpeg-turbo \
    hardware/libhardware/include \
    system/core/libgrallocusage/include \
    frameworks/native/include \
    frameworks/mead/compat/include
# Android 10's code calls String8::string() and String16::string(), which the
# fork's newer libutils keeps behind these.
LOCAL_CFLAGS := -Wall -Wextra -Wno-unused-parameter \
    -DENABLE_STRING8_OBSOLETE_METHODS -DENABLE_STRING16_OBSOLETE_METHODS \
    -include mead/android10_compat.h
LOCAL_CPPFLAGS := -std=gnu++17
LOCAL_SHARED_LIBRARIES := \
    libandroidfw \
    libbase \
    libbinder \
    libcutils \
    libgui \
    libhwui \
    liblog \
    libnativehelper \
    libnativewindow \
    libui \
    libutils \
    libziparchive
LOCAL_LDLIBS := -lpthread -ldl -lz
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
