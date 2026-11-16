#pragma once

#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../game/camera.hpp"
#include "../ui/cfg.hpp"
#include "../other/diag.hpp"
#include "../other/touch_inject.hpp"
#include <cmath>
#include <algorithm>
#include <chrono>

namespace triggerbot {

    using clk = std::chrono::steady_clock;

    enum State { IDLE, TAP_DOWN, COOLDOWN };
    inline State g_state{IDLE};
    inline clk::time_point g_tap_start{};
    inline clk::time_point g_last_fire{};
    inline bool g_firing{false};

    inline constexpr int TAP_MS = 45;

    inline bool is_on_crosshair(const Vector3& head, const Vector3& feet,
                                const matrix& view, const matrix& proj, float fov_px) {
        ImVec2 sh, sf;
        if (!world_to_screen(head, view, proj, sh)) return false;
        if (!world_to_screen(feet, view, proj, sf)) return false;

        const float cx = g_sw * 0.5f;
        const float cy = g_sh * 0.5f;
        const float mid_x = (sh.x + sf.x) * 0.5f;
        const float mid_y = (sh.y + sf.y) * 0.5f;
        const float dx = cx - mid_x;
        const float dy = cy - mid_y;
        return (dx * dx + dy * dy) <= fov_px * fov_px;
    }

    inline void tick(const cam::main_chain&,
                     const std::vector<player::frame_player>& frame, int local_team,
                     const matrix& view, const matrix& proj) {
        g_firing = false;

        if (!cfg::trigger::enabled) {
            if (g_state == TAP_DOWN) touch_inject::fire_up();
            g_state = IDLE;
            return;
        }
        if (frame.empty() || !frame[0].snap) return;

        const auto now = clk::now();
        auto ms_since = [&](clk::time_point t) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - t).count();
        };

        if (g_state == TAP_DOWN) {
            g_firing = true;
            if (ms_since(g_tap_start) >= TAP_MS) {
                touch_inject::fire_up();
                g_state = COOLDOWN;
                g_last_fire = now;
            }
            return;
        }

        if (g_state == COOLDOWN) {
            if (ms_since(g_last_fire) >= cfg::trigger::interval_ms)
                g_state = IDLE;
            return;
        }

        if (cfg::trigger::delay_ms > 0) {
            static clk::time_point delay_start{};
            static bool delay_active = false;
            bool found = false;

            for (size_t i = 1; i < frame.size(); ++i) {
                const auto& fp = frame[i];
                if (!fp.pose.valid || !fp.snap) continue;
                if (fp.snap->team == local_team) continue;
                if (fp.want_health && !(fp.health.valid && fp.health.hp > 0)) continue;

                const Vector3& head = fp.pose.head;
                const Vector3& feet = fp.pose.feet;
                float dx = frame[0].pose.head.x - feet.x;
                float dy = frame[0].pose.head.y - feet.y;
                float dz = frame[0].pose.head.z - feet.z;
                float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (!std::isfinite(dist) || dist > cfg::trigger::max_distance) continue;

                if (is_on_crosshair(head, feet, view, proj, cfg::trigger::fov)) {
                    found = true;
                    if (!delay_active) { delay_active = true; delay_start = now; }
                    if (ms_since(delay_start) >= cfg::trigger::delay_ms) {
                        touch_inject::fire_down(cfg::trigger::fire_x, cfg::trigger::fire_y);
                        g_state = TAP_DOWN;
                        g_tap_start = now;
                        g_firing = true;
                        delay_active = false;

                        static long long tlog = 0;
                        if (diag::every(tlog, 2000))
                            diag::log("TRIGGER", "TAP d=%.1f fov=%.0f", dist, cfg::trigger::fov);
                    }
                    break;
                }
            }
            if (!found) delay_active = false;
            return;
        }

        for (size_t i = 1; i < frame.size(); ++i) {
            const auto& fp = frame[i];
            if (!fp.pose.valid || !fp.snap) continue;
            if (fp.snap->team == local_team) continue;
            if (fp.want_health && !(fp.health.valid && fp.health.hp > 0)) continue;

            const Vector3& head = fp.pose.head;
            const Vector3& feet = fp.pose.feet;
            float dx = frame[0].pose.head.x - feet.x;
            float dy = frame[0].pose.head.y - feet.y;
            float dz = frame[0].pose.head.z - feet.z;
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (!std::isfinite(dist) || dist > cfg::trigger::max_distance) continue;

            if (is_on_crosshair(head, feet, view, proj, cfg::trigger::fov)) {
                touch_inject::fire_down(cfg::trigger::fire_x, cfg::trigger::fire_y);
                g_state = TAP_DOWN;
                g_tap_start = now;
                g_firing = true;

                static long long tlog = 0;
                if (diag::every(tlog, 2000))
                    diag::log("TRIGGER", "TAP d=%.1f fov=%.0f", dist, cfg::trigger::fov);
                return;
            }
        }
    }

    inline void draw_indicator() {
        if (!cfg::trigger::enabled || !cfg::trigger::draw_indicator) return;
        if (!g_firing) return;
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const float cx = g_sw * 0.5f;
        const float cy = g_sh * 0.5f;
        dl->AddCircleFilled(ImVec2(cx, cy), 5.f, IM_COL32(255, 40, 40, 220), 12);
    }
}
