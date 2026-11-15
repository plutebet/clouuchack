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

namespace triggerbot {

    inline constexpr uint64_t kWeaponStateOffset = 0x88; // uyg + 0x30 + 0x58

    struct trigger_state {
        bool on_target{false};
        uint64_t target_object{0};
        bool firing{false};
    };
    inline trigger_state g_state{};

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

    // --- Mode 0: Screen W2S bounding-box check (original Zenith) ---
    inline bool check_screen_mode(const player::frame_player& fp,
                                  const matrix& view, const matrix& projection,
                                  float cx, float cy, float scale) {
        ImVec2 scr_head, scr_feet;
        if (!world_to_screen(fp.pose.head + Vector3(0.f, 0.15f, 0.f), view, projection, scr_head)) return false;
        if (!world_to_screen(fp.pose.feet, view, projection, scr_feet)) return false;

        float y1 = std::min(scr_head.y, scr_feet.y);
        float y2 = std::max(scr_head.y, scr_feet.y);
        float h = y2 - y1;
        if (h < 5.f) return false;
        float half_w = h * 0.25f * scale;
        float mid_x = (scr_head.x + scr_feet.x) * 0.5f;

        return cx >= mid_x - half_w && cx <= mid_x + half_w &&
               cy >= y1 - h * 0.05f * scale && cy <= y2 + h * 0.05f * scale;
    }

    // --- Mode 1: 3D distance + direction check (Zenin-style) ---
    inline bool check_3d_mode(const player::frame_player& fp,
                              const Vector3& local_pos,
                              const matrix& view, const matrix& projection,
                              float max_dist) {
        Vector3 target = fp.pose.head;
        if (cfg::trigger::bone == 1 && fp.pose.has_chest) target = fp.pose.chest;
        else if (cfg::trigger::bone == 2) target = fp.pose.feet;

        float dist = calculate_distance(local_pos, target);
        if (dist < 0.001f || !std::isfinite(dist)) return false;
        if (dist > max_dist) return false;

        // Project target to screen and check if crosshair is near it
        // (Zenin uses a 3D cone/angle check; we approximate with
        //  screen-space distance scaled by distance for the same effect)
        ImVec2 scr;
        if (!world_to_screen(target, view, projection, scr)) return false;

        float cx = g_sw * 0.5f;
        float cy = g_sh * 0.5f;
        float dx = scr.x - cx;
        float dy = scr.y - cy;
        float screen_dist = std::sqrt(dx * dx + dy * dy);

        // Adaptive threshold: closer targets need less precision on screen.
        // At 5m the radius is ~80px, at 50m ~20px (scaled by FOV via projection).
        float threshold = std::max(15.f, 400.f / std::max(dist, 1.f));

        return screen_dist <= threshold;
    }

    inline void tick(const cam::main_chain& chain,
                     const std::vector<player::frame_player>& frame, int local_team,
                     const matrix& view, const matrix& projection) {
        const bool was_firing = g_state.firing;
        g_state = {};
        if (!cfg::trigger::enabled) return;
        if (frame.empty() || !frame[0].snap) return;

        const player_snapshot& local = *frame[0].snap;
        const uint64_t uyg_ptr = find_local_weapon_part(local);
        if (!uyg_ptr) return;

        const float cx = g_sw * 0.5f;
        const float cy = g_sh * 0.5f;

        // Local player position for 3D mode
        Vector3 local_pos{};
        if (cfg::trigger::mode == 1 && frame[0].pose.valid)
            local_pos = frame[0].pose.head;

        static uint64_t s_locked = 0;

        for (size_t i = 1; i < frame.size(); ++i) {
            const player::frame_player& fp = frame[i];
            if (!fp.pose.valid) continue;
            if (fp.snap->team == local_team) continue;
            if (fp.want_health && !(fp.health.valid && fp.health.hp > 0)) continue;

            bool on_target = false;

            if (cfg::trigger::mode == 0) {
                on_target = check_screen_mode(fp, view, projection, cx, cy,
                                              cfg::trigger::hitbox_scale);
            } else {
                on_target = check_3d_mode(fp, local_pos, view, projection,
                                          cfg::trigger::max_distance);
            }

            if (!on_target) continue;

            if (cfg::trigger::target_lock && s_locked && s_locked != fp.snap->object) continue;

            set_weapon_state(uyg_ptr, 1);

            g_state.on_target = true;
            g_state.target_object = fp.snap->object;
            g_state.firing = true;
            s_locked = fp.snap->object;

            static long long trig_log = 0;
            if (diag::every(trig_log, 2000))
                diag::log("TRIGGER", "FIRE mode=%d obj=0x%llx",
                          cfg::trigger::mode, (unsigned long long)fp.snap->object);
            return;
        }

        if (was_firing)
            set_weapon_state(uyg_ptr, 0);

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
