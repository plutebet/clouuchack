#pragma once
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../ui/cfg.hpp"
#include "../other/touch_inject.hpp"
#include "../other/diag.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace triggerbot {

struct target {
    uint64_t id;
    Vector3 head;
    Vector3 feet;
};

inline uint64_t s_tracking = 0;
inline uint64_t s_since = 0;
inline uint32_t s_confirmed = 0;
inline uint32_t s_last_confirm_seq = 0;
inline uint64_t s_next_shot = 0;
inline uint32_t s_last_shot_seq = 0;
inline uint32_t s_seq = 0;
inline bool s_pressed = false;
inline long long s_release_at = 0;

inline void reset() {
    s_tracking = 0;
    s_since = 0;
    s_confirmed = 0;
    s_last_confirm_seq = 0;
}

inline bool hit_point(const Vector3& point, const matrix& view, const matrix& proj,
                      float radius, float& depth) {
    Vector4 cam = matrix_multiply(view, Vector4(point.x, point.y, point.z, 1.f));
    Vector4 clip = matrix_multiply(proj, cam);
    depth = clip.w;
    if (!std::isfinite(depth) || depth < 0.05f) return false;

    float x = clip.x / depth * g_sw * 0.5f;
    float y = clip.y / depth * g_sh * 0.5f;
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    if (std::abs(x) > g_sw * 0.5f - 2.f || std::abs(y) > g_sh * 0.5f - 2.f) return false;

    const float* p = reinterpret_cast<const float*>(&proj);
    float pixels = std::clamp(radius * std::abs(p[0]) * g_sw * 0.5f / depth, 0.25f, 16.f);
    pixels *= cfg::trigger::fov;

    return x * x + y * y <= pixels * pixels;
}

inline uint64_t plan(const target* targets, int count,
                     const matrix& view, const matrix& proj, long long now_ms) {
    ++s_seq;
    uint64_t chosen = 0;
    float best = 1e9f;

    for (int i = 0; i < count; ++i) {
        const target& t = targets[i];
        for (int part = 0; part < 2; ++part) {
            if (cfg::trigger::region == 1 && part != 0) continue;
            if (cfg::trigger::region == 2 && part != 1) continue;

            float depth = 0.f;
            Vector3 point = part == 0
                ? t.head
                : t.feet + (t.head - t.feet) * 0.65f;
            float radius = part == 0 ? 0.09f : 0.16f;

            if (hit_point(point, view, proj, radius, depth) && depth < best) {
                best = depth;
                chosen = t.id;
            }
        }
    }

    if (!chosen) { reset(); return 0; }

    if (chosen != s_tracking) {
        s_tracking = chosen;
        s_since = now_ms;
        s_last_confirm_seq = 0;
        s_confirmed = 0;
    }

    if (s_seq != s_last_confirm_seq) {
        s_last_confirm_seq = s_seq;
        ++s_confirmed;
    }

    const uint32_t need = static_cast<uint32_t>(std::clamp(cfg::trigger::confirm, 1, 4));
    const int delay = std::clamp(cfg::trigger::delay_ms, 0, 300);
    const int interval = std::clamp(cfg::trigger::interval_ms, 1, 500);

    if (s_confirmed < need) return 0;
    if (now_ms - s_since < delay) return 0;
    if (now_ms < static_cast<long long>(s_next_shot)) return 0;
    if (s_last_shot_seq == s_seq) return 0;

    s_next_shot = now_ms + interval;
    s_last_shot_seq = s_seq;
    return chosen;
}

inline void tick(const target* targets, int count,
                 const matrix& view, const matrix& proj, long long now_ms) {
    if (s_pressed && now_ms >= s_release_at) {
        inject::tap_up();
        s_pressed = false;
    }

    if (!cfg::trigger::enabled) {
        if (s_pressed) { inject::tap_up(); s_pressed = false; }
        reset();
        return;
    }

    if (s_pressed) return;

    uint64_t id = plan(targets, count, view, proj, now_ms);
    if (!id) return;

    inject::tap_down(cfg::trigger::fire_x, cfg::trigger::fire_y);
    s_pressed = true;
    s_release_at = now_ms + 16;

    static long long fire_log = 0;
    if (diag::every(fire_log, 500))
        diag::log("TRIGGER", "fire target=0x%llx confirmed=%u",
                  (unsigned long long)id, s_confirmed);
}

}
