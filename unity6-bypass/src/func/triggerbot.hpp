#pragma once

#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../game/camera.hpp"
#include "../game/weapon.hpp"
#include "../ui/cfg.hpp"
#include "../other/memory.hpp"
#include "../other/diag.hpp"
#include "../other/touch_inject.hpp"
#include <cmath>
#include <algorithm>
#include <chrono>

// =============================================================================
// Zenin triggerbot reconstruction — 1:1 algorithm, structs, flow
// Firing via touch injection (external cheat cannot write weapon state)
// =============================================================================

namespace triggerbot {

    inline constexpr float ZENIN_MIN_DIST    = 2.26e-5f;
    inline constexpr float ZENIN_SMALL_DIST  = 0.00997f;
    inline constexpr float ZENIN_BONE_SCALE  = 100.0f;

    struct trigger_result {
        uint8_t status;
        uint8_t _pad[3];
        float distance;
        float weight_x;
        float weight_y;
        float confidence;
        float adj_distance;
        uint8_t iterations;
    };

    struct sorted_player {
        uint64_t object;
        Vector3 position;
        float distance;
        int bone_type;
        bool visible;
    };

    inline bool g_firing{false};
    inline uint64_t g_target_obj{0};

    using clock_t = std::chrono::steady_clock;
    inline clock_t::time_point g_last_fire{};
    inline clock_t::time_point g_target_acquired{};
    inline bool g_delay_pending{false};

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

    inline trigger_result zenin_aim_check(
        const Vector3& local_pos, const Vector3& target_pos,
        const matrix& view, float bone_radius_px, float max_dist)
    {
        trigger_result res{};
        res.status = 0;
        res.distance = 0.0f;
        res.weight_x = bone_radius_px;
        res.weight_y = bone_radius_px;
        res.confidence = 0.0f;
        res.adj_distance = 0.0f;
        res.iterations = 0;

        Vector3 dir = target_pos - local_pos;
        float dist = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
        if (dist <= ZENIN_MIN_DIST || !std::isfinite(dist)) return res;

        float inv_dist = 1.0f / dist;
        Vector3 norm_dir;
        norm_dir.x = dir.x * inv_dist;
        norm_dir.y = dir.y * inv_dist;
        norm_dir.z = dir.z * inv_dist;

        Vector3 cam_fwd(-view.m13, -view.m23, -view.m33);
        float fwd_len = std::sqrt(cam_fwd.x*cam_fwd.x + cam_fwd.y*cam_fwd.y + cam_fwd.z*cam_fwd.z);
        if (fwd_len < ZENIN_MIN_DIST) return res;
        cam_fwd.x /= fwd_len; cam_fwd.y /= fwd_len; cam_fwd.z /= fwd_len;

        float dot = cam_fwd.x*norm_dir.x + cam_fwd.y*norm_dir.y + cam_fwd.z*norm_dir.z;

        float fov_rad = 1.0472f;
        float screen_h = 1080.0f;
        float px_to_rad = fov_rad / screen_h;
        float threshold_rad = bone_radius_px * px_to_rad;
        float cos_threshold = std::cos(threshold_rad);

        bool raycast_ok = zenin_raycast(local_pos, target_pos, dist, view);

        res.distance = dist;
        res.adj_distance = dist;

        if (dot >= cos_threshold) {
            res.status = 1;
            res.confidence = 1.0f;
        } else if (dot >= std::cos(threshold_rad * 2.0f)) {
            res.status = raycast_ok ? 2 : 0;
            float t = (dot - std::cos(threshold_rad * 2.0f)) /
                      (cos_threshold - std::cos(threshold_rad * 2.0f));
            res.confidence = std::clamp(t, 0.0f, 1.0f);
        } else {
            res.status = 0;
        }

        if (res.iterations < 6) res.iterations++;
        return res;
    }

    // Main tick — fires via touch injection on the fire button
    inline void tick(const cam::main_chain& chain,
                     const std::vector<player::frame_player>& frame, int local_team,
                     const matrix& view, const matrix& projection) {
        const bool was_firing = g_firing;
        g_firing = false;

        if (!cfg::trigger::enabled) {
            g_delay_pending = false;
            g_target_obj = 0;
            if (was_firing) touch_inject::fire_up();
            return;
        }
        if (frame.empty() || !frame[0].snap) return;

        const player_snapshot& local = *frame[0].snap;
        const auto now = clock_t::now();

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

            float dx = local_pos.x - bone.x;
            float dy = local_pos.y - bone.y;
            float dz = local_pos.z - bone.z;
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (dist <= ZENIN_MIN_DIST || !std::isfinite(dist)) continue;
            if (dist > cfg::trigger::max_distance) continue;

            if (cfg::trigger::visible_check) {
                if (!zenin_raycast(local_pos, bone, dist, view)) continue;
            }

            trigger_result result = zenin_aim_check(
                local_pos, bone, view,
                cfg::trigger::bone_radius, cfg::trigger::max_distance);

            if (result.status != 1 && result.status != 2) continue;

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

            // Fire via touch injection
            touch_inject::fire_down(cfg::trigger::fire_x, cfg::trigger::fire_y);
            g_firing = true;
            g_target_obj = fp.snap->object;
            g_last_fire = now;
            g_delay_pending = false;

            static long long trig_log = 0;
            if (diag::every(trig_log, 2000))
                diag::log("TRIGGER", "FIRE s=%d d=%.1f c=%.2f obj=0x%llx touch=(%.0f,%.0f)",
                          result.status, result.distance, result.confidence,
                          (unsigned long long)fp.snap->object,
                          cfg::trigger::fire_x, cfg::trigger::fire_y);
            return;
        }

        // No target — release fire button
        g_delay_pending = false;
        g_target_obj = 0;
        if (was_firing)
            touch_inject::fire_up();
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
