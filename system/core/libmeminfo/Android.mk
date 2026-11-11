#
# Android 10's libmeminfo for the host: processes' and the system's memory,
# from /proc. A static library.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libmeminfo
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    pageacct.cpp \
    procmeminfo.cpp \
    sysmeminfo.cpp
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    system/core/libprocinfo/include \
    frameworks/mead/compat/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wall -fPIC -include mead/android10_compat.h
LOCAL_CPPFLAGS := -std=gnu++17
LOCAL_SHARED_LIBRARIES := libbase liblog
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_STATIC_LIBRARY)
