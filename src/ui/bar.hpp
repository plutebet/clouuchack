// ================================================================================
// Watermark "cloudhack" (top-left). Tapping it opens / closes the menu.
// With cfg::menu::open_watermark off the watermark is hidden and a double tap
// in the screen centre opens / closes the menu instead.
// ================================================================================

#pragma once
#include "imgui.h"

namespace ui::bar {
    inline bool g_open = false;          // menu requested open
    inline float g_alpha = 0.f;          // eased 0..1 of g_open
    inline float g_game_alpha = 1.f;     // 1 while the game runs, fades to 0 otherwise
    inline ImVec2 g_rect_min, g_rect_max; // watermark rectangle (menu anchors under it)
    inline bool g_dragging = false;       // finger is moving the watermark right now
    // Menu panel rectangle of the last frame (set by ui::menu): a centre
    // double tap that starts on the panel belongs to the panel.
    inline bool g_panel_visible = false;
    inline ImVec2 g_panel_min, g_panel_max;

    void render();
    void set_game_alpha(float a);
    float game_alpha();
}
