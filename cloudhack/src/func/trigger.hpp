// ================================================================================
// Trigger: taps the saved fire point while an enemy stays on the crosshair.
//   time checker  - the enemy must stay under the crosshair for lock_ms
//   tap           - `input tap x y` after tap_delay_ms, at most one per cooldown_ms
// No wall check. The overlay is a separate process, so it cannot call game code
// (Physics.Linecast) by address: the call landed in the overlay's own address
// space, faulted on every enemy at the crosshair, and the supervisor restarted it.
// Assumptions, verify on device:
//   - the crosshair is the screen centre
//   - g_sw/g_sh and `input tap` use the same pixel space (same orientation)
// ================================================================================

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include <unistd.h>
#include "imgui.h"
#include "../game/game.hpp"
#include "../other/vector3.h"
#include "../other/diag.hpp"
#include "../ui/cfg.hpp"
#include "../ui/theme/theme.hpp"

namespace trigger {

    struct target {
        uint64_t object{};
        Vector3 head{};
        float distance{0.f};
        ImVec2 box_min{}, box_max{};   // same box the ESP draws
    };

    inline uint64_t lock_object = 0;
    inline long long lock_since_us = 0;
    inline long long lock_seen_us = 0;
    inline long long last_tap_us = 0;
    inline std::atomic<bool> tap_busy{false};

    inline long long now_us() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    inline void fire(int x, int y, int delay_ms) {
        if (tap_busy.exchange(true)) return;
        std::thread([x, y, delay_ms] {
            if (delay_ms > 0) usleep(useconds_t(delay_ms) * 1000u);
            char cmd[96];
            std::snprintf(cmd, sizeof(cmd), "/system/bin/input tap %d %d", x, y);
            (void)std::system(cmd);
            tap_busy.store(false);
        }).detach();
    }

    inline void update(const std::vector<target>& targets, const matrix& view) {
        (void)view;   // kept for the call site; there is no wall check (see header)
        if (!cfg::trigger::enabled) { lock_object = 0; return; }

        const ImVec2 center(g_sw * .5f, g_sh * .5f);
        const target* best = nullptr;
        for (const target& t : targets) {
            if (center.x < t.box_min.x || center.x > t.box_max.x || center.y < t.box_min.y || center.y > t.box_max.y) continue;
            if (!best || t.distance < best->distance) best = &t;
        }
        if (!best) {
            lock_object = 0;
            return;
        }

        const long long now = now_us();
        // A lock that was not refreshed recently (frame skipped, target left the crosshair) starts over.
        if (best->object != lock_object || now - lock_seen_us > 250000) {
            lock_object = best->object;
            lock_since_us = now;
            lock_seen_us = now;
            return;
        }
        lock_seen_us = now;
        if (now - lock_since_us < cfg::trigger::lock_ms * 1000LL) return;
        if (!cfg::trigger::fire_set()) return;
        if (now - last_tap_us < cfg::trigger::cooldown_ms * 1000LL) return;
        if (tap_busy.load()) return;

        last_tap_us = now;
        fire(cfg::trigger::fire_x, cfg::trigger::fire_y, cfg::trigger::tap_delay_ms);
    }
}
