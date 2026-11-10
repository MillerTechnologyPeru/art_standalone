# Generated from Android.bp by frameworks/mead/tools/gen_hwui_mk.py; edit that instead.
#
# Android 10's libhwui for the host (glibc): Canvas, Bitmap, Paint, text
# through minikin, and the Skia pipelines, with Skia and minikin inside it.

LOCAL_PATH := $(call my-dir)

hwui_srcs := \
    hwui/AnimatedImageDrawable.cpp \
    hwui/AnimatedImageThread.cpp \
    hwui/Bitmap.cpp \
    hwui/Canvas.cpp \
    hwui/MinikinSkia.cpp \
    hwui/MinikinUtils.cpp \
    hwui/PaintImpl.cpp \
    hwui/Typeface.cpp \
    pipeline/skia/GLFunctorDrawable.cpp \
    pipeline/skia/LayerDrawable.cpp \
    pipeline/skia/RenderNodeDrawable.cpp \
    pipeline/skia/ReorderBarrierDrawables.cpp \
    pipeline/skia/ShaderCache.cpp \
    pipeline/skia/SkiaDisplayList.cpp \
    pipeline/skia/SkiaMemoryTracer.cpp \
    pipeline/skia/SkiaOpenGLPipeline.cpp \
    pipeline/skia/SkiaPipeline.cpp \
    pipeline/skia/SkiaProfileRenderer.cpp \
    pipeline/skia/SkiaRecordingCanvas.cpp \
    pipeline/skia/SkiaVulkanPipeline.cpp \
    pipeline/skia/VectorDrawableAtlas.cpp \
    pipeline/skia/VkFunctorDrawable.cpp \
    pipeline/skia/VkInteropFunctorDrawable.cpp \
    renderstate/RenderState.cpp \
    renderthread/CacheManager.cpp \
    renderthread/CanvasContext.cpp \
    renderthread/DrawFrameTask.cpp \
    renderthread/EglManager.cpp \
    renderthread/ReliableSurface.cpp \
    renderthread/VulkanManager.cpp \
    renderthread/VulkanSurface.cpp \
    renderthread/RenderProxy.cpp \
    renderthread/RenderTask.cpp \
    renderthread/RenderThread.cpp \
    renderthread/TimeLord.cpp \
    renderthread/Frame.cpp \
    thread/CommonPool.cpp \
    utils/Blur.cpp \
    utils/Color.cpp \
    utils/GLUtils.cpp \
    utils/LinearAllocator.cpp \
    utils/StringUtils.cpp \
    utils/VectorDrawableUtils.cpp \
    AnimationContext.cpp \
    Animator.cpp \
    AnimatorManager.cpp \
    CanvasTransform.cpp \
    DamageAccumulator.cpp \
    DeferredLayerUpdater.cpp \
    DeviceInfo.cpp \
    FrameInfo.cpp \
    FrameInfoVisualizer.cpp \
    GpuMemoryTracker.cpp \
    HardwareBitmapUploader.cpp \
    Interpolator.cpp \
    JankTracker.cpp \
    Layer.cpp \
    LayerUpdateQueue.cpp \
    Matrix.cpp \
    PathParser.cpp \
    ProfileData.cpp \
    ProfileDataContainer.cpp \
    Properties.cpp \
    PropertyValuesAnimatorSet.cpp \
    PropertyValuesHolder.cpp \
    Readback.cpp \
    RecordingCanvas.cpp \
    RenderNode.cpp \
    RenderProperties.cpp \
    SkiaCanvas.cpp \
    TreeInfo.cpp \
    WebViewFunctorManager.cpp \
    VectorDrawable.cpp \
    ../../../native/opengl/libs/EGL/BlobCache.cpp \
    ../../../native/opengl/libs/EGL/FileBlobCache.cpp

hwui_cflags := \
    -DEGL_EGLEXT_PROTOTYPES \
    -DGL_GLEXT_PROTOTYPES \
    -DATRACE_TAG=ATRACE_TAG_VIEW \
    -DLOG_TAG=\"OpenGLRenderer\" \
    -Wall \
    -Wno-unused-parameter \
    -Wunreachable-code \
    -Wno-free-nonheap-object \
    -Wno-missing-braces \
    -Wno-implicit-fallthrough

include $(CLEAR_VARS)
LOCAL_MODULE := libhwui
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(hwui_srcs)
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    external/skia/include/private \
    external/skia/src/core \
    external/skia/src/effects \
    external/skia/src/image \
    external/skia/src/utils \
    external/skia/src/gpu \
    external/skia/src/shaders \
    frameworks/native/opengl/libs/EGL \
    frameworks/native/include \
    system/core/libsync/include \
    frameworks/mead/hwui \
    frameworks/mead/compat/include
LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH) \
    external/skia/include/android \
    external/skia/include/atlastext \
    external/skia/include/c \
    external/skia/include/codec \
    external/skia/include/config \
    external/skia/include/core \
    external/skia/include/docs \
    external/skia/include/effects \
    external/skia/include/encode \
    external/skia/include/gpu \
    external/skia/include/pathops \
    external/skia/include/ports \
    external/skia/include/private \
    external/skia/include/svg \
    external/skia/include/utils \
    external/skia/include/utils/mac \
    external/skia/src/c \
    external/skia/src/codec \
    external/skia/src/core \
    external/skia/src/effects \
    external/skia/src/fonts \
    external/skia/src/gpu \
    external/skia/src/image \
    external/skia/src/images \
    external/skia/src/lazy \
    external/skia/src/opts \
    external/skia/src/pathops \
    external/skia/src/pdf \
    external/skia/src/ports \
    external/skia/src/sfnt \
    external/skia/src/shaders \
    external/skia/src/shaders/gradients \
    external/skia/src/sksl \
    external/skia/src/utils \
    external/skia/src/utils/win \
    external/skia/src/xml \
    external/skia/third_party/etc1 \
    external/skia/third_party/gif \
    external/skia/third_party/skcms \
    external/skia/third_party/vulkanmemoryallocator \
    external/skia/include/config/mead \
    external/skia/include/third_party/vulkan \
    frameworks/minikin/include \
    /usr/include/harfbuzz
# Without TextureView's SurfaceTexture (see DeferredLayerUpdater.h), and with
# EGL drawing to Mead's windows (see EglManager.cpp).
LOCAL_CFLAGS := $(hwui_cflags) -fPIC -DSK_BUILD_FOR_ANDROID -DHWUI_NO_SURFACE_TEXTURE -DHWUI_MEAD_WINDOWS \
    -DENABLE_STRING8_OBSOLETE_METHODS -DENABLE_STRING16_OBSOLETE_METHODS \
    -include mead/android10_compat.h
LOCAL_CPPFLAGS := -std=gnu++17
LOCAL_WHOLE_STATIC_LIBRARIES := libskia libskia_cc libjpeg_static libminikin libmead_egl_extensions
LOCAL_SHARED_LIBRARIES := libandroidfw libbase libbinder libcutils libgui liblog libnativehelper \
    libnativewindow libui libutils libziparchive
LOCAL_LDLIBS := -lEGL -lGLESv2 -l:libvulkan.so.1 -lcrypto -lfreetype -lharfbuzz -lpng16 -lwebp -lwebpmux -lwebpdemux -lexpat -lz -licuuc -licui18n -lpthread -ldl
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_MULTILIB := both
include $(BUILD_HOST_SHARED_LIBRARY)

