# -*- mode: makefile -*-
# Copyright (C) 2007 The Android Open Source Project
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
# The core library's native code, for the host: NativeCode.bp's modules
# (libandroidio, libjavacore and libopenjdk) as this build system knows them.
# The Java half comes from AOSP's own build of the same release.
#

ifeq ($(HOST_OS),linux)

core_native_cflags := -Wall -Wextra
core_native_cppflags := -DU_USING_ICU_NAMESPACE=0

luni_native_srcs := \
    ExecStrings.cpp \
    IcuUtilities.cpp \
    JniConstants.cpp \
    JniException.cpp \
    NetworkUtilities.cpp \
    Register.cpp \
    ZipUtilities.cpp \
    android_system_OsConstants.cpp \
    cbigint.cpp \
    java_lang_StringToReal.cpp \
    java_lang_invoke_MethodHandle.cpp \
    java_lang_invoke_VarHandle.cpp \
    java_math_NativeBN.cpp \
    java_util_regex_Matcher.cpp \
    java_util_regex_Pattern.cpp \
    libcore_icu_ICU.cpp \
    libcore_icu_NativeConverter.cpp \
    libcore_icu_TimeZoneNames.cpp \
    libcore_io_AsynchronousCloseMonitor.cpp \
    libcore_io_Linux.cpp \
    libcore_io_Memory.cpp \
    libcore_util_NativeAllocationRegistry.cpp \
    org_apache_harmony_xml_ExpatParser.cpp \
    sun_misc_Unsafe.cpp \
    valueOf.cpp

libopenjdk_native_srcs := \
    ZipFile.c \
    Inflater.c \
    Deflater.c \
    CRC32.c \
    Adler32.c \
    zip_util.c \
    jni_util.c \
    jni_util_md.c \
    io_util.c \
    canonicalize_md.c \
    FileDescriptor_md.c \
    DatagramChannelImpl.c \
    DatagramDispatcher.c \
    Console_md.c \
    IOUtil.c \
    PollArrayWrapper.c \
    SocketChannelImpl.c \
    FileChannelImpl.c \
    FileDispatcherImpl.c \
    FileOutputStream_md.c \
    FileInputStream.c \
    FileSystemPreferences.c \
    EPoll.c \
    EPollPort.c \
    UnixAsynchronousServerSocketChannelImpl.c \
    UnixAsynchronousSocketChannelImpl.c \
    io_util_md.c \
    NativeThread.c \
    FileKey.c \
    UnixFileSystem_md.c \
    ObjectStreamClass.c \
    ObjectOutputStream.c \
    ObjectInputStream.c \
    LinuxNativeDispatcher.c \
    LinuxWatchService.c \
    UnixCopyFile.c \
    UnixNativeDispatcher.c \
    InetAddress.c \
    net_util.c \
    net_util_md.c \
    Net.c \
    MappedByteBuffer.c \
    Inet6Address.c \
    Inet4Address.c \
    linux_close.cpp \
    ServerSocketChannelImpl.c \
    SocketInputStream.c \
    SocketOutputStream.c \
    Float.c \
    Double.c \
    StrictMath.c \
    Math.c \
    ProcessEnvironment_md.c \
    System.c \
    Runtime.c \
    UNIXProcess_md.c \
    Character.cpp \
    socket_tagger_util.cpp \
    OnLoad.cpp \
    JniConstants.cpp

# libandroidio: the monitor that wakes threads blocked on a file descriptor
# that another thread closes.
include $(CLEAR_VARS)
LOCAL_MODULE := libandroidio
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := luni/src/main/native/AsynchronousCloseMonitor.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/luni/src/main/native
LOCAL_CFLAGS := $(core_native_cflags)
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_LDLIBS := -lpthread
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/NativeCode.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libjavacore
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    $(addprefix luni/src/main/native/,$(luni_native_srcs)) \
    dalvik/src/main/native/org_apache_harmony_dalvik_NativeTestTarget.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/luni/src/main/native
LOCAL_CFLAGS := $(core_native_cflags)
LOCAL_CPPFLAGS := $(core_native_cppflags)
LOCAL_SHARED_LIBRARIES := libandroidio libbase liblog libnativehelper libziparchive
LOCAL_LDLIBS := -ldl -lpthread -lrt -lcrypto -lexpat -lz -licuuc -licui18n
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/NativeCode.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libopenjdk
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(addprefix ojluni/src/main/native/,$(libopenjdk_native_srcs))
LOCAL_C_INCLUDES := $(LOCAL_PATH)/luni/src/main/native
LOCAL_CFLAGS := $(core_native_cflags) \
    -Wno-unused-parameter \
    -Wno-unused-variable \
    -Wno-parentheses \
    -D_LARGEFILE64_SOURCE -D_GNU_SOURCE -DLINUX -D__GLIBC__
LOCAL_SHARED_LIBRARIES := libandroidio libbase liblog libnativehelper libopenjdkjvm
LOCAL_STATIC_LIBRARIES := libfdlibm
LOCAL_LDLIBS := -ldl -lpthread -lrt -lcrypto -licuuc -lz
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/ojluni/NOTICE
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/NativeCode.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)

endif # HOST_OS == linux
