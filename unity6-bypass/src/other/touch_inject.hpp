#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
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
inline uint8_t g_orientation = 0;
inline float g_mul_x = 1.f;
inline float g_mul_y = 1.f;
inline int g_tracking_id = 10000;

inline void write_ev(uint16_t type, uint16_t code, int32_t value) {
    struct input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    ::write(g_fd, &ev, sizeof(ev));
}

// Inverse of the touch reader's orientation transform (Touch.cpp).
inline void screen_to_device(float sx, float sy, int32_t& dx, int32_t& dy) {
    float raw_x = sx / g_mul_x;
    float raw_y = sy / g_mul_y;
    switch (g_orientation) {
        case 1:
            dx = g_abs_x.maximum - static_cast<int32_t>(raw_y);
            dy = static_cast<int32_t>(raw_x);
            break;
        case 2:
            dx = g_abs_x.maximum - static_cast<int32_t>(raw_x);
            dy = g_abs_y.maximum - static_cast<int32_t>(raw_y);
            break;
        case 3:
            dx = static_cast<int32_t>(raw_y);
            dy = g_abs_y.maximum - static_cast<int32_t>(raw_x);
            break;
        default:
            dx = static_cast<int32_t>(raw_x);
            dy = static_cast<int32_t>(raw_y);
            break;
    }
    if (dx < g_abs_x.minimum) dx = g_abs_x.minimum;
    if (dx > g_abs_x.maximum) dx = g_abs_x.maximum;
    if (dy < g_abs_y.minimum) dy = g_abs_y.minimum;
    if (dy > g_abs_y.maximum) dy = g_abs_y.maximum;
}

inline bool init(int screen_w, int screen_h, uint8_t orientation) {
    g_orientation = orientation;

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

        float mx = std::fmax(static_cast<float>(abs_x.maximum), static_cast<float>(abs_y.maximum));
        float mn = std::fmin(static_cast<float>(abs_x.maximum), static_cast<float>(abs_y.maximum));
        g_mul_x = static_cast<float>(screen_w) / (screen_w > screen_h ? mx : mn);
        g_mul_y = static_cast<float>(screen_h) / (screen_w > screen_h ? mn : mx);

        diag::log("INJECT", "device=%s absX=[%d..%d] absY=[%d..%d] mul=%.4f,%.4f orient=%d",
                  ev_path, abs_x.minimum, abs_x.maximum, abs_y.minimum, abs_y.maximum,
                  g_mul_x, g_mul_y, int(orientation));

        closedir(dir);
        return true;
    }
    closedir(dir);
    return false;
}

inline void tap_down(float sx, float sy) {
    if (g_fd < 0) return;
    int32_t dx, dy;
    screen_to_device(sx, sy, dx, dy);
    if (++g_tracking_id > 60000) g_tracking_id = 10000;

    static int log_count = 0;
    if (log_count < 5) {
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
}

inline void tap_up() {
    if (g_fd < 0) return;
    write_ev(EV_ABS, ABS_MT_SLOT, 9);
    write_ev(EV_ABS, ABS_MT_TRACKING_ID, -1);
    write_ev(EV_SYN, SYN_REPORT, 0);
}

inline void shutdown() {
    if (g_fd >= 0) { ::close(g_fd); g_fd = -1; }
}

}
