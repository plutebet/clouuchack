// ================================================================================
// Menu open state. No watermark: the menu opens with a double tap in the
// screen centre (auto-detected from the overlay size) and closes from the
// butterfly logo inside the menu (ui::menu calls close()).
// ================================================================================

#pragma once
#include "imgui.h"

namespace ui::bar {
    inline bool g_open = false;          // menu requested open
    inline float g_alpha = 0.f;          // eased 0..1 of g_open
    inline float g_game_alpha = 1.f;     // 1 while the game runs, fades to 0 otherwise
    // Auto-detected screen centre (overlay pixels); the menu opens centred on it.
    inline bool g_center_valid = false;
    inline ImVec2 g_center;
    // Menu panel rectangle of the last frame (set by ui::menu): a press that
    // starts on the panel belongs to the panel, not to the open gesture.
    inline bool g_panel_visible = false;
    inline ImVec2 g_panel_min, g_panel_max;

    void render();
    void close();
    void set_game_alpha(float a);
    float game_alpha();
}
