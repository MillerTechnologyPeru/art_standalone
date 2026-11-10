# libdl_bio, for building ART without bionic_translation installed.
# Built only when the system has no libdl_bio of its own.

LOCAL_PATH := $(call my-dir)

include $(LOCAL_PATH)/system_dl_bio.mk

ifeq ($(ART_SYSTEM_DL_BIO),)
include $(CLEAR_VARS)
LOCAL_MODULE := libdl_bio
LOCAL_SRC_FILES := dl_bio.c
LOCAL_CFLAGS += -Werror -Wall
LOCAL_LDLIBS := -ldl
include $(BUILD_HOST_SHARED_LIBRARY)
endif
