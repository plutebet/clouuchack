#pragma once

#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../game/camera.hpp"
#include "../game/weapon.hpp"
#include "../ui/cfg.hpp"
#include "../other/memory.hpp"
#include <cmath>
#include <algorithm>

namespace triggerbot {

    inline constexpr uint64_t kWeaponStateOffset = 0x88;

    inline bool g_firing{false};

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
        const bool was_firing = g_firing;
        g_firing = false;
        if (!cfg::trigger::enabled) return;
        if (frame.empty() || !frame[0].snap) return;

        const player_snapshot& local = *frame[0].snap;
        const uint64_t uyg_ptr = find_local_weapon_part(local);
        if (!uyg_ptr) return;

        const float cx = g_sw * 0.5f;
        const float cy = g_sh * 0.5f;

        for (size_t i = 1; i < frame.size(); ++i) {
            const player::frame_player& fp = frame[i];
            if (!fp.pose.valid) continue;
            if (fp.snap->team == local_team) continue;
            if (fp.want_health && !(fp.health.valid && fp.health.hp > 0)) continue;

            // W2S head + feet → bounding box
            ImVec2 scr_head, scr_feet;
            if (!world_to_screen(fp.pose.head + Vector3(0.f, 0.15f, 0.f), view, projection, scr_head)) continue;
            if (!world_to_screen(fp.pose.feet, view, projection, scr_feet)) continue;

            float y1 = std::min(scr_head.y, scr_feet.y);
            float y2 = std::max(scr_head.y, scr_feet.y);
            float h = y2 - y1;
            if (h < 5.f) continue;
            float half_w = h * 0.25f * cfg::trigger::hitbox_scale;
            float mid_x = (scr_head.x + scr_feet.x) * 0.5f;

            if (cx >= mid_x - half_w && cx <= mid_x + half_w &&
                cy >= y1 && cy <= y2) {
                set_weapon_state(uyg_ptr, 1);
                g_firing = true;
                return;
            }
        }

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
