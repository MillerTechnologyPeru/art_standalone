/*
 * android.media's JNI library for Mead: MediaPlayer and MediaExtractor on
 * the host's FFmpeg, in place of Android's media server.
 *
 * A MediaPlayer demuxes on one thread into two packet queues; one thread
 * decodes the audio and writes it to an android.media.AudioTrack (Mead's
 * audio), another decodes the video, converts it to RGBA and posts each
 * frame to the player's Surface (a window's, or a SurfaceTexture's) when its
 * time comes, by a clock that runs while playing. Its Java listeners hear
 * what Android's would: the video size, prepared, seek complete, completion
 * and errors. Data comes from a path or a file descriptor's range (a file
 * stored in an OBB or APK).
 *
 * MediaExtractor gives the tracks' formats. What isn't here (DRM, metadata,
 * playback params, reading samples, MediaDataSource) stays Mead's stubs,
 * registered before this library loads.
 */

#define LOG_TAG "MeadMedia"

#include "jni.h"
#include <nativehelper/JNIHelp.h>

#include <android/native_window.h>
#include <gui/Surface.h>
#include <system/window.h>
#include <android_runtime/android_view_Surface.h>
#include <binder/Parcel.h>
#include <utils/Log.h>

#include "android_os_Parcel.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <errno.h>
#include <mutex>
#include <string>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>

using namespace android;

namespace {

JavaVM* gVM;

struct {
    jfieldID context;
    jmethodID postEvent;
    jclass clazz;
} gPlayer;

struct {
    jfieldID context;
} gExtractor;

struct {
    jclass clazz;
    jmethodID ctor;
    jmethodID play;
    jmethodID pause;
    jmethodID stop;
    jmethodID release;
    jmethodID flush;
    jmethodID write;
    jmethodID setVolume;
    jmethodID minBufferSize;
} gAudioTrack;

enum {
    MEDIA_PREPARED = 1,
    MEDIA_PLAYBACK_COMPLETE = 2,
    MEDIA_SEEK_COMPLETE = 4,
    MEDIA_SET_VIDEO_SIZE = 5,
    MEDIA_ERROR = 100,
    MEDIA_ERROR_UNKNOWN = 1,
    MEDIA_ERROR_IO = -1004,
};

enum { TRACK_TYPE_VIDEO = 1, TRACK_TYPE_AUDIO = 2 };
enum { INVOKE_ID_GET_TRACK_INFO = 1, INVOKE_ID_GET_SELECTED_TRACK = 7 };

int64_t nowNanos() {
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return int64_t(time.tv_sec) * 1000000000LL + time.tv_nsec;
}

/* The calling thread's JNIEnv, attaching it for its lifetime if it's new. */
JNIEnv* attach(const char* name, bool* attached) {
    JNIEnv* env = nullptr;
    *attached = false;
    if (gVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) return env;
    JavaVMAttachArgs args = {JNI_VERSION_1_6, name, nullptr};
    if (gVM->AttachCurrentThread(&env, &args) != JNI_OK) return nullptr;
    *attached = true;
    return env;
}

void clearException(JNIEnv* env) {
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

const char* mimeFor(AVCodecID id) {
    switch (id) {
        case AV_CODEC_ID_H264: return "video/avc";
        case AV_CODEC_ID_HEVC: return "video/hevc";
        case AV_CODEC_ID_VP8: return "video/x-vnd.on2.vp8";
        case AV_CODEC_ID_VP9: return "video/x-vnd.on2.vp9";
        case AV_CODEC_ID_AV1: return "video/av01";
        case AV_CODEC_ID_MPEG4: return "video/mp4v-es";
        case AV_CODEC_ID_H263: return "video/3gpp";
        case AV_CODEC_ID_MPEG2VIDEO: return "video/mpeg2";
        case AV_CODEC_ID_AAC: return "audio/mp4a-latm";
        case AV_CODEC_ID_MP3: return "audio/mpeg";
        case AV_CODEC_ID_VORBIS: return "audio/vorbis";
        case AV_CODEC_ID_OPUS: return "audio/opus";
        case AV_CODEC_ID_FLAC: return "audio/flac";
        case AV_CODEC_ID_AC3: return "audio/ac3";
        case AV_CODEC_ID_EAC3: return "audio/eac3";
        case AV_CODEC_ID_PCM_S16LE: return "audio/raw";
        default: return "application/octet-stream";
    }
}

/* A media file: a path, or a file descriptor's range read with pread. */
struct Source {
    std::string path;
    int fd = -1;
    int64_t offset = 0;
    int64_t length = 0;
    int64_t position = 0;
    AVFormatContext* format = nullptr;
    AVIOContext* io = nullptr;

    ~Source() { close(); }

    void close() {
        if (format != nullptr) avformat_close_input(&format);
        if (io != nullptr) {
            av_freep(&io->buffer);
            avio_context_free(&io);
        }
        if (fd >= 0) ::close(fd);
        fd = -1;
    }

    static int read(void* opaque, uint8_t* buffer, int size) {
        Source* source = static_cast<Source*>(opaque);
        int64_t left = source->length - source->position;
        if (left <= 0) return AVERROR_EOF;
        if (size > left) size = int(left);
        ssize_t got = pread(source->fd, buffer, size, source->offset + source->position);
        if (got < 0) return AVERROR(errno);
        if (got == 0) return AVERROR_EOF;
        source->position += got;
        return int(got);
    }

    static int64_t seek(void* opaque, int64_t position, int whence) {
        Source* source = static_cast<Source*>(opaque);
        switch (whence & ~AVSEEK_FORCE) {
            case AVSEEK_SIZE: return source->length;
            case SEEK_SET: source->position = position; break;
            case SEEK_CUR: source->position += position; break;
            case SEEK_END: source->position = source->length + position; break;
            default: return -1;
        }
        return source->position;
    }

    /* Opens and probes it; false with a reason on failure. */
    bool open(std::string* error) {
        format = avformat_alloc_context();
        const char* url = nullptr;
        if (fd >= 0) {
            if (length <= 0) {
                off_t end = lseek(fd, 0, SEEK_END);
                length = end > offset ? end - offset : 0;
            }
            const int size = 64 * 1024;
            uint8_t* buffer = static_cast<uint8_t*>(av_malloc(size));
            io = avio_alloc_context(buffer, size, 0, this, read, nullptr, seek);
            format->pb = io;
            format->flags |= AVFMT_FLAG_CUSTOM_IO;
        } else {
            url = path.c_str();
        }
        int result = avformat_open_input(&format, url, nullptr, nullptr);
        if (result < 0) {
            char reason[128];
            av_strerror(result, reason, sizeof(reason));
            *error = std::string("can't open: ") + reason;
            return false;
        }
        result = avformat_find_stream_info(format, nullptr);
        if (result < 0) {
            *error = "can't find its streams";
            return false;
        }
        return true;
    }
};

/* Takes a data source from Java: a path, or a FileDescriptor's range. */
bool setSource(JNIEnv* env, Source* source, jobject fileDescriptor, jlong offset, jlong length,
               jstring path) {
    if (path != nullptr) {
        const char* chars = env->GetStringUTFChars(path, nullptr);
        std::string text = chars != nullptr ? chars : "";
        env->ReleaseStringUTFChars(path, chars);
        if (text.rfind("file://", 0) == 0) text = text.substr(7);
        if (text.empty()) {
            jniThrowException(env, "java/lang/IllegalArgumentException", "empty path");
            return false;
        }
        source->path = text;
        return true;
    }
    int fd = jniGetFDFromFileDescriptor(env, fileDescriptor);
    if (fd < 0) {
        jniThrowException(env, "java/lang/IllegalArgumentException", "invalid file descriptor");
        return false;
    }
    source->fd = dup(fd);
    source->offset = offset;
    source->length = length;
    return source->fd >= 0;
}

/* Packets for one decoder, bounded so the demuxer waits for it. */
struct PacketQueue {
    std::mutex lock;
    std::condition_variable changed;
    std::deque<AVPacket*> packets;
    bool ended = false;  // no more after these

    void clear() {
        std::lock_guard<std::mutex> guard(lock);
        for (AVPacket* packet : packets) av_packet_free(&packet);
        packets.clear();
        ended = false;
        changed.notify_all();
    }
};

class Player {
  public:
    explicit Player(JNIEnv* env, jobject weakThis) { mWeakThis = env->NewGlobalRef(weakThis); }

    ~Player() {
        reset(nullptr);
        bool attached;
        JNIEnv* env = attach("MediaPlayer", &attached);
        if (env != nullptr && mWeakThis != nullptr) env->DeleteGlobalRef(mWeakThis);
        if (attached) gVM->DetachCurrentThread();
    }

    Source& source() { return mSource; }

    bool prepare(JNIEnv* env) {
        std::string error;
        if (!mSource.open(&error)) {
            ALOGE("MediaPlayer: %s", error.c_str());
            jniThrowException(env, "java/io/IOException", error.c_str());
            return false;
        }
        AVFormatContext* format = mSource.format;
        mVideoStream = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        mAudioStream = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (mVideoStream >= 0 && !openDecoder(mVideoStream, &mVideoCodec)) mVideoStream = -1;
        if (mAudioStream >= 0 && !openDecoder(mAudioStream, &mAudioCodec)) mAudioStream = -1;
        if (mVideoStream < 0 && mAudioStream < 0) {
            jniThrowException(env, "java/io/IOException", "nothing it can decode");
            return false;
        }
        mDurationMs = format->duration > 0 ? format->duration / 1000 : 0;
        if (mVideoStream >= 0) {
            mWidth = mVideoCodec->width;
            mHeight = mVideoCodec->height;
        }
        mPrepared = true;
        ALOGI("MediaPlayer prepared %s: %s%s %dx%d, %lld ms", mSource.format->iformat->name,
              mVideoStream >= 0 ? avcodec_get_name(mVideoCodec->codec_id) : "",
              mAudioStream >= 0 ? (std::string(" ") + avcodec_get_name(mAudioCodec->codec_id)).c_str() : "",
              mWidth, mHeight, (long long)mDurationMs);
        post(env, MEDIA_SET_VIDEO_SIZE, mWidth, mHeight);
        post(env, MEDIA_PREPARED, 0, 0);
        return true;
    }

    /* prepare() on a thread of its own: PREPARED, or an error, comes as an
     * event, as Android's prepareAsync does. */
    void prepareAsync() {
        joinPreparer();
        mPreparer = std::thread([this] {
            bool attached;
            JNIEnv* env = attach("MediaPlayerPrepare", &attached);
            if (env == nullptr) return;
            if (!prepare(env)) {
                clearException(env);
                post(env, MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, MEDIA_ERROR_IO);
            }
            if (attached) gVM->DetachCurrentThread();
        });
    }

    void setSurface(JNIEnv* env, jobject surface) {
        std::lock_guard<std::mutex> guard(mWindowLock);
        mWindow = surface != nullptr ? android_view_Surface_getNativeWindow(env, surface) : nullptr;
        mWindowWidth = mWindowHeight = 0;
    }

    void start() {
        std::lock_guard<std::mutex> guard(mStateLock);
        if (!mPrepared) return;
        if (mPaused) {
            mPaused = false;
            mClockStart = nowNanos() - mPausedAt;
            mPauseChanged.notify_all();
            if (mTrack != nullptr) callTrack(gAudioTrack.play);
            return;
        }
        if (mCompleted) {
            mCompleted = false;
            if (mRunning) {
                seekLocked(0);  // from the start, as after completion on Android
                return;
            }
            mStartPositionNs = 0;
        }
        if (mRunning) return;
        ALOGI("MediaPlayer starts at %lld ms%s", (long long)(mStartPositionNs / 1000000),
              mWindow != nullptr ? "" : ", with no surface");
        startThreads(mStartPositionNs);
    }

    void pause() {
        std::lock_guard<std::mutex> guard(mStateLock);
        if (!mRunning || mPaused) return;
        mPaused = true;
        mPausedAt = nowNanos() - mClockStart;
        if (mTrack != nullptr) callTrack(gAudioTrack.pause);
    }

    void stop() {
        std::lock_guard<std::mutex> guard(mStateLock);
        stopThreads();
        mStartPositionNs = 0;
    }

    void seekTo(JNIEnv* env, int64_t ms) {
        {
            std::lock_guard<std::mutex> guard(mStateLock);
            seekLocked(ms * 1000000LL);
        }
        post(env, MEDIA_SEEK_COMPLETE, 0, 0);
    }

    void reset(JNIEnv*) {
        joinPreparer();
        std::lock_guard<std::mutex> guard(mStateLock);
        stopThreads();
        if (mVideoCodec != nullptr) avcodec_free_context(&mVideoCodec);
        if (mAudioCodec != nullptr) avcodec_free_context(&mAudioCodec);
        mSource.close();
        mSource = Source();
        mPrepared = false;
        mCompleted = false;
        mVideoStream = mAudioStream = -1;
        mWidth = mHeight = 0;
        mDurationMs = 0;
        mStartPositionNs = 0;
    }

    bool isPlaying() {
        std::lock_guard<std::mutex> guard(mStateLock);
        return mRunning && !mPaused && !mCompleted;
    }

    int currentPositionMs() {
        std::lock_guard<std::mutex> guard(mStateLock);
        int64_t position;
        if (!mRunning) position = mStartPositionNs;
        else if (mPaused) position = mPausedAt;
        else position = nowNanos() - mClockStart;
        if (mDurationMs > 0 && position / 1000000 > mDurationMs) return int(mDurationMs);
        return int(position / 1000000);
    }

    int durationMs() const { return int(mDurationMs); }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    void setLooping(bool looping) { mLooping = looping; }
    bool isLooping() const { return mLooping; }

    void setVolume(float left, float right) {
        std::lock_guard<std::mutex> guard(mStateLock);
        mVolume = left > right ? left : right;
        if (mTrack != nullptr) {
            bool attached;
            JNIEnv* env = attach("MediaPlayer", &attached);
            if (env != nullptr) {
                env->CallIntMethod(mTrack, gAudioTrack.setVolume, mVolume);
                clearException(env);
            }
            if (attached) gVM->DetachCurrentThread();
        }
    }

    /* MediaPlayer.invoke's requests: the tracks, and which is selected. */
    int invoke(JNIEnv* env, jobject requestObject, jobject replyObject) {
        Parcel* request = parcelForJavaObject(env, requestObject);
        Parcel* reply = parcelForJavaObject(env, replyObject);
        if (request == nullptr || reply == nullptr || request->dataSize() < 4) return -EINVAL;
        // The request's last int is its id, after the interface token.
        request->setDataPosition(request->dataSize() - 4);
        int32_t id = request->readInt32();
        if (id == INVOKE_ID_GET_TRACK_INFO) {
            std::vector<int> streams;
            if (mVideoStream >= 0) streams.push_back(mVideoStream);
            if (mAudioStream >= 0) streams.push_back(mAudioStream);
            reply->writeInt32(int32_t(streams.size()));
            for (int index : streams) {
                AVStream* stream = mSource.format->streams[index];
                AVDictionaryEntry* language = av_dict_get(stream->metadata, "language", nullptr, 0);
                reply->writeInt32(1);  // present, for createTypedArray
                reply->writeInt32(index == mVideoStream ? TRACK_TYPE_VIDEO : TRACK_TYPE_AUDIO);
                reply->writeString16(String16(mimeFor(stream->codecpar->codec_id)));
                reply->writeString16(String16(language != nullptr ? language->value : "und"));
            }
            return 0;
        }
        if (id == INVOKE_ID_GET_SELECTED_TRACK) {
            reply->writeInt32(-1);
            return 0;
        }
        return 0;
    }

  private:
    bool openDecoder(int index, AVCodecContext** context) {
        AVStream* stream = mSource.format->streams[index];
        const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (codec == nullptr) {
            ALOGW("MediaPlayer: no decoder for %s", avcodec_get_name(stream->codecpar->codec_id));
            return false;
        }
        *context = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(*context, stream->codecpar);
        (*context)->pkt_timebase = stream->time_base;
        (*context)->thread_count = 0;
        if (avcodec_open2(*context, codec, nullptr) < 0) {
            avcodec_free_context(context);
            return false;
        }
        return true;
    }

    void post(JNIEnv* env, int what, int arg1, int arg2) {
        if (mWeakThis == nullptr) return;
        env->CallStaticVoidMethod(gPlayer.clazz, gPlayer.postEvent, mWeakThis, what, arg1, arg2, nullptr);
        clearException(env);
    }

    void callTrack(jmethodID method) {
        bool attached;
        JNIEnv* env = attach("MediaPlayer", &attached);
        if (env != nullptr) {
            env->CallVoidMethod(mTrack, method);
            clearException(env);
        }
        if (attached) gVM->DetachCurrentThread();
    }

    /* With the state lock held. */
    void seekLocked(int64_t positionNs) {
        bool wasRunning = mRunning;
        bool wasPaused = mPaused;
        stopThreads();
        if (!mPrepared) return;
        int64_t target = positionNs / 1000;  // AV_TIME_BASE is microseconds
        avformat_seek_file(mSource.format, -1, INT64_MIN, target, target, 0);
        if (mVideoCodec != nullptr) avcodec_flush_buffers(mVideoCodec);
        if (mAudioCodec != nullptr) avcodec_flush_buffers(mAudioCodec);
        mStartPositionNs = positionNs;
        mCompleted = false;
        if (wasRunning) {
            startThreads(positionNs);
            if (wasPaused) {
                mPaused = true;
                mPausedAt = positionNs;
                if (mTrack != nullptr) callTrack(gAudioTrack.pause);
            }
        }
    }

    void startThreads(int64_t positionNs) {
        mStopping = false;
        mPaused = false;
        mRunning = true;
        mVideoQueue.clear();
        mAudioQueue.clear();
        mSkipBeforeNs = positionNs;
        mClockStart = nowNanos() - positionNs;
        mWorkersLeft = (mVideoStream >= 0 ? 1 : 0) + (mAudioStream >= 0 ? 1 : 0);
        mDemuxer = std::thread(&Player::demux, this);
        if (mVideoStream >= 0) mVideoThread = std::thread(&Player::decodeVideo, this);
        if (mAudioStream >= 0) mAudioThread = std::thread(&Player::decodeAudio, this);
    }

    /* With the state lock held: ends the threads, keeping the position. */
    void stopThreads() {
        if (!mRunning) return;
        mStartPositionNs = mPaused ? mPausedAt : nowNanos() - mClockStart;
        mStopping = true;
        mPaused = false;
        mPauseChanged.notify_all();
        mVideoQueue.changed.notify_all();
        mAudioQueue.changed.notify_all();
        // The threads take the state lock to report completion: let them.
        mStateLock.unlock();
        if (mDemuxer.joinable()) mDemuxer.join();
        if (mVideoThread.joinable()) mVideoThread.join();
        if (mAudioThread.joinable()) mAudioThread.join();
        mStateLock.lock();
        mVideoQueue.clear();
        mAudioQueue.clear();
        mRunning = false;
        if (mTrack != nullptr) {
            bool attached;
            JNIEnv* env = attach("MediaPlayer", &attached);
            if (env != nullptr) {
                env->CallVoidMethod(mTrack, gAudioTrack.stop);
                clearException(env);
                env->CallVoidMethod(mTrack, gAudioTrack.release);
                clearException(env);
                env->DeleteGlobalRef(mTrack);
            }
            if (attached) gVM->DetachCurrentThread();
            mTrack = nullptr;
        }
    }

    /* Waits while paused; false once stopping. */
    bool waitWhilePaused() {
        std::unique_lock<std::mutex> guard(mPauseLock);
        // Timed: pause and resume signal without this lock.
        while (mPaused && !mStopping) mPauseChanged.wait_for(guard, std::chrono::milliseconds(50));
        return !mStopping;
    }

    void push(PacketQueue& queue, AVPacket* packet) {
        std::unique_lock<std::mutex> guard(queue.lock);
        while (queue.packets.size() >= 96 && !mStopping) {
            queue.changed.wait_for(guard, std::chrono::milliseconds(50));
        }
        if (mStopping) {
            av_packet_free(&packet);
            return;
        }
        queue.packets.push_back(packet);
        queue.changed.notify_all();
    }

    /* The next packet, or null at the end or when stopping. */
    AVPacket* pop(PacketQueue& queue) {
        std::unique_lock<std::mutex> guard(queue.lock);
        while (queue.packets.empty() && !queue.ended && !mStopping) {
            queue.changed.wait_for(guard, std::chrono::milliseconds(50));
        }
        if (mStopping || queue.packets.empty()) return nullptr;
        AVPacket* packet = queue.packets.front();
        queue.packets.pop_front();
        queue.changed.notify_all();
        return packet;
    }

    void demux() {
        AVPacket* packet = av_packet_alloc();
        while (!mStopping) {
            int result = av_read_frame(mSource.format, packet);
            if (result < 0) break;
            if (packet->stream_index == mVideoStream) {
                push(mVideoQueue, av_packet_clone(packet));
            } else if (packet->stream_index == mAudioStream) {
                push(mAudioQueue, av_packet_clone(packet));
            }
            av_packet_unref(packet);
        }
        av_packet_free(&packet);
        for (PacketQueue* queue : {&mVideoQueue, &mAudioQueue}) {
            std::lock_guard<std::mutex> guard(queue->lock);
            queue->ended = true;
            queue->changed.notify_all();
        }
    }

    int64_t timeNs(AVFrame* frame, int stream) {
        int64_t pts = frame->best_effort_timestamp;
        if (pts == AV_NOPTS_VALUE) return -1;
        AVRational base = mSource.format->streams[stream]->time_base;
        int64_t start = mSource.format->streams[stream]->start_time;
        if (start != AV_NOPTS_VALUE) pts -= start;
        return av_rescale_q(pts, base, AVRational{1, 1000000000});
    }

    void decodeVideo() {
        bool attached;
        JNIEnv* env = attach("MediaPlayer video", &attached);
        AVFrame* frame = av_frame_alloc();
        SwsContext* scaler = nullptr;
        bool ended = false;
        while (!mStopping && !ended) {
            AVPacket* packet = pop(mVideoQueue);
            avcodec_send_packet(mVideoCodec, packet);  // null drains it
            if (packet == nullptr) ended = true;
            av_packet_free(&packet);
            while (!mStopping && avcodec_receive_frame(mVideoCodec, frame) == 0) {
                int64_t when = timeNs(frame, mVideoStream);
                if (when >= 0 && when + 50000000 < mSkipBeforeNs) {
                    av_frame_unref(frame);
                    continue;  // before a seek's target
                }
                if (!waitWhilePaused()) break;
                if (when >= 0) {
                    int64_t wait = when - (nowNanos() - mClockStart);
                    while (wait > 0 && !mStopping) {
                        int64_t step = wait < 20000000 ? wait : 20000000;
                        timespec sleep = {0, long(step)};
                        nanosleep(&sleep, nullptr);
                        if (!waitWhilePaused()) break;
                        wait = when - (nowNanos() - mClockStart);
                    }
                    // Far behind: skip showing it, to catch up.
                    if (wait < -100000000) {
                        av_frame_unref(frame);
                        continue;
                    }
                }
                if (!mStopping) show(frame, &scaler);
                av_frame_unref(frame);
            }
        }
        sws_freeContext(scaler);
        av_frame_free(&frame);
        finished(env);
        if (attached) gVM->DetachCurrentThread();
    }

    void show(AVFrame* frame, SwsContext** scaler) {
        std::lock_guard<std::mutex> guard(mWindowLock);
        if (mWindow == nullptr) return;
        ANativeWindow* window = mWindow.get();
        if (mWindowWidth != frame->width || mWindowHeight != frame->height) {
            ANativeWindow_setBuffersGeometry(window, frame->width, frame->height,
                                             WINDOW_FORMAT_RGBA_8888);
            mWindowWidth = frame->width;
            mWindowHeight = frame->height;
        }
        *scaler = sws_getCachedContext(*scaler, frame->width, frame->height,
                                       AVPixelFormat(frame->format), frame->width, frame->height,
                                       AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (*scaler == nullptr) return;
        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock(window, &buffer, nullptr) != 0) return;
        uint8_t* planes[4] = {static_cast<uint8_t*>(buffer.bits), nullptr, nullptr, nullptr};
        int strides[4] = {buffer.stride * 4, 0, 0, 0};
        sws_scale(*scaler, frame->data, frame->linesize, 0, frame->height, planes, strides);
        ANativeWindow_unlockAndPost(window);
    }

    void decodeAudio() {
        bool attached;
        JNIEnv* env = attach("MediaPlayer audio", &attached);
        AVFrame* frame = av_frame_alloc();
        SwrContext* resampler = nullptr;
        int rate = mAudioCodec->sample_rate > 0 ? mAudioCodec->sample_rate : 48000;
        if (rate > 48000) rate = 48000;
        AVChannelLayout stereo;
        av_channel_layout_default(&stereo, 2);
        if (env != nullptr) {
            jint minimum = env->CallStaticIntMethod(gAudioTrack.clazz, gAudioTrack.minBufferSize, rate,
                                                    12 /* CHANNEL_OUT_STEREO */, 2 /* PCM_16BIT */);
            clearException(env);
            jint size = minimum > 0 ? minimum * 2 : rate * 4 / 5;
            jobject track = env->NewObject(gAudioTrack.clazz, gAudioTrack.ctor, 3 /* STREAM_MUSIC */,
                                           rate, 12, 2, size, 1 /* MODE_STREAM */);
            clearException(env);
            if (track != nullptr) {
                std::lock_guard<std::mutex> guard(mStateLock);
                mTrack = env->NewGlobalRef(track);
                env->CallIntMethod(mTrack, gAudioTrack.setVolume, mVolume);
                clearException(env);
                if (!mPaused) env->CallVoidMethod(mTrack, gAudioTrack.play);
                clearException(env);
                env->DeleteLocalRef(track);
            }
        }
        std::vector<uint8_t> pcm;
        jbyteArray array = nullptr;
        jint arraySize = 0;
        bool ended = false;
        while (!mStopping && !ended) {
            AVPacket* packet = pop(mAudioQueue);
            avcodec_send_packet(mAudioCodec, packet);
            if (packet == nullptr) ended = true;
            av_packet_free(&packet);
            while (!mStopping && avcodec_receive_frame(mAudioCodec, frame) == 0) {
                int64_t when = timeNs(frame, mAudioStream);
                if (when >= 0 && when + 20000000 < mSkipBeforeNs) {
                    av_frame_unref(frame);
                    continue;
                }
                if (resampler == nullptr) {
                    swr_alloc_set_opts2(&resampler, &stereo, AV_SAMPLE_FMT_S16, rate, &frame->ch_layout,
                                        AVSampleFormat(frame->format), frame->sample_rate, 0, nullptr);
                    if (resampler == nullptr || swr_init(resampler) < 0) break;
                }
                int samples = swr_get_out_samples(resampler, frame->nb_samples);
                pcm.resize(size_t(samples) * 4);
                uint8_t* out = pcm.data();
                int converted = swr_convert(resampler, &out, samples,
                                            const_cast<const uint8_t**>(frame->extended_data),
                                            frame->nb_samples);
                av_frame_unref(frame);
                if (converted <= 0 || env == nullptr || mTrack == nullptr) continue;
                jint bytes = converted * 4;
                if (bytes > arraySize) {
                    if (array != nullptr) env->DeleteLocalRef(array);
                    array = env->NewByteArray(bytes);
                    arraySize = bytes;
                }
                env->SetByteArrayRegion(array, 0, bytes, reinterpret_cast<jbyte*>(pcm.data()));
                jint offset = 0;
                while (offset < bytes && !mStopping) {
                    if (!waitWhilePaused()) break;
                    jint wrote = env->CallIntMethod(mTrack, gAudioTrack.write, array, offset, bytes - offset);
                    clearException(env);
                    if (wrote <= 0) {
                        timespec sleep = {0, 5000000};
                        nanosleep(&sleep, nullptr);
                        continue;
                    }
                    offset += wrote;
                }
            }
        }
        if (array != nullptr && env != nullptr) env->DeleteLocalRef(array);
        swr_free(&resampler);
        av_frame_free(&frame);
        finished(env);
        if (attached) gVM->DetachCurrentThread();
    }

    /* A decoder reached the end (or was stopped): the last to finish
     * completes playback, or starts it over when looping. */
    void finished(JNIEnv* env) {
        if (mStopping) return;
        if (--mWorkersLeft > 0) return;
        if (mLooping) {
            std::thread([this] {
                std::lock_guard<std::mutex> guard(mStateLock);
                if (mRunning && !mStopping) seekLocked(0);
            }).detach();
            return;
        }
        {
            std::lock_guard<std::mutex> guard(mPauseLock);
            mCompleted = true;
        }
        if (env != nullptr) post(env, MEDIA_PLAYBACK_COMPLETE, 0, 0);
    }

    jobject mWeakThis = nullptr;
    Source mSource;
    AVCodecContext* mVideoCodec = nullptr;
    AVCodecContext* mAudioCodec = nullptr;
    int mVideoStream = -1, mAudioStream = -1;
    int mWidth = 0, mHeight = 0;
    int64_t mDurationMs = 0;
    bool mPrepared = false;
    std::atomic<bool> mCompleted{false};
    std::atomic<bool> mLooping{false};
    float mVolume = 1.0f;

    std::mutex mStateLock;
    bool mRunning = false;
    std::atomic<bool> mStopping{false};
    std::atomic<bool> mPaused{false};
    std::mutex mPauseLock;
    std::condition_variable mPauseChanged;
    int64_t mPausedAt = 0;
    std::atomic<int64_t> mClockStart{0};
    int64_t mStartPositionNs = 0;
    std::atomic<int64_t> mSkipBeforeNs{0};
    std::atomic<int> mWorkersLeft{0};
    std::thread mDemuxer, mVideoThread, mAudioThread;
    std::thread mPreparer;

    void joinPreparer() {
        if (mPreparer.joinable()) mPreparer.join();
    }
    PacketQueue mVideoQueue, mAudioQueue;
    jobject mTrack = nullptr;

    std::mutex mWindowLock;
    sp<ANativeWindow> mWindow;
    int mWindowWidth = 0, mWindowHeight = 0;
};

Player* player(JNIEnv* env, jobject thiz) {
    Player* found = reinterpret_cast<Player*>(env->GetLongField(thiz, gPlayer.context));
    if (found == nullptr) jniThrowException(env, "java/lang/IllegalStateException", "no player");
    return found;
}

// MediaPlayer's natives.

void MediaPlayer_init(JNIEnv*, jclass) {}

void MediaPlayer_setup(JNIEnv* env, jobject thiz, jobject weakThis) {
    ALOGI("MediaPlayer created");
    env->SetLongField(thiz, gPlayer.context, reinterpret_cast<jlong>(new Player(env, weakThis)));
}

void MediaPlayer_release(JNIEnv* env, jobject thiz) {
    Player* found = reinterpret_cast<Player*>(env->GetLongField(thiz, gPlayer.context));
    env->SetLongField(thiz, gPlayer.context, 0);
    delete found;
}

void MediaPlayer_finalize(JNIEnv* env, jobject thiz) { MediaPlayer_release(env, thiz); }

void MediaPlayer_setDataSourceFD(JNIEnv* env, jobject thiz, jobject fileDescriptor, jlong offset,
                                 jlong length) {
    ALOGI("MediaPlayer source: %lld bytes at %lld of a file", (long long)length, (long long)offset);
    if (Player* p = player(env, thiz)) setSource(env, &p->source(), fileDescriptor, offset, length, nullptr);
}

void MediaPlayer_setDataSourcePath(JNIEnv* env, jobject thiz, jobject, jstring path, jobjectArray,
                                   jobjectArray) {
    if (path != nullptr) {
        const char* chars = env->GetStringUTFChars(path, nullptr);
        ALOGI("MediaPlayer source: %s", chars);
        env->ReleaseStringUTFChars(path, chars);
    }
    if (Player* p = player(env, thiz)) setSource(env, &p->source(), nullptr, 0, 0, path);
}

void MediaPlayer_prepare(JNIEnv* env, jobject thiz) {
    if (Player* p = player(env, thiz)) p->prepare(env);
}

void MediaPlayer_prepareAsync(JNIEnv* env, jobject thiz) {
    if (Player* p = player(env, thiz)) p->prepareAsync();
}

void MediaPlayer_setVideoSurface(JNIEnv* env, jobject thiz, jobject surface) {
    if (Player* p = player(env, thiz)) p->setSurface(env, surface);
}

void MediaPlayer_start(JNIEnv* env, jobject thiz) {
    if (Player* p = player(env, thiz)) p->start();
}

void MediaPlayer_pause(JNIEnv* env, jobject thiz) {
    if (Player* p = player(env, thiz)) p->pause();
}

void MediaPlayer_stop(JNIEnv* env, jobject thiz) {
    if (Player* p = player(env, thiz)) p->stop();
}

void MediaPlayer_reset(JNIEnv* env, jobject thiz) {
    if (Player* p = player(env, thiz)) p->reset(env);
}

void MediaPlayer_seekTo(JNIEnv* env, jobject thiz, jlong ms, jint) {
    if (Player* p = player(env, thiz)) p->seekTo(env, ms);
}

jboolean MediaPlayer_isPlaying(JNIEnv* env, jobject thiz) {
    Player* p = player(env, thiz);
    return p != nullptr && p->isPlaying() ? JNI_TRUE : JNI_FALSE;
}

jint MediaPlayer_getCurrentPosition(JNIEnv* env, jobject thiz) {
    Player* p = player(env, thiz);
    return p != nullptr ? p->currentPositionMs() : 0;
}

jint MediaPlayer_getDuration(JNIEnv* env, jobject thiz) {
    Player* p = player(env, thiz);
    return p != nullptr ? p->durationMs() : 0;
}

jint MediaPlayer_getVideoWidth(JNIEnv* env, jobject thiz) {
    Player* p = player(env, thiz);
    return p != nullptr ? p->width() : 0;
}

jint MediaPlayer_getVideoHeight(JNIEnv* env, jobject thiz) {
    Player* p = player(env, thiz);
    return p != nullptr ? p->height() : 0;
}

void MediaPlayer_setLooping(JNIEnv* env, jobject thiz, jboolean looping) {
    if (Player* p = player(env, thiz)) p->setLooping(looping);
}

jboolean MediaPlayer_isLooping(JNIEnv* env, jobject thiz) {
    Player* p = player(env, thiz);
    return p != nullptr && p->isLooping() ? JNI_TRUE : JNI_FALSE;
}

void MediaPlayer_setVolume(JNIEnv* env, jobject thiz, jfloat left, jfloat right) {
    if (Player* p = player(env, thiz)) p->setVolume(left, right);
}

jint MediaPlayer_invoke(JNIEnv* env, jobject thiz, jobject request, jobject reply) {
    Player* p = player(env, thiz);
    return p != nullptr ? p->invoke(env, request, reply) : -EINVAL;
}

jint MediaPlayer_getAudioStreamType(JNIEnv*, jobject) { return 3; }

jint MediaPlayer_getAudioSessionId(JNIEnv*, jobject) { return 1; }

const JNINativeMethod gPlayerMethods[] = {
        {"native_init", "()V", (void*)MediaPlayer_init},
        {"native_setup", "(Ljava/lang/Object;)V", (void*)MediaPlayer_setup},
        {"native_finalize", "()V", (void*)MediaPlayer_finalize},
        {"_release", "()V", (void*)MediaPlayer_release},
        {"_setDataSource", "(Ljava/io/FileDescriptor;JJ)V", (void*)MediaPlayer_setDataSourceFD},
        {"nativeSetDataSource",
         "(Landroid/os/IBinder;Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)V",
         (void*)MediaPlayer_setDataSourcePath},
        {"_prepare", "()V", (void*)MediaPlayer_prepare},
        {"prepareAsync", "()V", (void*)MediaPlayer_prepareAsync},
        {"_setVideoSurface", "(Landroid/view/Surface;)V", (void*)MediaPlayer_setVideoSurface},
        {"_start", "()V", (void*)MediaPlayer_start},
        {"_pause", "()V", (void*)MediaPlayer_pause},
        {"_stop", "()V", (void*)MediaPlayer_stop},
        {"_reset", "()V", (void*)MediaPlayer_reset},
        {"_seekTo", "(JI)V", (void*)MediaPlayer_seekTo},
        {"isPlaying", "()Z", (void*)MediaPlayer_isPlaying},
        {"getCurrentPosition", "()I", (void*)MediaPlayer_getCurrentPosition},
        {"getDuration", "()I", (void*)MediaPlayer_getDuration},
        {"getVideoWidth", "()I", (void*)MediaPlayer_getVideoWidth},
        {"getVideoHeight", "()I", (void*)MediaPlayer_getVideoHeight},
        {"setLooping", "(Z)V", (void*)MediaPlayer_setLooping},
        {"isLooping", "()Z", (void*)MediaPlayer_isLooping},
        {"_setVolume", "(FF)V", (void*)MediaPlayer_setVolume},
        {"native_invoke", "(Landroid/os/Parcel;Landroid/os/Parcel;)I", (void*)MediaPlayer_invoke},
        {"_getAudioStreamType", "()I", (void*)MediaPlayer_getAudioStreamType},
        {"getAudioSessionId", "()I", (void*)MediaPlayer_getAudioSessionId},
};

// MediaExtractor's natives: the tracks' formats.

struct Extractor {
    Source source;
    std::vector<int> tracks;  // audio and video streams
};

Extractor* extractor(JNIEnv* env, jobject thiz) {
    Extractor* found = reinterpret_cast<Extractor*>(env->GetLongField(thiz, gExtractor.context));
    if (found == nullptr) jniThrowException(env, "java/lang/IllegalStateException", "no extractor");
    return found;
}

void openExtractor(JNIEnv* env, Extractor* e) {
    std::string error;
    if (!e->source.open(&error)) {
        jniThrowException(env, "java/io/IOException", error.c_str());
        return;
    }
    for (unsigned i = 0; i < e->source.format->nb_streams; i++) {
        AVMediaType type = e->source.format->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO || type == AVMEDIA_TYPE_AUDIO) e->tracks.push_back(int(i));
    }
}

void MediaExtractor_init(JNIEnv*, jclass) {}

void MediaExtractor_setup(JNIEnv* env, jobject thiz) {
    env->SetLongField(thiz, gExtractor.context, reinterpret_cast<jlong>(new Extractor()));
}

void MediaExtractor_release(JNIEnv* env, jobject thiz) {
    Extractor* found = reinterpret_cast<Extractor*>(env->GetLongField(thiz, gExtractor.context));
    env->SetLongField(thiz, gExtractor.context, 0);
    delete found;
}

void MediaExtractor_setDataSourceFD(JNIEnv* env, jobject thiz, jobject fileDescriptor, jlong offset,
                                    jlong length) {
    Extractor* e = extractor(env, thiz);
    if (e != nullptr && setSource(env, &e->source, fileDescriptor, offset, length, nullptr)) {
        openExtractor(env, e);
    }
}

void MediaExtractor_setDataSourcePath(JNIEnv* env, jobject thiz, jobject, jstring path, jobjectArray,
                                      jobjectArray) {
    Extractor* e = extractor(env, thiz);
    if (e != nullptr && setSource(env, &e->source, nullptr, 0, 0, path)) openExtractor(env, e);
}

jint MediaExtractor_getTrackCount(JNIEnv* env, jobject thiz) {
    Extractor* e = extractor(env, thiz);
    return e != nullptr ? jint(e->tracks.size()) : 0;
}

void put(JNIEnv* env, jobject map, jmethodID putMethod, const char* key, jobject value) {
    jstring name = env->NewStringUTF(key);
    jobject previous = env->CallObjectMethod(map, putMethod, name, value);
    if (previous != nullptr) env->DeleteLocalRef(previous);
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(value);
}

jobject boxInt(JNIEnv* env, int value) {
    jclass integer = env->FindClass("java/lang/Integer");
    jmethodID valueOf = env->GetStaticMethodID(integer, "valueOf", "(I)Ljava/lang/Integer;");
    jobject boxed = env->CallStaticObjectMethod(integer, valueOf, value);
    env->DeleteLocalRef(integer);
    return boxed;
}

jobject boxLong(JNIEnv* env, int64_t value) {
    jclass longClass = env->FindClass("java/lang/Long");
    jmethodID valueOf = env->GetStaticMethodID(longClass, "valueOf", "(J)Ljava/lang/Long;");
    jobject boxed = env->CallStaticObjectMethod(longClass, valueOf, jlong(value));
    env->DeleteLocalRef(longClass);
    return boxed;
}

jobject MediaExtractor_getTrackFormat(JNIEnv* env, jobject thiz, jint index) {
    Extractor* e = extractor(env, thiz);
    if (e == nullptr) return nullptr;
    if (index < 0 || size_t(index) >= e->tracks.size()) {
        jniThrowException(env, "java/lang/IllegalArgumentException", "no such track");
        return nullptr;
    }
    AVStream* stream = e->source.format->streams[e->tracks[index]];
    AVCodecParameters* codec = stream->codecpar;
    jclass hashMap = env->FindClass("java/util/HashMap");
    jmethodID ctor = env->GetMethodID(hashMap, "<init>", "()V");
    jmethodID putMethod = env->GetMethodID(hashMap, "put",
                                           "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject map = env->NewObject(hashMap, ctor);
    put(env, map, putMethod, "mime", env->NewStringUTF(mimeFor(codec->codec_id)));
    int64_t duration = stream->duration != AV_NOPTS_VALUE
            ? av_rescale_q(stream->duration, stream->time_base, AVRational{1, 1000000})
            : (e->source.format->duration > 0 ? e->source.format->duration : 0);
    if (duration > 0) put(env, map, putMethod, "durationUs", boxLong(env, duration));
    if (codec->codec_type == AVMEDIA_TYPE_VIDEO) {
        put(env, map, putMethod, "width", boxInt(env, codec->width));
        put(env, map, putMethod, "height", boxInt(env, codec->height));
        if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
            put(env, map, putMethod, "frame-rate",
                boxInt(env, int(av_q2d(stream->avg_frame_rate) + 0.5)));
        }
    } else {
        put(env, map, putMethod, "sample-rate", boxInt(env, codec->sample_rate));
        put(env, map, putMethod, "channel-count", boxInt(env, codec->ch_layout.nb_channels));
    }
    AVDictionaryEntry* language = av_dict_get(stream->metadata, "language", nullptr, 0);
    if (language != nullptr) put(env, map, putMethod, "language", env->NewStringUTF(language->value));
    env->DeleteLocalRef(hashMap);
    return map;
}

const JNINativeMethod gExtractorMethods[] = {
        {"native_init", "()V", (void*)MediaExtractor_init},
        {"native_setup", "()V", (void*)MediaExtractor_setup},
        {"native_finalize", "()V", (void*)MediaExtractor_release},
        {"release", "()V", (void*)MediaExtractor_release},
        {"setDataSource", "(Ljava/io/FileDescriptor;JJ)V", (void*)MediaExtractor_setDataSourceFD},
        {"nativeSetDataSource",
         "(Landroid/os/IBinder;Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)V",
         (void*)MediaExtractor_setDataSourcePath},
        {"getTrackCount", "()I", (void*)MediaExtractor_getTrackCount},
        {"getTrackFormatNative", "(I)Ljava/util/Map;", (void*)MediaExtractor_getTrackFormat},
};

bool registerClass(JNIEnv* env, const char* name, const JNINativeMethod* methods, int count) {
    jclass clazz = env->FindClass(name);
    if (clazz == nullptr) return false;
    bool ok = env->RegisterNatives(clazz, methods, count) == JNI_OK;
    env->DeleteLocalRef(clazz);
    return ok;
}

}  // namespace

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    gVM = vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    jclass player = env->FindClass("android/media/MediaPlayer");
    jclass extractor = env->FindClass("android/media/MediaExtractor");
    jclass track = env->FindClass("android/media/AudioTrack");
    if (player == nullptr || extractor == nullptr || track == nullptr) return JNI_ERR;
    gPlayer.clazz = static_cast<jclass>(env->NewGlobalRef(player));
    gPlayer.context = env->GetFieldID(player, "mNativeContext", "J");
    gPlayer.postEvent = env->GetStaticMethodID(player, "postEventFromNative",
                                               "(Ljava/lang/Object;IIILjava/lang/Object;)V");
    gExtractor.context = env->GetFieldID(extractor, "mNativeContext", "J");
    gAudioTrack.clazz = static_cast<jclass>(env->NewGlobalRef(track));
    gAudioTrack.ctor = env->GetMethodID(track, "<init>", "(IIIIII)V");
    gAudioTrack.play = env->GetMethodID(track, "play", "()V");
    gAudioTrack.pause = env->GetMethodID(track, "pause", "()V");
    gAudioTrack.stop = env->GetMethodID(track, "stop", "()V");
    gAudioTrack.release = env->GetMethodID(track, "release", "()V");
    gAudioTrack.flush = env->GetMethodID(track, "flush", "()V");
    gAudioTrack.write = env->GetMethodID(track, "write", "([BII)I");
    gAudioTrack.setVolume = env->GetMethodID(track, "setVolume", "(F)I");
    gAudioTrack.minBufferSize = env->GetStaticMethodID(track, "getMinBufferSize", "(III)I");
    if (env->ExceptionCheck() || gPlayer.context == nullptr || gPlayer.postEvent == nullptr ||
        gExtractor.context == nullptr || gAudioTrack.ctor == nullptr || gAudioTrack.write == nullptr) {
        clearException(env);
        return JNI_ERR;
    }
    if (!registerClass(env, "android/media/MediaPlayer", gPlayerMethods, NELEM(gPlayerMethods)) ||
        !registerClass(env, "android/media/MediaExtractor", gExtractorMethods,
                       NELEM(gExtractorMethods))) {
        clearException(env);
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}
