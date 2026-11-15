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
            case 1: return head - Vector3(0.f, 0.12f, 0.f);
            default: {
                const float mid_y = (feet.y + head.y) * 0.5f + 0.1f;
                return Vector3(head.x, mid_y, head.z);
            }
        }
    }

    inline float normalize_angle(float a) {
        a = std::fmod(a, 360.f);
        if (a > 180.f)  a -= 360.f;
        if (a < -180.f) a += 360.f;
        return a;
    }

    inline Vector3 angle_to(const Vector3& from, const Vector3& to) {
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float dz = to.z - from.z;
        const float dist_xz = std::sqrt(dx * dx + dz * dz);
        float pitch = -std::atan2(dy, dist_xz) * (180.f / 3.14159265f);
        float yaw = std::atan2(dx, dz) * (180.f / 3.14159265f);
        return Vector3(pitch, yaw, 0.f);
    }

    inline float angle_distance_screen(const Vector3& current, const Vector3& target) {
        const float dp = normalize_angle(target.x - current.x);
        const float dy = normalize_angle(target.y - current.y);
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
    inline long long g_last_tick_ms = 0;

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
            const float dist = angle_distance_screen(cur_euler, desired);

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
        float dt = (g_last_tick_ms > 0) ? float(now - g_last_tick_ms) * 0.001f : 0.016f;
        dt = std::clamp(dt, 0.001f, 0.1f);
        g_last_tick_ms = now;

        const float smooth_factor = std::clamp(cfg::aim::smooth, 1.f, 30.f);
        const float speed = 15.f / smooth_factor;
        const float t = std::clamp(speed * dt, 0.01f, 1.f);

        Vector3 new_euler;
        new_euler.x = cur_euler.x + normalize_angle(best.desired_angles.x - cur_euler.x) * t;
        new_euler.y = cur_euler.y + normalize_angle(best.desired_angles.y - cur_euler.y) * t;
        new_euler.z = 0.f;

        new_euler.x = normalize_angle(new_euler.x);
        new_euler.y = normalize_angle(new_euler.y);

        mem_write(chain.main_bor + v100::bor_transform + sizeof(Vector3),
                  &new_euler, sizeof(Vector3));

        static long long aim_log = 0;
        if (diag::every(aim_log, 3000))
            diag::log("AIMBOT", "target=0x%llx bone=%d dist=%.1f euler=(%.1f %.1f) -> (%.1f %.1f) smooth=%.1f dt=%.3f vis=%d",
                      (unsigned long long)best.object, cfg::aim::bone, best.screen_dist,
                      cur_euler.x, cur_euler.y, new_euler.x, new_euler.y, smooth_factor, dt,
                      cfg::aim::visible_check ? 1 : 0);

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
