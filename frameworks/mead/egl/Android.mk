#
# The EGL and GLES extension functions Android's libEGL and libGLESv2 export,
# which the framework's graphics call directly, for EGL implementations that
# only hand them out through eglGetProcAddress (Mesa's). A static library,
# inside libhwui.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libmead_egl_extensions
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := egl_extensions.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include frameworks/native/libs/nativewindow/include
LOCAL_CFLAGS := -Wall -Wextra -fPIC
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_STATIC_LIBRARY)
