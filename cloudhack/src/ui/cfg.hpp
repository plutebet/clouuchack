
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

namespace trigger {
    inline bool enabled = false;          // off by default, nothing fires on launch
    inline int lock_ms = 80;              // time checker: enemy must stay on the crosshair this long
    inline int tap_delay_ms = 30;         // delay between the lock and the tap
    inline int cooldown_ms = 150;         // minimum gap between two taps
    inline int fire_x = -1, fire_y = -1;  // fire button in screen px, set from the menu (bind row)
    inline bool bind_mode = false;        // menu: the next short tap on screen becomes the fire point
    inline bool button_set = false;               // screen toggle button placed
    inline float button_x = 0.f, button_y = 0.f;  // its centre, screen px
    inline bool button_bind_mode = false;         // menu: the next short tap places the toggle button
    inline bool fire_set() { return fire_x >= 0 && fire_y >= 0; }
}

namespace esp {
    inline bool box = true;
    inline bool name = false;
    inline bool health = true;
    inline bool weapon = true;
    inline bool skeleton = true;
    inline int box_type = 0;
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


