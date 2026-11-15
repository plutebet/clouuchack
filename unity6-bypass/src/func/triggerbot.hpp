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

    // Zenin-style: get target bone position based on config
    inline Vector3 get_bone_pos(const player::frame_player& fp) {
        switch (cfg::trigger::bones) {
            case 0: return fp.pose.head;
            case 1: return fp.pose.has_chest ? fp.pose.chest : fp.pose.head;
            case 2: return fp.pose.has_chest ? fp.pose.chest :
                           Vector3((fp.pose.head.x + fp.pose.feet.x) * 0.5f,
                                   (fp.pose.head.y + fp.pose.feet.y) * 0.5f,
                                   (fp.pose.head.z + fp.pose.feet.z) * 0.5f);
            case 3: return fp.pose.feet;
            default: return fp.pose.head;
        }
    }

    // Zenin algorithm: 3D distance + screen bone radius + visibility
    inline void tick(const cam::main_chain& chain,
                     const std::vector<player::frame_player>& frame, int local_team,
                     const matrix& view, const matrix& projection) {
        const bool was_firing = g_firing;
        g_firing = false;
        if (!cfg::trigger::enabled) {
            g_delay_pending = false;
            g_target_obj = 0;
            if (was_firing) {
                if (!frame.empty() && frame[0].snap) {
                    uint64_t uyg = find_local_weapon_part(*frame[0].snap);
                    if (uyg) set_weapon_state(uyg, 0);
                }
            }
            return;
        }
        if (frame.empty() || !frame[0].snap) return;

        const player_snapshot& local = *frame[0].snap;
        const uint64_t uyg_ptr = find_local_weapon_part(local);
        if (!uyg_ptr) return;

        const float cx = g_sw * 0.5f;
        const float cy = g_sh * 0.5f;
        const auto now = clock_t::now();

        // Interval check (Zenin: minimum ms between shots)
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

            // Step 1: 3D distance check (Zenin: config.max_distance)
            float dist = calculate_distance(local_pos, bone);
            if (dist <= MIN_DIST || !std::isfinite(dist)) continue;
            if (dist > cfg::trigger::max_distance) continue;

            // Step 2: Visible check (Zenin: visibility_data[0x34C])
            if (cfg::trigger::visible_check) {
                if (fp.want_health && fp.health.valid) {
                    // Use health as proxy for visibility — if we can read hp, player is loaded
                } else {
                    continue;
                }
            }

            // Step 3: Project bone to screen and check radius (Zenin: bone_radius)
            ImVec2 scr;
            if (!world_to_screen(bone, view, projection, scr)) continue;

            float dx = scr.x - cx;
            float dy = scr.y - cy;
            float screen_dist = std::sqrt(dx * dx + dy * dy);

            if (screen_dist > cfg::trigger::bone_radius) continue;

            // Step 4: Delay check (Zenin: delay_ms before firing)
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

            // === FIRE ===
            set_weapon_state(uyg_ptr, 1);
            g_firing = true;
            g_target_obj = fp.snap->object;
            g_last_fire = now;
            g_delay_pending = false;
            return;
        }

        // No target found
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
