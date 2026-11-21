#pragma once
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../game/hitbox_defs.hpp"
#include "../ui/cfg.hpp"
#include "../other/touch_inject.hpp"
#include "../other/diag.hpp"
#include <cmath>
#include <algorithm>

namespace aimbot {

struct aim_target {
    uint64_t id;
    Vector3 head;
    Vector3 feet;
    int hp;
    bool visible;
    player::BoneData bones;
};

inline uint64_t s_locked_id = 0;
inline uint32_t s_aim_count = 0;

inline int target_bone_id() {
    switch (cfg::aim::target) {
        case 0: return player::BoneHead;
        case 1: return player::BoneNeck;
        case 2: return player::BoneChest;
        default: return player::BoneHead;
    }
}

inline void tick(const aim_target* targets, int count,
                 const matrix& view, const matrix& proj, long long now_ms) {
    if (!cfg::aim::enabled) {
        s_locked_id = 0;
        return;
    }

    const float center_x = g_sw * 0.5f;
    const float center_y = g_sh * 0.5f;
    const float fov_px = cfg::aim::fov;
    const float smooth = std::clamp(cfg::aim::smooth, 1.f, 30.f);

    int best_bone = target_bone_id();
    uint64_t chosen = 0;
    float best_dist2 = fov_px * fov_px;
    ImVec2 best_screen{};

    // If we have a locked target, prefer it (target stickiness)
    bool lock_alive = false;
    if (s_locked_id) {
        for (int i = 0; i < count; ++i) {
            if (targets[i].id == s_locked_id && targets[i].hp > 0) {
                lock_alive = true;
                break;
            }
        }
        if (!lock_alive) s_locked_id = 0;
    }

    for (int i = 0; i < count; ++i) {
        const aim_target& t = targets[i];
        if (t.hp <= 0) continue;
        if (cfg::aim::vis_check && !t.visible) continue;

        // Pick the target bone, fall back to head or chest
        Vector3 bone_pos{};
        if (t.bones.has(best_bone)) {
            bone_pos = t.bones.positions[best_bone];
        } else if (t.bones.has(player::BoneHead)) {
            bone_pos = t.bones.positions[player::BoneHead];
        } else if (t.bones.has(player::BoneChest)) {
            bone_pos = t.bones.positions[player::BoneChest];
        } else {
            bone_pos = t.head;
        }

        ImVec2 screen_pos;
        if (!world_to_screen(bone_pos, view, proj, screen_pos)) continue;

        float dx = screen_pos.x - center_x;
        float dy = screen_pos.y - center_y;
        float dist2 = dx * dx + dy * dy;

        // Stickiness bonus for locked target
        float effective_dist2 = dist2;
        if (t.id == s_locked_id) effective_dist2 *= 0.5f;

        if (effective_dist2 < best_dist2) {
            best_dist2 = effective_dist2;
            chosen = t.id;
            best_screen = screen_pos;
        }
    }

    if (!chosen) {
        s_locked_id = 0;
        return;
    }

    s_locked_id = chosen;

    float dx = best_screen.x - center_x;
    float dy = best_screen.y - center_y;

    // Apply smoothing: move a fraction of the offset each frame
    float move_x = dx / smooth;
    float move_y = dy / smooth;

    // Dead zone: don't flick if already very close
    if (std::fabs(dx) < 2.f && std::fabs(dy) < 2.f) return;

    // Clamp maximum flick per frame to prevent jerky movement
    float max_flick = std::clamp(fov_px * 0.3f, 10.f, 150.f);
    float move_len = std::sqrt(move_x * move_x + move_y * move_y);
    if (move_len > max_flick) {
        float scale = max_flick / move_len;
        move_x *= scale;
        move_y *= scale;
    }

    inject::aim_flick(move_x, move_y);
    ++s_aim_count;

    static long long aim_log = 0;
    if (diag::every(aim_log, 3000)) {
        diag::log("AIM", "target=0x%llx bone=%d offset=(%.1f,%.1f) flick=(%.1f,%.1f) smooth=%.1f fov=%.0f count=%u",
                  (unsigned long long)chosen, best_bone, dx, dy, move_x, move_y,
                  smooth, fov_px, s_aim_count);
    }
}

} // namespace aimbot
