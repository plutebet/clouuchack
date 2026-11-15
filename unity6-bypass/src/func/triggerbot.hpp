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

namespace triggerbot {

    inline Vector3 bone_pos(const player::PlayerPose& pose, int bone) {
        switch (bone) {
            case 0: return pose.head;
            case 1:
                if (pose.has_chest) return pose.chest;
                return Vector3(pose.head.x,
                    pose.head.y - (pose.head.y - pose.feet.y) * 0.30f,
                    pose.head.z);
            case 2: return pose.feet;
            default: return pose.head;
        }
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
        float yaw   =  std::atan2(dx, dz) * (180.f / 3.14159265f);
        return Vector3(wrap_360(pitch), wrap_360(yaw), 0.f);
    }

    struct trigger_state {
        bool on_target{false};
        uint64_t target_object{0};
        Vector3 snap_angles{};
    };
    inline trigger_state g_state{};

    inline void tick(const cam::main_chain& chain,
                     const std::vector<player::frame_player>& frame, int local_team,
                     const matrix& view, const matrix& projection) {
        g_state = {};
        if (!cfg::trigger::enabled) return;
        if (!chain.main_bor) return;

        struct Raw { Vector3 position; Vector3 euler; };
        Raw raw{};
        if (!mem_read(chain.main_bor + v100::bor_transform, &raw, sizeof(raw))) return;
        if (!std::isfinite(raw.position.x) || !std::isfinite(raw.euler.x)) return;

        const float cx = g_sw * 0.5f;
        const float cy = g_sh * 0.5f;
        const float scale = cfg::trigger::hitbox_scale;

        const uint64_t* lock_obj = cfg::trigger::target_lock ? &g_state.target_object : nullptr;
        static uint64_t s_locked = 0;

        for (size_t i = 1; i < frame.size(); ++i) {
            const player::frame_player& fp = frame[i];
            if (!fp.pose.valid) continue;
            if (fp.snap->team == local_team) continue;
            if (fp.want_health && !(fp.health.valid && fp.health.hp > 0)) continue;

            ImVec2 scr_head, scr_feet;
            if (!world_to_screen(fp.pose.head + Vector3(0.f, 0.15f, 0.f), view, projection, scr_head)) continue;
            if (!world_to_screen(fp.pose.feet, view, projection, scr_feet)) continue;

            float y1 = std::min(scr_head.y, scr_feet.y);
            float y2 = std::max(scr_head.y, scr_feet.y);
            float h = y2 - y1;
            if (h < 5.f) continue;
            float half_w = h * 0.25f * scale;
            float mid_x = (scr_head.x + scr_feet.x) * 0.5f;

            if (cx >= mid_x - half_w && cx <= mid_x + half_w &&
                cy >= y1 - h * 0.05f * scale && cy <= y2 + h * 0.05f * scale) {

                if (cfg::trigger::target_lock && s_locked && s_locked != fp.snap->object) continue;

                const Vector3 bone = bone_pos(fp.pose, cfg::trigger::bone);
                const Vector3 angles = angle_to(raw.position, bone);

                Vector3 snap;
                snap.x = angles.x;
                snap.y = angles.y;
                snap.z = 0.f;

                mem_write(chain.main_bor + v100::bor_transform + sizeof(Vector3),
                          &snap, sizeof(Vector3));

                g_state.on_target = true;
                g_state.target_object = fp.snap->object;
                g_state.snap_angles = snap;
                s_locked = fp.snap->object;

                static long long trig_log = 0;
                if (diag::every(trig_log, 2000))
                    diag::log("TRIGGER", "SNAP obj=0x%llx bone=%d euler=(%.1f %.1f) hitbox=(%.0f,%.0f)-(%.0f,%.0f)",
                              (unsigned long long)fp.snap->object, cfg::trigger::bone,
                              snap.x, snap.y, mid_x - half_w, y1, mid_x + half_w, y2);
                return;
            }
        }
        s_locked = 0;
    }

    inline void draw_indicator() {
        if (!cfg::trigger::enabled || !cfg::trigger::draw_indicator) return;
        if (!g_state.on_target) return;
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const float cx = g_sw * 0.5f;
        const float cy = g_sh * 0.5f;
        dl->AddCircleFilled(ImVec2(cx, cy), 4.f, IM_COL32(255, 50, 50, 200), 12);
    }
}
