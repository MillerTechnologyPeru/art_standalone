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
#include <malloc.h>
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

// android_util_Binder.cpp and some of the graphics JNI define their register
// functions outside the namespace.
extern int register_android_os_Binder(JNIEnv* env);
extern int register_android_os_Process(JNIEnv* env);
extern int register_android_hardware_HardwareBuffer(JNIEnv* env);
extern int register_android_graphics_Bitmap(JNIEnv* env);
extern int register_android_graphics_BitmapFactory(JNIEnv* env);
extern int register_android_graphics_BitmapRegionDecoder(JNIEnv* env);
extern int register_android_graphics_ByteBufferStreamAdaptor(JNIEnv* env);
extern int register_android_graphics_Camera(JNIEnv* env);
extern int register_android_graphics_CreateJavaOutputStreamAdaptor(JNIEnv* env);
extern int register_android_graphics_GraphicBuffer(JNIEnv* env);
extern int register_android_graphics_Graphics(JNIEnv* env);
extern int register_android_graphics_ImageDecoder(JNIEnv* env);
extern int register_android_graphics_drawable_AnimatedImageDrawable(JNIEnv* env);
extern int register_android_graphics_Interpolator(JNIEnv* env);
extern int register_android_graphics_MaskFilter(JNIEnv* env);
extern int register_android_graphics_NinePatch(JNIEnv* env);
extern int register_android_graphics_PathEffect(JNIEnv* env);
extern int register_android_graphics_Shader(JNIEnv* env);
extern int register_android_graphics_Typeface(JNIEnv* env);
extern int register_android_graphics_YuvImage(JNIEnv* env);
extern int register_android_opengl_jni_EGL14(JNIEnv* env);
extern int register_android_opengl_jni_GLES20(JNIEnv* env);
extern int register_android_opengl_jni_GLES30(JNIEnv* env);
extern int register_android_opengl_jni_GLES31(JNIEnv* env);

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
extern int register_android_view_RenderNode(JNIEnv* env);
extern int register_android_view_RenderNodeAnimator(JNIEnv* env);
extern int register_android_view_DisplayListCanvas(JNIEnv* env);
extern int register_android_view_ThreadedRenderer(JNIEnv* env);
extern int register_android_view_Surface(JNIEnv* env);
extern int register_android_view_SurfaceControl(JNIEnv* env);
extern int register_android_media_AudioSystem(JNIEnv* env);
extern int register_android_view_SurfaceSession(JNIEnv* env);
extern int register_android_os_MessageQueue(JNIEnv* env);
extern int register_android_util_PathParser(JNIEnv* env);
extern int register_android_animation_PropertyValuesHolder(JNIEnv* env);
extern int register_android_text_AndroidCharacter(JNIEnv* env);
extern int register_android_text_Hyphenator(JNIEnv* env);
extern int register_android_view_PointerIcon(JNIEnv* env);
extern int register_com_android_internal_view_animation_NativeInterpolatorFactoryHelper(JNIEnv* env);
extern int register_android_os_FileObserver(JNIEnv* env);
extern int register_android_os_MemoryFile(JNIEnv* env);
extern int register_android_os_SharedMemory(JNIEnv* env);
extern int register_android_content_res_Configuration(JNIEnv* env);
extern int register_android_view_InputEventSender(JNIEnv* env);
extern int register_android_server_NetworkManagementSocketTagger(JNIEnv* env);
extern int register_com_android_internal_util_VirtualRefBasePtr(JNIEnv* env);
extern int register_android_database_CursorWindow(JNIEnv* env);
extern int register_android_database_SQLiteConnection(JNIEnv* env);
extern int register_android_database_SQLiteGlobal(JNIEnv* env);
extern int register_android_database_SQLiteDebug(JNIEnv* env);
extern int register_android_view_InputChannel(JNIEnv* env);
extern int register_android_view_InputQueue(JNIEnv* env);
extern int register_android_graphics_SurfaceTexture(JNIEnv* env);
extern int register_android_app_NativeActivity(JNIEnv* env);
extern int register_android_view_InputEventReceiver(JNIEnv* env);
extern int register_android_view_KeyEvent(JNIEnv* env);
extern int register_android_view_MotionEvent(JNIEnv* env);
extern int register_android_view_InputDevice(JNIEnv* env);
extern int register_android_view_KeyCharacterMap(JNIEnv* env);
extern int register_android_view_VelocityTracker(JNIEnv* env);
extern int register_android_view_DisplayEventReceiver(JNIEnv* env);
extern int register_android_graphics_Canvas(JNIEnv* env);
extern int register_android_graphics_CanvasProperty(JNIEnv* env);
extern int register_android_graphics_ColorFilter(JNIEnv* env);
extern int register_android_graphics_ColorSpace(JNIEnv* env);
extern int register_android_graphics_DrawFilter(JNIEnv* env);
extern int register_android_graphics_FontFamily(JNIEnv* env);
extern int register_android_graphics_Matrix(JNIEnv* env);
extern int register_android_graphics_Paint(JNIEnv* env);
extern int register_android_graphics_Path(JNIEnv* env);
extern int register_android_graphics_PathMeasure(JNIEnv* env);
extern int register_android_graphics_Picture(JNIEnv* env);
extern int register_android_graphics_Region(JNIEnv* env);
extern int register_android_graphics_drawable_AnimatedVectorDrawable(JNIEnv* env);
extern int register_android_graphics_drawable_VectorDrawable(JNIEnv* env);
extern int register_android_graphics_fonts_Font(JNIEnv* env);
extern int register_android_graphics_fonts_FontFamily(JNIEnv* env);
extern int register_android_graphics_text_MeasuredText(JNIEnv* env);
extern int register_android_graphics_text_LineBreaker(JNIEnv* env);

JavaVM* AndroidRuntime::mJavaVM = nullptr;

static AndroidRuntime* gCurRuntime = nullptr;

// The process's AndroidRuntime, which app_process makes on Android, here
// made as libandroid_runtime loads. Its argument block, which setArgv0
// writes the process's new name into, is a buffer of its own: Mead's command
// line stays as it was.
static char gArgBlock[256];

class MeadRuntime : public AndroidRuntime {
public:
    MeadRuntime() : AndroidRuntime(gArgBlock, sizeof(gArgBlock)) {}
    void onStarted() override {}
};

AndroidRuntime::AndroidRuntime(char* argBlockStart, const size_t argBlockLength)
      : mExitWithoutCleanup(false), mArgBlockStart(argBlockStart), mArgBlockLength(argBlockLength) {
    mOptions.setCapacity(20);
    gCurRuntime = this;
}

AndroidRuntime::~AndroidRuntime() {}

void AndroidRuntime::onVmCreated(JNIEnv*) {}

/*static*/ AndroidRuntime* AndroidRuntime::getRuntime() {
    return gCurRuntime;
}

void AndroidRuntime::setArgv0(const char* argv0, bool setProcName) {
    if (setProcName) {
        int len = strlen(argv0);
        if (len < 15) {
            pthread_setname_np(pthread_self(), argv0);
        } else {
            pthread_setname_np(pthread_self(), argv0 + len - 15);
        }
    }
    memset(mArgBlockStart, 0, mArgBlockLength);
    strlcpy(mArgBlockStart, argv0, mArgBlockLength);
}

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

// ClassLoaderFactory's linker namespace for an app's class loader: on Android
// libnativeloader makes one per app, to keep its native libraries apart.
// Here an app's native libraries load through Mead's loader (MeadDLBio),
// which has no namespaces, so there's nothing to make; null means success.
static jstring createClassloaderNamespace(JNIEnv*, jclass, jobject, jint, jstring, jstring,
                                          jboolean, jstring) {
    return nullptr;
}

static int register_com_android_internal_os_ClassLoaderFactory(JNIEnv* env) {
    static const JNINativeMethod methods[] = {
            {"createClassloaderNamespace",
             "(Ljava/lang/ClassLoader;ILjava/lang/String;Ljava/lang/String;ZLjava/lang/String;)Ljava/lang/String;",
             (void*)createClassloaderNamespace},
    };
    return AndroidRuntime::registerNativeMethods(env, "com/android/internal/os/ClassLoaderFactory",
                                                 methods, NELEM(methods));
}

// GraphicsEnvironment: Android's choice of GPU driver (updatable drivers,
// ANGLE) and of debug layers, through libgraphicsenv. Here the driver is the
// system's (Mesa's), so there's nothing to choose: no driver path, no
// ANGLE, no layers.
static jint getCanLoadSystemLibraries(JNIEnv*, jclass) {
    return 0;
}

static jboolean getShouldUseAngle(JNIEnv*, jclass, jstring) {
    return JNI_FALSE;
}

// For each setting, which takes the arguments its signature says and has
// nothing to do with them.
static void graphicsEnvironmentNoOp(JNIEnv*, jclass) {}

static int register_android_os_GraphicsEnvironment(JNIEnv* env) {
    static const JNINativeMethod methods[] = {
            {"getCanLoadSystemLibraries", "()I", (void*)getCanLoadSystemLibraries},
            {"setDriverPathAndSphalLibraries", "(Ljava/lang/String;Ljava/lang/String;)V",
             (void*)graphicsEnvironmentNoOp},
            {"setGpuStats", "(Ljava/lang/String;Ljava/lang/String;JJLjava/lang/String;I)V",
             (void*)graphicsEnvironmentNoOp},
            {"setAngleInfo",
             "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/io/FileDescriptor;JJ)V",
             (void*)graphicsEnvironmentNoOp},
            {"getShouldUseAngle", "(Ljava/lang/String;)Z", (void*)getShouldUseAngle},
            {"setLayerPaths", "(Ljava/lang/ClassLoader;Ljava/lang/String;)V",
             (void*)graphicsEnvironmentNoOp},
            {"setDebugLayers", "(Ljava/lang/String;)V", (void*)graphicsEnvironmentNoOp},
            {"setDebugLayersGLES", "(Ljava/lang/String;)V", (void*)graphicsEnvironmentNoOp},
            {"hintActivityLaunch", "()V", (void*)graphicsEnvironmentNoOp},
    };
    return AndroidRuntime::registerNativeMethods(env, "android/os/GraphicsEnvironment", methods,
                                                 NELEM(methods));
}

// EventLog: Android's binary events buffer, which this tree's older liblog
// doesn't have. Events are accepted and kept nowhere; reading finds none.
static jint writeEvent(JNIEnv*, jclass) {
    return 0;
}

static void readEvents(JNIEnv*, jclass) {}

static int register_android_util_EventLog(JNIEnv* env) {
    // Each takes the arguments its signature says, which these never read.
    static const JNINativeMethod methods[] = {
            {"writeEvent", "(II)I", (void*)writeEvent},
            {"writeEvent", "(IJ)I", (void*)writeEvent},
            {"writeEvent", "(IF)I", (void*)writeEvent},
            {"writeEvent", "(ILjava/lang/String;)I", (void*)writeEvent},
            {"writeEvent", "(I[Ljava/lang/Object;)I", (void*)writeEvent},
            {"readEvents", "([ILjava/util/Collection;)V", (void*)readEvents},
            {"readEventsOnWrapping", "([IJLjava/util/Collection;)V", (void*)readEvents},
    };
    return AndroidRuntime::registerNativeMethods(env, "android/util/EventLog", methods,
                                                 NELEM(methods));
}

// ActivityThread's: bionic's mallopt(M_PURGE) is glibc's malloc_trim. There's
// no graphics memory dump, and no zygote whose children are profiled.
static void purgePendingResources(JNIEnv*, jclass) {
    malloc_trim(0);
}

static void dumpGraphicsInfo(JNIEnv*, jclass, jobject) {}

static void initZygoteChildHeapProfiling(JNIEnv*, jclass) {}

static int register_android_app_ActivityThread(JNIEnv* env) {
    static const JNINativeMethod methods[] = {
            {"nPurgePendingResources", "()V", (void*)purgePendingResources},
            {"nDumpGraphicsInfo", "(Ljava/io/FileDescriptor;)V", (void*)dumpGraphicsInfo},
            {"nInitZygoteChildHeapProfiling", "()V", (void*)initZygoteChildHeapProfiling},
    };
    return AndroidRuntime::registerNativeMethods(env, "android/app/ActivityThread", methods,
                                                 NELEM(methods));
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
    REG_JNI(register_android_os_Process),
    REG_JNI(register_com_android_internal_os_ClassLoaderFactory),
    REG_JNI(register_android_os_GraphicsEnvironment),
    REG_JNI(register_android_util_EventLog),
    REG_JNI(register_android_app_ActivityThread),
    REG_JNI(register_android_os_Binder),
    REG_JNI(register_android_os_Parcel),
    REG_JNI(register_android_os_Trace),
    REG_JNI(register_android_graphics_Canvas),
    REG_JNI(register_android_view_RenderNode),
    REG_JNI(register_android_view_RenderNodeAnimator),
    REG_JNI(register_android_view_DisplayListCanvas),
    REG_JNI(register_android_view_ThreadedRenderer),
    REG_JNI(register_android_view_Surface),
    REG_JNI(register_android_view_SurfaceControl),
    REG_JNI(register_android_media_AudioSystem),
    REG_JNI(register_android_view_SurfaceSession),
    REG_JNI(register_android_graphics_ColorSpace),
    REG_JNI(register_android_graphics_Graphics),
    REG_JNI(register_android_graphics_Bitmap),
    REG_JNI(register_android_graphics_BitmapFactory),
    REG_JNI(register_android_graphics_BitmapRegionDecoder),
    REG_JNI(register_android_graphics_ByteBufferStreamAdaptor),
    REG_JNI(register_android_graphics_Camera),
    REG_JNI(register_android_graphics_CreateJavaOutputStreamAdaptor),
    REG_JNI(register_android_graphics_CanvasProperty),
    REG_JNI(register_android_graphics_ColorFilter),
    REG_JNI(register_android_graphics_DrawFilter),
    REG_JNI(register_android_graphics_FontFamily),
    REG_JNI(register_android_graphics_GraphicBuffer),
    REG_JNI(register_android_graphics_ImageDecoder),
    REG_JNI(register_android_graphics_drawable_AnimatedImageDrawable),
    REG_JNI(register_android_graphics_Interpolator),
    REG_JNI(register_android_graphics_MaskFilter),
    REG_JNI(register_android_graphics_Matrix),
    REG_JNI(register_android_graphics_NinePatch),
    REG_JNI(register_android_graphics_Paint),
    REG_JNI(register_android_graphics_Path),
    REG_JNI(register_android_graphics_PathMeasure),
    REG_JNI(register_android_graphics_PathEffect),
    REG_JNI(register_android_graphics_Picture),
    REG_JNI(register_android_graphics_Region),
    REG_JNI(register_android_graphics_Shader),
    REG_JNI(register_android_graphics_Typeface),
    REG_JNI(register_android_graphics_YuvImage),
    REG_JNI(register_android_graphics_drawable_AnimatedVectorDrawable),
    REG_JNI(register_android_graphics_drawable_VectorDrawable),
    REG_JNI(register_android_graphics_fonts_Font),
    REG_JNI(register_android_graphics_fonts_FontFamily),
    REG_JNI(register_android_graphics_text_MeasuredText),
    REG_JNI(register_android_graphics_text_LineBreaker),
    REG_JNI(register_android_hardware_HardwareBuffer),
    REG_JNI(register_android_os_MessageQueue),
    REG_JNI(register_android_util_PathParser),
    REG_JNI(register_android_animation_PropertyValuesHolder),
    REG_JNI(register_android_text_AndroidCharacter),
    REG_JNI(register_android_text_Hyphenator),
    REG_JNI(register_android_view_PointerIcon),
    REG_JNI(register_com_android_internal_view_animation_NativeInterpolatorFactoryHelper),
    REG_JNI(register_android_os_FileObserver),
    REG_JNI(register_android_os_MemoryFile),
    REG_JNI(register_android_os_SharedMemory),
    REG_JNI(register_android_content_res_Configuration),
    REG_JNI(register_android_view_InputEventSender),
    REG_JNI(register_android_server_NetworkManagementSocketTagger),
    REG_JNI(register_com_android_internal_util_VirtualRefBasePtr),
    REG_JNI(register_android_database_CursorWindow),
    REG_JNI(register_android_database_SQLiteConnection),
    REG_JNI(register_android_database_SQLiteGlobal),
    REG_JNI(register_android_database_SQLiteDebug),
    REG_JNI(register_android_view_InputChannel),
    REG_JNI(register_android_view_InputQueue),
    REG_JNI(register_android_graphics_SurfaceTexture),
    REG_JNI(register_android_app_NativeActivity),
    REG_JNI(register_android_opengl_jni_EGL14),
    REG_JNI(register_android_opengl_jni_GLES20),
    REG_JNI(register_android_opengl_jni_GLES30),
    REG_JNI(register_android_opengl_jni_GLES31),
    REG_JNI(register_android_view_InputEventReceiver),
    REG_JNI(register_android_view_KeyEvent),
    REG_JNI(register_android_view_MotionEvent),
    REG_JNI(register_android_view_InputDevice),
    REG_JNI(register_android_view_KeyCharacterMap),
    REG_JNI(register_android_view_VelocityTracker),
    REG_JNI(register_android_view_DisplayEventReceiver),
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
    static MeadRuntime* runtime = new MeadRuntime();
    (void)runtime;
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
