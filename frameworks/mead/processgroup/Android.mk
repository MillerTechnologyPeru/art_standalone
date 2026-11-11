#
# Mead's libprocessgroup: Android's process and thread cgroup policies,
# recorded and not applied (see processgroup.cpp), behind Android 10's
# headers (system/core/libprocessgroup/include).
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libprocessgroup
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := processgroup.cpp
LOCAL_C_INCLUDES := system/core/libprocessgroup/include
LOCAL_EXPORT_C_INCLUDE_DIRS := system/core/libprocessgroup/include
LOCAL_CFLAGS := -Wall -Wextra -fPIC
LOCAL_CPPFLAGS := -std=gnu++17
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_STATIC_LIBRARY)
