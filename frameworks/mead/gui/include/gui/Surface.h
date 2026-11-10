/*
 * Mead's Surface: the ANativeWindow the framework's graphics draw into, a
 * queue of buffers from Mead's gralloc whose frames go to Mead's display
 * bridge (mead_surface_create), in place of Android's, which queues them to
 * SurfaceFlinger. It keeps the part of Android's Surface that hwui uses.
 */

#pragma once

#include <stdarg.h>

#include <condition_variable>
#include <mutex>
#include <vector>

#include <gui/BufferQueueDefs.h>  // as Android's Surface.h has it, for its users
#include <mead/gui.h>
#include <system/window.h>
#include <ui/ANativeObjectBase.h>
#include <ui/Fence.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicTypes.h>
#include <utils/RefBase.h>
#include <utils/Timers.h>

namespace android {

class Surface : public ANativeObjectBase<ANativeWindow, Surface, RefBase> {
public:
    Surface(uint32_t width, uint32_t height, int32_t format,
            const MeadSurfaceCallbacks& callbacks);

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    // The size of the buffers handed out from now on.
    void resize(uint32_t width, uint32_t height);

    // Allocates the buffers ahead of the first frame.
    void allocateBuffers();

    uint64_t getNextFrameNumber() const;

    // How long dequeueBuffer waits for a free buffer; negative waits on.
    status_t setDequeueTimeout(nsecs_t timeout);

    // The buffer queued last, with no fence and an identity transform.
    status_t getLastQueuedBuffer(sp<GraphicBuffer>* outBuffer, sp<Fence>* outFence,
                                 float outTransformMatrix[16]);

    nsecs_t getLastDequeueStartTime() const;

    ui::Dataspace getBuffersDataSpace();

    virtual int query(int what, int* value) const;

protected:
    virtual ~Surface();

    virtual int dequeueBuffer(ANativeWindowBuffer** buffer, int* fenceFd);
    virtual int cancelBuffer(ANativeWindowBuffer* buffer, int fenceFd);
    virtual int queueBuffer(ANativeWindowBuffer* buffer, int fenceFd);
    virtual int perform(int operation, va_list args);
    virtual int setSwapInterval(int interval);
    virtual int lockBuffer_DEPRECATED(ANativeWindowBuffer* buffer);
    virtual int setBufferCount(int bufferCount);

private:
    struct Slot {
        sp<GraphicBuffer> buffer;
        bool dequeued = false;
    };

    static int hook_setSwapInterval(ANativeWindow* window, int interval);
    static int hook_dequeueBuffer(ANativeWindow* window, ANativeWindowBuffer** buffer,
                                  int* fenceFd);
    static int hook_cancelBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer,
                                 int fenceFd);
    static int hook_queueBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer, int fenceFd);
    static int hook_query(const ANativeWindow* window, int what, int* value);
    static int hook_perform(ANativeWindow* window, int operation, ...);
    static int hook_dequeueBuffer_DEPRECATED(ANativeWindow* window,
                                             ANativeWindowBuffer** buffer);
    static int hook_cancelBuffer_DEPRECATED(ANativeWindow* window, ANativeWindowBuffer* buffer);
    static int hook_lockBuffer_DEPRECATED(ANativeWindow* window, ANativeWindowBuffer* buffer);
    static int hook_queueBuffer_DEPRECATED(ANativeWindow* window, ANativeWindowBuffer* buffer);

    int slotOf(const ANativeWindowBuffer* buffer) const;
    status_t allocate(Slot* slot);

    MeadSurfaceCallbacks mCallbacks;

    mutable std::mutex mMutex;
    std::condition_variable mFreed;
    std::vector<Slot> mSlots;
    uint32_t mDefaultWidth;
    uint32_t mDefaultHeight;
    uint32_t mUserWidth = 0;
    uint32_t mUserHeight = 0;
    int32_t mFormat;
    uint64_t mUsage = GraphicBuffer::USAGE_SW_READ_OFTEN | GraphicBuffer::USAGE_HW_RENDER |
            GraphicBuffer::USAGE_HW_TEXTURE;
    ui::Dataspace mDataspace = ui::Dataspace::UNKNOWN;
    uint32_t mTransform = 0;
    nsecs_t mDequeueTimeout = -1;
    nsecs_t mLastDequeueStartTime = 0;
    nsecs_t mLastDequeueDuration = 0;
    nsecs_t mLastQueueDuration = 0;
    uint64_t mFrameNumber = 0;
    sp<GraphicBuffer> mLastQueued;
};

} // namespace android
