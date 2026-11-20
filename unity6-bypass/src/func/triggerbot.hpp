#pragma once
// Triggerbot ported from larpsense PC (pinkie::trigger) to mobile touch.
// Ray-HitVolume selection, TriggerDelay/TriggerFire state machines,
// teammate blocking, dual-layer visibility, damage-feedback wallshot detect.

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
#include <optional>
#include <unordered_map>

namespace triggerbot {

// ---- target snapshot (filled by visuals.cpp) ----
struct target {
    uint64_t id;
    Vector3 head;
    Vector3 feet;
    int hp;
    int max_hp;
    bool visible;
    player::BoneData bones;
};

// ---- TriggerTarget: selected enemy with hit info (from pinkie::TriggerTarget) ----
struct TriggerTarget {
    uint64_t id{};
    float distance{};
    int bone{-1};
    float damage{};
};

// ---- TriggerDelay: target must be held for N ms (from pinkie::TriggerDelay) ----
class TriggerDelay {
    uint64_t target_{};
    int delay_{-1};
    long long entered_{};
public:
    void reset() { *this = TriggerDelay{}; }
    bool ready(const std::optional<TriggerTarget>& tgt, int delay, long long now_ms) {
        if (!tgt) { reset(); return false; }
        delay = std::clamp(delay, 0, 1000);
        if (target_ != tgt->id || delay_ != delay || now_ms < entered_) {
            target_ = tgt->id;
            delay_ = delay;
            entered_ = now_ms;
        }
        return (now_ms - entered_) >= delay;
    }
};

// ---- TriggerFire: press/release state machine (pinkie::TriggerFire, touch adapted) ----
enum class FireAction { None, Press, Release };

class TriggerFire {
    bool held_{false};
    long long release_at_{};
    long long next_at_{};
public:
    FireAction action(bool allowed, long long now_ms) {
        if (held_) return (!allowed || now_ms >= release_at_) ? FireAction::Release : FireAction::None;
        return (allowed && now_ms >= next_at_) ? FireAction::Press : FireAction::None;
    }
    void complete(FireAction act, long long now_ms) {
        if (act == FireAction::Press) {
            held_ = true;
            release_at_ = now_ms + 30;
        }
        if (act == FireAction::Release) {
            held_ = false;
            next_at_ = now_ms + 16;
        }
    }
    bool held() const { return held_; }
    void force_release() { held_ = false; }
};

// ---- damage-feedback wallshot detection (from original triggerbot) ----
struct damage_state {
    int hp_at_fire{0};
    long long fire_ms{0};
    long long blacklist_until{0};
    int no_damage_streak{0};
    bool awaiting{false};
};

inline std::unordered_map<uint64_t, damage_state> s_damage;
static constexpr long long kDamageWindowMs = 350;
static constexpr long long kBlacklistBaseMs = 3000;
static constexpr long long kBlacklistMaxMs = 10000;
static constexpr int kWallshotThreshold = 1;

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

// ---- visibility timer: sustained visibility confirmation (pinkie vis freshness) ----
struct vis_timer_entry {
    long long first_visible_ms{0};
    bool tracking{false};
};
inline std::unordered_map<uint64_t, vis_timer_entry> s_vis_timer;
static constexpr long long kVisConfirmMs = 150;

// ---- ray-hitvolume target selection (pinkie::select_trigger_target) ----
// Builds a ray from camera eye along view forward, tests all enemy HitVolumes.
// A nearer teammate blocks a farther enemy (same as larpsense).
inline std::optional<TriggerTarget> select_trigger_target(
    const target* targets, int count,
    const matrix& view, long long now_ms)
{
    const float* vm = reinterpret_cast<const float*>(&view);
    const Vector3 eye = hitbox::camera_eye(vm);
    const Vector3 fwd = hitbox::camera_forward(vm);

    // Validate eye/forward
    if (!std::isfinite(eye.x) || !std::isfinite(eye.y) || !std::isfinite(eye.z)) return std::nullopt;
    if (!std::isfinite(fwd.x) || !std::isfinite(fwd.y) || !std::isfinite(fwd.z)) return std::nullopt;

    struct Candidate {
        uint64_t id;
        float distance;
        int bone;
        int target_index;
        bool is_enemy;
    };

    // Collect all hits across all entities
    hitbox::HitVolume volumes[player::kBoneCount];
    std::vector<Candidate> candidates;

    for (int i = 0; i < count; ++i) {
        const target& t = targets[i];

        // Visibility timer check (like pinkie freshness)
        if (cfg::trigger::vis_check) {
            auto& vt = s_vis_timer[t.id];
            if (t.visible) {
                if (!vt.tracking) { vt.first_visible_ms = now_ms; vt.tracking = true; }
            } else {
                vt.tracking = false;
            }
            if (!vt.tracking || (now_ms - vt.first_visible_ms) < kVisConfirmMs) continue;
        }

        if (is_blacklisted(t.id, now_ms)) continue;

        if (t.bones.mask) {
            int nv = hitbox::make_hit_volumes(t.bones, volumes, cfg::trigger::fov);
            for (int v = 0; v < nv; ++v) {
                auto d = hitbox::ray_hit_volume(eye, fwd, volumes[v]);
                if (d) {
                    candidates.push_back({t.id, *d, volumes[v].bone, i, true});
                }
            }
        } else if (t.head.y > t.feet.y + 0.3f) {
            // Fallback: capsule from feet to head
            float radius = std::clamp((t.head.y - t.feet.y) * 0.15f, 0.1f, 0.3f) * cfg::trigger::fov;
            auto d = hitbox::ray_capsule(eye, fwd, t.feet, t.head, radius);
            if (d) {
                candidates.push_back({t.id, *d, player::BoneChest, i, true});
            }
        }
    }

    if (candidates.empty()) return std::nullopt;

    // Sort by distance (nearest first) — same as pinkie: nearer teammate blocks farther enemy
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });

    // Return the nearest hit that is an enemy (not teammate — but we don't have
    // teammate data here since visuals.cpp already filters by team)
    for (const auto& c : candidates) {
        return TriggerTarget{c.id, c.distance, c.bone, 0.f};
    }

    return std::nullopt;
}

// ---- core state ----
inline TriggerDelay s_delay;
inline TriggerFire s_fire;
inline uint32_t s_fire_count = 0;

inline void reset() {
    s_delay.reset();
    s_fire.force_release();
}

// ---- main tick (called each frame from visuals.cpp) ----
inline void tick(const target* targets, int count,
                 const matrix& view, const matrix& proj, long long now_ms) {
    // Status logging
    static long long status_log = 0;
    if (diag::every(status_log, 3000)) {
        int blacklisted = 0;
        for (const auto& [id, ds] : s_damage)
            if (now_ms < ds.blacklist_until) ++blacklisted;

        const float* vm = reinterpret_cast<const float*>(&view);
        const Vector3 eye = hitbox::camera_eye(vm);
        diag::log("TRIGGER", "status: en=%d fire_set=%d pos=(%.0f,%.0f) tgts=%d held=%d fires=%u fd=%d vc=%d bl=%d eye=(%.1f,%.1f,%.1f)",
                  int(cfg::trigger::enabled), int(cfg::trigger::fire_set),
                  cfg::trigger::fire_x, cfg::trigger::fire_y,
                  count, int(s_fire.held()), s_fire_count,
                  inject::g_fd, int(cfg::trigger::vis_check), blacklisted,
                  eye.x, eye.y, eye.z);

        // Prune stale vis_timer and damage entries
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

    // Update damage feedback
    update_damage(targets, count, now_ms);

    // Handle release if fire held
    {
        FireAction act = s_fire.action(false, now_ms);
        if (act == FireAction::Release) {
            inject::tap_up();
            s_fire.complete(act, now_ms);
        }
    }

    if (!cfg::trigger::enabled) {
        if (s_fire.held()) { inject::tap_up(); s_fire.force_release(); }
        reset();
        return;
    }
    if (!cfg::trigger::fire_set) return;
    if (s_fire.held()) return;

    // Select target via ray-hitbox (pinkie style)
    auto selected = select_trigger_target(targets, count, view, now_ms);

    // TriggerDelay: must be on target for delay_ms
    if (!s_delay.ready(selected, cfg::trigger::delay_ms, now_ms)) return;
    if (!selected) return;

    // Find HP and distance for the selected target
    int hp = 0;
    float distance = 0.f;
    for (int i = 0; i < count; ++i) {
        if (targets[i].id == selected->id) {
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

    int bone_group = selected->bone >= 0 ? hitbox::bone_damage_group(selected->bone) : 1;
    float est_dmg = ballistics::estimate_damage(ballistics::kDefault, bone_group, 1, distance);

    record_fire(selected->id, hp, now_ms);

    // Fire via touch injection
    FireAction act = s_fire.action(true, now_ms);
    if (act == FireAction::Press) {
        inject::tap_down(cfg::trigger::fire_x, cfg::trigger::fire_y);
        s_fire.complete(act, now_ms);
        ++s_fire_count;

        diag::log("TRIGGER", "fire #%u id=0x%llx hp=%d bone=%d grp=%d dist=%.1f est_dmg=%.0f",
                  s_fire_count, (unsigned long long)selected->id, hp,
                  selected->bone, bone_group, distance, est_dmg);
    }
}

}
