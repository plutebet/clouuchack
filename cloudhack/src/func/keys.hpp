// ================================================================================
// Hardware toggle key for the trigger.
//   The menu binds one key: the next press from a keyboard, gamepad or volume
//   button becomes the toggle, and each press of it flips cfg::trigger::enabled.
//   Reads go through evdev (/dev/input/event*). evdev hands every event to every
//   reader, so the game and the system still get the key: volume keys still change
//   the volume, gamepad buttons still act in the game.
//   The reader thread never touches ImGui or cfg. It only sets atomics that the
//   render loop consumes in apply_toggle(), which is the only writer of the flag.
//   The binding lives in memory, like the fire point.
// ================================================================================

#pragma once

#include <atomic>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <linux/input.h>
#include "../ui/cfg.hpp"
#include "../other/diag.hpp"

namespace keys {

    inline std::atomic<int> g_bound{-1};       // KEY_* / BTN_* code of the toggle, -1 = unbound
    inline std::atomic<bool> g_wait{false};    // menu: the next key press becomes the binding
    inline std::atomic<bool> g_toggle{false};  // set by the reader, consumed by apply_toggle()
    inline std::atomic<bool> g_started{false};

    inline int bound() { return g_bound.load(); }
    inline bool waiting() { return g_wait.load(); }
    inline void set_wait(bool on) { g_wait.store(on); }

    // Touchscreens report ABS_MT_POSITION_X. The touch reader owns those devices.
    inline bool is_touchscreen(int fd) {
        input_absinfo info{};
        return ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &info) != -1;
    }

    inline bool has_key_events(int fd) {
        unsigned char types[(EV_MAX + 7) / 8] = {};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(types)), types) == -1) return false;
        return ((types[EV_KEY / 8] >> (EV_KEY % 8)) & 1) != 0;
    }

    inline std::vector<int> open_key_devices() {
        std::vector<int> fds;
        DIR* dir = opendir("/dev/input");
        if (!dir) return fds;
        while (const dirent* entry = readdir(dir)) {
            if (!std::strstr(entry->d_name, "event")) continue;
            char path[300];
            std::snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
            const int fd = ::open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) continue;
            if (!has_key_events(fd) || is_touchscreen(fd)) {
                ::close(fd);
                continue;
            }
            fds.push_back(fd);
        }
        closedir(dir);
        return fds;
    }

    inline void reader() {
        const std::vector<int> fds = open_key_devices();
        if (fds.empty()) {
            diag::log("WARN", "keys: no keyboard/gamepad/volume device readable in /dev/input; toggle key unavailable");
            return;
        }
        diag::log("INFO", "keys: watching %zu key device(s)", fds.size());

        std::vector<pollfd> watch;
        for (int fd : fds) watch.push_back(pollfd{fd, POLLIN, 0});

        input_event events[32];
        while (true) {
            if (poll(watch.data(), watch.size(), 200) <= 0) continue;
            for (pollfd& p : watch) {
                if (p.fd < 0) continue;
                if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    ::close(p.fd);   // device unplugged: poll() ignores negative fds
                    p.fd = -1;
                    continue;
                }
                if (!(p.revents & POLLIN)) continue;
                const ssize_t got = ::read(p.fd, events, sizeof(events));
                if (got <= 0) continue;
                const size_t count = size_t(got) / sizeof(input_event);
                for (size_t i = 0; i < count; ++i) {
                    const input_event& ev = events[i];
                    if (ev.type != EV_KEY || ev.value != 1) continue;   // presses only, no auto-repeat
                    if (g_wait.exchange(false)) {
                        g_bound.store(int(ev.code));
                        diag::log("INFO", "keys: toggle key bound to code %d", int(ev.code));
                        continue;
                    }
                    if (int(ev.code) == g_bound.load()) g_toggle.store(true);
                }
            }
        }
    }

    inline void start() {
        if (g_started.exchange(true)) return;
        std::thread(reader).detach();
    }

    // Render loop only. The reader thread never writes cfg.
    inline void apply_toggle() {
        if (!g_toggle.exchange(false)) return;
        cfg::trigger::enabled = !cfg::trigger::enabled;
        diag::log("INFO", "trigger %s (toggle key)", cfg::trigger::enabled ? "on" : "off");
    }
}
