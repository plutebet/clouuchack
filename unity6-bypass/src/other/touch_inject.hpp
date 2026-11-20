#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include "diag.hpp"

namespace inject {

inline int g_fd = -1;
inline input_absinfo g_abs_x{};
inline input_absinfo g_abs_y{};
inline int g_screen_w = 0;
inline int g_screen_h = 0;
inline bool g_device_portrait = false;
inline int g_tracking_id = 10000;
// A press must never outlive its intended hold: every tap is timestamped and
// the render loop closes an overstayed tap even when the trigger does not get
// a frame (early returns, menu open, lost camera).
inline bool g_tap_active = false;
inline long long g_tap_down_ms = 0;

inline void write_ev(uint16_t type, uint16_t code, int32_t value) {
    struct input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    ::write(g_fd, &ev, sizeof(ev));
}

inline void screen_to_device(float sx, float sy, int32_t& dx, int32_t& dy) {
    // Overlay is ALWAYS landscape (screen_w > screen_h = 2720x1224).
    // Touch device coordinates are in native (physical) orientation.
    // On a portrait-native device (abs_y > abs_x), we must rotate:
    //   overlay X (long edge) → device Y (long axis)
    //   overlay Y (short edge) → device X (short axis), inverted
    if (g_device_portrait) {
        float scale_long  = static_cast<float>(g_abs_y.maximum) / static_cast<float>(g_screen_w);
        float scale_short = static_cast<float>(g_abs_x.maximum) / static_cast<float>(g_screen_h);
        dy = static_cast<int32_t>(sx * scale_long);
        dx = g_abs_x.maximum - static_cast<int32_t>(sy * scale_short);
    } else {
        float scale_x = static_cast<float>(g_abs_x.maximum) / static_cast<float>(g_screen_w);
        float scale_y = static_cast<float>(g_abs_y.maximum) / static_cast<float>(g_screen_h);
        dx = static_cast<int32_t>(sx * scale_x);
        dy = static_cast<int32_t>(sy * scale_y);
    }
    dx = std::clamp(dx, g_abs_x.minimum, g_abs_x.maximum);
    dy = std::clamp(dy, g_abs_y.minimum, g_abs_y.maximum);
}

inline bool init(int screen_w, int screen_h, uint8_t orientation) {
    (void)orientation;
    g_screen_w = screen_w;
    g_screen_h = screen_h;

    const char* path = "/dev/input/";
    auto dir = opendir(path);
    if (!dir) return false;

    dirent* ptr;
    while ((ptr = readdir(dir))) {
        if (!strstr(ptr->d_name, "event")) continue;
        char ev_path[300];
        std::snprintf(ev_path, sizeof(ev_path), "%s%s", path, ptr->d_name);

        int fd = open(ev_path, O_WRONLY);
        if (fd < 0) continue;

        input_absinfo abs_x{}, abs_y{};
        if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x) == -1 ||
            ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_y) == -1 ||
            abs_x.maximum <= 0 || abs_y.maximum <= 0) {
            ::close(fd);
            continue;
        }

        g_fd = fd;
        g_abs_x = abs_x;
        g_abs_y = abs_y;
        g_device_portrait = abs_y.maximum > abs_x.maximum;

        diag::log("INJECT", "device=%s absX=[%d..%d] absY=[%d..%d] portrait=%d sw=%d sh=%d",
                  ev_path, abs_x.minimum, abs_x.maximum, abs_y.minimum, abs_y.maximum,
                  int(g_device_portrait), screen_w, screen_h);

        closedir(dir);
        return true;
    }
    closedir(dir);
    return false;
}

inline void tap_up();

inline void tap_down(float sx, float sy) {
    if (g_fd < 0) return;
    if (g_tap_active) tap_up();   // never leak a second tracking id
    int32_t dx, dy;
    screen_to_device(sx, sy, dx, dy);
    if (++g_tracking_id > 60000) g_tracking_id = 10000;

    static int log_count = 0;
    if (log_count < 10) {
        ++log_count;
        diag::log("INJECT", "tap screen=(%.0f,%.0f) device=(%d,%d) tid=%d",
                  sx, sy, dx, dy, g_tracking_id);
    }

    write_ev(EV_ABS, ABS_MT_SLOT, 9);
    write_ev(EV_ABS, ABS_MT_TRACKING_ID, g_tracking_id);
    write_ev(EV_ABS, ABS_MT_POSITION_X, dx);
    write_ev(EV_ABS, ABS_MT_POSITION_Y, dy);
    write_ev(EV_ABS, ABS_MT_TOUCH_MAJOR, 6);
    write_ev(EV_ABS, ABS_MT_PRESSURE, 50);
    write_ev(EV_SYN, SYN_REPORT, 0);
    g_tap_active = true;
    g_tap_down_ms = diag::now_ms();
}

inline void tap_up() {
    if (g_fd < 0) return;
    write_ev(EV_ABS, ABS_MT_SLOT, 9);
    write_ev(EV_ABS, ABS_MT_TRACKING_ID, -1);
    write_ev(EV_SYN, SYN_REPORT, 0);
    g_tap_active = false;
}

// Called once per rendered frame, unconditionally.
inline void watchdog(long long now_ms, long long max_hold_ms) {
    if (g_tap_active && now_ms - g_tap_down_ms > max_hold_ms) tap_up();
}

// ---- aimbot touch: flick gesture on slot 8 ----
inline void aim_flick(float dx_screen, float dy_screen) {
    if (g_fd < 0) return;
    if (std::fabs(dx_screen) < 0.3f && std::fabs(dy_screen) < 0.3f) return;

    float base_x = g_screen_w * 0.75f;
    float base_y = g_screen_h * 0.5f;

    int32_t d1x, d1y, d2x, d2y;
    screen_to_device(base_x, base_y, d1x, d1y);
    screen_to_device(base_x + dx_screen, base_y + dy_screen, d2x, d2y);

    static int aim_tid = 20000;
    if (++aim_tid > 50000) aim_tid = 20000;

    write_ev(EV_ABS, ABS_MT_SLOT, 8);
    write_ev(EV_ABS, ABS_MT_TRACKING_ID, aim_tid);
    write_ev(EV_ABS, ABS_MT_POSITION_X, d1x);
    write_ev(EV_ABS, ABS_MT_POSITION_Y, d1y);
    write_ev(EV_ABS, ABS_MT_TOUCH_MAJOR, 6);
    write_ev(EV_ABS, ABS_MT_PRESSURE, 50);
    write_ev(EV_SYN, SYN_REPORT, 0);

    write_ev(EV_ABS, ABS_MT_SLOT, 8);
    write_ev(EV_ABS, ABS_MT_POSITION_X, d2x);
    write_ev(EV_ABS, ABS_MT_POSITION_Y, d2y);
    write_ev(EV_SYN, SYN_REPORT, 0);

    write_ev(EV_ABS, ABS_MT_SLOT, 8);
    write_ev(EV_ABS, ABS_MT_TRACKING_ID, -1);
    write_ev(EV_SYN, SYN_REPORT, 0);
}

inline void shutdown() {
    if (g_fd >= 0) { ::close(g_fd); g_fd = -1; }
}

}
