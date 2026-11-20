#pragma once
// Strict triggerbot: PC pinkie architecture + OBB ray test + bone-motion vis check.
// NO fov scaling, NO fallback capsule — fires ONLY when crosshair is on a real hitbox.

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

// ---- szi visibility state (all 6 fields from game struct) ----
struct szi_state {
    uint8_t fhzz{};   // +0x10
    uint8_t mow{};     // +0x11 enum None=0/InFrustumOnly=1/Always=2
    uint8_t lixl{};    // +0x12
    uint8_t ojje{};    // +0x13
    uint8_t rjux{};    // +0x14
    uint8_t ukgq{};    // +0x15
};

// ---- target snapshot (filled by visuals.cpp) ----
struct target {
    uint64_t id;
    Vector3 head;
    Vector3 feet;
    int hp;
    int max_hp;
    bool visible;         // PlayerAnimationData.IsVisible
    szi_state szi;        // full szi struct
    player::BoneData bones;
    long long bones_updated_ms{}; // when bones were last read from game
};

// ---- TriggerTarget: selected enemy with hit info ----
struct TriggerTarget {
    uint64_t id{};
    float distance{};
    int bone{-1};
    float damage{};
};

// ---- TriggerDelay: target must be held for N ms (pinkie::TriggerDelay) ----
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

// ---- TriggerFire: press/release state machine (pinkie::TriggerFire) ----
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

// ---- damage-feedback wallshot detection ----
struct damage_state {
    int hp_at_fire{0};
    long long fire_ms{0};
    long long blacklist_until{0};
    int no_damage_streak{0};
    bool awaiting{false};
};

inline std::unordered_map<uint64_t, damage_state> s_damage;
static constexpr long long kDamageWindowMs = 250;
static constexpr long long kBlacklistBaseMs = 1500;
static constexpr long long kBlacklistMaxMs = 6000;

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
            if (ds.no_damage_streak >= 2) {
                long long bl = kBlacklistBaseMs * std::min(ds.no_damage_streak, 4);
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
    if (it == s_damage.end()) return false;
    if (now_ms < it->second.blacklist_until) return true;
    // Don't fire again while still awaiting damage feedback from previous shot
    if (it->second.awaiting) return true;
    return false;
}

inline void record_fire(uint64_t id, int hp, long long now_ms) {
    damage_state& ds = s_damage[id];
    ds.hp_at_fire = hp;
    ds.fire_ms = now_ms;
    ds.awaiting = true;
}

// ---- wall check via raw bone freshness ----
// The game stops sending fresh bone data to occluded players. We track when
// the RAW (un-interpolated) bone positions last changed in player::g_bone_cache.
// If they haven't changed in >200ms the player is behind a wall.
static constexpr long long kBoneFreezeMs = 200;

// ---- multi-layer visibility check ----
inline bool is_visible(const target& t, long long now_ms) {
    // Layer 1: szi.mow must indicate in frustum (>0)
    if (t.szi.mow == 0) return false;

    // Layer 2: PlayerAnimationData.IsVisible
    if (!t.visible) return false;

    // Layer 3: raw bone freshness (game stops updating occluded players)
    if (!player::raw_bones_fresh(t.id, now_ms, kBoneFreezeMs)) return false;

    return true;
}

// ---- strict ray-hitbox target selection ----
// NO fov scaling, NO fallback capsule. Only fires when crosshair is on a real hitbox.
static constexpr float kShrinkFactor = 0.92f; // safety margin — slightly smaller than game hitboxes
static constexpr float kMinDistance = 1.5f;    // skip targets closer than this (parallax)

inline std::optional<TriggerTarget> select_trigger_target(
    const target* targets, int count,
    const matrix& view, long long now_ms)
{
    const float* vm = reinterpret_cast<const float*>(&view);
    const Vector3 eye = hitbox::camera_eye(vm);
    const Vector3 fwd = hitbox::camera_forward(vm);

    if (!std::isfinite(eye.x) || !std::isfinite(eye.y) || !std::isfinite(eye.z)) return std::nullopt;
    if (!std::isfinite(fwd.x) || !std::isfinite(fwd.y) || !std::isfinite(fwd.z)) return std::nullopt;
    float fwd_len = std::sqrt(fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z);
    if (std::abs(fwd_len - 1.0f) > 0.01f) return std::nullopt;

    struct Candidate {
        uint64_t id;
        float distance;
        int bone;
        int target_index;
    };

    hitbox::HitVolume volumes[player::kBoneCount];
    Candidate candidates[player::kBoneCount * 4];
    int n_candidates = 0;

    for (int i = 0; i < count; ++i) {
        const target& t = targets[i];

        // Vis check: all layers must pass
        if (cfg::trigger::vis_check && !is_visible(t, now_ms)) continue;
        if (is_blacklisted(t.id, now_ms)) continue;

        // STRICT: require real bone data, no fallback capsule
        if (!t.bones.mask) continue;

        // Bone data freshness: reject stale bones (>100ms old)
        if (t.bones_updated_ms > 0 && (now_ms - t.bones_updated_ms) > 100) continue;

        // Build hit volumes at exact game size * shrink (strict, NO fov scaling)
        int nv = hitbox::make_hit_volumes(t.bones, volumes, kShrinkFactor);
        for (int v = 0; v < nv; ++v) {
            auto d = hitbox::ray_hit_volume(eye, fwd, volumes[v]);
            if (d && *d >= kMinDistance && n_candidates < (int)(sizeof(candidates)/sizeof(candidates[0]))) {
                candidates[n_candidates++] = {t.id, *d, volumes[v].bone, i};
            }
        }
    }

    if (n_candidates == 0) return std::nullopt;

    // Nearest hit wins (teammate blocking handled by visuals.cpp pre-filter)
    int best = 0;
    for (int i = 1; i < n_candidates; ++i)
        if (candidates[i].distance < candidates[best].distance) best = i;

    return TriggerTarget{candidates[best].id, candidates[best].distance, candidates[best].bone, 0.f};
}

// ---- core state ----
inline TriggerDelay s_delay;
inline TriggerFire s_fire;
inline uint32_t s_fire_count = 0;

inline void reset() {
    s_delay.reset();
    s_fire.force_release();
}

// ---- main tick ----
inline void tick(const target* targets, int count,
                 const matrix& view, const matrix& proj, long long now_ms) {
    static long long status_log = 0;
    if (diag::every(status_log, 3000)) {
        int blacklisted = 0, frozen = 0, awaiting = 0;
        for (const auto& [id, ds] : s_damage) {
            if (now_ms < ds.blacklist_until) ++blacklisted;
            if (ds.awaiting) ++awaiting;
        }
        for (const auto& [id, bc] : player::g_bone_cache)
            if (bc.cur.mask && (now_ms - bc.last_raw_change_ms) >= kBoneFreezeMs) ++frozen;

        const float* vm = reinterpret_cast<const float*>(&view);
        const Vector3 eye = hitbox::camera_eye(vm);
        diag::log("TRIGGER", "strict: en=%d fire_set=%d tgts=%d held=%d fires=%u vc=%d bl=%d frozen=%d await=%d shrink=%.2f eye=(%.1f,%.1f,%.1f)",
                  int(cfg::trigger::enabled), int(cfg::trigger::fire_set),
                  count, int(s_fire.held()), s_fire_count,
                  int(cfg::trigger::vis_check), blacklisted, frozen, awaiting, kShrinkFactor,
                  eye.x, eye.y, eye.z);

        // Prune stale damage entries
        for (auto it = s_damage.begin(); it != s_damage.end(); ) {
            bool found = false;
            for (int i = 0; i < count; ++i) if (targets[i].id == it->first) { found = true; break; }
            if (!found) it = s_damage.erase(it); else ++it;
        }
    }

    update_damage(targets, count, now_ms);

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

    auto selected = select_trigger_target(targets, count, view, now_ms);

    if (!s_delay.ready(selected, cfg::trigger::delay_ms, now_ms)) return;
    if (!selected) return;

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
