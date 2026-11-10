/*
 * libandroid_runtime's entry for a runtime that isn't app_process.
 *
 * On Android, app_process starts the VM through AndroidRuntime, whose
 * startReg registers every framework JNI function before any Java runs.
 * Here the VM is already running (Mead's, or dalvikvm) when this library is
 * loaded, so JNI_OnLoad registers the framework's functions instead, and
 * this file defines the parts of AndroidRuntime that those functions use.
 * JNI_OnLoad also loads $ANDROID_ROOT/build.prop, as init does on Android.
 */

#define LOG_TAG "AndroidRuntime"

#include <android-base/file.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <log/log.h>
#include <nativehelper/JNIHelp.h>

#include <jni.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <string>

// What AndroidRuntime.h includes, first, so that only AndroidRuntime itself
// is opened up below.
#include <binder/IBinder.h>
#include <utils/Errors.h>
#include <utils/String16.h>
#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/threads.h>

// AndroidRuntime keeps the VM in a private static that app_process's
// AndroidRuntime::startVm sets; here JNI_OnLoad sets it.
#define private public
#include <android_runtime/AndroidRuntime.h>
#undef private

// android_util_Binder.cpp defines its register function outside the namespace.
extern int register_android_os_Binder(JNIEnv* env);

namespace android {

extern int register_android_util_Log(JNIEnv* env);
extern int register_android_os_SystemClock(JNIEnv* env);
extern int register_android_os_SystemProperties(JNIEnv* env);
extern int register_android_os_Trace(JNIEnv* env);
extern int register_android_os_Parcel(JNIEnv* env);
extern int register_android_content_AssetManager(JNIEnv* env);
extern int register_android_content_res_ApkAssets(JNIEnv* env);
extern int register_android_content_StringBlock(JNIEnv* env);
extern int register_android_content_XmlBlock(JNIEnv* env);

JavaVM* AndroidRuntime::mJavaVM = nullptr;

/*static*/ int AndroidRuntime::registerNativeMethods(JNIEnv* env, const char* className,
                                                    const JNINativeMethod* gMethods,
                                                    int numMethods) {
    return jniRegisterNativeMethods(env, className, gMethods, numMethods);
}

/*static*/ JNIEnv* AndroidRuntime::getJNIEnv() {
    JNIEnv* env = nullptr;
    if (mJavaVM == nullptr || mJavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_4) != JNI_OK) {
        return nullptr;
    }
    return env;
}

/*static*/ char* AndroidRuntime::toSlashClassName(const char* className) {
    char* result = strdup(className);
    for (char* cp = result; *cp != '\0'; cp++) {
        if (*cp == '.') {
            *cp = '/';
        }
    }
    return result;
}

/*static*/ jclass AndroidRuntime::findClass(JNIEnv* env, const char* className) {
    char* slashName = toSlashClassName(className);
    jclass type = env->FindClass(slashName);
    free(slashName);
    return type;
}

/*static*/ jstring AndroidRuntime::NewStringLatin1(JNIEnv* env, const char* bytes) {
    // Latin-1 is UTF-16 with the high byte zero.
    size_t length = strlen(bytes);
    jchar* chars = new jchar[length];
    for (size_t i = 0; i < length; i++) {
        chars[i] = static_cast<unsigned char>(bytes[i]);
    }
    jstring result = env->NewString(chars, static_cast<jsize>(length));
    delete[] chars;
    return result;
}

struct RegJNIRec {
    int (*mProc)(JNIEnv*);
    const char* mName;
};

#define REG_JNI(name) { name, #name }

// The framework's JNI that runs here so far, in AndroidRuntime.cpp's order.
static const RegJNIRec gRegJNI[] = {
    REG_JNI(register_android_os_SystemClock),
    REG_JNI(register_android_util_Log),
    REG_JNI(register_android_content_AssetManager),
    REG_JNI(register_android_content_StringBlock),
    REG_JNI(register_android_content_XmlBlock),
    REG_JNI(register_android_content_res_ApkAssets),
    REG_JNI(register_android_os_SystemProperties),
    REG_JNI(register_android_os_Binder),
    REG_JNI(register_android_os_Parcel),
    REG_JNI(register_android_os_Trace),
};

}  // namespace android

using namespace android;

// What init does on Android with /system/build.prop: every property in
// $ANDROID_ROOT/build.prop that isn't already set, is.
static void loadBuildProperties() {
    const char* root = getenv("ANDROID_ROOT");
    if (root == nullptr) {
        return;
    }
    std::string path = std::string(root) + "/build.prop";
    std::string contents;
    if (!android::base::ReadFileToString(path, &contents)) {
        ALOGW("libandroid_runtime: no %s", path.c_str());
        return;
    }
    for (const std::string& line : android::base::Split(contents, "\n")) {
        std::string entry = android::base::Trim(line);
        if (entry.empty() || entry[0] == '#') {
            continue;
        }
        size_t equals = entry.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        std::string name = android::base::Trim(entry.substr(0, equals));
        std::string value = android::base::Trim(entry.substr(equals + 1));
        if (android::base::GetProperty(name, "").empty()) {
            android::base::SetProperty(name, value);
        }
    }
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    loadBuildProperties();
    AndroidRuntime::mJavaVM = vm;
    JNIEnv* env = AndroidRuntime::getJNIEnv();
    if (env == nullptr) {
        ALOGE("libandroid_runtime: no JNIEnv on the loading thread");
        return JNI_ERR;
    }
    for (const RegJNIRec& reg : gRegJNI) {
        if (reg.mProc(env) < 0) {
            ALOGE("libandroid_runtime: %s failed", reg.mName);
            return JNI_ERR;
        }
    }
    return JNI_VERSION_1_6;
}
