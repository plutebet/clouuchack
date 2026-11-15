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

    // WeaponContext is embedded in uyg (WeaponPart) at +0x30.
    // State<WeaponState>.Value._raw is at WeaponContext+0x58.
    // buw enum: Idle=0, Attack=1, Reload=2, Inspect=3, Take=4, Charge=5.
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
        const float scale = cfg::trigger::hitbox_scale;

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

                set_weapon_state(uyg_ptr, 1);

                g_state.on_target = true;
                g_state.target_object = fp.snap->object;
                g_state.firing = true;
                s_locked = fp.snap->object;

                static long long trig_log = 0;
                if (diag::every(trig_log, 2000))
                    diag::log("TRIGGER", "FIRE obj=0x%llx hitbox=(%.0f,%.0f)-(%.0f,%.0f)",
                              (unsigned long long)fp.snap->object,
                              mid_x - half_w, y1, mid_x + half_w, y2);
                return;
            }
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
