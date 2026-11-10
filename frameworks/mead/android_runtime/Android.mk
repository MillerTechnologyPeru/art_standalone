#
# libandroid_runtime for the host: Android 10's framework JNI
# (frameworks/base/core/jni), as much of it as runs here so far, registered
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
    ../../base/core/jni/android_util_XmlBlock.cpp
LOCAL_C_INCLUDES := \
    $(android_runtime_jni) \
    $(android_runtime_jni)/include \
    frameworks/native/include \
    frameworks/mead/compat/include
# Android 10's code calls String8::string() and String16::string(), which the
# fork's newer libutils keeps behind these.
LOCAL_CFLAGS := -Wall -Wextra -Wno-unused-parameter \
    -DENABLE_STRING8_OBSOLETE_METHODS -DENABLE_STRING16_OBSOLETE_METHODS \
    -include mead/android10_compat.h
LOCAL_SHARED_LIBRARIES := \
    libandroidfw \
    libbase \
    libbinder \
    libcutils \
    liblog \
    libnativehelper \
    libutils \
    libziparchive
LOCAL_LDLIBS := -lpthread -ldl
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
