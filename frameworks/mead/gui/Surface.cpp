/*
 * Mead's Surface: a queue of gralloc buffers whose frames go to Mead's
 * display bridge. See gui/Surface.h.
 */

#define LOG_TAG "MeadSurface"

#include <gui/Surface.h>

#include <unistd.h>

#include <chrono>

#include <log/log.h>
#include <sync/sync.h>

namespace android {

namespace {

constexpr size_t kDefaultBufferCount = 3;
constexpr size_t kMaxBufferCount = 64;

void waitAndClose(int fenceFd) {
    if (fenceFd >= 0) {
        sync_wait(fenceFd, -1);
        close(fenceFd);
    }
}

} // namespace

Surface::Surface(uint32_t width, uint32_t height, int32_t format,
                 const MeadSurfaceCallbacks& callbacks)
      : mCallbacks(callbacks),
        mSlots(kDefaultBufferCount),
        mDefaultWidth(width),
        mDefaultHeight(height),
        mFormat(format) {
    ANativeWindow::setSwapInterval = hook_setSwapInterval;
    ANativeWindow::dequeueBuffer = hook_dequeueBuffer;
    ANativeWindow::cancelBuffer = hook_cancelBuffer;
    ANativeWindow::queueBuffer = hook_queueBuffer;
    ANativeWindow::query = hook_query;
    ANativeWindow::perform = hook_perform;

    ANativeWindow::dequeueBuffer_DEPRECATED = hook_dequeueBuffer_DEPRECATED;
    ANativeWindow::cancelBuffer_DEPRECATED = hook_cancelBuffer_DEPRECATED;
    ANativeWindow::lockBuffer_DEPRECATED = hook_lockBuffer_DEPRECATED;
    ANativeWindow::queueBuffer_DEPRECATED = hook_queueBuffer_DEPRECATED;

    const_cast<int&>(ANativeWindow::minSwapInterval) = 0;
    const_cast<int&>(ANativeWindow::maxSwapInterval) = 1;
    const_cast<uint32_t&>(ANativeWindow::flags) = 0;
}

Surface::~Surface() = default;

// ANativeWindow's hooks

int Surface::hook_setSwapInterval(ANativeWindow* window, int interval) {
    return getSelf(window)->setSwapInterval(interval);
}

int Surface::hook_dequeueBuffer(ANativeWindow* window, ANativeWindowBuffer** buffer,
                                int* fenceFd) {
    return getSelf(window)->dequeueBuffer(buffer, fenceFd);
}

int Surface::hook_cancelBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer, int fenceFd) {
    return getSelf(window)->cancelBuffer(buffer, fenceFd);
}

int Surface::hook_queueBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer, int fenceFd) {
    return getSelf(window)->queueBuffer(buffer, fenceFd);
}

int Surface::hook_query(const ANativeWindow* window, int what, int* value) {
    return getSelf(window)->query(what, value);
}

int Surface::hook_perform(ANativeWindow* window, int operation, ...) {
    va_list args;
    va_start(args, operation);
    int result = getSelf(window)->perform(operation, args);
    va_end(args);
    return result;
}

int Surface::hook_dequeueBuffer_DEPRECATED(ANativeWindow* window, ANativeWindowBuffer** buffer) {
    int fenceFd = -1;
    int result = getSelf(window)->dequeueBuffer(buffer, &fenceFd);
    waitAndClose(fenceFd);
    return result;
}

int Surface::hook_cancelBuffer_DEPRECATED(ANativeWindow* window, ANativeWindowBuffer* buffer) {
    return getSelf(window)->cancelBuffer(buffer, -1);
}

int Surface::hook_lockBuffer_DEPRECATED(ANativeWindow* window, ANativeWindowBuffer* buffer) {
    return getSelf(window)->lockBuffer_DEPRECATED(buffer);
}

int Surface::hook_queueBuffer_DEPRECATED(ANativeWindow* window, ANativeWindowBuffer* buffer) {
    return getSelf(window)->queueBuffer(buffer, -1);
}

// The queue

int Surface::slotOf(const ANativeWindowBuffer* buffer) const {
    for (size_t i = 0; i < mSlots.size(); i++) {
        if (mSlots[i].buffer != nullptr && mSlots[i].buffer->getNativeBuffer() == buffer) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

status_t Surface::allocate(Slot* slot) {
    uint32_t width = mUserWidth ? mUserWidth : mDefaultWidth;
    uint32_t height = mUserHeight ? mUserHeight : mDefaultHeight;
    const sp<GraphicBuffer>& buffer = slot->buffer;
    if (buffer != nullptr && buffer->getWidth() == width && buffer->getHeight() == height &&
        buffer->getPixelFormat() == mFormat && (buffer->getUsage() & mUsage) == mUsage) {
        return NO_ERROR;
    }
    sp<GraphicBuffer> allocated =
            new GraphicBuffer(width, height, mFormat, 1, mUsage, "MeadSurface");
    status_t error = allocated->initCheck();
    if (error != NO_ERROR) {
        ALOGE("Allocating a %ux%u buffer of format %d failed: %d", width, height, mFormat, error);
        return error;
    }
    slot->buffer = allocated;
    return NO_ERROR;
}

void Surface::resize(uint32_t width, uint32_t height) {
    std::lock_guard<std::mutex> lock(mMutex);
    mDefaultWidth = width;
    mDefaultHeight = height;
}

void Surface::allocateBuffers() {
    std::lock_guard<std::mutex> lock(mMutex);
    for (Slot& slot : mSlots) {
        if (!slot.dequeued && !slot.held) allocate(&slot);
    }
}

int Surface::dequeueBuffer(ANativeWindowBuffer** buffer, int* fenceFd) {
    std::unique_lock<std::mutex> lock(mMutex);
    nsecs_t start = systemTime(SYSTEM_TIME_MONOTONIC);
    mLastDequeueStartTime = start;
    auto free = [this] {
        for (const Slot& slot : mSlots) {
            if (!slot.dequeued && !slot.held) return true;
        }
        return false;
    };
    if (mDequeueTimeout < 0) {
        mFreed.wait(lock, free);
    } else if (!mFreed.wait_for(lock, std::chrono::nanoseconds(mDequeueTimeout), free)) {
        return TIMED_OUT;
    }
    for (Slot& slot : mSlots) {
        if (slot.dequeued || slot.held) continue;
        status_t error = allocate(&slot);
        if (error != NO_ERROR) return error;
        slot.dequeued = true;
        *buffer = slot.buffer->getNativeBuffer();
        *fenceFd = -1;
        mLastDequeueDuration = systemTime(SYSTEM_TIME_MONOTONIC) - start;
        return NO_ERROR;
    }
    return NO_MEMORY;
}

int Surface::cancelBuffer(ANativeWindowBuffer* buffer, int fenceFd) {
    if (fenceFd >= 0) close(fenceFd);
    std::lock_guard<std::mutex> lock(mMutex);
    int slot = slotOf(buffer);
    if (slot < 0 || !mSlots[slot].dequeued) return BAD_VALUE;
    mSlots[slot].dequeued = false;
    mFreed.notify_one();
    return NO_ERROR;
}

int Surface::queueBuffer(ANativeWindowBuffer* buffer, int fenceFd) {
    nsecs_t start = systemTime(SYSTEM_TIME_MONOTONIC);
    waitAndClose(fenceFd);
    sp<GraphicBuffer> queued;
    int queuedSlot;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        queuedSlot = slotOf(buffer);
        if (queuedSlot < 0 || !mSlots[queuedSlot].dequeued) return BAD_VALUE;
        queued = mSlots[queuedSlot].buffer;
    }
    bool held = false;
    if (mCallbacks.presentBuffer != nullptr) {
        void* pixels = nullptr;
        status_t error = queued->lock(GraphicBuffer::USAGE_SW_READ_OFTEN, &pixels);
        if (error == NO_ERROR) {
            // The buffer's memfd, from Mead's gralloc's handle.
            const native_handle_t* handle = queued->handle;
            int fd = handle != nullptr && handle->numFds > 0 ? handle->data[0] : -1;
            {
                // Held before the bridge sees it, which may release it at once.
                std::lock_guard<std::mutex> lock(mMutex);
                mSlots[queuedSlot].held = true;
            }
            held = true;
            mCallbacks.presentBuffer(mCallbacks.context, queuedSlot, fd, queued->getWidth(),
                                     queued->getHeight(), queued->getStride(),
                                     queued->getPixelFormat(), pixels);
            queued->unlock();
        } else {
            ALOGE("Locking a queued buffer failed: %d", error);
        }
    } else if (mCallbacks.present != nullptr) {
        void* pixels = nullptr;
        status_t error = queued->lock(GraphicBuffer::USAGE_SW_READ_OFTEN, &pixels);
        if (error == NO_ERROR) {
            mCallbacks.present(mCallbacks.context, pixels, queued->getWidth(),
                               queued->getHeight(), queued->getStride(),
                               queued->getPixelFormat());
            queued->unlock();
        } else {
            ALOGE("Locking a queued buffer failed: %d", error);
        }
    }
    std::lock_guard<std::mutex> lock(mMutex);
    int slot = slotOf(buffer);
    if (slot >= 0) mSlots[slot].dequeued = false;
    (void)held;
    mLastQueued = queued;
    mFrameNumber++;
    mLastQueueDuration = systemTime(SYSTEM_TIME_MONOTONIC) - start;
    mFreed.notify_one();
    return NO_ERROR;
}

int Surface::setSwapInterval(int) {
    // Frames are presented as they are queued.
    return NO_ERROR;
}

int Surface::lockBuffer_DEPRECATED(ANativeWindowBuffer*) {
    return NO_ERROR;
}

int Surface::setBufferCount(int bufferCount) {
    std::lock_guard<std::mutex> lock(mMutex);
    size_t count = bufferCount > 0 ? static_cast<size_t>(bufferCount) : kDefaultBufferCount;
    if (count > kMaxBufferCount) return BAD_VALUE;
    for (const Slot& slot : mSlots) {
        if (slot.dequeued) return INVALID_OPERATION;
    }
    mSlots.assign(count, Slot());
    return NO_ERROR;
}

int Surface::query(int what, int* value) const {
    std::lock_guard<std::mutex> lock(mMutex);
    switch (what) {
        case NATIVE_WINDOW_WIDTH:
        case NATIVE_WINDOW_DEFAULT_WIDTH:
            *value = static_cast<int>(mUserWidth ? mUserWidth : mDefaultWidth);
            return NO_ERROR;
        case NATIVE_WINDOW_HEIGHT:
        case NATIVE_WINDOW_DEFAULT_HEIGHT:
            *value = static_cast<int>(mUserHeight ? mUserHeight : mDefaultHeight);
            return NO_ERROR;
        case NATIVE_WINDOW_FORMAT:
            *value = mFormat;
            return NO_ERROR;
        case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
            *value = 1;
            return NO_ERROR;
        case NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER:
        case NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND:
        case NATIVE_WINDOW_CONSUMER_USAGE_BITS:
        case NATIVE_WINDOW_TRANSFORM_HINT:
        case NATIVE_WINDOW_STICKY_TRANSFORM:
        case NATIVE_WINDOW_FRAME_TIMESTAMPS_SUPPORTS_PRESENT:
        case NATIVE_WINDOW_CONSUMER_IS_PROTECTED:
        // Each queued frame is copied out, so a buffer's old contents
        // aren't kept for partial updates: ages are unknown.
        case NATIVE_WINDOW_BUFFER_AGE:
            *value = 0;
            return NO_ERROR;
        case NATIVE_WINDOW_CONCRETE_TYPE:
            *value = NATIVE_WINDOW_SURFACE;
            return NO_ERROR;
        case NATIVE_WINDOW_DEFAULT_DATASPACE:
        case NATIVE_WINDOW_DATASPACE:
            *value = static_cast<int>(mDataspace);
            return NO_ERROR;
        case NATIVE_WINDOW_LAST_DEQUEUE_DURATION:
            *value = static_cast<int>(ns2us(mLastDequeueDuration));
            return NO_ERROR;
        case NATIVE_WINDOW_LAST_QUEUE_DURATION:
            *value = static_cast<int>(ns2us(mLastQueueDuration));
            return NO_ERROR;
        case NATIVE_WINDOW_LAYER_COUNT:
        case NATIVE_WINDOW_IS_VALID:
            *value = 1;
            return NO_ERROR;
        case NATIVE_WINDOW_MAX_BUFFER_COUNT:
            *value = static_cast<int>(kMaxBufferCount);
            return NO_ERROR;
        default:
            return BAD_VALUE;
    }
}

int Surface::perform(int operation, va_list args) {
    std::lock_guard<std::mutex> lock(mMutex);
    switch (operation) {
        case NATIVE_WINDOW_SET_USAGE:
            mUsage = va_arg(args, uint64_t) | GraphicBuffer::USAGE_SW_READ_OFTEN;
            return NO_ERROR;
        case NATIVE_WINDOW_SET_BUFFER_COUNT: {
            size_t count = va_arg(args, size_t);
            if (count == 0) count = kDefaultBufferCount;
            if (count > kMaxBufferCount) return BAD_VALUE;
            for (const Slot& slot : mSlots) {
                if (slot.dequeued) return INVALID_OPERATION;
            }
            mSlots.assign(count, Slot());
            return NO_ERROR;
        }
        case NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS:
        case NATIVE_WINDOW_SET_BUFFERS_USER_DIMENSIONS: {
            int width = va_arg(args, int);
            int height = va_arg(args, int);
            if (width < 0 || height < 0) return BAD_VALUE;
            mUserWidth = static_cast<uint32_t>(width);
            mUserHeight = static_cast<uint32_t>(height);
            return NO_ERROR;
        }
        case NATIVE_WINDOW_SET_BUFFERS_FORMAT: {
            int format = va_arg(args, int);
            if (format != 0) mFormat = format;
            return NO_ERROR;
        }
        case NATIVE_WINDOW_SET_BUFFERS_DATASPACE:
            mDataspace = static_cast<ui::Dataspace>(va_arg(args, int));
            return NO_ERROR;
        case NATIVE_WINDOW_SET_BUFFERS_TRANSFORM:
            mTransform = static_cast<uint32_t>(va_arg(args, int));
            return NO_ERROR;
        case NATIVE_WINDOW_CONNECT:
        case NATIVE_WINDOW_DISCONNECT:
        case NATIVE_WINDOW_API_CONNECT:
        case NATIVE_WINDOW_API_DISCONNECT:
        case NATIVE_WINDOW_SET_CROP:
        case NATIVE_WINDOW_SET_BUFFERS_GEOMETRY:
        case NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP:
        case NATIVE_WINDOW_SET_SCALING_MODE:
        case NATIVE_WINDOW_SET_BUFFERS_STICKY_TRANSFORM:
        case NATIVE_WINDOW_SET_SURFACE_DAMAGE:
        case NATIVE_WINDOW_SET_SHARED_BUFFER_MODE:
        case NATIVE_WINDOW_SET_AUTO_REFRESH:
        case NATIVE_WINDOW_ENABLE_FRAME_TIMESTAMPS:
            return NO_ERROR;
        case NATIVE_WINDOW_GET_REFRESH_CYCLE_DURATION: {
            MeadDisplay display;
            mead_gui_get_display(&display);
            *va_arg(args, nsecs_t*) =
                    static_cast<nsecs_t>(1e9 / (display.refreshRate > 0 ? display.refreshRate : 60));
            return NO_ERROR;
        }
        case NATIVE_WINDOW_GET_NEXT_FRAME_ID:
            *va_arg(args, uint64_t*) = mFrameNumber + 1;
            return NO_ERROR;
        case NATIVE_WINDOW_LOCK: {
            // ANativeWindow_lock's: under the lock perform holds, which lock()
            // doesn't take.
            ANativeWindow_Buffer* outBuffer = va_arg(args, ANativeWindow_Buffer*);
            ARect* inOutDirtyBounds = va_arg(args, ARect*);
            mMutex.unlock();
            status_t result = Surface::lock(outBuffer, inOutDirtyBounds);
            mMutex.lock();
            return result;
        }
        case NATIVE_WINDOW_UNLOCK_AND_POST: {
            mMutex.unlock();
            status_t result = unlockAndPost();
            mMutex.lock();
            return result;
        }
        case MEAD_SURFACE_GET_NATIVE_WINDOW:
            *va_arg(args, uintptr_t*) = mNativeWindow;
            return NO_ERROR;
        case NATIVE_WINDOW_GET_WIDE_COLOR_SUPPORT:
        case NATIVE_WINDOW_GET_HDR_SUPPORT:
            *va_arg(args, bool*) = false;
            return NO_ERROR;
        default:
            return INVALID_OPERATION;
    }
}

void Surface::releaseBuffer(int slot) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (slot < 0 || static_cast<size_t>(slot) >= mSlots.size()) return;
    mSlots[slot].held = false;
    mFreed.notify_one();
}

void Surface::setNativeWindow(uintptr_t window) {
    std::lock_guard<std::mutex> lock(mMutex);
    mNativeWindow = window;
}

status_t Surface::lock(ANativeWindow_Buffer* outBuffer, ARect* inOutDirtyBounds) {
    if (mLocked != nullptr) {
        ALOGE("lock: the surface is already locked");
        return INVALID_OPERATION;
    }
    ANativeWindowBuffer* buffer = nullptr;
    int fenceFd = -1;
    status_t error = dequeueBuffer(&buffer, &fenceFd);
    if (error != NO_ERROR) return error;
    waitAndClose(fenceFd);
    sp<GraphicBuffer> graphicBuffer = GraphicBuffer::from(buffer);
    void* bits = nullptr;
    error = graphicBuffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN |
                                        GraphicBuffer::USAGE_SW_WRITE_OFTEN,
                                &bits);
    if (error != NO_ERROR) {
        cancelBuffer(buffer, -1);
        return error;
    }
    mLocked = graphicBuffer;
    outBuffer->width = static_cast<int32_t>(graphicBuffer->getWidth());
    outBuffer->height = static_cast<int32_t>(graphicBuffer->getHeight());
    outBuffer->stride = static_cast<int32_t>(graphicBuffer->getStride());
    outBuffer->format = graphicBuffer->getPixelFormat();
    outBuffer->bits = bits;
    if (inOutDirtyBounds != nullptr) {
        inOutDirtyBounds->left = 0;
        inOutDirtyBounds->top = 0;
        inOutDirtyBounds->right = outBuffer->width;
        inOutDirtyBounds->bottom = outBuffer->height;
    }
    return NO_ERROR;
}

status_t Surface::unlockAndPost() {
    if (mLocked == nullptr) {
        ALOGE("unlockAndPost: the surface isn't locked");
        return INVALID_OPERATION;
    }
    sp<GraphicBuffer> locked = mLocked;
    mLocked = nullptr;
    locked->unlock();
    return queueBuffer(locked->getNativeBuffer(), -1);
}

int Surface::setScalingMode(int) {
    return NO_ERROR;
}

uint64_t Surface::getNextFrameNumber() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mFrameNumber + 1;
}

status_t Surface::setDequeueTimeout(nsecs_t timeout) {
    std::lock_guard<std::mutex> lock(mMutex);
    mDequeueTimeout = timeout;
    return NO_ERROR;
}

status_t Surface::getLastQueuedBuffer(sp<GraphicBuffer>* outBuffer, sp<Fence>* outFence,
                                      float outTransformMatrix[16]) {
    std::lock_guard<std::mutex> lock(mMutex);
    *outBuffer = mLastQueued;
    *outFence = Fence::NO_FENCE;
    for (int i = 0; i < 16; i++) outTransformMatrix[i] = i % 5 == 0 ? 1.0f : 0.0f;
    return NO_ERROR;
}

nsecs_t Surface::getLastDequeueStartTime() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mLastDequeueStartTime;
}

ui::Dataspace Surface::getBuffersDataSpace() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mDataspace;
}

} // namespace android

using android::Surface;

ANativeWindow* mead_surface_create(uint32_t width, uint32_t height, int32_t format,
                                   const MeadSurfaceCallbacks* callbacks) {
    MeadSurfaceCallbacks none = {nullptr, nullptr};
    Surface* surface = new Surface(width, height, format, callbacks ? *callbacks : none);
    surface->incStrong(nullptr);
    return surface;
}

void mead_surface_release_buffer(ANativeWindow* window, int32_t slot) {
    static_cast<Surface*>(window)->releaseBuffer(slot);
}

void mead_surface_set_native_window(ANativeWindow* window, uintptr_t nativeWindow) {
    static_cast<Surface*>(window)->setNativeWindow(nativeWindow);
}

uintptr_t mead_surface_get_native_window(ANativeWindow* window) {
    uintptr_t nativeWindow = 0;
    if (window == nullptr ||
        window->perform(window, MEAD_SURFACE_GET_NATIVE_WINDOW, &nativeWindow) != android::NO_ERROR) {
        return 0;
    }
    return nativeWindow;
}

void mead_surface_resize(ANativeWindow* window, uint32_t width, uint32_t height) {
    static_cast<Surface*>(window)->resize(width, height);
}
