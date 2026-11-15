// Touch injection for external triggerbot — simulates fire button press
// via /dev/input multi-touch protocol B.
#pragma once

#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "diag.hpp"

namespace touch_inject {

    inline int g_fd = -1;
    inline bool g_down = false;
    inline int g_tracking_id = 9000;
    inline int g_slot = 9;

    inline int g_abs_x_max = 0;
    inline int g_abs_y_max = 0;

    inline float g_screen_w = 2720.f;
    inline float g_screen_h = 1224.f;
    inline uint8_t g_orientation = 0;

    inline void write_event(uint16_t type, uint16_t code, int32_t value) {
        if (g_fd < 0) return;
        struct input_event ev{};
        ev.type = type;
        ev.code = code;
        ev.value = value;
        ::write(g_fd, &ev, sizeof(ev));
    }

    inline void sync() { write_event(EV_SYN, SYN_REPORT, 0); }

    // Reverse the transform from Touch.cpp input_thread + calculateMultipliers
    inline void screen_to_raw(float sx, float sy, int& rx, int& ry) {
        float abs_max = std::fmax((float)g_abs_x_max, (float)g_abs_y_max);
        float abs_min = std::fmin((float)g_abs_x_max, (float)g_abs_y_max);
        bool landscape = g_screen_w > g_screen_h;
        float mult_x = g_screen_w / (landscape ? abs_max : abs_min);
        float mult_y = g_screen_h / (landscape ? abs_min : abs_max);

        float pre_x = sx / mult_x;
        float pre_y = sy / mult_y;

        switch (g_orientation) {
            case 1:
                ry = (int)pre_x;
                rx = g_abs_x_max - (int)pre_y;
                break;
            case 2:
                rx = g_abs_x_max - (int)pre_x;
                ry = g_abs_y_max - (int)pre_y;
                break;
            case 3:
                ry = g_abs_y_max - (int)pre_x;
                rx = (int)pre_y;
                break;
            default:
                rx = (int)pre_x;
                ry = (int)pre_y;
                break;
        }

        if (rx < 0) rx = 0;
        if (ry < 0) ry = 0;
        if (rx > g_abs_x_max) rx = g_abs_x_max;
        if (ry > g_abs_y_max) ry = g_abs_y_max;
    }

    inline bool init(float screen_w, float screen_h, uint8_t orientation) {
        g_screen_w = screen_w;
        g_screen_h = screen_h;
        g_orientation = orientation;

        if (g_fd >= 0) return true;

        DIR* dir = opendir("/dev/input/");
        if (!dir) return false;

        char path[64];
        struct dirent* ent;
        while ((ent = readdir(dir))) {
            if (!strstr(ent->d_name, "event")) continue;
            snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);

            int fd = open(path, O_RDWR);
            if (fd < 0) continue;

            input_absinfo abs_x{}, abs_y{};
            if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x) == 0 &&
                ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_y) == 0 &&
                abs_x.maximum > 0 && abs_y.maximum > 0) {

                g_fd = fd;
                g_abs_x_max = abs_x.maximum;
                g_abs_y_max = abs_y.maximum;

                input_absinfo abs_slot{};
                if (ioctl(fd, EVIOCGABS(ABS_MT_SLOT), &abs_slot) == 0 && abs_slot.maximum > 0)
                    g_slot = abs_slot.maximum;
                else
                    g_slot = 9;

                diag::log("TOUCH", "inject: %s abs_x=%d abs_y=%d slot=%d",
                          path, g_abs_x_max, g_abs_y_max, g_slot);
                closedir(dir);
                return true;
            }
            close(fd);
        }
        closedir(dir);
        diag::log("TOUCH", "inject: no writable touchscreen found");
        return false;
    }

    inline void update_orientation(uint8_t orientation) {
        g_orientation = orientation;
    }

    inline void fire_down(float sx, float sy) {
        if (g_fd < 0 || g_down) return;

        int rx, ry;
        screen_to_raw(sx, sy, rx, ry);

        write_event(EV_ABS, ABS_MT_SLOT, g_slot);
        write_event(EV_ABS, ABS_MT_TRACKING_ID, g_tracking_id);
        write_event(EV_ABS, ABS_MT_POSITION_X, rx);
        write_event(EV_ABS, ABS_MT_POSITION_Y, ry);
        write_event(EV_ABS, ABS_MT_TOUCH_MAJOR, 6);
        write_event(EV_ABS, ABS_MT_PRESSURE, 50);
        sync();

        g_down = true;
    }

    inline void fire_up() {
        if (g_fd < 0 || !g_down) return;

        write_event(EV_ABS, ABS_MT_SLOT, g_slot);
        write_event(EV_ABS, ABS_MT_TRACKING_ID, -1);
        sync();

        g_down = false;
        g_tracking_id++;
    }

    inline void shutdown() {
        if (g_down) fire_up();
        if (g_fd >= 0) { close(g_fd); g_fd = -1; }
    }
}
