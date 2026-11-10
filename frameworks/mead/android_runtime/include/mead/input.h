/*
 * Mead's input, as seen from Mead's display bridge: input channels whose
 * client end Java's InputEventReceiver reads, as an app's window gets from
 * the window manager on Android, and whose server end the bridge publishes
 * key and motion events to, as Android's input dispatcher does.
 */

#ifndef MEAD_INPUT_H
#define MEAD_INPUT_H

#include <stdint.h>

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MeadInput MeadInput;

/* A new input channel pair named name: returns the client end, an
 * android.view.InputChannel (a local reference), and the server end in
 * *out, or NULL with *out NULL. */
jobject mead_input_open(JNIEnv* env, const char* name, MeadInput** out);

/* Closes the server end; the client sees its channel closed. */
void mead_input_close(MeadInput* input);

/* Publishes a KeyEvent (AKEY_EVENT_ACTION_*), times on CLOCK_MONOTONIC in
 * nanoseconds. Returns 0, or a negative error: the channel is full until the
 * app finishes events, or closed. */
int mead_input_send_key(MeadInput* input, int32_t deviceId, int32_t source, int32_t action,
                        int32_t flags, int32_t keyCode, int32_t scanCode, int32_t metaState,
                        int32_t repeatCount, int64_t downTime, int64_t eventTime);

/* The most axes a pointer has here: AMOTION_EVENT_AXIS_* from 0 to 47. */
#define MEAD_INPUT_AXES 48

/* Publishes a MotionEvent (AMOTION_EVENT_ACTION_*) of pointerCount pointers:
 * each pointer's id and tool type, and its axes' values, MEAD_INPUT_AXES
 * floats per pointer indexed by axis, of which axisBits (bit n for axis n,
 * the same for every pointer) says which are present. */
int mead_input_send_motion(MeadInput* input, int32_t deviceId, int32_t source, int32_t action,
                           int32_t actionButton, int32_t flags, int32_t metaState,
                           int32_t buttonState, int64_t downTime, int64_t eventTime,
                           uint32_t pointerCount, const int32_t* pointerIds,
                           const int32_t* toolTypes, uint64_t axisBits, const float* axisValues);

#ifdef __cplusplus
}
#endif

#endif
