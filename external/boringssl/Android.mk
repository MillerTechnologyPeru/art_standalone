# Copyright (C) 2015 The Android Open Source Project
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
# BoringSSL for the host, as static libraries for Conscrypt's libjavacrypto
# alone: built without BORINGSSL_SHARED_LIBRARY, so that every symbol stays
# hidden inside libjavacrypto and none meets the distribution's OpenSSL,
# which libjavacore links.
#

LOCAL_PATH := $(call my-dir)

include $(LOCAL_PATH)/sources.mk

boringssl_cflags := \
    -fvisibility=hidden \
    -DBORINGSSL_IMPLEMENTATION \
    -DOPENSSL_SMALL \
    -D_XOPEN_SOURCE=700 \
    -Wno-unused-parameter
boringssl_conlyflags := -std=c99

boringssl_ssl_sources := $(addprefix src/ssl/, \
    bio_ssl.cc d1_both.cc d1_lib.cc d1_pkt.cc d1_srtp.cc dtls_method.cc \
    dtls_record.cc handoff.cc handshake.cc handshake_client.cc \
    handshake_server.cc s3_both.cc s3_lib.cc s3_pkt.cc ssl_aead_ctx.cc \
    ssl_asn1.cc ssl_buffer.cc ssl_cert.cc ssl_cipher.cc ssl_file.cc \
    ssl_key_share.cc ssl_lib.cc ssl_privkey.cc ssl_session.cc ssl_stat.cc \
    ssl_transcript.cc ssl_versions.cc ssl_x509.cc t1_enc.cc t1_lib.cc \
    tls13_both.cc tls13_client.cc tls13_enc.cc tls13_server.cc \
    tls_method.cc tls_record.cc)

include $(CLEAR_VARS)
LOCAL_MODULE := libcrypto_static
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(crypto_sources)
ifeq ($(HOST_ARCH),arm64)
LOCAL_SRC_FILES += $(linux_aarch64_sources)
LOCAL_ASFLAGS := -march=armv8-a+crypto
else ifeq ($(HOST_ARCH),x86_64)
LOCAL_SRC_FILES += $(linux_x86_64_sources)
else
LOCAL_CFLAGS := -DOPENSSL_NO_ASM
endif
LOCAL_C_INCLUDES := $(LOCAL_PATH)/src/include $(LOCAL_PATH)/src/crypto
LOCAL_CFLAGS += $(boringssl_cflags)
LOCAL_CONLYFLAGS := $(boringssl_conlyflags)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/src/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk $(LOCAL_PATH)/sources.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libssl_static
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := $(boringssl_ssl_sources)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/src/include
LOCAL_CFLAGS := $(boringssl_cflags)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/src/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_STATIC_LIBRARY)
