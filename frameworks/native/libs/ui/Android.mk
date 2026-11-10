#
# Android 10's libui for the host: GraphicBuffer, Fence, Rect, Region and the
# display and color types, without BufferHub (as in Android's vendor
# configuration). Buffers come from Mead's gralloc (frameworks/mead/ui), in
# shared memory, instead of the gralloc HALs.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libui
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    ColorSpace.cpp \
    DebugUtils.cpp \
    Fence.cpp \
    FenceTime.cpp \
    FrameStats.cpp \
    Gralloc.cpp \
    GraphicBuffer.cpp \
    GraphicBufferAllocator.cpp \
    GraphicBufferMapper.cpp \
    HdrCapabilities.cpp \
    PixelFormat.cpp \
    PublicFormat.cpp \
    Rect.cpp \
    Region.cpp \
    Size.cpp \
    Transform.cpp \
    UiConfig.cpp \
    ../../../mead/ui/MeadGralloc.cpp
libui_includes := \
    frameworks/native/libs/ui/include \
    frameworks/native/libs/nativewindow/include \
    frameworks/native/libs/nativebase/include \
    frameworks/native/libs/arect/include \
    frameworks/native/libs/math/include \
    hardware/libhardware/include \
    system/core/libsystem/include \
    system/libhidl/base/include \
    frameworks/mead/hidl/include
LOCAL_C_INCLUDES := \
    $(libui_includes) \
    frameworks/native/include \
    frameworks/mead/ui \
    frameworks/mead/compat/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(libui_includes)
LOCAL_CFLAGS := -Wall -fPIC \
    -DENABLE_STRING8_OBSOLETE_METHODS -DENABLE_STRING16_OBSOLETE_METHODS \
    -include mead/android10_compat.h
LOCAL_CPPFLAGS := -std=gnu++17
# libcutils' property functions are in libmead_android10_compat.
LOCAL_SHARED_LIBRARIES := libbase libcutils liblog libmead_android10_compat libutils
LOCAL_WHOLE_STATIC_LIBRARIES := libhidlbase libsync libgrallocusage
LOCAL_LDLIBS := -lpthread
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
