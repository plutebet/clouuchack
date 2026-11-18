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
    uint8_t szi[32];   // szi+0x10..+0x2F for vis scan
    bool szi_ok;
};

// ---- damage-feedback wall detection ----
inline std::unordered_map<uint64_t, long long> s_wall; // id -> unblock time
inline uint64_t s_wt_id = 0;
inline int      s_wt_hp = 0;
inline uint32_t s_wt_fires = 0;
inline long long s_wt_first = 0;
inline long long s_wt_last = 0;
inline uint32_t s_wall_blocks = 0;

static constexpr uint32_t kWallFires = 3;
static constexpr long long kWallTimeMs = 250;
static constexpr long long kWallCooldownMs = 1500;
static constexpr long long kWallStaleMs = 2000;

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

// ---- sphere hitbox (head or body center) ----
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

// ---- full-body capsule hitbox (head to feet) ----
inline bool hit_capsule(const Vector3& head, const Vector3& feet,
                        const matrix& view, const matrix& proj, float& depth) {
    Vector4 cam_h = matrix_multiply(view, Vector4(head.x, head.y, head.z, 1.f));
    Vector4 clip_h = matrix_multiply(proj, cam_h);
    if (!std::isfinite(clip_h.w) || clip_h.w < 0.05f) return false;

    Vector4 cam_f = matrix_multiply(view, Vector4(feet.x, feet.y, feet.z, 1.f));
    Vector4 clip_f = matrix_multiply(proj, cam_f);
    if (!std::isfinite(clip_f.w) || clip_f.w < 0.05f) return false;

    float hx = clip_h.x / clip_h.w * g_sw * 0.5f;
    float hy = clip_h.y / clip_h.w * g_sh * 0.5f;
    float fx = clip_f.x / clip_f.w * g_sw * 0.5f;
    float fy = clip_f.y / clip_f.w * g_sh * 0.5f;

    if (!std::isfinite(hx) || !std::isfinite(hy) ||
        !std::isfinite(fx) || !std::isfinite(fy)) return false;

    float dx = fx - hx, dy = fy - hy;
    float len2 = dx * dx + dy * dy;
    float height = std::sqrt(len2);

    float half_w = std::clamp(height * 0.20f, 6.f, g_sw * 0.5f);
    half_w *= cfg::trigger::fov;

    float t = len2 > 0.001f ? std::clamp((-hx * dx + -hy * dy) / len2, 0.f, 1.f) : 0.f;
    float cx = hx + t * dx;
    float cy = hy + t * dy;
    float dist2 = cx * cx + cy * cy;

    depth = clip_h.w * (1.f - t) + clip_f.w * t;
    return dist2 <= half_w * half_w;
}

inline uint64_t plan(const target* targets, int count,
                     const matrix& view, const matrix& proj, long long now_ms) {
    ++s_seq;
    uint64_t chosen = 0;
    float best = 1e9f;

    if (cfg::trigger::vis_check) {
        for (auto it = s_wall.begin(); it != s_wall.end(); )
            if (now_ms >= it->second) it = s_wall.erase(it); else ++it;
    }

    for (int i = 0; i < count; ++i) {
        const target& t = targets[i];

        if (cfg::trigger::vis_check && s_wall.count(t.id)) continue;

        bool hit = false;
        float depth = 0.f;

        if (cfg::trigger::region == 1) {
            hit = hit_point(t.head, view, proj, 0.14f, depth);
        } else if (cfg::trigger::region == 2) {
            Vector3 body = t.feet + (t.head - t.feet) * 0.65f;
            hit = hit_point(body, view, proj, 0.25f, depth);
        } else {
            hit = hit_capsule(t.head, t.feet, view, proj, depth);
        }

        if (hit && depth < best) {
            best = depth;
            chosen = t.id;
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
        diag::log("TRIGGER", "status: en=%d fire_set=%d pos=(%.0f,%.0f) tgts=%d pressed=%d fires=%u walls=%u wt=(0x%llx f=%u hp=%d) fd=%d vc=%d",
                  int(cfg::trigger::enabled), int(cfg::trigger::fire_set),
                  cfg::trigger::fire_x, cfg::trigger::fire_y,
                  count, int(s_pressed), s_fire_count, s_wall_blocks,
                  (unsigned long long)s_wt_id, s_wt_fires, s_wt_hp,
                  inject::g_fd, int(cfg::trigger::vis_check));
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
    if (!id) return;

    // ---- damage-feedback wall check ----
    int hp = 0;
    const target* tgt = nullptr;
    for (int i = 0; i < count; ++i)
        if (targets[i].id == id) { hp = targets[i].hp; tgt = &targets[i]; break; }

    if (cfg::trigger::vis_check) {
        if (id != s_wt_id || (s_wt_fires > 0 && now_ms - s_wt_last > kWallStaleMs)) {
            s_wt_id = id;
            s_wt_hp = hp;
            s_wt_fires = 0;
            s_wt_first = now_ms;
        }

        if (hp < s_wt_hp && hp > 0) {
            s_wt_hp = hp;
            s_wt_fires = 0;
            s_wt_first = now_ms;
            s_wall.erase(id);
        }

        if (s_wt_fires >= kWallFires &&
            now_ms - s_wt_first >= kWallTimeMs &&
            hp >= s_wt_hp) {
            s_wall[id] = now_ms + kWallCooldownMs;
            ++s_wall_blocks;
            diag::log("TRIGGER", "WALL id=0x%llx hp=%d fires=%u dt=%lldms block=%lldms (total=%u)",
                      (unsigned long long)id, hp, s_wt_fires,
                      now_ms - s_wt_first, kWallCooldownMs, s_wall_blocks);
            s_wt_id = 0;
            s_wt_fires = 0;
            reset();
            return;
        }

        s_wt_fires++;
        s_wt_last = now_ms;
    }

    inject::tap_down(cfg::trigger::fire_x, cfg::trigger::fire_y);
    s_pressed = true;
    s_release_at = now_ms + 30;
    ++s_fire_count;

    // fire log with szi hex dump for vis-byte discovery
    char szi_hex[32 * 3 + 1] = {0};
    if (tgt && tgt->szi_ok) {
        for (int j = 0; j < 32; ++j)
            std::snprintf(szi_hex + j * 3, 4, "%02x ", tgt->szi[j]);
    }
    diag::log("TRIGGER", "fire #%u id=0x%llx hp=%d conf=%u wf=%u szi=[%s]",
              s_fire_count, (unsigned long long)id, hp, s_confirmed, s_wt_fires,
              tgt && tgt->szi_ok ? szi_hex : "N/A");
}

}
