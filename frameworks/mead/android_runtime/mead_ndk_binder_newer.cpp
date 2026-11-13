/*
 * The NDK binder functions Android 11 and 12 added, which Android 10's
 * libbinder_ndk (mead_ndk_binder.cpp) lacks, as mead_ndk_ names, with what
 * holds in Mead: a process with its own services and no others, no VINTF
 * manifest, and no shell to send commands.
 */

#include "mead/ndk_binder_names.h"

#include <android/binder_ibinder.h>
#include <android/binder_manager.h>

#include <map>
#include <mutex>
#include <string>

#include <utils/String8.h>

#include "../../native/libs/binder/ndk/ibinder_internal.h"

typedef binder_status_t (*AIBinder_handleShellCommand)(AIBinder* binder, int in, int out, int err,
                                                        const char** argv, uint32_t argc);

extern "C" {

// The class's descriptor, in UTF-8; kept for as long as the process runs, as
// classes are.
const char* AIBinder_Class_getDescriptor(const AIBinder_Class* clazz) {
    static std::mutex lock;
    static std::map<const AIBinder_Class*, std::string> descriptors;
    std::lock_guard<std::mutex> guard(lock);
    auto found = descriptors.find(clazz);
    if (found == descriptors.end()) {
        found = descriptors.emplace(clazz, android::String8(clazz->getInterfaceDescriptor()).string()).first;
    }
    return found->second.c_str();
}

// No shell sends this process commands.
void AIBinder_Class_setHandleShellCommand(AIBinder_Class* clazz, AIBinder_handleShellCommand handleShellCommand) {
    (void)clazz;
    (void)handleShellCommand;
}

// Stability matters across partitions; Mead has the one process.
void AIBinder_markVintfStability(AIBinder* binder) {
    (void)binder;
}

// Mead has no VINTF manifest: nothing is declared.
bool AServiceManager_isDeclared(const char* instance) {
    (void)instance;
    return false;
}

// The service, if this process has it. Android waits for one that will
// start; in Mead no other process will, so there is no waiting.
AIBinder* AServiceManager_waitForService(const char* instance) {
    return AServiceManager_checkService(instance);
}

}  // extern "C"
