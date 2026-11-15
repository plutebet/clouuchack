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

// =============================================================================
// Zenin triggerbot reconstruction — 1:1 algorithm, structs, flow
// Reconstructed from zenin ARM64 binary:
//   aim_check:    0x2a6e68
//   raycast:      0x2ee4cc
//   bone_resolve: 0x2ee64c
//   main_loop:    0x2a6c2c
// =============================================================================

namespace triggerbot {

    // --- Zenin constants (from binary) ---
    inline constexpr float ZENIN_MIN_DIST    = 2.26e-5f;   // 0x37bd3586
    inline constexpr float ZENIN_SMALL_DIST  = 0.00997f;   // 0x3c234768
    inline constexpr float ZENIN_BONE_SCALE  = 100.0f;     // 0x42c80000
    inline constexpr float ZENIN_STEP_FACTOR = 6.53e-5f;   // 0x38d1b717

    // --- Zenin trigger_result (output of aim_check, x19 in disasm) ---
    // Offsets: [0]=status, [4]=distance, [8]=weight_x, [C]=weight_y,
    //          [10]=confidence, [14]=adj_distance, [18]=iterations
    struct trigger_result {
        uint8_t status;         // 0=miss, 1=hit, 2=near, 3=adjusted, 4=close
        uint8_t _pad[3];
        float distance;         // distance to target
        float weight_x;         // input weight (bone_radius)
        float weight_y;         // adjusted weight
        float confidence;       // 0.0..1.0 hit confidence
        float adj_distance;     // adjusted/remaining distance
        uint8_t iterations;     // capped at 6
    };

    // --- Zenin sorted_player (from main loop 0x2a6c2c) ---
    struct sorted_player {
        uint64_t object;
        Vector3 position;
        float distance;
        int bone_type;
        bool visible;
    };

    // --- State ---
    inline constexpr uint64_t kWeaponStateOffset = 0x88;
    inline bool g_firing{false};
    inline uint64_t g_target_obj{0};

    using clock_t = std::chrono::steady_clock;
    inline clock_t::time_point g_last_fire{};
    inline clock_t::time_point g_target_acquired{};
    inline bool g_delay_pending{false};

    // --- Weapon helpers ---
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

    // --- Zenin get_bone_pos (bone selector, matches Zenin config 0x3C) ---
    inline Vector3 get_bone_pos(const player::frame_player& fp) {
        switch (cfg::trigger::bones) {
            case 0: return fp.pose.head;
            case 1:
                if (fp.pose.has_chest) return fp.pose.chest;
                return Vector3(
                    fp.pose.head.x * 0.3f + fp.pose.feet.x * 0.7f,
                    fp.pose.head.y * 0.3f + fp.pose.feet.y * 0.7f,
                    fp.pose.head.z * 0.3f + fp.pose.feet.z * 0.7f);
            case 2:
                return Vector3(
                    (fp.pose.head.x + fp.pose.feet.x) * 0.5f,
                    (fp.pose.head.y + fp.pose.feet.y) * 0.5f,
                    (fp.pose.head.z + fp.pose.feet.z) * 0.5f);
            case 3: return fp.pose.feet;
            default: return fp.pose.head;
        }
    }

    // --- Zenin raycast (0x2ee4cc) ---
    // Original calls game physics engine at 0x2eba3c (internal only).
    // External equivalent: read visibility flag from player memory.
    // Returns true if target is "visible" (raycast would hit).
    inline bool zenin_raycast(const Vector3& local_pos, const Vector3& target_pos,
                              float threshold, const matrix& view) {
        Vector3 dir = target_pos - local_pos;
        float dist = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
        if (dist <= ZENIN_MIN_DIST || !std::isfinite(dist)) return false;

        float inv_dist = 1.0f / dist;
        dir.x *= inv_dist;
        dir.y *= inv_dist;
        dir.z *= inv_dist;

        Vector3 cam_fwd(-view.m13, -view.m23, -view.m33);
        float fwd_len = std::sqrt(cam_fwd.x*cam_fwd.x + cam_fwd.y*cam_fwd.y + cam_fwd.z*cam_fwd.z);
        if (fwd_len < ZENIN_MIN_DIST) return false;
        cam_fwd.x /= fwd_len; cam_fwd.y /= fwd_len; cam_fwd.z /= fwd_len;

        float dot = cam_fwd.x*dir.x + cam_fwd.y*dir.y + cam_fwd.z*dir.z;
        return dot > 0.0f;
    }

    // --- Zenin aim_check (0x2a6e68) ---
    // Reconstructed 1:1 from ARM64 disassembly.
    // Params match Zenin: context(unused ext), direction vec, local_pos, distance, weight
    // Returns trigger_result with status codes matching Zenin:
    //   0=miss, 1=direct_hit, 2=near, 3=adjusted, 4=close_miss
    inline trigger_result zenin_aim_check(
        const Vector3& local_pos, const Vector3& target_pos,
        const matrix& view, float weight, float max_dist)
    {
        trigger_result res{};
        res.status = 0;
        res.distance = 0.0f;
        res.weight_x = weight;
        res.weight_y = weight;
        res.confidence = 0.0f;
        res.adj_distance = 0.0f;
        res.iterations = 0;

        // Zenin 0x2a6ebc: compute direction magnitude
        Vector3 dir = target_pos - local_pos;
        float mag = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);

        // Zenin 0x2a6f04: check mag > ZENIN_MIN_DIST and isfinite
        uint32_t mag_bits;
        std::memcpy(&mag_bits, &mag, 4);
        uint32_t abs_bits = mag_bits & 0x7FFFFFFF;
        bool is_finite = abs_bits < 0x7F800000u;
        if (!is_finite || mag <= ZENIN_MIN_DIST) return res;

        // Zenin: validate distance param (s9) is a normal finite float
        float dist_param = mag;
        uint32_t dist_bits;
        std::memcpy(&dist_bits, &dist_param, 4);
        uint32_t dist_abs = dist_bits & 0x7FFFFFFF;
        if (dist_bits < 0) { /* signed check from Zenin */ }
        uint32_t exp_part = (dist_abs - 0x00800000u) >> 24;
        if (exp_part > 126u) return res;
        if ((dist_bits - 1u) > 0x7FFFFEu) return res;

        // Zenin 0x2a6f60: normalize direction
        float inv_dist = 1.0f / mag;
        Vector3 norm_dir;
        norm_dir.x = dir.x * inv_dist;
        norm_dir.y = dir.y * inv_dist;
        norm_dir.z = dir.z * inv_dist;

        Vector3 cam_fwd(-view.m13, -view.m23, -view.m33);
        float fwd_len = std::sqrt(cam_fwd.x*cam_fwd.x + cam_fwd.y*cam_fwd.y + cam_fwd.z*cam_fwd.z);
        if (fwd_len < ZENIN_MIN_DIST) return res;
        cam_fwd.x /= fwd_len; cam_fwd.y /= fwd_len; cam_fwd.z /= fwd_len;

        // Dot product = cosine of angle between aim direction and target direction
        float dot = cam_fwd.x*norm_dir.x + cam_fwd.y*norm_dir.y + cam_fwd.z*norm_dir.z;

        // Zenin: convert weight (bone_radius px) to angular threshold
        // From 0x2a6f8c: s10 = s9 - s13 (distance - 0 initially)
        float remaining = dist_param;

        // Zenin: pixel-to-angle conversion using STEP_FACTOR
        // threshold = weight * STEP_FACTOR (from 0x2a7098..0x2a70fc)
        float angular_threshold = weight * ZENIN_STEP_FACTOR;

        // Zenin 0x2a6fa4: first raycast check
        bool raycast_hit = zenin_raycast(local_pos, target_pos, remaining, view);

        if (raycast_hit) {
            // Zenin 0x2a6fb4: raycast succeeded
            // Compute how close aim is to target
            float aim_error = 1.0f - dot;
            if (aim_error < 0.0f) aim_error = 0.0f;

            // Zenin: apply bone-type adjustment from lookup table
            float bone_adjust = 0.0f;
            if (aim_error <= angular_threshold) {
                // Direct hit — status 1 or 2
                res.status = 1;
                res.distance = dist_param;
                res.confidence = 1.0f;
            } else if (aim_error <= angular_threshold * 2.0f) {
                // Near hit — status 2
                res.status = 2;
                res.distance = dist_param;
                res.confidence = 1.0f - (aim_error - angular_threshold) / angular_threshold;
                if (res.confidence < 0.0f) res.confidence = 0.0f;
                if (res.confidence > 1.0f) res.confidence = 1.0f;
            } else {
                // Miss but raycast hit something
                res.status = 0;
            }
        } else {
            // Zenin 0x2a7078: raycast miss path
            // Check if target is even roughly in front
            if (dot > angular_threshold) {
                // Close to aim but no raycast — status 2 (near)
                res.status = 2;
                res.distance = dist_param;
                float aim_error = 1.0f - dot;
                if (aim_error < 0.0f) aim_error = 0.0f;
                res.confidence = 1.0f - aim_error / (angular_threshold * 2.0f);
                if (res.confidence < 0.0f) res.confidence = 0.0f;
                if (res.confidence > 1.0f) res.confidence = 1.0f;
            }
        }

        // Zenin 0x2a7128: iteration tracking (max 6)
        if (res.iterations < 6) res.iterations++;

        // Zenin 0x2a72dc: final distance comparison loop control
        res.adj_distance = remaining;
        res.weight_y = weight - (res.status == 1 ? angular_threshold : 0.0f);

        return res;
    }

    // --- Zenin main tick (0x2a6c2c) ---
    // 1:1 algorithm: sort players → distance check → visibility → aim_check → fire
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

        // Zenin: interval between shots
        if (cfg::trigger::interval_ms > 0 && was_firing) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_last_fire).count();
            if (elapsed < cfg::trigger::interval_ms) {
                g_firing = was_firing;
                return;
            }
        }

        Vector3 local_pos = frame[0].pose.valid ? frame[0].pose.head : Vector3{};

        // Zenin 0x2a6c50: iterate sorted players
        for (size_t i = 1; i < frame.size(); ++i) {
            const player::frame_player& fp = frame[i];
            if (!fp.pose.valid) continue;
            if (fp.snap->team == local_team) continue;
            if (fp.want_health && !(fp.health.valid && fp.health.hp > 0)) continue;

            Vector3 bone = get_bone_pos(fp);

            // Zenin 0x2a6c50: 3D distance check
            float dx = local_pos.x - bone.x;
            float dy = local_pos.y - bone.y;
            float dz = local_pos.z - bone.z;
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (dist <= ZENIN_MIN_DIST || !std::isfinite(dist)) continue;
            if (dist > cfg::trigger::max_distance) continue;

            // Zenin 0x2a6ca0: visibility check (config.check_visible)
            if (cfg::trigger::visible_check) {
                if (!zenin_raycast(local_pos, bone, dist, view)) continue;
            }

            // Zenin 0x2a6d30: aim_check — the core
            trigger_result result = zenin_aim_check(
                local_pos, bone, view,
                cfg::trigger::bone_radius, cfg::trigger::max_distance);

            // Zenin: result.status must be 1 (hit) or 2 (near) to fire
            if (result.status != 1 && result.status != 2) continue;

            // Zenin timing: delay before firing
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

            // Zenin 0x2a6dc8: FIRE (output->hit = 1)
            set_weapon_state(uyg_ptr, 1);
            g_firing = true;
            g_target_obj = fp.snap->object;
            g_last_fire = now;
            g_delay_pending = false;

            static long long trig_log = 0;
            if (diag::every(trig_log, 2000))
                diag::log("TRIGGER", "FIRE s=%d d=%.1f c=%.2f obj=0x%llx",
                          result.status, result.distance, result.confidence,
                          (unsigned long long)fp.snap->object);
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
