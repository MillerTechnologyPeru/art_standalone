/*
 * Mead's DisplayEventReceiver, and the display it reports on. See
 * gui/DisplayEventReceiver.h and mead/gui.h.
 */

#define LOG_TAG "MeadDisplayEvents"

#include <gui/DisplayEventReceiver.h>

#include <errno.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>

#include <log/log.h>
#include <mead/gui.h>

namespace android {

// The display, the receivers and the vsync timer.
struct DisplayEvents {
    std::mutex mutex;
    std::condition_variable wanted;
    MeadDisplay display{1280, 720, 160.0f, 160.0f, 60.0f, 1.0f};
    bool displaySet = false;
    uint32_t nativeKind = 0;
    void* nativeDisplay = nullptr;
    std::set<DisplayEventReceiver*> receivers;
    bool timerStarted = false;
    bool bridgeVsync = false;
    uint32_t vsyncCount = 0;

    static DisplayEvents& get() {
        // Never destroyed: the timer thread may outlive static destructors.
        static DisplayEvents* events = new DisplayEvents();
        return *events;
    }

    static void send(DisplayEventReceiver* receiver, const DisplayEventReceiver::Event& event) {
        ssize_t sent;
        do {
            sent = ::send(receiver->mSendFd, &event, sizeof(event), MSG_DONTWAIT | MSG_NOSIGNAL);
        } while (sent < 0 && errno == EINTR);
        // A full queue drops the event, as SurfaceFlinger's does.
    }

    bool wantsVsync() const {
        for (DisplayEventReceiver* receiver : receivers) {
            if (receiver->mVsyncRate > 0 || receiver->mVsyncRequested) return true;
        }
        return false;
    }

    // Under mutex.
    void vsync(nsecs_t timestamp) {
        vsyncCount++;
        DisplayEventReceiver::Event event{};
        event.header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
        event.header.displayId = 0;
        event.header.timestamp = timestamp;
        event.vsync.count = vsyncCount;
        for (DisplayEventReceiver* receiver : receivers) {
            bool deliver = false;
            if (receiver->mVsyncRate > 0) {
                deliver = receiver->mVsyncCount++ % receiver->mVsyncRate == 0;
            } else if (receiver->mVsyncRequested) {
                receiver->mVsyncRequested = false;
                deliver = true;
            }
            if (deliver) send(receiver, event);
        }
    }

    // Under mutex: Mead's own vsync, until the bridge delivers it.
    void startTimer() {
        if (timerStarted || bridgeVsync) return;
        timerStarted = true;
        std::thread([this] {
            std::unique_lock<std::mutex> lock(mutex);
            timespec next;
            clock_gettime(CLOCK_MONOTONIC, &next);
            while (!bridgeVsync) {
                wanted.wait(lock, [this] { return bridgeVsync || wantsVsync(); });
                if (bridgeVsync) break;
                float rate = display.refreshRate > 0 ? display.refreshRate : 60.0f;
                long period = static_cast<long>(1e9f / rate);
                timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                next.tv_nsec += period;
                while (next.tv_nsec >= 1000000000L) {
                    next.tv_nsec -= 1000000000L;
                    next.tv_sec++;
                }
                // After an idle stretch, start again from now.
                if (next.tv_sec < now.tv_sec ||
                    (next.tv_sec == now.tv_sec && next.tv_nsec < now.tv_nsec)) {
                    next = now;
                }
                lock.unlock();
                while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr) == EINTR) {
                }
                lock.lock();
                if (!bridgeVsync) vsync(static_cast<nsecs_t>(next.tv_sec) * 1000000000LL + next.tv_nsec);
            }
            timerStarted = false;
        }).detach();
    }
};

DisplayEventReceiver::DisplayEventReceiver(ISurfaceComposer::VsyncSource) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, fds) != 0) {
        ALOGE("socketpair failed: %s", strerror(errno));
        return;
    }
    mReceiveFd = fds[0];
    mSendFd = fds[1];
    DisplayEvents& events = DisplayEvents::get();
    std::lock_guard<std::mutex> lock(events.mutex);
    events.receivers.insert(this);
    events.startTimer();
}

DisplayEventReceiver::~DisplayEventReceiver() {
    {
        DisplayEvents& events = DisplayEvents::get();
        std::lock_guard<std::mutex> lock(events.mutex);
        events.receivers.erase(this);
    }
    if (mReceiveFd >= 0) close(mReceiveFd);
    if (mSendFd >= 0) close(mSendFd);
}

status_t DisplayEventReceiver::initCheck() const {
    return mReceiveFd >= 0 ? NO_ERROR : NO_INIT;
}

int DisplayEventReceiver::getFd() const {
    return mReceiveFd;
}

ssize_t DisplayEventReceiver::getEvents(Event* events, size_t count) {
    if (mReceiveFd < 0) return NO_INIT;
    size_t read = 0;
    while (read < count) {
        ssize_t received = recv(mReceiveFd, &events[read], sizeof(Event), MSG_DONTWAIT);
        if (received < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return -errno;
        }
        if (received != sizeof(Event)) break;
        read++;
    }
    return static_cast<ssize_t>(read);
}

status_t DisplayEventReceiver::setVsyncRate(uint32_t count) {
    DisplayEvents& events = DisplayEvents::get();
    std::lock_guard<std::mutex> lock(events.mutex);
    mVsyncRate = count;
    mVsyncCount = 0;
    events.wanted.notify_all();
    return NO_ERROR;
}

status_t DisplayEventReceiver::requestNextVsync() {
    DisplayEvents& events = DisplayEvents::get();
    std::lock_guard<std::mutex> lock(events.mutex);
    mVsyncRequested = true;
    events.wanted.notify_all();
    return NO_ERROR;
}

} // namespace android

using android::DisplayEventReceiver;
using android::DisplayEvents;

void mead_gui_set_display(const MeadDisplay* display) {
    DisplayEvents& events = DisplayEvents::get();
    std::lock_guard<std::mutex> lock(events.mutex);
    bool changed = memcmp(&events.display, display, sizeof(*display)) != 0;
    events.display = *display;
    bool announce = changed && events.displaySet;
    events.displaySet = true;
    if (!announce) return;
    DisplayEventReceiver::Event event{};
    event.header.type = DisplayEventReceiver::DISPLAY_EVENT_CONFIG_CHANGED;
    event.header.displayId = 0;
    event.header.timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
    event.config.configId = 0;
    for (DisplayEventReceiver* receiver : events.receivers) DisplayEvents::send(receiver, event);
}

void mead_gui_set_native_display(uint32_t kind, void* display) {
    DisplayEvents& events = DisplayEvents::get();
    std::lock_guard<std::mutex> lock(events.mutex);
    events.nativeKind = kind;
    events.nativeDisplay = display;
}

void mead_gui_get_native_display(uint32_t* kind, void** display) {
    DisplayEvents& events = DisplayEvents::get();
    std::lock_guard<std::mutex> lock(events.mutex);
    *kind = events.nativeKind;
    *display = events.nativeDisplay;
}

void mead_gui_get_display(MeadDisplay* display) {
    DisplayEvents& events = DisplayEvents::get();
    std::lock_guard<std::mutex> lock(events.mutex);
    *display = events.display;
}

void mead_gui_vsync(int64_t timestamp) {
    DisplayEvents& events = DisplayEvents::get();
    std::lock_guard<std::mutex> lock(events.mutex);
    if (!events.bridgeVsync) {
        events.bridgeVsync = true;
        events.wanted.notify_all();
    }
    events.vsync(timestamp);
}
