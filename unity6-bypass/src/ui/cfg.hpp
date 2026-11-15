
// ================================================================================
// Origin: @imguisource_bot
// Community:
//  - ImGui Source: @ImGui_Source_FIXNEY
//  - shit src 🧨: @il2cpp_src
// ================================================================================

#pragma once
#include "imgui.h"

namespace cfg {

namespace esp {
    inline bool box = true;
    inline bool name = false;
    inline bool health = true;
    inline bool weapon = true;
    // 0 Full (outlined), 1 Corner, 2 Clean: thin translucent rectangle,
    // width = half the height, no outline (reference screenshot style).
    inline int box_type = 2;
    inline float box_rounding = 0.f;
    // Off (default): boxes are drawn from the latest game frame and the
    // overlay only presents when that frame changed, so it updates in step
    // with the game model and costs no presents while nothing moves.
    // On: enemy positions are interpolated between game frames (one game
    // frame of delay) and the overlay presents at the display rate.
    inline bool smooth = false;
    // Default white for the Clean box; the menu Color row drives box and weapon icon.
    inline ImVec4 box_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 name_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 health_col = ImVec4(240/255.f, 64/255.f, 90/255.f, 1.f);
}

namespace trigger {
    inline bool enabled = false;
    inline int mode = 0;               // 0 Bones
    inline int bones = 0;              // 0 Head, 1 Upper, 2 Body, 3 Lower
    inline float bone_radius = 8.0f;   // px radius around bone on screen
    inline int delay_ms = 0;           // ms before firing
    inline int interval_ms = 120;      // ms between shots
    inline float max_distance = 40.f;  // meters
    inline bool scoped_only = false;
    inline bool untouchable_check = false;
    inline bool visible_check = true;
    inline int method = 0;             // 0 Mixed, 1 Head, 2 Body
    inline bool auto_wall = false;
    inline bool spread_prediction = false;
    inline bool min_damage = false;
    inline int damage_hp = 1;
    inline bool force_lethal = false;
    inline bool draw_indicator = true;
}

}


