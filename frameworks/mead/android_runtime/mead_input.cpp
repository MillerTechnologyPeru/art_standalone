/*
 * Mead's input channels for the display bridge. See mead/input.h.
 */

#define LOG_TAG "MeadInput"

#include <mead/input.h>

#include <mutex>

#include <input/Input.h>
#include <input/InputTransport.h>
#include <log/log.h>

#include "android_view_InputChannel.h"
#include "android_view_KeyCharacterMap.h"

#include <input/KeyCharacterMap.h>

namespace android {

struct MeadInputChannel {
    sp<InputChannel> channel;
    InputPublisher publisher;
    std::mutex mutex;
    uint32_t sequence = 0;

    explicit MeadInputChannel(const sp<InputChannel>& server)
          : channel(server), publisher(server) {}

    uint32_t nextSequence() {
        // The finished signals the app has sent back, which would otherwise
        // fill the channel; 0 is never a sequence number.
        uint32_t finished;
        bool handled;
        while (publisher.receiveFinishedSignal(&finished, &handled) == OK) {
        }
        if (++sequence == 0) sequence = 1;
        return sequence;
    }
};

} // namespace android

using android::MeadInputChannel;

struct MeadInput : MeadInputChannel {
    using MeadInputChannel::MeadInputChannel;
};

jobject mead_input_open(JNIEnv* env, const char* name, MeadInput** out) {
    *out = nullptr;
    jclass channelClass = env->FindClass("android/view/InputChannel");
    if (channelClass == nullptr) return nullptr;
    jmethodID openPair = env->GetStaticMethodID(channelClass, "openInputChannelPair",
                                                "(Ljava/lang/String;)[Landroid/view/InputChannel;");
    if (openPair == nullptr) return nullptr;
    jstring javaName = env->NewStringUTF(name);
    auto pair = static_cast<jobjectArray>(
            env->CallStaticObjectMethod(channelClass, openPair, javaName));
    env->DeleteLocalRef(javaName);
    env->DeleteLocalRef(channelClass);
    if (pair == nullptr || env->ExceptionCheck()) return nullptr;
    jobject server = env->GetObjectArrayElement(pair, 0);
    jobject client = env->GetObjectArrayElement(pair, 1);
    env->DeleteLocalRef(pair);
    android::sp<android::InputChannel> channel =
            android::android_view_InputChannel_getInputChannel(env, server);
    if (channel == nullptr) {
        env->DeleteLocalRef(server);
        env->DeleteLocalRef(client);
        return nullptr;
    }
    // The bridge holds the server end itself; Java's object for it is
    // disposed of, leaving the native channel open.
    jmethodID dispose = env->GetMethodID(env->GetObjectClass(server), "dispose", "()V");
    MeadInput* input = new MeadInput(channel);
    if (dispose != nullptr) env->CallVoidMethod(server, dispose);
    env->DeleteLocalRef(server);
    *out = input;
    return client;
}

jobject mead_input_load_key_character_map(JNIEnv* env, int32_t deviceId, const char* path) {
    android::sp<android::KeyCharacterMap> map;
    if (android::KeyCharacterMap::load(path, android::KeyCharacterMap::FORMAT_BASE, &map) !=
        android::OK) {
        ALOGE("Can't load the key character map %s", path);
        return nullptr;
    }
    return android::android_view_KeyCharacterMap_create(env, deviceId, map);
}

void mead_input_close(MeadInput* input) {
    delete input;
}

int mead_input_send_key(MeadInput* input, int32_t deviceId, int32_t source, int32_t action,
                        int32_t flags, int32_t keyCode, int32_t scanCode, int32_t metaState,
                        int32_t repeatCount, int64_t downTime, int64_t eventTime) {
    std::lock_guard<std::mutex> lock(input->mutex);
    return input->publisher.publishKeyEvent(input->nextSequence(), deviceId, source,
                                            ADISPLAY_ID_DEFAULT, action, flags, keyCode,
                                            scanCode, metaState, repeatCount, downTime, eventTime);
}

int mead_input_send_motion(MeadInput* input, int32_t deviceId, int32_t source, int32_t action,
                           int32_t actionButton, int32_t flags, int32_t metaState,
                           int32_t buttonState, int64_t downTime, int64_t eventTime,
                           uint32_t pointerCount, const int32_t* pointerIds,
                           const int32_t* toolTypes, uint64_t axisBits, const float* axisValues) {
    if (pointerCount == 0 || pointerCount > MAX_POINTERS) return android::BAD_VALUE;
    android::PointerProperties properties[MAX_POINTERS];
    android::PointerCoords coords[MAX_POINTERS];
    for (uint32_t i = 0; i < pointerCount; i++) {
        properties[i].clear();
        properties[i].id = pointerIds[i];
        properties[i].toolType = toolTypes[i];
        coords[i].clear();
        for (int axis = 0; axis < MEAD_INPUT_AXES; axis++) {
            if (axisBits & (uint64_t(1) << axis)) {
                coords[i].setAxisValue(axis, axisValues[i * MEAD_INPUT_AXES + axis]);
            }
        }
    }
    std::lock_guard<std::mutex> lock(input->mutex);
    return input->publisher.publishMotionEvent(
            input->nextSequence(), deviceId, source, ADISPLAY_ID_DEFAULT, action,
            actionButton, flags, AMOTION_EVENT_EDGE_FLAG_NONE, metaState, buttonState,
            android::MotionClassification::NONE, 0, 0, 1, 1, downTime, eventTime, pointerCount,
            properties, coords);
}
