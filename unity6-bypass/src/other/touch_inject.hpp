#pragma once
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/uinput.h>

namespace inject {

inline int g_fd = -1;
inline int g_max_x = 0;
inline int g_max_y = 0;

inline void write_ev(uint16_t type, uint16_t code, int32_t value) {
    struct input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    ::write(g_fd, &ev, sizeof(ev));
}

inline bool init(int screen_w, int screen_h) {
    g_max_x = screen_w;
    g_max_y = screen_h;
    g_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (g_fd < 0) return false;

    ioctl(g_fd, UI_SET_EVBIT, EV_ABS);
    ioctl(g_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(g_fd, UI_SET_EVBIT, EV_SYN);
    ioctl(g_fd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(g_fd, UI_SET_ABSBIT, ABS_MT_SLOT);
    ioctl(g_fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
    ioctl(g_fd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
    ioctl(g_fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
    ioctl(g_fd, UI_SET_ABSBIT, ABS_MT_TOUCH_MAJOR);
    ioctl(g_fd, UI_SET_ABSBIT, ABS_MT_PRESSURE);

    struct uinput_user_dev dev{};
    std::snprintf(dev.name, UINPUT_MAX_NAME_SIZE, "sf_vtouch");
    dev.id.bustype = BUS_VIRTUAL;
    dev.id.vendor = 0x1234;
    dev.id.product = 0x5678;
    dev.id.version = 1;
    dev.absmin[ABS_MT_POSITION_X] = 0;
    dev.absmax[ABS_MT_POSITION_X] = screen_w;
    dev.absmin[ABS_MT_POSITION_Y] = 0;
    dev.absmax[ABS_MT_POSITION_Y] = screen_h;
    dev.absmin[ABS_MT_SLOT] = 0;
    dev.absmax[ABS_MT_SLOT] = 9;
    dev.absmin[ABS_MT_TRACKING_ID] = 0;
    dev.absmax[ABS_MT_TRACKING_ID] = 65535;
    dev.absmin[ABS_MT_TOUCH_MAJOR] = 0;
    dev.absmax[ABS_MT_TOUCH_MAJOR] = 255;
    dev.absmin[ABS_MT_PRESSURE] = 0;
    dev.absmax[ABS_MT_PRESSURE] = 255;

    if (::write(g_fd, &dev, sizeof(dev)) < 0) { close(g_fd); g_fd = -1; return false; }
    if (ioctl(g_fd, UI_DEV_CREATE) < 0) { close(g_fd); g_fd = -1; return false; }
    usleep(100000);
    return true;
}

inline void tap_down(float sx, float sy) {
    if (g_fd < 0) return;
    int x = static_cast<int>(sx);
    int y = static_cast<int>(sy);
    write_ev(EV_ABS, ABS_MT_SLOT, 0);
    write_ev(EV_ABS, ABS_MT_TRACKING_ID, 100);
    write_ev(EV_ABS, ABS_MT_POSITION_X, x);
    write_ev(EV_ABS, ABS_MT_POSITION_Y, y);
    write_ev(EV_ABS, ABS_MT_TOUCH_MAJOR, 6);
    write_ev(EV_ABS, ABS_MT_PRESSURE, 50);
    write_ev(EV_KEY, BTN_TOUCH, 1);
    write_ev(EV_SYN, SYN_REPORT, 0);
}

inline void tap_up() {
    if (g_fd < 0) return;
    write_ev(EV_ABS, ABS_MT_TRACKING_ID, -1);
    write_ev(EV_KEY, BTN_TOUCH, 0);
    write_ev(EV_SYN, SYN_REPORT, 0);
}

inline void shutdown() {
    if (g_fd >= 0) {
        ioctl(g_fd, UI_DEV_DESTROY);
        close(g_fd);
        g_fd = -1;
    }
}

}
