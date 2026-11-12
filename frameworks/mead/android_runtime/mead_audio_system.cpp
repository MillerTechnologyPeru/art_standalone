/*
 * android.media.AudioSystem's natives for Mead: Android's
 * (android_media_AudioSystem.cpp) ask AudioFlinger and the audio policy
 * service over binder, which aren't here. Apps' audio goes through AAudio
 * and OpenSL ES to the host; what Java asks of the system is answered as a
 * device with one output, its speaker, at 48 kHz: AudioManager.getProperty
 * (the output's rate and buffer size, which engines may open their streams
 * at), session and player IDs, and no streams playing anywhere else. Its
 * audio ports and patches are listed as none (AudioManager.getDevices).
 * Setters succeed and change nothing; making patches, audio sources and
 * policy mixes aren't registered, and throw if called.
 */

#include <atomic>

#include <jni.h>
#include <nativehelper/JNIHelp.h>

namespace android {

namespace {

const jint kSuccess = 0;
const jint kOutputRate = 48000;
const jint kOutputFrames = 256;
const jint kOutputLatencyMs = 40;
const jint kDeviceOutSpeaker = 0x2;

std::atomic<jint> gNextId{1};

jint nativeGetFcc8(JNIEnv*, jclass) { return 8; }

jint succeed(JNIEnv*, jclass) { return kSuccess; }
jint succeedInt(JNIEnv*, jclass, jint) { return kSuccess; }
jint succeedIntInt(JNIEnv*, jclass, jint, jint) { return kSuccess; }
jint succeedIntIntInt(JNIEnv*, jclass, jint, jint, jint) { return kSuccess; }
jint succeedBoolean(JNIEnv*, jclass, jboolean) { return kSuccess; }
jint succeedFloat(JNIEnv*, jclass, jfloat) { return kSuccess; }
jint succeedString(JNIEnv*, jclass, jstring) { return kSuccess; }
jint succeedBooleanLong(JNIEnv*, jclass, jboolean, jlong) { return kSuccess; }
jint succeedIntArray(JNIEnv*, jclass, jintArray) { return kSuccess; }
jint succeedDeviceConnection(JNIEnv*, jclass, jint, jint, jstring, jstring, jint) { return kSuccess; }
jint succeedDeviceConfig(JNIEnv*, jclass, jint, jstring, jstring, jint) { return kSuccess; }
jint succeedAttributes(JNIEnv*, jclass, jobject, jint, jint) { return kSuccess; }
jint succeedList(JNIEnv*, jclass, jobject) { return kSuccess; }
jint succeedMap(JNIEnv*, jclass, jobject, jboolean) { return kSuccess; }

jboolean no(JNIEnv*, jclass) { return JNI_FALSE; }
jboolean noInt(JNIEnv*, jclass, jint) { return JNI_FALSE; }
jboolean noIntInt(JNIEnv*, jclass, jint, jint) { return JNI_FALSE; }
jboolean noOffload(JNIEnv*, jclass, jint, jint, jint, jint, jint) { return JNI_FALSE; }

jint newId(JNIEnv*, jclass) { return gNextId++; }

jstring getParameters(JNIEnv* env, jclass, jstring) { return env->NewStringUTF(""); }

// Not connected: no devices but the speaker.
jint getDeviceConnectionState(JNIEnv*, jclass, jint, jstring) { return 0; }
jint getForceUse(JNIEnv*, jclass, jint) { return 0; }
jint getDevicesForStream(JNIEnv*, jclass, jint) { return kDeviceOutSpeaker; }

jint getStreamVolumeIndex(JNIEnv*, jclass, jint, jint) { return 15; }
jint getVolumeIndexForAttributes(JNIEnv*, jclass, jobject, jint) { return 15; }
jint getMinVolumeIndexForAttributes(JNIEnv*, jclass, jobject) { return 0; }
jint getMaxVolumeIndexForAttributes(JNIEnv*, jclass, jobject) { return 15; }
jfloat getMasterVolume(JNIEnv*, jclass) { return 1.0f; }
jfloat getMasterBalance(JNIEnv*, jclass) { return 0.0f; }
jfloat getStreamVolumeDB(JNIEnv*, jclass, jint, jint, jint) { return 0.0f; }

jint getPrimaryOutputSamplingRate(JNIEnv*, jclass) { return kOutputRate; }
jint getPrimaryOutputFrameCount(JNIEnv*, jclass) { return kOutputFrames; }
jint getOutputLatency(JNIEnv*, jclass, jint) { return kOutputLatencyMs; }
jint getAudioHwSyncForSession(JNIEnv*, jclass, jint) { return 0; }

void registerCallback(JNIEnv*, jclass) {}

// No ports or patches, at generation 1 (AudioManager's cache waits for a
// generation both lists agree on).
jint listNone(JNIEnv* env, jclass, jobject, jintArray generation) {
    if (generation != nullptr && env->GetArrayLength(generation) > 0) {
        jint one = 1;
        env->SetIntArrayRegion(generation, 0, 1, &one);
    }
    return kSuccess;
}

const JNINativeMethod kAudioSystemMethods[] = {
    {"native_get_FCC_8", "()I", (void*)nativeGetFcc8},
    {"muteMicrophone", "(Z)I", (void*)succeedBoolean},
    {"isMicrophoneMuted", "()Z", (void*)no},
    {"isStreamActive", "(II)Z", (void*)noIntInt},
    {"isStreamActiveRemotely", "(II)Z", (void*)noIntInt},
    {"isSourceActive", "(I)Z", (void*)noInt},
    {"newAudioSessionId", "()I", (void*)newId},
    {"newAudioPlayerId", "()I", (void*)newId},
    {"newAudioRecorderId", "()I", (void*)newId},
    {"setParameters", "(Ljava/lang/String;)I", (void*)succeedString},
    {"getParameters", "(Ljava/lang/String;)Ljava/lang/String;", (void*)getParameters},
    {"setDeviceConnectionState", "(IILjava/lang/String;Ljava/lang/String;I)I", (void*)succeedDeviceConnection},
    {"getDeviceConnectionState", "(ILjava/lang/String;)I", (void*)getDeviceConnectionState},
    {"handleDeviceConfigChange", "(ILjava/lang/String;Ljava/lang/String;I)I", (void*)succeedDeviceConfig},
    {"setPhoneState", "(I)I", (void*)succeedInt},
    {"setForceUse", "(II)I", (void*)succeedIntInt},
    {"getForceUse", "(I)I", (void*)getForceUse},
    {"initStreamVolume", "(III)I", (void*)succeedIntIntInt},
    {"setStreamVolumeIndex", "(III)I", (void*)succeedIntIntInt},
    {"getStreamVolumeIndex", "(II)I", (void*)getStreamVolumeIndex},
    {"setVolumeIndexForAttributes", "(Landroid/media/AudioAttributes;II)I", (void*)succeedAttributes},
    {"getVolumeIndexForAttributes", "(Landroid/media/AudioAttributes;I)I", (void*)getVolumeIndexForAttributes},
    {"getMinVolumeIndexForAttributes", "(Landroid/media/AudioAttributes;)I", (void*)getMinVolumeIndexForAttributes},
    {"getMaxVolumeIndexForAttributes", "(Landroid/media/AudioAttributes;)I", (void*)getMaxVolumeIndexForAttributes},
    {"setMasterVolume", "(F)I", (void*)succeedFloat},
    {"getMasterVolume", "()F", (void*)getMasterVolume},
    {"setMasterMute", "(Z)I", (void*)succeedBoolean},
    {"getMasterMute", "()Z", (void*)no},
    {"getDevicesForStream", "(I)I", (void*)getDevicesForStream},
    {"getMasterMono", "()Z", (void*)no},
    {"setMasterMono", "(Z)I", (void*)succeedBoolean},
    {"setRttEnabled", "(Z)I", (void*)succeedBoolean},
    {"getMasterBalance", "()F", (void*)getMasterBalance},
    {"setMasterBalance", "(F)I", (void*)succeedFloat},
    {"getPrimaryOutputSamplingRate", "()I", (void*)getPrimaryOutputSamplingRate},
    {"getPrimaryOutputFrameCount", "()I", (void*)getPrimaryOutputFrameCount},
    {"getOutputLatency", "(I)I", (void*)getOutputLatency},
    {"setLowRamDevice", "(ZJ)I", (void*)succeedBooleanLong},
    {"checkAudioFlinger", "()I", (void*)succeed},
    {"listAudioPorts", "(Ljava/util/ArrayList;[I)I", (void*)listNone},
    {"listAudioPatches", "(Ljava/util/ArrayList;[I)I", (void*)listNone},
    {"native_register_dynamic_policy_callback", "()V", (void*)registerCallback},
    {"native_register_recording_callback", "()V", (void*)registerCallback},
    {"getAudioHwSyncForSession", "(I)I", (void*)getAudioHwSyncForSession},
    {"systemReady", "()I", (void*)succeed},
    {"getStreamVolumeDB", "(III)F", (void*)getStreamVolumeDB},
    {"setAllowedCapturePolicy", "(II)I", (void*)succeedIntInt},
    {"native_is_offload_supported", "(IIIII)Z", (void*)noOffload},
    {"getMicrophones", "(Ljava/util/ArrayList;)I", (void*)succeedList},
    {"getSurroundFormats", "(Ljava/util/Map;Z)I", (void*)succeedMap},
    {"getHwOffloadEncodingFormatsSupportedForA2DP", "(Ljava/util/ArrayList;)I", (void*)succeedList},
    {"setSurroundFormatEnabled", "(IZ)I", (void*)succeedIntInt},
    {"setAssistantUid", "(I)I", (void*)succeedInt},
    {"setA11yServicesUids", "([I)I", (void*)succeedIntArray},
    {"isHapticPlaybackSupported", "()Z", (void*)no},
};

// AudioManager's port and patch events: there are none.
void portEventSetup(JNIEnv*, jobject, jobject) {}
void portEventFinalize(JNIEnv*, jobject) {}

const JNINativeMethod kPortEventMethods[] = {
    {"native_setup", "(Ljava/lang/Object;)V", (void*)portEventSetup},
    {"native_finalize", "()V", (void*)portEventFinalize},
};

}  // namespace

int register_android_media_AudioSystem(JNIEnv* env) {
    int result = jniRegisterNativeMethods(env, "android/media/AudioSystem", kAudioSystemMethods,
                                          NELEM(kAudioSystemMethods));
    if (result < 0) return result;
    return jniRegisterNativeMethods(env, "android/media/AudioPortEventHandler", kPortEventMethods,
                                    NELEM(kPortEventMethods));
}

}  // namespace android
