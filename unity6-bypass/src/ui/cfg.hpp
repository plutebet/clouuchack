
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

}


