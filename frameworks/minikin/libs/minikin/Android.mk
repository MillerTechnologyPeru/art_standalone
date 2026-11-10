#
# Android 10's minikin, the framework's text layout, for the host: a static
# library for hwui, over the distribution's HarfBuzz and ICU.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libminikin
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    BidiUtils.cpp \
    CmapCoverage.cpp \
    Emoji.cpp \
    FontCollection.cpp \
    FontFamily.cpp \
    FontUtils.cpp \
    GraphemeBreak.cpp \
    GreedyLineBreaker.cpp \
    Hyphenator.cpp \
    HyphenatorMap.cpp \
    Layout.cpp \
    LayoutCore.cpp \
    LayoutUtils.cpp \
    LineBreaker.cpp \
    LineBreakerUtil.cpp \
    Locale.cpp \
    LocaleListCache.cpp \
    MeasuredText.cpp \
    Measurement.cpp \
    MinikinInternal.cpp \
    OptimalLineBreaker.cpp \
    SparseBitSet.cpp \
    SystemFonts.cpp \
    WordBreaker.cpp
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    frameworks/minikin/include \
    frameworks/mead/compat/include \
    /usr/include/harfbuzz
LOCAL_EXPORT_C_INCLUDE_DIRS := frameworks/minikin/include /usr/include/harfbuzz
# Its symbols stay visible: it is inside libhwui, and the graphics JNI in
# libandroid_runtime calls it there.
LOCAL_CFLAGS := -Wall -fPIC -include mead/android10_compat.h
LOCAL_SHARED_LIBRARIES := libbase liblog libutils
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_STATIC_LIBRARY)
