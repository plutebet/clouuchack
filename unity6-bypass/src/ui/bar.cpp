// ================================================================================
// Menu open controller (no watermark).
//   double tap in the screen centre  -> open the menu (only while it is closed)
//   ui::bar::close()                 -> close it (butterfly logo in the menu)
// The centre is detected from the real overlay size and re-detected when the
// surface size changes. Both taps must land on the centre point itself (a
// fingertip tolerance of 40 base px). After the first tap a small dot marks
// the point until the double-tap window expires.
// ================================================================================

#include "bar.hpp"
#include "open_gesture.hpp"
#include "../other/diag.hpp"
#include "theme/theme.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>

namespace ui::bar {
    static float ease(float t) { return -(cosf(3.14159265f * t) - 1.f) / 2.f; }

    void set_game_alpha(float a) { g_game_alpha = a; }
    float game_alpha() { return g_game_alpha; }

    namespace {
        bool g_state = false;     // menu open requested
        float g_prog = 0.f;
        float g_last_toggle = -1.f;
        gesture::double_tap g_tap;
        gesture::center_zone g_zone;

        void update_zone(float k) {
            const ImGuiIO& io = ImGui::GetIO();
            const float w = io.DisplaySize.x >= 2.f ? io.DisplaySize.x : g_sw;
            const float h = io.DisplaySize.y >= 2.f ? io.DisplaySize.y : g_sh;
            const float tolerance = 40.f * k;
            if (g_zone.valid && g_zone.width == w && g_zone.height == h && g_zone.radius == tolerance) return;
            if (gesture::zone_from_display(w, h, tolerance, g_zone))
                diag::log("INFO", "menu open: screen centre point detected at (%.0f, %.0f) tolerance=%.0f px, display %.0fx%.0f",
                          g_zone.cx, g_zone.cy, g_zone.radius, w, h);
            else
                diag::log("WARN", "menu open: display size %.0fx%.0f unusable, centre double tap disabled", w, h);
            g_center_valid = g_zone.valid;
            g_center = ImVec2(g_zone.cx, g_zone.cy);
        }

        void draw_hint(float k, float ct) {
            if (!g_tap.armed || !g_zone.valid || g_state) return;
            const float left = 1.f - ImClamp((ct - float(g_tap.first_t)) / g_tap.max_gap_s, 0.f, 1.f);
            const float a = left * g_game_alpha;
            if (a < 0.01f) return;
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            dl->AddCircleFilled(ImVec2(g_zone.cx, g_zone.cy), 6.f * k, IM_COL32(59, 196, 111, int(200 * a)), 16);
        }
    }

    void close() {
        g_state = false;
        g_tap.reset();
    }

    void render() {
        if (g_game_alpha < 0.001f) return;
        const ImGuiIO& io = ImGui::GetIO();
        const float k = style::k();
        const float ct = float(ImGui::GetTime());

        update_zone(k);
        g_tap.max_move_px = 24.f * k;
        g_tap.max_pair_px = 2.f * g_zone.radius;
        const ImVec2 mp = io.MousePos;
        const bool on_panel = g_panel_visible && mp.x >= g_panel_min.x && mp.x <= g_panel_max.x &&
                              mp.y >= g_panel_min.y && mp.y <= g_panel_max.y;
        // The gesture only opens; while the menu is up it is closed from the logo.
        const bool fired = g_tap.update(io.MouseDown[0], mp.x, mp.y, double(ct), g_zone, on_panel || g_state);
        if (fired && !g_state && ct - g_last_toggle >= 0.25f) {
            g_state = true;
            g_last_toggle = ct;
        }

        float dt = io.DeltaTime;
        if (dt <= 0.f || dt > 0.1f) dt = 0.016f;
        const float tgt = g_state ? 1.f : 0.f;
        const float spd = 10.f * dt;
        if (g_prog < tgt) g_prog = ImMin(g_prog + spd, tgt);
        else if (g_prog > tgt) g_prog = ImMax(g_prog - spd, tgt);
        g_alpha = ease(g_prog);
        g_open = g_state;

        draw_hint(k, ct);
    }
}
