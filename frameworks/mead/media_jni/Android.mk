#
# libmedia_jni for the host: android.media's JNI library, which MediaPlayer
# and MediaExtractor load in their static initializers. MediaPlayer and
# MediaExtractor run on the host's FFmpeg (media_jni.cpp); the rest of
# android.media's natives are Mead's stubs.
#

LOCAL_PATH := $(call my-dir)

android_runtime_jni := frameworks/base/core/jni

include $(CLEAR_VARS)
LOCAL_MODULE := libmedia_jni
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := media_jni.cpp
LOCAL_C_INCLUDES := \
    $(android_runtime_jni) \
    $(android_runtime_jni)/include \
    frameworks/native/include \
    frameworks/native/libs/arect/include \
    frameworks/native/libs/nativebase/include \
    frameworks/mead/gui/include \
    system/core/libsystem/include \
    frameworks/mead/compat/include
LOCAL_CFLAGS := -Wall -Wextra -Wno-unused-parameter \
    -DENABLE_STRING8_OBSOLETE_METHODS -DENABLE_STRING16_OBSOLETE_METHODS \
    -include mead/android10_compat.h
LOCAL_CPPFLAGS := -std=gnu++17
LOCAL_SHARED_LIBRARIES := \
    libandroid_runtime \
    libbase \
    libbinder \
    libcutils \
    libgui \
    libui \
    liblog \
    libmead_android10_compat \
    libnativehelper \
    libnativewindow \
    libutils
LOCAL_LDLIBS := -lpthread -lavformat -lavcodec -lavutil -lswscale -lswresample
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
