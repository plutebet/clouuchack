#pragma once

#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../game/camera.hpp"
#include "../ui/cfg.hpp"
#include "../other/memory.hpp"
#include "../other/diag.hpp"
#include <cmath>
#include <algorithm>

namespace aimbot {

    inline Vector3 bone_position(const Vector3& feet, const Vector3& head, int bone) {
        switch (bone) {
            case 0: return head;
            case 1: {
                float t = 0.25f;
                return Vector3(head.x, head.y - (head.y - feet.y) * t, head.z);
            }
            case 2: {
                float t = 0.70f;
                return Vector3(head.x, head.y - (head.y - feet.y) * t, head.z);
            }
            default: return head;
        }
    }

    inline float shortest_delta(float from, float to) {
        float d = std::fmod(to - from, 360.f);
        if (d > 180.f)  d -= 360.f;
        if (d < -180.f) d += 360.f;
        return d;
    }

    inline float wrap_360(float a) {
        a = std::fmod(a, 360.f);
        if (a < 0.f) a += 360.f;
        return a;
    }

    inline Vector3 angle_to(const Vector3& from, const Vector3& to) {
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float dz = to.z - from.z;
        const float dist_xz = std::sqrt(dx * dx + dz * dz);
        float pitch = -std::atan2(dy, dist_xz) * (180.f / 3.14159265f);
        float yaw = std::atan2(dx, dz) * (180.f / 3.14159265f);
        return Vector3(wrap_360(pitch), wrap_360(yaw), 0.f);
    }

    inline float angle_distance(float from, float to) {
        float d = shortest_delta(from, to);
        return std::fabs(d);
    }

    inline float angle_distance_2d(float pitch_from, float yaw_from, float pitch_to, float yaw_to) {
        float dp = shortest_delta(pitch_from, pitch_to);
        float dy = shortest_delta(yaw_from, yaw_to);
        return std::sqrt(dp * dp + dy * dy);
    }

    struct target_info {
        Vector3 bone_pos{};
        Vector3 desired_angles{};
        float screen_dist{0.f};
        uint64_t object{0};
        bool valid{false};
    };

    inline target_info g_target{};
    inline long long g_last_write = 0;

    inline void tick(const cam::main_chain& chain, const Vector3& local_pos,
                     const std::vector<player::frame_player>& frame, int local_team,
                     const matrix& view, const matrix& projection) {
        g_target = {};
        if (!cfg::aim::enabled) return;
        if (!chain.main_bor) return;

        const cam::bor_pose pose = cam::read_bor(chain.main_bor);
        if (!pose.valid) return;

        struct Raw { Vector3 position; Vector3 euler; };
        Raw raw{};
        if (!mem_read(chain.main_bor + v100::bor_transform, &raw, sizeof(raw))) return;

        const Vector3 cam_pos = raw.position;
        const Vector3 cur_euler = raw.euler;

        target_info best{};
        best.screen_dist = cfg::aim::fov + 1.f;

        for (size_t i = 1; i < frame.size(); ++i) {
            const player::frame_player& fp = frame[i];
            if (!fp.pose.valid) continue;
            if (fp.snap->team == local_team) continue;
            if (fp.want_health && !(fp.health.valid && fp.health.hp > 0)) continue;

            const Vector3 bone = bone_position(fp.pose.feet, fp.pose.head, cfg::aim::bone);

            if (cfg::aim::visible_check) {
                ImVec2 scr;
                if (!world_to_screen(bone, view, projection, scr)) continue;
                if (scr.x < 0.f || scr.x > g_sw || scr.y < 0.f || scr.y > g_sh) continue;
            }

            const Vector3 desired = angle_to(cam_pos, bone);
            const float dist = angle_distance_2d(cur_euler.x, cur_euler.y, desired.x, desired.y);

            if (dist < best.screen_dist) {
                best.bone_pos = bone;
                best.desired_angles = desired;
                best.screen_dist = dist;
                best.object = fp.snap->object;
                best.valid = true;
            }
        }

        if (!best.valid || best.screen_dist > cfg::aim::fov) return;
        g_target = best;

        const long long now = diag::now_ms();

        const float smooth_factor = std::clamp(cfg::aim::smooth, 1.f, 30.f);
        const float t = 1.f / smooth_factor;

        float dp = shortest_delta(cur_euler.x, best.desired_angles.x);
        float dy = shortest_delta(cur_euler.y, best.desired_angles.y);

        Vector3 new_euler;
        new_euler.x = wrap_360(cur_euler.x + dp * t);
        new_euler.y = wrap_360(cur_euler.y + dy * t);
        new_euler.z = 0.f;

        mem_write(chain.main_bor + v100::bor_transform + sizeof(Vector3),
                  &new_euler, sizeof(Vector3));

        static long long aim_log = 0;
        if (diag::every(aim_log, 3000))
            diag::log("AIMBOT", "tgt=0x%llx bone=%d dist=%.1f cur=(%.1f %.1f) dst=(%.1f %.1f) out=(%.1f %.1f) t=%.2f dp=%.1f dy=%.1f",
                      (unsigned long long)best.object, cfg::aim::bone, best.screen_dist,
                      cur_euler.x, cur_euler.y, best.desired_angles.x, best.desired_angles.y,
                      new_euler.x, new_euler.y, t, dp, dy);

        g_last_write = now;
    }

    inline void draw_fov_circle(float camera_fov) {
        if (!cfg::aim::enabled || !cfg::aim::draw_fov) return;
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const float cx = g_sw * 0.5f;
        const float cy = g_sh * 0.5f;
        const float screen_h = g_sh;
        const float actual_fov = (camera_fov > 1.f) ? camera_fov : 60.f;
        const float radius = (cfg::aim::fov / actual_fov) * (screen_h * 0.5f);
        dl->AddCircle(ImVec2(cx, cy), radius,
                      IM_COL32(255, 255, 255, 80), 64, 1.5f);
    }
}
