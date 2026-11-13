#
# libmedia_jni for the host: android.media's JNI library, which MediaPlayer
# and MediaExtractor load in their static initializers. Mead answers their
# natives itself for now (doing nothing: no video or media playback), so the
# library only has to load.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libmedia_jni
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := media_jni.cpp
LOCAL_CFLAGS := -Wall -Wextra
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
