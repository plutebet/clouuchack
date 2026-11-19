#pragma once
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/hitbox_defs.hpp"
#include "../game/ballistics.hpp"
#include "../ui/cfg.hpp"
#include "../other/touch_inject.hpp"
#include "../other/diag.hpp"
#include "../game/player.hpp"
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
    bool visible;
    player::BoneData bones;
};

// ---- damage-feedback wallshot detection (PC-style substitute for Physics.Linecast) ----
struct damage_state {
    int hp_at_fire{0};
    long long fire_ms{0};
    long long blacklist_until{0};
    int no_damage_streak{0};
    bool awaiting{false};
};
inline std::unordered_map<uint64_t, damage_state> s_damage;
static constexpr long long kDamageWindowMs = 400;
static constexpr long long kBlacklistBaseMs = 2000;
static constexpr long long kBlacklistMaxMs = 8000;
static constexpr int kWallshotThreshold = 2;

inline void update_damage(const target* targets, int count, long long now_ms) {
    for (int i = 0; i < count; ++i) {
        auto it = s_damage.find(targets[i].id);
        if (it == s_damage.end() || !it->second.awaiting) continue;
        damage_state& ds = it->second;
        if (targets[i].hp < ds.hp_at_fire) {
            ds.awaiting = false;
            ds.no_damage_streak = 0;
            ds.blacklist_until = 0;
        } else if (now_ms - ds.fire_ms > kDamageWindowMs) {
            ds.awaiting = false;
            ++ds.no_damage_streak;
            if (ds.no_damage_streak >= kWallshotThreshold) {
                long long bl = kBlacklistBaseMs * std::min(ds.no_damage_streak - 1, 4);
                bl = std::min(bl, kBlacklistMaxMs);
                ds.blacklist_until = now_ms + bl;
                diag::log("TRIGGER", "wallshot id=0x%llx hp=%d streak=%d bl=%lldms",
                          (unsigned long long)targets[i].id, targets[i].hp,
                          ds.no_damage_streak, bl);
            }
        }
    }
}

inline bool is_blacklisted(uint64_t id, long long now_ms) {
    auto it = s_damage.find(id);
    return it != s_damage.end() && now_ms < it->second.blacklist_until;
}

inline void record_fire(uint64_t id, int hp, long long now_ms) {
    damage_state& ds = s_damage[id];
    ds.hp_at_fire = hp;
    ds.fire_ms = now_ms;
    ds.awaiting = true;
}

// ---- visibility timer (150ms sustained visibility) ----
struct vis_timer_entry {
    long long first_visible_ms{0};
    bool tracking{false};
};
inline std::unordered_map<uint64_t, vis_timer_entry> s_vis_timer;
static constexpr long long kVisConfirmMs = 150;

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
inline int s_last_hit_bone = -1;

inline void reset() {
    s_tracking = 0;
    s_since = 0;
    s_confirmed = 0;
    s_last_confirm_seq = 0;
    s_last_hit_bone = -1;
}

// ---- 3D ray-hitbox target selection ----
// Builds a ray from camera eye along the view forward direction and intersects
// world-space hit volumes built from the game's actual collider dimensions.
inline uint64_t plan(const target* targets, int count,
                     const matrix& view, const matrix& proj, long long now_ms) {
    ++s_seq;

    if (cfg::trigger::vis_check) {
        for (int i = 0; i < count; ++i) {
            auto& vt = s_vis_timer[targets[i].id];
            if (targets[i].visible) {
                if (!vt.tracking) { vt.first_visible_ms = now_ms; vt.tracking = true; }
            } else {
                vt.tracking = false;
            }
        }
    }

    update_damage(targets, count, now_ms);

    const float* vm = reinterpret_cast<const float*>(&view);
    const Vector3 eye = hitbox::camera_eye(vm);
    const Vector3 fwd = hitbox::camera_forward(vm);

    uint64_t chosen = 0;
    float best_dist = 1e9f;
    int best_bone = -1;

    hitbox::HitVolume volumes[player::kBoneCount];

    for (int i = 0; i < count; ++i) {
        const target& t = targets[i];

        if (cfg::trigger::vis_check) {
            auto& vt = s_vis_timer[t.id];
            if (!vt.tracking || (now_ms - vt.first_visible_ms) < kVisConfirmMs) continue;
        }

        if (is_blacklisted(t.id, now_ms)) continue;

        bool hit = false;
        float hit_dist = 1e9f;
        int hit_bone = -1;

        if (t.bones.mask) {
            // 3D ray-hitbox test against actual game collider volumes
            int nv = hitbox::make_hit_volumes(t.bones, volumes, cfg::trigger::fov);
            for (int v = 0; v < nv; ++v) {
                auto d = hitbox::ray_hit_volume(eye, fwd, volumes[v]);
                if (d && *d < hit_dist) {
                    hit_dist = *d;
                    hit_bone = volumes[v].bone;
                    hit = true;
                }
            }
        } else if (t.head.y > t.feet.y + 0.3f) {
            // No bones: approximate as capsule from feet to head
            Vector3 mid = {(t.head.x + t.feet.x) * 0.5f, (t.head.y + t.feet.y) * 0.5f,
                           (t.head.z + t.feet.z) * 0.5f};
            float height = t.head.y - t.feet.y;
            float radius = std::clamp(height * 0.15f, 0.1f, 0.3f) * cfg::trigger::fov;
            auto d = hitbox::ray_capsule(eye, fwd, t.feet, t.head, radius);
            if (d) { hit_dist = *d; hit_bone = player::BoneChest; hit = true; }
        }

        if (hit && hit_dist < best_dist) {
            best_dist = hit_dist;
            chosen = t.id;
            best_bone = hit_bone;
        }
    }

    if (!chosen) { reset(); return 0; }

    s_last_hit_bone = best_bone;

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
        int blacklisted = 0;
        for (const auto& [id, ds] : s_damage)
            if (now_ms < ds.blacklist_until) ++blacklisted;

        const float* vm = reinterpret_cast<const float*>(&view);
        const Vector3 eye = hitbox::camera_eye(vm);
        diag::log("TRIGGER", "status: en=%d fire_set=%d pos=(%.0f,%.0f) tgts=%d pressed=%d fires=%u fd=%d vc=%d bl=%d eye=(%.1f,%.1f,%.1f)",
                  int(cfg::trigger::enabled), int(cfg::trigger::fire_set),
                  cfg::trigger::fire_x, cfg::trigger::fire_y,
                  count, int(s_pressed), s_fire_count,
                  inject::g_fd, int(cfg::trigger::vis_check), blacklisted,
                  eye.x, eye.y, eye.z);

        for (auto it = s_vis_timer.begin(); it != s_vis_timer.end(); ) {
            bool found = false;
            for (int i = 0; i < count; ++i)
                if (targets[i].id == it->first) { found = true; break; }
            if (!found) it = s_vis_timer.erase(it); else ++it;
        }
        for (auto it = s_damage.begin(); it != s_damage.end(); ) {
            bool found = false;
            for (int i = 0; i < count; ++i)
                if (targets[i].id == it->first) { found = true; break; }
            if (!found) it = s_damage.erase(it); else ++it;
        }
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
    float distance = 0.f;
    for (int i = 0; i < count; ++i) {
        if (targets[i].id == id) {
            hp = targets[i].hp;
            const float* vm = reinterpret_cast<const float*>(&view);
            Vector3 eye = hitbox::camera_eye(vm);
            Vector3 mid = {(targets[i].head.x + targets[i].feet.x) * 0.5f,
                           (targets[i].head.y + targets[i].feet.y) * 0.5f,
                           (targets[i].head.z + targets[i].feet.z) * 0.5f};
            distance = calculate_distance(eye, mid);
            break;
        }
    }

    int bone_group = s_last_hit_bone >= 0 ? hitbox::bone_damage_group(s_last_hit_bone) : 1;
    float est_dmg = ballistics::estimate_damage(ballistics::kDefault, bone_group, 1, distance);

    record_fire(id, hp, now_ms);

    inject::tap_down(cfg::trigger::fire_x, cfg::trigger::fire_y);
    s_pressed = true;
    s_release_at = now_ms + 30;
    ++s_fire_count;

    diag::log("TRIGGER", "fire #%u id=0x%llx hp=%d bone=%d grp=%d dist=%.1f est_dmg=%.0f conf=%u",
              s_fire_count, (unsigned long long)id, hp, s_last_hit_bone, bone_group, distance, est_dmg, s_confirmed);
}

}
