/*
 * The thread's android::Looper, the one Java's MessageQueue polls, for the
 * NDK's ALooper: Mead's libandroid hands ALooper_* calls here while ART
 * runs, as Android's libandroid (frameworks/base/native/android/looper.cpp)
 * does. A MeadLooper* is an android::Looper*.
 */

#ifndef MEAD_LOOPER_H
#define MEAD_LOOPER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MeadLooper MeadLooper;

typedef int (*MeadLooperCallback)(int fd, int events, void* data);

/* The calling thread's looper, NULL for none; no reference is taken. */
MeadLooper* mead_looper_for_thread(void);

/* The calling thread's looper, made if it has none (ALOOPER_PREPARE_*
 * options); the thread holds the reference. */
MeadLooper* mead_looper_prepare(int options);

void mead_looper_acquire(MeadLooper* looper);
void mead_looper_release(MeadLooper* looper);

/* Polls the calling thread's looper: ALOOPER_POLL_ERROR when it has none. */
int mead_looper_poll_once(int timeoutMillis, int* outFd, int* outEvents, void** outData);
int mead_looper_poll_all(int timeoutMillis, int* outFd, int* outEvents, void** outData);

void mead_looper_wake(MeadLooper* looper);
int mead_looper_add_fd(MeadLooper* looper, int fd, int ident, int events,
                       MeadLooperCallback callback, void* data);
int mead_looper_remove_fd(MeadLooper* looper, int fd);

#ifdef __cplusplus
}
#endif

#endif
