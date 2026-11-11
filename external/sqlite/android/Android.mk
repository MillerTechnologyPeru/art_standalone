#
# Android 10's SQLite extensions for the host: the LOCALIZED and UNICODE
# collators, over ICU, and the phone number functions, which the
# framework's database JNI registers on every connection. A static library
# with hidden symbols.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libsqlite3_android
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    PhoneNumberUtils.cpp \
    OldPhoneNumberUtils.cpp \
    sqlite3_android.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH) external/sqlite/dist frameworks/mead/compat/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
LOCAL_CFLAGS := -fPIC -fvisibility=hidden -DSQLITE_ENABLE_ICU -Wall -Wno-unused-function \
    -Wno-unused-variable -include mead/android10_compat.h
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_STATIC_LIBRARY)
