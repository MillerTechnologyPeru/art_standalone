#
# Mead's libgui: the part of Android's libgui the framework's graphics use,
# over Mead's display bridge instead of SurfaceFlinger (see mead/gui.h).
# SyncFeatures is Android 10's own.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libgui
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    DisplayEventReceiver.cpp \
    Surface.cpp \
    SurfaceComposerClient.cpp \
    ../../native/libs/gui/SyncFeatures.cpp
libgui_includes := \
    frameworks/mead/gui/include \
    frameworks/native/libs/gui/include
LOCAL_C_INCLUDES := \
    $(libgui_includes) \
    frameworks/native/libs/ui/include \
    frameworks/native/libs/nativewindow/include \
    frameworks/native/libs/nativebase/include \
    frameworks/native/libs/arect/include \
    frameworks/native/libs/math/include \
    frameworks/native/libs/binder/include \
    hardware/libhardware/include \
    system/core/libsystem/include \
    system/core/libsync/include \
    system/libhidl/base/include \
    frameworks/mead/hidl/include \
    frameworks/mead/compat/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(libgui_includes)
LOCAL_CFLAGS := -Wall -fPIC \
    -DENABLE_STRING8_OBSOLETE_METHODS -DENABLE_STRING16_OBSOLETE_METHODS \
    -include mead/android10_compat.h
LOCAL_CPPFLAGS := -std=gnu++17
LOCAL_SHARED_LIBRARIES := libbase libbinder libcutils liblog libnativewindow libui libutils
LOCAL_LDLIBS := -lEGL -lpthread
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
