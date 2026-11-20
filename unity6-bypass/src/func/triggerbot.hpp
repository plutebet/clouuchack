#pragma once
// ---------------------------------------------------------------------------
// Trigger — the PC donor's architecture, driven by touch.
//
// Selection is a strict ray test against the real hit volumes (capsules and
// OBBs built from the 19 HumanBodyBones), the nearest volume wins, and a nearer
// teammate blocks a farther enemy instead of being skipped.
//
// A shot is allowed only when the target is actually reachable:
//   * a native camera matrix produced the ray (a synthetic bor/ios fallback
//     camera does not match the crosshair and must never fire),
//   * the eye -> hit point segment is clear of level geometry
//     (mapworld::Tracker, layer-14 bullet meshes), or, when no map could be
//     identified and Strict is off, the game's own flags AND fresh bones
//     agree that the player is being rendered,
//   * the target is not blacklisted by damage feedback.
//
// Damage feedback closes the loop: a press stores the target's HP, and if the
// HP did not drop within 250 ms the target is blacklisted with a growing
// back-off. A confirmation is valid for kConfirmTtlMs only — the old
// implementation turned it into a permanent "visible" flag, which is why the
// trigger kept firing through walls after one lucky hit.
// ---------------------------------------------------------------------------

#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/hitbox_defs.hpp"
#include "../game/map_world.hpp"
#include "../game/player.hpp"
#include "../ui/cfg.hpp"
#include "../other/touch_inject.hpp"
#include "../other/diag.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace triggerbot {

// ---- szi visibility state (all 6 fields from the game struct) ----
struct szi_state {
    uint8_t fhzz{};     // +0x10
    uint8_t mow{};      // +0x11 enum None=0/InFrustumOnly=1/Always=2
    uint8_t lixl{};     // +0x12
    uint8_t ojje{};     // +0x13
    uint8_t rjux{};     // +0x14
    uint8_t ukgq{};     // +0x15
};

// ---- target snapshot (filled by visuals.cpp) ----
struct target {
    uint64_t id{};
    Vector3 head{};
    Vector3 feet{};
    int hp{0};
    int max_hp{0};
    bool visible{false};          // PlayerAnimationData.IsVisible
    bool bones_fresh{true};       // raw bone transforms still change
    szi_state szi{};
    player::BoneData bones{};
    long long bones_updated_ms{};
};

struct TriggerTarget {
    uint64_t id{};
    float distance{};
    int bone{-1};
    Vector3 point{};
};

// ---- TriggerDelay: the crosshair must stay on the target for N ms ----
class TriggerDelay {
    uint64_t target_{};
    int delay_{-1};
    long long entered_{};
public:
    void reset() { *this = TriggerDelay{}; }
    bool ready(const std::optional<TriggerTarget>& candidate, int delay, long long now_ms) {
        if (!candidate) { reset(); return false; }
        delay = std::clamp(delay, 0, 1000);
        if (target_ != candidate->id || delay_ != delay || now_ms < entered_) {
            target_ = candidate->id;
            delay_ = delay;
            entered_ = now_ms;
        }
        return (now_ms - entered_) >= delay;
    }
};

// ---- TriggerFire: press/release state machine ----
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
    void complete(FireAction act, long long now_ms, int hold_ms) {
        if (act == FireAction::Press) {
            held_ = true;
            release_at_ = now_ms + std::clamp(hold_ms, 10, 200);
        }
        if (act == FireAction::Release) {
            held_ = false;
            next_at_ = now_ms + 16;
        }
    }
    bool held() const { return held_; }
    void force_release() { held_ = false; }
};

// ---- damage-confirmed feedback ----
struct damage_state {
    int hp_at_fire{0};
    long long fire_ms{0};
    long long blacklist_until{0};
    long long confirmed_until{0};
    int no_damage_streak{0};
    bool awaiting{false};
};

inline std::unordered_map<uint64_t, damage_state> s_damage;
static constexpr long long kDamageWindowMs = 250;
static constexpr long long kConfirmTtlMs = 2500;
// A map that says "clear" while the bullets do nothing is a wrong map (or a
// moved collider): drop it after three such shots and let the tracker re-check.
inline bool s_map_ready = false;
inline int s_map_misses = 0;

inline long long blacklist_for_streak(int streak) {
    if (streak <= 1) return 400;     // spread or a moving target, not necessarily a wall
    if (streak == 2) return 2000;
    if (streak == 3) return 6000;
    return 15000;
}

inline bool is_blacklisted(uint64_t id, long long now_ms) {
    const auto it = s_damage.find(id);
    if (it == s_damage.end()) return false;
    if (now_ms < it->second.blacklist_until) return true;
    return it->second.awaiting;   // a press is still waiting for its damage
}

inline void record_fire(uint64_t id, int hp, long long now_ms) {
    damage_state& state = s_damage[id];
    state.hp_at_fire = hp;
    state.fire_ms = now_ms;
    state.awaiting = true;
}

inline void update_damage(const target* targets, int count, long long now_ms) {
    for (int i = 0; i < count; ++i) {
        const auto it = s_damage.find(targets[i].id);
        if (it == s_damage.end() || !it->second.awaiting) continue;
        damage_state& state = it->second;
        if (targets[i].hp < state.hp_at_fire) {
            state.awaiting = false;
            state.no_damage_streak = 0;
            state.blacklist_until = 0;
            state.confirmed_until = now_ms + kConfirmTtlMs;
            s_map_misses = 0;
            diag::log("TRIGGER", "confirmed id=0x%llx hp=%d->%d",
                      (unsigned long long)targets[i].id, state.hp_at_fire, targets[i].hp);
        } else if (now_ms - state.fire_ms > kDamageWindowMs) {
            state.awaiting = false;
            ++state.no_damage_streak;
            const long long window = blacklist_for_streak(state.no_damage_streak);
            state.blacklist_until = now_ms + window;
            diag::log("TRIGGER", "no damage id=0x%llx hp=%d streak=%d blacklist=%lldms",
                      (unsigned long long)targets[i].id, targets[i].hp,
                      state.no_damage_streak, window);
            if (s_map_ready && ++s_map_misses >= 3) {
                s_map_misses = 0;
                mapworld::g_tracker.invalidate("three shots with no damage while the map called the path clear");
            }
        }
    }
}

// Confirmation is a short-lived hint, never a permanent bypass.
inline bool recently_confirmed(uint64_t id, long long now_ms) {
    const auto it = s_damage.find(id);
    return it != s_damage.end() && now_ms < it->second.confirmed_until;
}

// ---- fallback visibility (no map asset): every signal must agree ----
// szi.mow alone is only "inside the camera frustum" — InFrustumOnly is 1, so
// mow != 0 is true for an enemy standing behind a wall in plain view.
inline bool flags_visible(const target& t, long long now_ms) {
    if (t.szi.mow == 0) return false;          // not even in the frustum
    if (!t.visible) return false;              // PlayerAnimationData.IsVisible
    if (!t.bones_fresh) return false;          // the game stopped animating him
    if (t.bones_updated_ms > 0 && now_ms - t.bones_updated_ms > 80) return false;
    return true;
}

// ---- strict ray-hitbox selection ----
static constexpr float kShrinkFactor = 0.92f;
static constexpr float kMinDistance = 1.5f;

struct Selection {
    std::optional<TriggerTarget> candidate;
    bool blocker_ahead{false};     // the nearest volume belongs to a teammate
    const char* reject{nullptr};   // why there is no candidate
};

inline Selection select(const target* enemies, int enemy_count,
                        const target* blockers, int blocker_count,
                        const matrix& view, long long now_ms) {
    Selection out;
    const float* vm = reinterpret_cast<const float*>(&view);
    const Vector3 eye = hitbox::camera_eye(vm);
    const Vector3 forward = hitbox::camera_forward(vm);
    if (!std::isfinite(eye.x) || !std::isfinite(eye.y) || !std::isfinite(eye.z) ||
        !std::isfinite(forward.x) || !std::isfinite(forward.y) || !std::isfinite(forward.z)) {
        out.reject = "camera basis";
        return out;
    }

    hitbox::HitVolume volumes[player::kBoneCount];
    float best = 1e30f;
    int best_bone = -1;
    uint64_t best_id = 0;
    bool best_blocker = false;

    auto consider = [&](const target* list, int count, bool blocker) {
        for (int i = 0; i < count; ++i) {
            const target& t = list[i];
            // A teammate is only geometry here: it blocks, it is never shot at.
            if (!blocker) {
                if (t.hp <= 0) continue;
                if (is_blacklisted(t.id, now_ms)) continue;
                if (!t.bones.mask) continue;
                if (t.bones_updated_ms > 0 && now_ms - t.bones_updated_ms > 80) continue;
            } else if (!t.bones.mask) {
                continue;
            }
            const int nv = hitbox::make_hit_volumes(t.bones, volumes, kShrinkFactor);
            for (int v = 0; v < nv; ++v) {
                const auto distance = hitbox::ray_hit_volume(eye, forward, volumes[v]);
                if (!distance || *distance < kMinDistance) continue;
                if (*distance < best) {
                    best = *distance;
                    best_bone = volumes[v].bone;
                    best_id = t.id;
                    best_blocker = blocker;
                }
            }
        }
    };
    consider(enemies, enemy_count, false);
    consider(blockers, blocker_count, true);

    if (best_id == 0) {
        out.reject = "crosshair not on a hit volume";
        return out;
    }
    if (best_blocker) {
        out.blocker_ahead = true;
        out.reject = "teammate in the line of fire";
        return out;
    }
    out.candidate = TriggerTarget{best_id, best, best_bone,
                                  Vector3{eye.x + forward.x * best,
                                          eye.y + forward.y * best,
                                          eye.z + forward.z * best}};
    return out;
}

// ---- core state ----
inline TriggerDelay s_delay;
inline TriggerFire s_fire;
inline uint32_t s_fire_count = 0;
inline uint32_t s_blocked_count = 0;

inline void reset() {
    s_delay.reset();
    s_fire.force_release();
}

// Reasons are logged, not stored: one line per second at most.
inline void log_reject(const char* reason) {
    static long long last = 0;
    static const char* previous = nullptr;
    if (reason == previous && !diag::every(last, 1000)) return;
    previous = reason;
    last = diag::now_ms();
    diag::log("TRIGGER", "hold: %s", reason);
}

inline void tick(const target* enemies, int enemy_count,
                 const target* blockers, int blocker_count,
                 const matrix& view,
                 bool camera_is_native,
                 bool map_ready,
                 long long now_ms) {
    s_map_ready = map_ready;
    static long long status_log = 0;
    if (diag::every(status_log, 4000)) {
        int blacklisted = 0, confirmed = 0, awaiting = 0;
        for (const auto& entry : s_damage) {
            if (now_ms < entry.second.blacklist_until) ++blacklisted;
            if (now_ms < entry.second.confirmed_until) ++confirmed;
            if (entry.second.awaiting) ++awaiting;
        }
        const Vector3 eye = hitbox::camera_eye(reinterpret_cast<const float*>(&view));
        diag::log("TRIGGER", "en=%d tgts=%d blockers=%d fires=%u wall_holds=%u bl=%d conf=%d await=%d map=%d cam=%d eye=(%.1f %.1f %.1f)",
                  int(cfg::trigger::enabled), enemy_count, blocker_count, s_fire_count,
                  s_blocked_count, blacklisted, confirmed, awaiting,
                  int(map_ready), int(camera_is_native), eye.x, eye.y, eye.z);
        for (auto it = s_damage.begin(); it != s_damage.end(); ) {
            bool live = false;
            for (int i = 0; i < enemy_count && !live; ++i) live = enemies[i].id == it->first;
            if (!live) it = s_damage.erase(it); else ++it;
        }
    }

    update_damage(enemies, enemy_count, now_ms);

    // Release outside every other branch: a pending press must never stick.
    if (!cfg::trigger::enabled || !cfg::trigger::fire_set) {
        if (s_fire.held()) { inject::tap_up(); s_fire.force_release(); }
        s_delay.reset();
        return;
    }

    const FireAction pending = s_fire.action(false, now_ms);
    if (pending == FireAction::Release) {
        inject::tap_up();
        s_fire.complete(pending, now_ms, cfg::trigger::hold_ms);
    }
    if (s_fire.held()) return;

    if (!camera_is_native) {
        log_reject("camera is not the native game camera");
        s_delay.reset();
        return;
    }

    const bool want_wall_check = cfg::trigger::vis_check;
    if (want_wall_check && !map_ready && cfg::trigger::strict) {
        log_reject("strict: waiting for a map collider (assets/maps)");
        s_delay.reset();
        return;
    }

    const Selection selection = select(enemies, enemy_count, blockers, blocker_count, view, now_ms);
    if (!selection.candidate) {
        log_reject(selection.reject ? selection.reject : "no target");
        s_delay.reset();
        return;
    }
    TriggerTarget candidate = *selection.candidate;
    const target* entry = nullptr;
    for (int i = 0; i < enemy_count; ++i)
        if (enemies[i].id == candidate.id) { entry = &enemies[i]; break; }
    if (!entry) { s_delay.reset(); return; }

    // ---- the actual visibility decision ----
    if (want_wall_check) {
        if (map_ready) {
            const Vector3 eye = hitbox::camera_eye(reinterpret_cast<const float*>(&view));
            float hit = 0.f;
            if (mapworld::g_tracker.blocked(eye, candidate.point, &hit)) {
                ++s_blocked_count;
                static long long wall_log = 0;
                if (diag::every(wall_log, 1000))
                    diag::log("TRIGGER", "hold: wall at %.2f m before id=0x%llx (bone=%d)",
                              hit, (unsigned long long)candidate.id, candidate.bone);
                s_delay.reset();
                return;
            }
        } else if (!flags_visible(*entry, now_ms) && !recently_confirmed(candidate.id, now_ms)) {
            log_reject("game flags say the player is not rendered");
            s_delay.reset();
            return;
        }
    }

    if (!s_delay.ready(candidate, cfg::trigger::delay_ms, now_ms)) return;

    if (cfg::trigger::min_distance > 0.f && candidate.distance < cfg::trigger::min_distance) {
        log_reject("target closer than the minimum distance");
        s_delay.reset();
        return;
    }
    if (cfg::trigger::max_distance > 0.f && candidate.distance > cfg::trigger::max_distance) return;

    const FireAction act = s_fire.action(true, now_ms);
    if (act != FireAction::Press) return;

    // Only an actual press arms the damage window: recording it earlier would
    // blacklist the target for a shot the fire state machine never took.
    record_fire(candidate.id, entry->hp, now_ms);
    inject::tap_down(cfg::trigger::fire_x, cfg::trigger::fire_y);
    s_fire.complete(act, now_ms, cfg::trigger::hold_ms);
    ++s_fire_count;
    diag::log("TRIGGER", "fire #%u id=0x%llx hp=%d bone=%d dist=%.1f map=%d",
              s_fire_count, (unsigned long long)candidate.id, entry->hp,
              candidate.bone, candidate.distance, int(map_ready));
}

// Compatibility entry point for the rise build, which keeps its own camera
// and compares triggers from a flat entity list. It has no bone data, so it
// still never selects a target; the overload just keeps that binary building.
inline void tick(const target* targets, int count,
                 const matrix& view, const matrix& /*projection*/,
                 long long now_ms) {
    tick(targets, count, nullptr, 0, view, true, mapworld::g_tracker.ready(), now_ms);
}

} // namespace triggerbot
