// ================================================================================
// Trigger: taps the saved fire point while an enemy stays on the crosshair.
//   time checker  - the enemy must stay under the crosshair for lock_ms
//   visible check - Physics.Linecast camera -> enemy head; a hit in front of the
//                   enemy (wall, prop) blocks the shot
//   tap           - `input tap x y` after tap_delay_ms, at most one per cooldown_ms
// Assumptions, verify on device:
//   - the crosshair is the screen centre
//   - g_sw/g_sh and `input tap` use the same pixel space (same orientation)
// ================================================================================

#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    inline int linecast_state = 0;     // 0 unchecked, 1 ok, -1 RVA outside .text

    inline long long now_us() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    using linecast_hit_fn = bool (*)(Vector3 from, Vector3 to, void* hit, int32_t layer_mask, const void* method);

    // Camera world position from the row-major view matrix (same formula as visuals.cpp).
    inline Vector3 camera_position(const matrix& view) {
        const float* v = reinterpret_cast<const float*>(&view);
        return Vector3(
            -(v[0] * v[12] + v[1] * v[13] + v[2] * v[14]),
            -(v[4] * v[12] + v[5] * v[13] + v[6] * v[14]),
            -(v[8] * v[12] + v[9] * v[13] + v[10] * v[14]));
    }

    // true when nothing stands between the camera and the enemy head.
    inline bool visible(const matrix& view, Vector3 head) {
        if (!proc::lib) return true;
        if (linecast_state == 0) {
            linecast_state = (v100::physics_linecast_hit >= v100::text_begin &&
                              v100::physics_linecast_hit < v100::text_end) ? 1 : -1;
            diag::log("TRIGGER", "linecast rva=0x%llx state=%d", (unsigned long long)v100::physics_linecast_hit, linecast_state);
        }
        if (linecast_state < 0) return true;   // never call an address we cannot vouch for

        const Vector3 from = camera_position(view);
        const Vector3 d = head - from;
        const float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);

        // RaycastHit (dump.cs): point 0x00, normal 0x0C, faceID 0x18, distance 0x1C, uv 0x20, collider 0x28.
        alignas(8) uint8_t hit[0x40] = {};
        const auto fn = reinterpret_cast<linecast_hit_fn>(proc::lib + v100::physics_linecast_hit);
        if (!fn(from, head, hit, -1, nullptr)) return true;

        float hit_dist = 0.f;
        std::memcpy(&hit_dist, hit + 0x1C, sizeof(hit_dist));
        if (hit_dist < 0.3f) return true;        // camera-side collider, not a wall
        return hit_dist >= dist - 0.6f;          // first hit is the enemy's own collider
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
        if (!cfg::trigger::enabled) { lock_object = 0; return; }

        const ImVec2 center(g_sw * .5f, g_sh * .5f);
        const target* best = nullptr;
        for (const target& t : targets) {
            if (center.x < t.box_min.x || center.x > t.box_max.x || center.y < t.box_min.y || center.y > t.box_max.y) continue;
            if (!best || t.distance < best->distance) best = &t;
        }
        if (!best || (cfg::trigger::visible_only && !visible(view, best->head))) {
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
