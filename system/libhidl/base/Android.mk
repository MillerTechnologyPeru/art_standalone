#
# Android 10's libhidlbase for the host: the HIDL support types (hidl_handle,
# hidl_vec, hidl_string) that libui's generated graphics types are built on.
# A static library, without TaskRunner: Mead has no HIDL transport.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libhidlbase
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    HidlInternal.cpp \
    HidlSupport.cpp \
    Status.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include frameworks/mead/compat/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wall -fPIC -include mead/android10_compat.h
LOCAL_SHARED_LIBRARIES := libbase libcutils liblog libutils
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_STATIC_LIBRARY)
