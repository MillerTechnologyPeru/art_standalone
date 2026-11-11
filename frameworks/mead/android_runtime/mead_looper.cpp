/*
 * The NDK's ALooper over libutils' Looper, as Android's libandroid has it,
 * for Mead's libandroid to call while ART runs: a native activity's or a
 * game's looper is then the one Java's MessageQueue polls on that thread.
 */

#include <mead/looper.h>

#include <utils/Looper.h>

using android::Looper;
using android::sp;

static Looper* toLooper(MeadLooper* looper) {
    return reinterpret_cast<Looper*>(looper);
}

static MeadLooper* toMead(Looper* looper) {
    return reinterpret_cast<MeadLooper*>(looper);
}

MeadLooper* mead_looper_for_thread(void) {
    return toMead(Looper::getForThread().get());
}

MeadLooper* mead_looper_prepare(int options) {
    return toMead(Looper::prepare(options).get());
}

void mead_looper_acquire(MeadLooper* looper) {
    toLooper(looper)->incStrong((void*)mead_looper_acquire);
}

void mead_looper_release(MeadLooper* looper) {
    toLooper(looper)->decStrong((void*)mead_looper_acquire);
}

int mead_looper_poll_once(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    sp<Looper> looper = Looper::getForThread();
    if (looper == nullptr) return Looper::POLL_ERROR;
    return looper->pollOnce(timeoutMillis, outFd, outEvents, outData);
}

int mead_looper_poll_all(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    sp<Looper> looper = Looper::getForThread();
    if (looper == nullptr) return Looper::POLL_ERROR;
    return looper->pollAll(timeoutMillis, outFd, outEvents, outData);
}

void mead_looper_wake(MeadLooper* looper) {
    toLooper(looper)->wake();
}

int mead_looper_add_fd(MeadLooper* looper, int fd, int ident, int events,
                       MeadLooperCallback callback, void* data) {
    return toLooper(looper)->addFd(fd, ident, events, callback, data);
}

int mead_looper_remove_fd(MeadLooper* looper, int fd) {
    return toLooper(looper)->removeFd(fd);
}
