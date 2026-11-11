/*
 * Mead's libgui, as seen from Mead's display bridge (Swift, over SDL): what
 * the framework's graphics need of SurfaceFlinger, which Mead doesn't have.
 *
 * The bridge describes the display, delivers vsync and creates the surfaces
 * the framework draws into, receiving each frame they queue.
 */

#ifndef MEAD_GUI_H
#define MEAD_GUI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ANativeWindow;

/* A display, as SurfaceComposerClient describes it. */
typedef struct MeadDisplay {
    uint32_t width;     /* pixels */
    uint32_t height;    /* pixels */
    float xdpi;
    float ydpi;
    float refreshRate;  /* frames per second */
    float density;      /* the framework's density: 1.0 at 160 dpi */
} MeadDisplay;

/* Describes the display the framework draws for. Until it is called, the
 * display is 1280×720 at 160 dpi and 60 Hz. Changing it later sends a
 * configuration change to every display event receiver. */
void mead_gui_set_display(const MeadDisplay* display);

/* The display described. */
void mead_gui_get_display(MeadDisplay* display);

/* A vsync at timestamp (CLOCK_MONOTONIC, nanoseconds), for the display event
 * receivers that asked for one. Until the bridge first calls it, Mead's
 * libgui keeps vsync with a timer at the display's refresh rate. */
void mead_gui_vsync(int64_t timestamp);

/* Windowing systems, for EGL. */
enum {
    MEAD_GUI_NATIVE_X11 = 1,
    MEAD_GUI_NATIVE_WAYLAND = 2,
};

/* The windowing connection the bridge's windows are on: an X11 Display or a
 * wl_display, of kind MEAD_GUI_NATIVE_*. hwui's EGL display is made on it. */
void mead_gui_set_native_display(uint32_t kind, void* display);

/* The windowing connection set, or kind 0 and NULL. */
void mead_gui_get_native_display(uint32_t* kind, void** display);

/* Makes the bridge the vsync source from now on, without the timer: when a
 * display event receiver wants vsync, libgui calls request(context), on the
 * thread asking, and the bridge answers with mead_gui_vsync once the display
 * has refreshed (a compositor's frame callback). request is called again
 * after each vsync while receivers still want it. */
void mead_gui_set_vsync_request(void (*request)(void* context), void* context);

/* Receives a surface's frames. */
typedef struct MeadSurfaceCallbacks {
    void* context;
    /* A frame was queued: pixels in format (HAL_PIXEL_FORMAT_*), rows of
     * stride pixels, readable until it returns. */
    void (*present)(void* context, const void* pixels, uint32_t width, uint32_t height,
                    uint32_t stride, int32_t format);
    /* In place of present, when set: a frame was queued in buffer slot, which
     * is the shared memory at fd (a memfd, from its start), width×height in
     * format, rows of stride pixels; pixels is its mapping, readable until
     * this returns. The buffer isn't reused until it's handed back with
     * mead_surface_release_buffer, so a compositor can read it in place. */
    void (*presentBuffer)(void* context, int32_t slot, int fd, uint32_t width, uint32_t height,
                          uint32_t stride, int32_t format, const void* pixels);
} MeadSurfaceCallbacks;

/* Hands back a buffer presentBuffer gave, which the surface may now reuse. */
void mead_surface_release_buffer(struct ANativeWindow* surface, int32_t slot);

/* A surface of width×height in format (HAL_PIXEL_FORMAT_*), whose frames go
 * to callbacks. It holds one reference, which ANativeWindow_release drops. */
struct ANativeWindow* mead_surface_create(uint32_t width, uint32_t height, int32_t format,
                                          const MeadSurfaceCallbacks* callbacks);

/* What EGL draws to for a surface made by mead_surface_create: an X11
 * Window or a wl_egl_window, on the native display. With one, hwui's GL
 * pipeline renders to the window itself; software frames still go to the
 * surface's callbacks. */
void mead_surface_set_native_window(struct ANativeWindow* surface, uintptr_t window);

/* A surface's native window, as above, or 0: for EGL outside hwui, an app's
 * through the NDK's ANativeWindow, which can't call perform itself. */
uintptr_t mead_surface_get_native_window(struct ANativeWindow* surface);

/* ANativeWindow's perform code for a surface's native window, as above:
 * perform(window, MEAD_SURFACE_GET_NATIVE_WINDOW, uintptr_t* out). It is
 * 0 for a surface without one. */
#define MEAD_SURFACE_GET_NATIVE_WINDOW 0x4d454144

/* Windows for SurfaceControl: the window manager's surfaces, each its own
 * window, which the bridge makes. create returns a surface from
 * mead_surface_create on a new window of width×height pixels named name,
 * with one reference for the caller, or NULL; destroy closes the window
 * whose surface that was, once its SurfaceControl is destroyed. */
typedef struct MeadWindowCallbacks {
    void* context;
    struct ANativeWindow* (*create)(void* context, const char* name, uint32_t width,
                                    uint32_t height);
    void (*destroy)(void* context, struct ANativeWindow* window);
} MeadWindowCallbacks;

void mead_gui_set_window_callbacks(const MeadWindowCallbacks* callbacks);

/* A window from the bridge's create, or NULL without a bridge. */
struct ANativeWindow* mead_gui_create_window(const char* name, uint32_t width, uint32_t height);

/* The bridge's destroy, for a window from mead_gui_create_window. */
void mead_gui_destroy_window(struct ANativeWindow* window);

/* A layer's state in its parent window, as SurfaceControl transactions set
 * it: its place in the parent's surface and its buffer's size, in pixels;
 * its order, under the parent's content when negative (a SurfaceView's hole)
 * and over it otherwise; whether it shows; and the scale its matrix shows
 * its buffer at (a SurfaceView's with a fixed size, scaled to the view). */
typedef struct MeadLayerState {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    int32_t z;
    int32_t shown;
    float scale_x;
    float scale_y;
} MeadLayerState;

/* Layers for SurfaceControl: a child surface of a window (a SurfaceView's),
 * which the bridge makes. create returns a surface from mead_surface_create
 * for a new layer of width×height pixels named name in the window whose
 * surface parent is, hidden, with one reference for the caller, or NULL;
 * update applies a transaction's state to it. A layer goes, as a window
 * does, with mead_gui_destroy_window. */
typedef struct MeadLayerCallbacks {
    void* context;
    struct ANativeWindow* (*create)(void* context, struct ANativeWindow* parent, const char* name,
                                    uint32_t width, uint32_t height);
    void (*update)(void* context, struct ANativeWindow* layer, const MeadLayerState* state);
} MeadLayerCallbacks;

void mead_gui_set_layer_callbacks(const MeadLayerCallbacks* callbacks);

/* A layer from the bridge's create, or NULL without a bridge. */
struct ANativeWindow* mead_gui_create_layer(struct ANativeWindow* parent, const char* name,
                                            uint32_t width, uint32_t height);

/* The bridge's update, for a layer from mead_gui_create_layer. */
void mead_gui_update_layer(struct ANativeWindow* layer, const MeadLayerState* state);

/* Resizes a surface made by mead_surface_create: the buffers it hands out
 * from now on are width×height. */
void mead_surface_resize(struct ANativeWindow* surface, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif
