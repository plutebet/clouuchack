#pragma once
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../ui/cfg.hpp"
#include "../other/touch_inject.hpp"
#include "../other/diag.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace triggerbot {

struct target {
    uint64_t id;
    Vector3 head;
    Vector3 feet;
    int hp;
    int max_hp;
};

// ---- damage feedback vis check ----
// Track HP of the target we're shooting. If N taps don't reduce HP,
// the target is behind a wall — blacklist it for a cooldown period.
struct damage_fb {
    int start_hp{0};
    uint32_t taps{0};
    long long started{0};
};
inline std::unordered_map<uint64_t, damage_fb> s_dmg;
inline std::unordered_map<uint64_t, long long> s_wall; // id -> unblock time

static constexpr uint32_t kWallTaps = 5;
static constexpr long long kWallTimeMs = 400;
static constexpr long long kWallCooldownMs = 1500;

inline uint64_t s_fire_target = 0;
inline uint32_t s_wall_blocks = 0;

// ---- core state ----
inline uint64_t s_tracking = 0;
inline long long s_since = 0;
inline uint32_t s_confirmed = 0;
inline uint32_t s_last_confirm_seq = 0;
inline long long s_next_shot = 0;
inline uint32_t s_last_shot_seq = 0;
inline uint32_t s_seq = 0;
inline bool s_pressed = false;
inline long long s_release_at = 0;
inline uint32_t s_fire_count = 0;

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

    float ndc_x = clip.x / depth;
    float ndc_y = clip.y / depth;
    if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y)) return false;

    float px = ndc_x * g_sw * 0.5f;
    float py = ndc_y * g_sh * 0.5f;

    const float* p = reinterpret_cast<const float*>(&proj);
    float px_radius = std::clamp(radius * std::abs(p[0]) * g_sw * 0.5f / depth, 1.f, g_sw);
    px_radius *= cfg::trigger::fov;

    return px * px + py * py <= px_radius * px_radius;
}

inline uint64_t plan(const target* targets, int count,
                     const matrix& view, const matrix& proj, long long now_ms) {
    ++s_seq;
    uint64_t chosen = 0;
    float best = 1e9f;

    // clean expired blacklist
    if (cfg::trigger::vis_check) {
        for (auto it = s_wall.begin(); it != s_wall.end(); )
            if (now_ms >= it->second) it = s_wall.erase(it); else ++it;
    }

    for (int i = 0; i < count; ++i) {
        const target& t = targets[i];

        // damage feedback: skip wall-blocked targets
        if (cfg::trigger::vis_check && s_wall.count(t.id)) continue;

        for (int part = 0; part < 2; ++part) {
            if (cfg::trigger::region == 1 && part != 0) continue;
            if (cfg::trigger::region == 2 && part != 1) continue;

            float depth = 0.f;
            Vector3 point = part == 0
                ? t.head
                : t.feet + (t.head - t.feet) * 0.65f;
            float radius = part == 0 ? 0.14f : 0.25f;

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
    if (now_ms < s_next_shot) return 0;
    if (s_last_shot_seq == s_seq) return 0;

    s_next_shot = now_ms + interval;
    s_last_shot_seq = s_seq;
    return chosen;
}

inline void tick(const target* targets, int count,
                 const matrix& view, const matrix& proj, long long now_ms) {
    static long long status_log = 0;
    if (diag::every(status_log, 3000)) {
        diag::log("TRIGGER", "status: en=%d fire_set=%d pos=(%.0f,%.0f) tgts=%d pressed=%d fires=%u walls=%u fd=%d vc=%d",
                  int(cfg::trigger::enabled), int(cfg::trigger::fire_set),
                  cfg::trigger::fire_x, cfg::trigger::fire_y,
                  count, int(s_pressed), s_fire_count, s_wall_blocks, inject::g_fd,
                  int(cfg::trigger::vis_check));
    }

    if (s_pressed && now_ms >= s_release_at) {
        inject::tap_up();
        s_pressed = false;
    }

    if (!cfg::trigger::enabled) {
        if (s_pressed) { inject::tap_up(); s_pressed = false; }
        reset();
        return;
    }

    if (!cfg::trigger::fire_set) return;

    if (s_pressed) return;

    uint64_t id = plan(targets, count, view, proj, now_ms);
    if (!id) {
        // lost target — reset damage tracker
        if (s_fire_target) { s_dmg.erase(s_fire_target); s_fire_target = 0; }
        return;
    }

    // ---- damage feedback vis check ----
    if (cfg::trigger::vis_check) {
        // find this target's HP
        int hp = 0;
        for (int i = 0; i < count; ++i)
            if (targets[i].id == id) { hp = targets[i].hp; break; }

        // target changed — reset old tracker
        if (id != s_fire_target) {
            if (s_fire_target) s_dmg.erase(s_fire_target);
            s_fire_target = id;
            s_dmg.erase(id);
        }

        auto& df = s_dmg[id];
        if (df.taps == 0) {
            // first tap at this target
            df.start_hp = hp;
            df.started = now_ms;
        } else if (hp < df.start_hp) {
            // HP dropped — our shots are landing, target is visible
            df.start_hp = hp;
            df.taps = 0;
            df.started = now_ms;
            s_wall.erase(id);
        }

        // wall detection: enough taps with no HP change
        if (df.taps >= kWallTaps && now_ms - df.started >= kWallTimeMs && hp >= df.start_hp) {
            s_wall[id] = now_ms + kWallCooldownMs;
            ++s_wall_blocks;
            diag::log("TRIGGER", "WALL id=0x%llx hp=%d after %u taps, block %lldms (total walls=%u)",
                      (unsigned long long)id, hp, df.taps, kWallCooldownMs, s_wall_blocks);
            df.taps = 0;
            df.started = now_ms;
            reset();
            return;
        }

        df.taps++;
    }

    inject::tap_down(cfg::trigger::fire_x, cfg::trigger::fire_y);
    s_pressed = true;
    s_release_at = now_ms + 40;
    ++s_fire_count;

    diag::log("TRIGGER", "fire #%u target=0x%llx hp=%d confirmed=%u pos=(%.0f,%.0f)",
              s_fire_count, (unsigned long long)id,
              [&]{ for (int i = 0; i < count; ++i) if (targets[i].id == id) return targets[i].hp; return 0; }(),
              s_confirmed, cfg::trigger::fire_x, cfg::trigger::fire_y);
}

}
