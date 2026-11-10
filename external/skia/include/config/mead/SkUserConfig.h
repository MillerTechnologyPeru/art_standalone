/*
 * Skia's configuration for Mead: Android's (include/config/android), for
 * the Android 10 framework running on glibc, without the codecs and
 * documents whose libraries Mead doesn't ship (RAW, HEIF and PDF).
 *
 * SK_BUILD_FOR_ANDROID is defined on the command line: there is no
 * __ANDROID__ here to imply it.
 */

#pragma once

#include "SkUserConfigManual.h"

#define SK_ALLOW_STATIC_GLOBAL_INITIALIZERS 0
#define SK_ENABLE_DISCRETE_GPU
#define SK_GAMMA_APPLY_TO_A8
#define SK_GAMMA_CONTRAST 0.0
#define SK_GAMMA_EXPONENT 1.4
#define SK_HAS_JPEG_LIBRARY
#define SK_HAS_PNG_LIBRARY
#define SK_HAS_WEBP_LIBRARY
#define SK_VULKAN
#define SK_XML

#ifndef SK_BUILD_FOR_ANDROID
    #error "SK_BUILD_FOR_ANDROID must be defined!"
#endif
#if defined(SK_BUILD_FOR_IOS) || defined(SK_BUILD_FOR_MAC) || \
    defined(SK_BUILD_FOR_UNIX) || defined(SK_BUILD_FOR_WIN)
    #error "Only SK_BUILD_FOR_ANDROID should be defined!"
#endif
