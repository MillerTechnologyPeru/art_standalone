#
# libandroid_runtime for the host: Android 10's framework JNI
# (frameworks/base/core/jni), as much of it as runs here so far (now with
# the graphics JNI over hwui, without Movie, SurfaceTexture and PDF, and
# HardwareRenderer, RenderNode and Mead's own android.view.Surface, input
# events and channels, and Choreographer's display events), registered
# from JNI_OnLoad by android_runtime_mead.cpp instead of by app_process.
#

LOCAL_PATH := $(call my-dir)

android_runtime_jni := frameworks/base/core/jni

include $(CLEAR_VARS)
LOCAL_MODULE := libandroid_runtime
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    android_runtime_mead.cpp \
    android_view_Surface.cpp \
    android_view_SurfaceControl.cpp \
    mead_input.cpp \
    mead_looper.cpp \
    mead_audio_system.cpp \
    android_graphics_SurfaceTexture.cpp \
    mead_ndk_input.cpp \
    mead_ndk_asset_manager.cpp \
    mead_ndk_native_activity.cpp \
    mead_ndk_configuration.cpp \
    mead_ndk_binder.cpp \
    mead_ndk_binder_newer.cpp \
    ../../base/core/jni/android_app_NativeActivity.cpp \
    ../../base/core/jni/android_view_InputQueue.cpp \
    ../../base/core/jni/android_opengl_EGL14.cpp \
    ../../base/core/jni/android_opengl_GLES20.cpp \
    ../../base/core/jni/android_opengl_GLES30.cpp \
    ../../base/core/jni/android_opengl_GLES31.cpp \
    ../../base/core/jni/com_google_android_gles_jni_EGLImpl.cpp \
    ../../base/core/jni/com_google_android_gles_jni_GLImpl.cpp \
    mead_gles1_extensions.cpp \
    ../../../libandroidfw/DisplayEventDispatcher.cpp \
    ../../base/core/jni/android_view_DisplayEventReceiver.cpp \
    ../../base/core/jni/android_view_InputChannel.cpp \
    ../../base/core/jni/android_view_InputDevice.cpp \
    ../../base/core/jni/android_view_InputEventReceiver.cpp \
    ../../base/core/jni/android_view_KeyCharacterMap.cpp \
    ../../base/core/jni/android_view_KeyEvent.cpp \
    ../../base/core/jni/android_view_MotionEvent.cpp \
    ../../base/core/jni/android_view_VelocityTracker.cpp \
    ../../base/core/jni/android_os_MessageQueue.cpp \
    ../../base/core/jni/android_view_DisplayListCanvas.cpp \
    ../../base/core/jni/android_view_RenderNode.cpp \
    ../../base/core/jni/android_view_RenderNodeAnimator.cpp \
    ../../base/core/jni/android_view_ThreadedRenderer.cpp \
    ../../base/core/jni/android_util_Log.cpp \
    ../../base/core/jni/android_os_SystemClock.cpp \
    ../../base/core/jni/android_os_SystemProperties.cpp \
    ../../base/core/jni/android_os_Trace.cpp \
    ../../base/core/jni/android_os_Parcel.cpp \
    ../../base/core/jni/android_util_Binder.cpp \
    ../../base/core/jni/android_util_AssetManager.cpp \
    ../../base/core/jni/android_content_res_ApkAssets.cpp \
    ../../base/core/jni/android_util_StringBlock.cpp \
    ../../base/core/jni/android_util_XmlBlock.cpp \
    ../../base/core/jni/android_util_Process.cpp \
    ../../base/core/jni/android_server_NetworkManagementSocketTagger.cpp \
    ../../base/core/jni/android_util_FileObserver.cpp \
    ../../base/core/jni/android_animation_PropertyValuesHolder.cpp \
    ../../base/core/jni/android_content_res_Configuration.cpp \
    ../../base/core/jni/android_os_MemoryFile.cpp \
    ../../base/core/jni/android_os_SharedMemory.cpp \
    ../../base/core/jni/android_text_AndroidCharacter.cpp \
    ../../base/core/jni/android_text_Hyphenator.cpp \
    ../../base/core/jni/android_util_PathParser.cpp \
    ../../base/core/jni/android_view_InputEventSender.cpp \
    ../../base/core/jni/android_view_PointerIcon.cpp \
    ../../base/core/jni/com_android_internal_view_animation_NativeInterpolatorFactoryHelper.cpp \
    ../../base/core/jni/com_android_internal_util_VirtualRefBasePtr.cpp \
    ../../base/core/jni/android_database_CursorWindow.cpp \
    ../../base/core/jni/android_database_SQLiteCommon.cpp \
    ../../base/core/jni/android_database_SQLiteConnection.cpp \
    ../../base/core/jni/android_database_SQLiteDebug.cpp \
    ../../base/core/jni/android_database_SQLiteGlobal.cpp \
    ../../../libandroidfw/CursorWindow.cpp \
    ../../base/core/jni/android_hardware_HardwareBuffer.cpp \
    ../../base/core/jni/android_nio_utils.cpp \
    ../../base/core/jni/android_graphics_Canvas.cpp \
    ../../base/core/jni/android_graphics_ColorSpace.cpp \
    ../../base/core/jni/android_graphics_Picture.cpp \
    ../../base/core/jni/android_graphics_drawable_AnimatedVectorDrawable.cpp \
    ../../base/core/jni/android_graphics_drawable_VectorDrawable.cpp \
    ../../base/core/jni/android/graphics/AnimatedImageDrawable.cpp \
    ../../base/core/jni/android/graphics/Bitmap.cpp \
    ../../base/core/jni/android/graphics/BitmapFactory.cpp \
    ../../base/core/jni/android/graphics/BitmapRegionDecoder.cpp \
    ../../base/core/jni/android/graphics/ByteBufferStreamAdaptor.cpp \
    ../../base/core/jni/android/graphics/Camera.cpp \
    ../../base/core/jni/android/graphics/CanvasProperty.cpp \
    ../../base/core/jni/android/graphics/ColorFilter.cpp \
    ../../base/core/jni/android/graphics/CreateJavaOutputStreamAdaptor.cpp \
    ../../base/core/jni/android/graphics/FontFamily.cpp \
    ../../base/core/jni/android/graphics/FontUtils.cpp \
    ../../base/core/jni/android/graphics/GraphicBuffer.cpp \
    ../../base/core/jni/android/graphics/Graphics.cpp \
    ../../base/core/jni/android/graphics/ImageDecoder.cpp \
    ../../base/core/jni/android/graphics/Interpolator.cpp \
    ../../base/core/jni/android/graphics/MaskFilter.cpp \
    ../../base/core/jni/android/graphics/Matrix.cpp \
    ../../base/core/jni/android/graphics/NinePatch.cpp \
    ../../base/core/jni/android/graphics/NinePatchPeeker.cpp \
    ../../base/core/jni/android/graphics/Paint.cpp \
    ../../base/core/jni/android/graphics/PaintFilter.cpp \
    ../../base/core/jni/android/graphics/Path.cpp \
    ../../base/core/jni/android/graphics/PathEffect.cpp \
    ../../base/core/jni/android/graphics/PathMeasure.cpp \
    ../../base/core/jni/android/graphics/Picture.cpp \
    ../../base/core/jni/android/graphics/Region.cpp \
    ../../base/core/jni/android/graphics/Shader.cpp \
    ../../base/core/jni/android/graphics/Typeface.cpp \
    ../../base/core/jni/android/graphics/Utils.cpp \
    ../../base/core/jni/android/graphics/YuvToJpegEncoder.cpp \
    ../../base/core/jni/android/graphics/fonts/Font.cpp \
    ../../base/core/jni/android/graphics/fonts/FontFamily.cpp \
    ../../base/core/jni/android/graphics/text/LineBreaker.cpp \
    ../../base/core/jni/android/graphics/text/MeasuredText.cpp
LOCAL_C_INCLUDES := \
    $(android_runtime_jni) \
    $(android_runtime_jni)/include \
    $(LOCAL_PATH)/include \
    frameworks/native/libs/binder/ndk/include_ndk \
    frameworks/native/libs/binder/ndk/include_apex \
    $(android_runtime_jni)/android/graphics \
    external/libjpeg-turbo \
    frameworks/mead/egl/include \
    system/core/libmeminfo/include \
    external/sqlite/dist \
    external/sqlite/android \
    system/core/libprocessgroup/include \
    hardware/libhardware/include \
    system/core/libgrallocusage/include \
    frameworks/native/include \
    frameworks/mead/compat/include
# Android 10's code calls String8::string() and String16::string(), which the
# fork's newer libutils keeps behind these.
# Mead's libgui has no BufferQueue: what renders into one is left out.
LOCAL_CFLAGS := -Wall -Wextra -Wno-unused-parameter -DLIBGUI_NO_BUFFER_QUEUE \
    -DENABLE_STRING8_OBSOLETE_METHODS -DENABLE_STRING16_OBSOLETE_METHODS \
    -include mead/android10_compat.h
LOCAL_CPPFLAGS := -std=gnu++17
LOCAL_STATIC_LIBRARIES := libmeminfo libprocessgroup libsqlite3_android libsqlite
# Android's SQLite stays inside, apart from the system's libsqlite3.
LOCAL_LDFLAGS := -Wl,--exclude-libs,libsqlite.a -Wl,--exclude-libs,libsqlite3_android.a
LOCAL_SHARED_LIBRARIES := \
    libandroidfw \
    libbase \
    libbinder \
    libcutils \
    libgui \
    libhwui \
    libinput \
    libmead_android10_compat \
    liblog \
    libnativehelper \
    libnativewindow \
    libui \
    libutils \
    libziparchive
LOCAL_LDLIBS := -lpthread -ldl -lz -licuuc -licui18n -lEGL -lGLESv1_CM -lGLESv2
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)
