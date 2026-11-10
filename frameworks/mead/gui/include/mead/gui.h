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

/* Receives a surface's frames. */
typedef struct MeadSurfaceCallbacks {
    void* context;
    /* A frame was queued: pixels in format (HAL_PIXEL_FORMAT_*), rows of
     * stride pixels, readable until it returns. */
    void (*present)(void* context, const void* pixels, uint32_t width, uint32_t height,
                    uint32_t stride, int32_t format);
} MeadSurfaceCallbacks;

/* A surface of width×height in format (HAL_PIXEL_FORMAT_*), whose frames go
 * to callbacks. It holds one reference, which ANativeWindow_release drops. */
struct ANativeWindow* mead_surface_create(uint32_t width, uint32_t height, int32_t format,
                                          const MeadSurfaceCallbacks* callbacks);

/* Resizes a surface made by mead_surface_create: the buffers it hands out
 * from now on are width×height. */
void mead_surface_resize(struct ANativeWindow* surface, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif
