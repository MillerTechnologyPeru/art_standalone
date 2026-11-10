# Copyright (C) 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# Conscrypt's platform JNI library, libjavacrypto, for the host: Android.bp's
# module as this build system knows it. BoringSSL is linked in statically
# and stays hidden inside it. conscrypt.jar comes from AOSP's build.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libjavacrypto
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := $(addprefix common/src/jni/main/cpp/conscrypt/, \
    compatibility_close_monitor.cc \
    jniload.cc \
    jniutil.cc \
    native_crypto.cc \
    netutil.cc \
    trace.cc)
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/common/src/jni/main/include \
    libcore/luni/src/main/native
LOCAL_CFLAGS := -Wall -Wextra -fvisibility=hidden -DJNI_JARJAR_PREFIX=com/android/
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_STATIC_LIBRARIES := libssl_static libcrypto_static
LOCAL_LDLIBS := -lpthread -ldl
# Only JNI_OnLoad leaves the library.
LOCAL_LDFLAGS := -Wl,--exclude-libs,ALL
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
