# Copyright (C) 2010 The Android Open Source Project
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
# Android 10's libandroidfw for the host, as a shared library for the
# framework's JNI (AssetManager, ApkAssets and the rest): Android.bp's
# device sources that don't need the display or binder services. String8's
# path methods, which the legacy AssetManager uses, are PathUtils' functions
# in the fork's newer libutils, as in newer AOSP.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libandroidfw
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    ApkAssets.cpp \
    Asset.cpp \
    AssetDir.cpp \
    AssetManager.cpp \
    AssetManager2.cpp \
    AttributeResolution.cpp \
    ChunkIterator.cpp \
    ConfigDescription.cpp \
    Idmap.cpp \
    LoadedArsc.cpp \
    Locale.cpp \
    LocaleData.cpp \
    misc.cpp \
    ObbFile.cpp \
    PathUtils.cpp \
    PosixUtils.cpp \
    ResourceTypes.cpp \
    ResourceUtils.cpp \
    StreamingZipInflater.cpp \
    TypeWrappers.cpp \
    Util.cpp \
    ZipFileRO.cpp \
    ZipUtils.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/include_pathutils frameworks/native/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
# Android 10's code calls String8::string() and String16::string(), which the
# fork's newer libutils keeps behind these.
LOCAL_CFLAGS := -Wunreachable-code \
    -DENABLE_STRING8_OBSOLETE_METHODS -DENABLE_STRING16_OBSOLETE_METHODS
LOCAL_SHARED_LIBRARIES := libbase libcutils liblog libutils libziparchive
LOCAL_LDLIBS := -lz -lpthread
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
