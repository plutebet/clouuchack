#pragma once

#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../game/camera.hpp"
#include "../game/weapon.hpp"
#include "../ui/cfg.hpp"
#include "../other/memory.hpp"
#include "../other/diag.hpp"
#include <cmath>
#include <algorithm>
#include <chrono>

namespace triggerbot {

    inline constexpr uint64_t kWeaponStateOffset = 0x88;
    inline constexpr float MIN_DIST = 0.001f;

    inline bool g_firing{false};
    inline uint64_t g_target_obj{0};

    using clock_t = std::chrono::steady_clock;
    inline clock_t::time_point g_last_fire{};
    inline clock_t::time_point g_target_acquired{};
    inline bool g_delay_pending{false};

    inline uint64_t find_local_weapon_part(const player_snapshot& local) {
        weapon::reset_if_context_changed();
        if (!weapon::g_part_class)
            weapon::g_part_class = typeinfo(v100::weapon_part_typeinfo);
        auto it = weapon::g_players.find(local.object);
        if (it != weapon::g_players.end() && it->second.part) {
            if (weapon::g_part_class && rpm<uint64_t>(it->second.part) == weapon::g_part_class)
                return it->second.part;
        }
        return weapon::find_part(local);
    }

    inline void set_weapon_state(uint64_t uyg_ptr, uint8_t state) {
        mem_write(uyg_ptr + kWeaponStateOffset, &state, sizeof(state));
    }

    // Get target bone position based on config (Zenin: Bones selector)
    inline Vector3 get_bone_pos(const player::frame_player& fp) {
        switch (cfg::trigger::bones) {
            case 0: return fp.pose.head;
            case 1: // Upper body
                if (fp.pose.has_chest) return fp.pose.chest;
                return Vector3(
                    fp.pose.head.x * 0.3f + fp.pose.feet.x * 0.7f,
                    fp.pose.head.y * 0.3f + fp.pose.feet.y * 0.7f,
                    fp.pose.head.z * 0.3f + fp.pose.feet.z * 0.7f);
            case 2: // Body center
                return Vector3(
                    (fp.pose.head.x + fp.pose.feet.x) * 0.5f,
                    (fp.pose.head.y + fp.pose.feet.y) * 0.5f,
                    (fp.pose.head.z + fp.pose.feet.z) * 0.5f);
            case 3: return fp.pose.feet;
            default: return fp.pose.head;
        }
    }

    // Zenin aim_check equivalent: check if crosshair direction aligns with target
    // Returns: 0=miss, 1=on target, 2=near target
    // Zenin does: normalize(target - local), pass to aim_function, check result byte
    // We replicate by computing the angle between camera forward and direction to target
    inline int aim_check(const Vector3& local_pos, const Vector3& target_pos,
                         const matrix& view, float bone_radius_px) {
        // Camera forward from view matrix (row 2, negated = forward in Unity)
        Vector3 cam_fwd(-view.m[2], -view.m[6], -view.m[10]);
        float fwd_len = std::sqrt(cam_fwd.x*cam_fwd.x + cam_fwd.y*cam_fwd.y + cam_fwd.z*cam_fwd.z);
        if (fwd_len < MIN_DIST) return 0;
        cam_fwd.x /= fwd_len; cam_fwd.y /= fwd_len; cam_fwd.z /= fwd_len;

        // Direction to target (normalized) — Zenin: inv_dist * (target - local)
        Vector3 to_target = target_pos - local_pos;
        float dist = std::sqrt(to_target.x*to_target.x + to_target.y*to_target.y + to_target.z*to_target.z);
        if (dist < MIN_DIST) return 0;
        to_target.x /= dist; to_target.y /= dist; to_target.z /= dist;

        // Dot product = cos(angle between camera forward and direction to target)
        float dot = cam_fwd.x*to_target.x + cam_fwd.y*to_target.y + cam_fwd.z*to_target.z;

        // Convert bone_radius from pixels to angular threshold
        // At typical FOV 60, screen height ~1080px, angular pixel size ≈ 0.05 deg/px
        // bone_radius 8px ≈ 0.4 degrees ≈ cos(0.4°) ≈ 0.99998
        float angle_rad = std::atan2(bone_radius_px, 1080.f) * 2.f;
        float threshold = std::cos(angle_rad);

        if (dot >= threshold) return 1;  // dead on
        if (dot >= threshold * 0.85f) return 2;  // near
        return 0;
    }

    // Zenin visibility: reads [visibility_ptr + 0x34C]
    // We approximate: if player has valid HP and is within distance,
    // and the bone projects to screen successfully, consider them potentially visible.
    // Also check if the target is behind the camera (dot < 0 = behind = not visible).
    inline bool visibility_check(const Vector3& local_pos, const Vector3& target_pos,
                                 const matrix& view) {
        Vector3 cam_fwd(-view.m[2], -view.m[6], -view.m[10]);
        float fwd_len = std::sqrt(cam_fwd.x*cam_fwd.x + cam_fwd.y*cam_fwd.y + cam_fwd.z*cam_fwd.z);
        if (fwd_len < MIN_DIST) return false;
        cam_fwd.x /= fwd_len; cam_fwd.y /= fwd_len; cam_fwd.z /= fwd_len;

        Vector3 to_target = target_pos - local_pos;
        float dist = std::sqrt(to_target.x*to_target.x + to_target.y*to_target.y + to_target.z*to_target.z);
        if (dist < MIN_DIST) return false;
        to_target.x /= dist; to_target.y /= dist; to_target.z /= dist;

        float dot = cam_fwd.x*to_target.x + cam_fwd.y*to_target.y + cam_fwd.z*to_target.z;
        return dot > 0.f;  // in front of camera
    }

    // Main tick — Zenin algorithm:
    // 1. Loop sorted players
    // 2. 3D distance check (config.max_distance)
    // 3. Visibility check (config.check_visible → [vis+0x34C])
    // 4. Aim check (normalize direction, check angle vs bone_radius)
    // 5. If aim_check returns 1 or 2 → FIRE
    inline void tick(const cam::main_chain& chain,
                     const std::vector<player::frame_player>& frame, int local_team,
                     const matrix& view, const matrix& projection) {
        const bool was_firing = g_firing;
        g_firing = false;
        if (!cfg::trigger::enabled) {
            g_delay_pending = false;
            g_target_obj = 0;
            if (was_firing && !frame.empty() && frame[0].snap) {
                uint64_t uyg = find_local_weapon_part(*frame[0].snap);
                if (uyg) set_weapon_state(uyg, 0);
            }
            return;
        }
        if (frame.empty() || !frame[0].snap) return;

        const player_snapshot& local = *frame[0].snap;
        const uint64_t uyg_ptr = find_local_weapon_part(local);
        if (!uyg_ptr) return;

        const auto now = clock_t::now();

        // Zenin: interval check between shots
        if (cfg::trigger::interval_ms > 0 && was_firing) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_last_fire).count();
            if (elapsed < cfg::trigger::interval_ms) {
                g_firing = was_firing;
                return;
            }
        }

        Vector3 local_pos = frame[0].pose.valid ? frame[0].pose.head : Vector3{};

        for (size_t i = 1; i < frame.size(); ++i) {
            const player::frame_player& fp = frame[i];
            if (!fp.pose.valid) continue;
            if (fp.snap->team == local_team) continue;
            if (fp.want_health && !(fp.health.valid && fp.health.hp > 0)) continue;

            Vector3 bone = get_bone_pos(fp);

            // Step 1 (Zenin 0x2a6c50): 3D distance
            float dist = calculate_distance(local_pos, bone);
            if (dist <= MIN_DIST || !std::isfinite(dist)) continue;
            if (dist > cfg::trigger::max_distance) continue;

            // Step 2 (Zenin 0x2a6ca0): Visibility/raycast check
            if (cfg::trigger::visible_check) {
                if (!visibility_check(local_pos, bone, view)) continue;
            }

            // Step 3 (Zenin 0x2a6d30): Aim check — normalize direction, check angle
            // Zenin: aim_check(ctx, &direction, &result, weight)
            // result must be 1 or 2 to fire
            int aim_result = aim_check(local_pos, bone, view, cfg::trigger::bone_radius);
            if (aim_result != 1 && aim_result != 2) continue;

            // Step 4 (Zenin timing): Delay before firing
            if (cfg::trigger::delay_ms > 0) {
                if (!g_delay_pending || g_target_obj != fp.snap->object) {
                    g_delay_pending = true;
                    g_target_acquired = now;
                    g_target_obj = fp.snap->object;
                    return;
                }
                auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_target_acquired).count();
                if (waited < cfg::trigger::delay_ms) return;
            }

            // === FIRE (Zenin 0x2a6dc8: output->hit = 1) ===
            set_weapon_state(uyg_ptr, 1);
            g_firing = true;
            g_target_obj = fp.snap->object;
            g_last_fire = now;
            g_delay_pending = false;

            static long long trig_log = 0;
            if (diag::every(trig_log, 2000))
                diag::log("TRIGGER", "FIRE aim=%d dist=%.1f obj=0x%llx",
                          aim_result, dist, (unsigned long long)fp.snap->object);
            return;
        }

        // Zenin 0x2a6e10: no target — clear
        g_delay_pending = false;
        g_target_obj = 0;
        if (was_firing)
            set_weapon_state(uyg_ptr, 0);
    }

    inline void draw_indicator() {
        if (!cfg::trigger::enabled || !cfg::trigger::draw_indicator) return;
        if (!g_firing) return;
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const float cx = g_sw * 0.5f;
        const float cy = g_sh * 0.5f;
        dl->AddCircleFilled(ImVec2(cx, cy), 4.f, IM_COL32(255, 50, 50, 200), 12);
    }
}
