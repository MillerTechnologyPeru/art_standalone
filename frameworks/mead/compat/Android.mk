#
# What Android 10's framework modules (libbinder, libui, libandroid_runtime)
# expect of bionic, liblog and libcutils that this tree's don't provide: a
# header they're compiled with (-include mead/android10_compat.h), a stand-in
# for bionic's <sys/system_properties.h>, and libcutils' property functions,
# in a shared library of their own, over libbase's system properties.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libmead_android10_compat
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := properties.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wall -Wextra -fPIC
LOCAL_SHARED_LIBRARIES := libbase
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
