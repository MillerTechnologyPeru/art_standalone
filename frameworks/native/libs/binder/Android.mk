# Copyright (C) 2009 The Android Open Source Project
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
# Android 10's libbinder for the host: Android.bp's vendor sources, without
# the system services' generated interfaces. Mead uses it in-process, for
# Parcel and local binders; nothing opens /dev/binder unless asked to.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libbinder
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    Binder.cpp \
    BpBinder.cpp \
    BufferedTextOutput.cpp \
    Debug.cpp \
    IInterface.cpp \
    IMemory.cpp \
    IPCThreadState.cpp \
    IPermissionController.cpp \
    IResultReceiver.cpp \
    IServiceManager.cpp \
    IShellCallback.cpp \
    MemoryBase.cpp \
    MemoryDealer.cpp \
    MemoryHeapBase.cpp \
    Parcel.cpp \
    ParcelFileDescriptor.cpp \
    PersistableBundle.cpp \
    ProcessState.cpp \
    Static.cpp \
    Status.cpp \
    TextOutput.cpp \
    Value.cpp \
    ../binderthreadstate/IPCThreadStateBase.cpp
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../binderthreadstate/include \
    frameworks/mead/compat/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
# Android 10's code calls String8::string() and String16::string(), which the
# fork's newer libutils keeps behind these.
LOCAL_CFLAGS := -Wall -Wextra \
    -DENABLE_STRING8_OBSOLETE_METHODS -DENABLE_STRING16_OBSOLETE_METHODS \
    -include mead/android10_compat.h
# libcutils' property functions are in libmead_android10_compat.
LOCAL_SHARED_LIBRARIES := libbase liblog libcutils libmead_android10_compat libutils
LOCAL_LDLIBS := -lpthread
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
