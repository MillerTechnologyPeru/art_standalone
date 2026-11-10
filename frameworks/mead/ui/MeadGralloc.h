/*
 * Mead's gralloc for libui: graphics buffers in shared memory (memfd), in
 * place of Android's gralloc HALs. GraphicBufferMapper and
 * GraphicBufferAllocator use these instead of Gralloc2 and Gralloc3.
 *
 * A buffer handle carries one file descriptor, the memfd, and the buffer's
 * description, so it can be passed to another process (a compositor) as it
 * is. Mapping happens on first lock and lasts until the buffer is freed.
 */

#pragma once

#include <ui/Gralloc.h>

namespace android {

class MeadGrallocMapper : public GrallocMapper {
public:
    bool isLoaded() const override { return true; }

    status_t createDescriptor(void* bufferDescriptorInfo,
                              void* outBufferDescriptor) const override;

    status_t importBuffer(const hardware::hidl_handle& rawHandle,
                          buffer_handle_t* outBufferHandle) const override;

    void freeBuffer(buffer_handle_t bufferHandle) const override;

    status_t validateBufferSize(buffer_handle_t bufferHandle, uint32_t width, uint32_t height,
                                android::PixelFormat format, uint32_t layerCount, uint64_t usage,
                                uint32_t stride) const override;

    void getTransportSize(buffer_handle_t bufferHandle, uint32_t* outNumFds,
                          uint32_t* outNumInts) const override;

    status_t lock(buffer_handle_t bufferHandle, uint64_t usage, const Rect& bounds,
                  int acquireFence, void** outData, int32_t* outBytesPerPixel,
                  int32_t* outBytesPerStride) const override;

    status_t lock(buffer_handle_t bufferHandle, uint64_t usage, const Rect& bounds,
                  int acquireFence, android_ycbcr* ycbcr) const override;

    int unlock(buffer_handle_t bufferHandle) const override;

    status_t isSupported(uint32_t width, uint32_t height, android::PixelFormat format,
                         uint32_t layerCount, uint64_t usage, bool* outSupported) const override;
};

class MeadGrallocAllocator : public GrallocAllocator {
public:
    bool isLoaded() const override { return true; }

    std::string dumpDebugInfo() const override;

    status_t allocate(uint32_t width, uint32_t height, PixelFormat format, uint32_t layerCount,
                      uint64_t usage, uint32_t bufferCount, uint32_t* outStride,
                      buffer_handle_t* outBufferHandles) const override;
};

} // namespace android
