#
# Android 10's libgrallocusage for the host: converting gralloc 1 usage bits
# to gralloc 0's.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libgrallocusage
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := GrallocUsageConversion.cpp
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    hardware/libhardware/include \
    system/core/libsystem/include \
    frameworks/native/libs/nativewindow/include \
    frameworks/mead/compat/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wall -fPIC \
    -include mead/android10_compat.h
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_STATIC_LIBRARY)
