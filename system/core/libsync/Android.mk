#
# Android 10's libsync for the host: waiting on and merging sync file fences,
# through the kernel's sync_file interface.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libsync
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := sync.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include frameworks/mead/compat/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
# C23 dropped ATOMIC_VAR_INIT, which sync.c uses.
LOCAL_CONLYFLAGS := -std=gnu11
LOCAL_CFLAGS := -Wall -fPIC -include mead/android10_compat.h
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_STATIC_LIBRARY)
