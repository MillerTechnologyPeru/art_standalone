#
# Android 10's libnativewindow for the host: the NDK's ANativeWindow and
# AHardwareBuffer functions, over libui's GraphicBuffer.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libnativewindow
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    AHardwareBuffer.cpp \
    ANativeWindow.cpp
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/include-private \
    frameworks/native/libs/ui/include \
    frameworks/native/libs/nativebase/include \
    frameworks/native/libs/arect/include \
    frameworks/native/libs/math/include \
    hardware/libhardware/include \
    system/core/libsystem/include \
    system/libhidl/base/include \
    system/core/libgrallocusage/include \
    frameworks/mead/hidl/include \
    frameworks/mead/compat/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include $(LOCAL_PATH)/include-private
LOCAL_CFLAGS := -Wall -fPIC -Wno-enum-compare -Wno-unused-function \
    -DENABLE_STRING8_OBSOLETE_METHODS -DENABLE_STRING16_OBSOLETE_METHODS \
    -include mead/android10_compat.h
LOCAL_CPPFLAGS := -std=gnu++17
LOCAL_SHARED_LIBRARIES := libbase libcutils liblog libutils libui
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
