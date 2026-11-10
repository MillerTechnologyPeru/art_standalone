/*
 * Mead's gralloc for libui: graphics buffers in shared memory. See
 * MeadGralloc.h.
 */

#define LOG_TAG "MeadGralloc"

#include "MeadGralloc.h"

#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <mutex>
#include <unordered_map>

#include <android-base/stringprintf.h>
#include <cutils/native_handle.h>
#include <log/log.h>
#include <sync/sync.h>
#include <system/graphics.h>

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

extern "C" int memfd_create(const char* name, unsigned int flags);

namespace android {

namespace {

constexpr int kMagic = 0x4d454144;  // "MEAD"

// The ints after the memfd in a buffer handle.
enum : int {
    kIntMagic,
    kIntWidth,
    kIntHeight,
    kIntStride,
    kIntFormat,
    kIntLayerCount,
    kIntUsageLow,
    kIntUsageHigh,
    kIntSizeLow,
    kIntSizeHigh,
    kNumInts
};

struct Buffer {
    int fd;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int32_t format;
    uint32_t layerCount;
    uint64_t usage;
    size_t size;
};

bool describe(buffer_handle_t handle, Buffer* buffer) {
    if (handle == nullptr || handle->numFds != 1 || handle->numInts != kNumInts) return false;
    const int* ints = &handle->data[1];
    if (ints[kIntMagic] != kMagic) return false;
    buffer->fd = handle->data[0];
    buffer->width = static_cast<uint32_t>(ints[kIntWidth]);
    buffer->height = static_cast<uint32_t>(ints[kIntHeight]);
    buffer->stride = static_cast<uint32_t>(ints[kIntStride]);
    buffer->format = ints[kIntFormat];
    buffer->layerCount = static_cast<uint32_t>(ints[kIntLayerCount]);
    buffer->usage = static_cast<uint32_t>(ints[kIntUsageLow]) |
            static_cast<uint64_t>(static_cast<uint32_t>(ints[kIntUsageHigh])) << 32;
    buffer->size = static_cast<uint32_t>(ints[kIntSizeLow]) |
            static_cast<uint64_t>(static_cast<uint32_t>(ints[kIntSizeHigh])) << 32;
    return true;
}

// Bytes per pixel of the single-plane formats, 0 for the others (with more
// formats than libui's bytesPerPixel).
int32_t pixelBytes(int32_t format) {
    switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
        case HAL_PIXEL_FORMAT_RGBA_1010102:
            return 4;
        case HAL_PIXEL_FORMAT_RGB_888:
            return 3;
        case HAL_PIXEL_FORMAT_RGB_565:
        case HAL_PIXEL_FORMAT_RAW16:
        case HAL_PIXEL_FORMAT_Y16:
            return 2;
        case HAL_PIXEL_FORMAT_RGBA_FP16:
            return 8;
        case HAL_PIXEL_FORMAT_Y8:
        case HAL_PIXEL_FORMAT_BLOB:
        case 0x38:  // R_8
            return 1;
        default:
            return 0;
    }
}

bool isYuv420(int32_t format) {
    return format == HAL_PIXEL_FORMAT_YCBCR_420_888 || format == HAL_PIXEL_FORMAT_YV12 ||
            format == HAL_PIXEL_FORMAT_YCRCB_420_SP;
}

// Rows of single-plane formats start at 64-byte boundaries where the stride
// can be kept whole in pixels.
uint32_t strideFor(uint32_t width, int32_t format) {
    int32_t bpp = pixelBytes(format);
    if (format == HAL_PIXEL_FORMAT_BLOB || bpp == 0 || bpp == 3) {
        return isYuv420(format) ? (width + 15) & ~15u : width;
    }
    uint32_t pixelsPer64 = 64 / static_cast<uint32_t>(bpp);
    return (width + pixelsPer64 - 1) / pixelsPer64 * pixelsPer64;
}

size_t sizeFor(uint32_t stride, uint32_t height, int32_t format, uint32_t layerCount) {
    size_t layer;
    if (isYuv420(format)) {
        layer = static_cast<size_t>(stride) * height * 3 / 2;
    } else {
        layer = static_cast<size_t>(stride) * height * static_cast<size_t>(pixelBytes(format));
    }
    return layer * (layerCount ? layerCount : 1);
}

// Mappings, by the memfd's handle, from first lock until free.
std::mutex gMappingsLock;
std::unordered_map<buffer_handle_t, void*> gMappings;

void* map(buffer_handle_t handle, const Buffer& buffer) {
    std::lock_guard<std::mutex> lock(gMappingsLock);
    auto found = gMappings.find(handle);
    if (found != gMappings.end()) return found->second;
    void* data = mmap(nullptr, buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, buffer.fd, 0);
    if (data == MAP_FAILED) {
        ALOGE("mmap of a %zu-byte buffer failed: %s", buffer.size, strerror(errno));
        return nullptr;
    }
    gMappings.emplace(handle, data);
    return data;
}

void waitAndClose(int fence) {
    if (fence >= 0) {
        sync_wait(fence, -1);
        close(fence);
    }
}

} // namespace

status_t MeadGrallocMapper::createDescriptor(void*, void*) const {
    return INVALID_OPERATION;
}

status_t MeadGrallocMapper::importBuffer(const hardware::hidl_handle& rawHandle,
                                         buffer_handle_t* outBufferHandle) const {
    Buffer buffer;
    const native_handle_t* raw = rawHandle.getNativeHandle();
    if (!describe(raw, &buffer)) return BAD_VALUE;
    native_handle_t* imported = native_handle_clone(raw);
    if (imported == nullptr) return NO_MEMORY;
    *outBufferHandle = imported;
    return NO_ERROR;
}

void MeadGrallocMapper::freeBuffer(buffer_handle_t bufferHandle) const {
    Buffer buffer;
    if (!describe(bufferHandle, &buffer)) return;
    {
        std::lock_guard<std::mutex> lock(gMappingsLock);
        auto found = gMappings.find(bufferHandle);
        if (found != gMappings.end()) {
            munmap(found->second, buffer.size);
            gMappings.erase(found);
        }
    }
    native_handle_t* handle = const_cast<native_handle_t*>(bufferHandle);
    native_handle_close(handle);
    native_handle_delete(handle);
}

status_t MeadGrallocMapper::validateBufferSize(buffer_handle_t bufferHandle, uint32_t width,
                                               uint32_t height, android::PixelFormat format,
                                               uint32_t layerCount, uint64_t,
                                               uint32_t stride) const {
    Buffer buffer;
    if (!describe(bufferHandle, &buffer)) return BAD_VALUE;
    if (width > buffer.width || height > buffer.height || format != buffer.format ||
        layerCount > buffer.layerCount || stride != buffer.stride) {
        return BAD_VALUE;
    }
    return NO_ERROR;
}

void MeadGrallocMapper::getTransportSize(buffer_handle_t bufferHandle, uint32_t* outNumFds,
                                         uint32_t* outNumInts) const {
    *outNumFds = static_cast<uint32_t>(bufferHandle->numFds);
    *outNumInts = static_cast<uint32_t>(bufferHandle->numInts);
}

status_t MeadGrallocMapper::lock(buffer_handle_t bufferHandle, uint64_t, const Rect&,
                                 int acquireFence, void** outData, int32_t* outBytesPerPixel,
                                 int32_t* outBytesPerStride) const {
    waitAndClose(acquireFence);
    Buffer buffer;
    if (!describe(bufferHandle, &buffer)) return BAD_VALUE;
    void* data = map(bufferHandle, buffer);
    if (data == nullptr) return NO_MEMORY;
    int32_t bpp = pixelBytes(buffer.format);
    *outData = data;
    if (outBytesPerPixel) *outBytesPerPixel = bpp ? bpp : -1;
    if (outBytesPerStride) {
        *outBytesPerStride = bpp ? static_cast<int32_t>(buffer.stride) * bpp : -1;
    }
    return NO_ERROR;
}

status_t MeadGrallocMapper::lock(buffer_handle_t bufferHandle, uint64_t, const Rect&,
                                 int acquireFence, android_ycbcr* ycbcr) const {
    waitAndClose(acquireFence);
    Buffer buffer;
    if (!describe(bufferHandle, &buffer)) return BAD_VALUE;
    if (!isYuv420(buffer.format)) return BAD_VALUE;
    auto* data = static_cast<uint8_t*>(map(bufferHandle, buffer));
    if (data == nullptr) return NO_MEMORY;
    size_t luma = static_cast<size_t>(buffer.stride) * buffer.height;
    memset(ycbcr, 0, sizeof(*ycbcr));
    ycbcr->y = data;
    ycbcr->ystride = buffer.stride;
    if (buffer.format == HAL_PIXEL_FORMAT_YCRCB_420_SP) {
        // NV21: interleaved V then U.
        ycbcr->cr = data + luma;
        ycbcr->cb = data + luma + 1;
        ycbcr->cstride = buffer.stride;
        ycbcr->chroma_step = 2;
    } else {
        // I420 for YCbCr_420_888; YV12 has V before U.
        size_t chroma = luma / 4;
        uint8_t* first = data + luma;
        uint8_t* second = first + chroma;
        bool yv12 = buffer.format == HAL_PIXEL_FORMAT_YV12;
        ycbcr->cb = yv12 ? second : first;
        ycbcr->cr = yv12 ? first : second;
        ycbcr->cstride = buffer.stride / 2;
        ycbcr->chroma_step = 1;
    }
    return NO_ERROR;
}

int MeadGrallocMapper::unlock(buffer_handle_t) const {
    // Writes through the shared mapping are visible at once; no fence.
    return -1;
}

status_t MeadGrallocMapper::isSupported(uint32_t width, uint32_t height,
                                        android::PixelFormat format, uint32_t layerCount,
                                        uint64_t, bool* outSupported) const {
    *outSupported = width > 0 && height > 0 && layerCount > 0 &&
            (pixelBytes(format) != 0 || isYuv420(format));
    return NO_ERROR;
}

std::string MeadGrallocAllocator::dumpDebugInfo() const {
    std::lock_guard<std::mutex> lock(gMappingsLock);
    return base::StringPrintf("Mead gralloc: %zu buffers mapped\n", gMappings.size());
}

status_t MeadGrallocAllocator::allocate(uint32_t width, uint32_t height, PixelFormat format,
                                        uint32_t layerCount, uint64_t usage, uint32_t bufferCount,
                                        uint32_t* outStride,
                                        buffer_handle_t* outBufferHandles) const {
    if (width == 0 || height == 0 || (pixelBytes(format) == 0 && !isYuv420(format))) {
        return BAD_VALUE;
    }
    uint32_t stride = strideFor(width, format);
    size_t size = sizeFor(stride, height, format, layerCount);
    for (uint32_t i = 0; i < bufferCount; i++) {
        int fd = memfd_create("mead-gralloc", MFD_CLOEXEC);
        if (fd < 0 || ftruncate(fd, static_cast<off_t>(size)) != 0) {
            ALOGE("Allocating a %zu-byte buffer failed: %s", size, strerror(errno));
            if (fd >= 0) close(fd);
            for (uint32_t j = 0; j < i; j++) MeadGrallocMapper().freeBuffer(outBufferHandles[j]);
            return NO_MEMORY;
        }
        native_handle_t* handle = native_handle_create(1, kNumInts);
        handle->data[0] = fd;
        int* ints = &handle->data[1];
        ints[kIntMagic] = kMagic;
        ints[kIntWidth] = static_cast<int>(width);
        ints[kIntHeight] = static_cast<int>(height);
        ints[kIntStride] = static_cast<int>(stride);
        ints[kIntFormat] = format;
        ints[kIntLayerCount] = static_cast<int>(layerCount);
        ints[kIntUsageLow] = static_cast<int>(usage & 0xffffffff);
        ints[kIntUsageHigh] = static_cast<int>(usage >> 32);
        ints[kIntSizeLow] = static_cast<int>(size & 0xffffffff);
        ints[kIntSizeHigh] = static_cast<int>(static_cast<uint64_t>(size) >> 32);
        outBufferHandles[i] = handle;
    }
    *outStride = stride;
    return NO_ERROR;
}

} // namespace android
