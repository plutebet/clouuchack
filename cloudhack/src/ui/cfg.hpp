
// ================================================================================
// Origin: @imguisource_bot
// Community:
//  - ImGui Source: @ImGui_Source_FIXNEY
//  - shit src 🧨: @il2cpp_src
// ================================================================================

#pragma once
#include "imgui.h"

namespace cfg {

namespace menu {
    inline float scale_x = 1.f;                // panel width scale, set by the grip in the bottom-right corner
    inline float scale_y = 1.f;                // panel height scale
}

namespace esp {
    inline bool box = true;
    inline bool name = false;
    inline bool health = true;
    inline bool weapon = true;
    // The three below add elements the ESP did not draw before, so they stay
    // off until they are switched on in the menu.
    inline bool distance = false;   // "23m" under the box / next to an arrow
    inline bool arrows = false;     // direction arrows for enemies outside the view
    inline bool corner = false;     // corner-style box instead of the full outline
    inline float box_rounding = 0.f;
    // Interpolate enemy positions between game frames (one game frame of
    // delay, no stepping when the overlay runs faster than the game).
    inline bool smooth = true;
    // cloudhack default: red (240,64,90); the menu Color row drives box/health.
    inline ImVec4 box_col = ImVec4(240/255.f, 64/255.f, 90/255.f, 1.f);
    inline ImVec4 name_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 health_col = ImVec4(240/255.f, 64/255.f, 90/255.f, 1.f);
}

}


