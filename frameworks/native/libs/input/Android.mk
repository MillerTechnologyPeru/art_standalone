#
# Android 10's libinput for the host: input events, key maps, velocity
# tracking and the input channel transport (InputChannel, InputPublisher,
# InputConsumer), which Android builds only for the device. Without the
# input flinger's binder interfaces, and with Android 10's libutils PropertyMap,
# which this tree's newer libutils no longer has.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libinput
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    Input.cpp \
    InputDevice.cpp \
    InputTransport.cpp \
    Keyboard.cpp \
    KeyCharacterMap.cpp \
    KeyLayoutMap.cpp \
    PropertyMap.cpp \
    TouchVideoFrame.cpp \
    VelocityControl.cpp \
    VelocityTracker.cpp \
    VirtualKeyMap.cpp
LOCAL_C_INCLUDES := \
    frameworks/native/include \
    frameworks/mead/compat/include
LOCAL_CFLAGS := -Wall -fPIC \
    -DENABLE_STRING8_OBSOLETE_METHODS -DENABLE_STRING16_OBSOLETE_METHODS \
    -include mead/android10_compat.h
LOCAL_CPPFLAGS := -std=gnu++17
LOCAL_SHARED_LIBRARIES := libbase libbinder libcutils liblog libui libutils
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
