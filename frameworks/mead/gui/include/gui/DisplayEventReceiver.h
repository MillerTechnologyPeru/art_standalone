/*
 * Mead's DisplayEventReceiver: vsync and display configuration events, as
 * Android's delivers them from SurfaceFlinger, from Mead's display bridge
 * (mead_gui_vsync, mead_gui_set_display) or, until the bridge delivers
 * vsync, a timer at the display's refresh rate.
 */

#pragma once

#include <stdint.h>
#include <sys/types.h>

#include <gui/ISurfaceComposer.h>
#include <ui/GraphicTypes.h>
#include <utils/Errors.h>
#include <utils/Timers.h>

namespace android {

static inline constexpr uint32_t fourcc(char c1, char c2, char c3, char c4) {
    return static_cast<uint32_t>(c1) << 24 | static_cast<uint32_t>(c2) << 16 |
            static_cast<uint32_t>(c3) << 8 | static_cast<uint32_t>(c4);
}

class DisplayEventReceiver {
public:
    enum {
        DISPLAY_EVENT_VSYNC = fourcc('v', 's', 'y', 'n'),
        DISPLAY_EVENT_HOTPLUG = fourcc('p', 'l', 'u', 'g'),
        DISPLAY_EVENT_CONFIG_CHANGED = fourcc('c', 'o', 'n', 'f'),
    };

    struct Event {
        struct Header {
            uint32_t type;
            PhysicalDisplayId displayId;
            nsecs_t timestamp __attribute__((aligned(8)));
        };

        struct VSync {
            uint32_t count;
        };

        struct Hotplug {
            bool connected;
        };

        struct Config {
            int32_t configId;
        };

        Header header;
        union {
            VSync vsync;
            Hotplug hotplug;
            Config config;
        };
    };

    // Vsync events are off until setVsyncRate or requestNextVsync.
    explicit DisplayEventReceiver(
            ISurfaceComposer::VsyncSource vsyncSource = ISurfaceComposer::eVsyncSourceApp);
    ~DisplayEventReceiver();

    DisplayEventReceiver(const DisplayEventReceiver&) = delete;
    DisplayEventReceiver& operator=(const DisplayEventReceiver&) = delete;

    status_t initCheck() const;

    // Readable when events are waiting; the receiver keeps it.
    int getFd() const;

    // Reads up to count waiting events; 0 when there are none.
    ssize_t getEvents(Event* events, size_t count);

    // Every count-th vsync, or with 0, only those asked for with
    // requestNextVsync.
    status_t setVsyncRate(uint32_t count);

    // The next vsync, when the rate is 0.
    status_t requestNextVsync();

private:
    friend struct DisplayEvents;

    int mReceiveFd = -1;
    int mSendFd = -1;
    uint32_t mVsyncRate = 0;
    uint32_t mVsyncCount = 0;
    bool mVsyncRequested = false;
};

} // namespace android
