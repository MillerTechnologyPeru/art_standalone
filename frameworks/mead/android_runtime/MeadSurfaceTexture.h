/*
 * Mead's SurfaceTexture (android_graphics_SurfaceTexture.cpp): the producer
 * surface behind a Java SurfaceTexture, for Surface(SurfaceTexture).
 */

#ifndef MEAD_SURFACE_TEXTURE_H
#define MEAD_SURFACE_TEXTURE_H

#include <gui/Surface.h>
#include "jni.h"

namespace android {

/* The surface whose frames the SurfaceTexture shows; null once released. */
sp<Surface> mead_SurfaceTexture_getProducer(JNIEnv* env, jobject surfaceTexture);

}  // namespace android

#endif
