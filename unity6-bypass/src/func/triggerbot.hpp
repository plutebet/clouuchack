#pragma once
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../ui/cfg.hpp"
#include "../other/touch_inject.hpp"
#include "../other/diag.hpp"
#include "../game/player.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace triggerbot {

struct target {
    uint64_t id;
    Vector3 head;
    Vector3 feet;
    int hp;
    int max_hp;
    bool visible;              // memory-based vis check (szi.mow + anim.IsVisible)
    player::BoneData bones;    // bone positions (Head=10 for precise headshot)
};

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

    for (int i = 0; i < count; ++i) {
        const target& t = targets[i];

        if (cfg::trigger::vis_check && !t.visible) continue;

        bool hit = false;
        float depth = 1e9f;

        if (t.bones.mask) {
            for (int b = 0; b < player::kBoneCount; ++b) {
                if (!t.bones.has(b)) continue;
                float d = 0.f;
                if (hit_point(t.bones.positions[b], view, proj, 0.10f, d) && d < depth) {
                    depth = d; hit = true;
                }
            }
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
        diag::log("TRIGGER", "status: en=%d fire_set=%d pos=(%.0f,%.0f) tgts=%d pressed=%d fires=%u fd=%d vc=%d",
                  int(cfg::trigger::enabled), int(cfg::trigger::fire_set),
                  cfg::trigger::fire_x, cfg::trigger::fire_y,
                  count, int(s_pressed), s_fire_count,
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

    int hp = 0;
    for (int i = 0; i < count; ++i)
        if (targets[i].id == id) { hp = targets[i].hp; break; }

    inject::tap_down(cfg::trigger::fire_x, cfg::trigger::fire_y);
    s_pressed = true;
    s_release_at = now_ms + 30;
    ++s_fire_count;

    diag::log("TRIGGER", "fire #%u id=0x%llx hp=%d conf=%u",
              s_fire_count, (unsigned long long)id, hp, s_confirmed);
}

}
