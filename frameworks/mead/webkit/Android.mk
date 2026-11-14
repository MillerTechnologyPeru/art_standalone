# Mead's WebView engine (mead.webkit): WPE WebKit, off screen, and its
# web-process extension for JavaScript interfaces. Built against the host's
# WPE WebKit 2.x (pkg-config wpe-webkit-2.0).

LOCAL_PATH := $(call my-dir)

wpe_cflags := $(shell pkg-config --cflags wpe-webkit-2.0 wpebackend-fdo-1.0)
wpe_libs := $(shell pkg-config --libs wpe-webkit-2.0 wpebackend-fdo-1.0)

include $(CLEAR_VARS)
LOCAL_MODULE := libmead_webkit
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := mead_webkit.c
LOCAL_CFLAGS := -Wall -Wno-unused-parameter -std=gnu11 $(wpe_cflags)
LOCAL_LDLIBS := -lpthread -ldl $(wpe_libs) -l:libwayland-server.so.0
# A host without WPE gets it bundled beside this library, in wpe/lib: an
# RPATH (not RUNPATH) is searched for WPE's own dependencies too, and only
# for what this library loads.
LOCAL_LDFLAGS := -Wl,--disable-new-dtags -Wl,-rpath,'$$ORIGIN/wpe/lib'
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
include $(BUILD_HOST_SHARED_LIBRARY)

wpe_extension_cflags := $(shell pkg-config --cflags wpe-web-process-extension-2.0)
wpe_extension_libs := $(shell pkg-config --libs wpe-web-process-extension-2.0)

include $(CLEAR_VARS)
LOCAL_MODULE := libmead_webkit_extension
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := mead_webkit_extension.c
LOCAL_CFLAGS := -Wall -Wno-unused-parameter -std=gnu11 $(wpe_extension_cflags)
LOCAL_LDLIBS := -lpthread $(wpe_extension_libs)
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
include $(BUILD_HOST_SHARED_LIBRARY)
