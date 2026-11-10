#
# Android 10's libjpeg-turbo for the host, as a static library inside Skia:
# Android.bp's libjpeg sources, with NEON on arm64. Static, because the
# distributions name their libjpeg differently (Debian's is libjpeg.so.62,
# ROCKNIX's libjpeg.so.8).
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libjpeg_static
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    jaricom.c \
    jcapimin.c \
    jcapistd.c \
    jcarith.c \
    jccoefct.c \
    jccolor.c \
    jcdctmgr.c \
    jchuff.c \
    jcinit.c \
    jcmainct.c \
    jcmarker.c \
    jcmaster.c \
    jcomapi.c \
    jcparam.c \
    jcphuff.c \
    jcprepct.c \
    jcsample.c \
    jctrans.c \
    jdapimin.c \
    jdapistd.c \
    jdarith.c \
    jdatadst.c \
    jdatasrc.c \
    jdcoefct.c \
    jdcolor.c \
    jddctmgr.c \
    jdhuff.c \
    jdinput.c \
    jdmainct.c \
    jdmarker.c \
    jdmaster.c \
    jdmerge.c \
    jdphuff.c \
    jdpostct.c \
    jdsample.c \
    jdtrans.c \
    jerror.c \
    jfdctflt.c \
    jfdctfst.c \
    jfdctint.c \
    jidctflt.c \
    jidctfst.c \
    jidctint.c \
    jidctred.c \
    jmemmgr.c \
    jmemnobs.c \
    jquant1.c \
    jquant2.c \
    jutils.c
ifeq ($(HOST_ARCH),arm64)
LOCAL_SRC_FILES += simd/arm64/jsimd.c simd/arm64/jsimd_neon.S
else
# The x86 SIMD is written for nasm, which this build doesn't run.
LOCAL_SRC_FILES += jsimd_none.c
endif
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
# Its symbols stay visible: it is inside libhwui, and the graphics JNI's
# YuvImage in libandroid_runtime calls it there.
LOCAL_CFLAGS := -O3 -fstrict-aliasing -fPIC -Wno-sign-compare -Wno-unused-parameter
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_STATIC_LIBRARY)
