#pragma once
#include "imgui.h"

namespace cfg {

namespace esp {
    inline bool box = true;
    inline bool name = true;
    inline bool health = true;
    inline bool weapon = true;
    inline bool distance = true;
    inline int box_style = 1;        // 0=full rect, 1=cornered, 2=rounded
    inline float box_rounding = 4.f;
    inline bool box_fill = true;
    inline bool smooth = false;
    inline bool teammates = false;
    inline bool visible_check = true;
    inline ImVec4 box_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 name_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 health_col = ImVec4(240/255.f, 64/255.f, 90/255.f, 1.f);
    inline ImVec4 team_col = ImVec4(0.3f, 0.6f, 1.f, 1.f);
    inline bool skeleton = true;
}

namespace trigger {
    inline bool enabled = false;
    inline int delay_ms = 0;
    // How long the injected finger stays down per shot. The PC donor holds a
    // mouse button for 16 ms; a phone fire button needs a little more.
    inline int hold_ms = 35;
    inline float fire_x = 0.f;
    inline float fire_y = 0.f;
    inline bool fire_set = false;
    inline bool picking = false;
    // Wall check: ray-cast the eye -> hit point segment against the map
    // collider set (assets/maps). Off = shoot at anything the crosshair touches.
    inline bool vis_check = true;
    // Refuse to fire while no map collider is loaded. Off falls back to the
    // game's own visibility flags, which are far weaker (see triggerbot.hpp).
    inline bool strict = true;
    // A teammate between the eye and the enemy cancels the shot.
    inline bool block_teammates = true;
    inline float min_distance = 0.f;
    // 0 = no limit. Without a limit the trigger fired at 40 m through the whole
    // level (session 20261121: 375 shots, 312 of them dealt no damage).
    inline float max_distance = 30.f;
}

namespace aim {
    inline bool enabled = false;
    inline float fov = 60.f;      // FOV circle radius in pixels
    inline float smooth = 5.f;    // smoothing (higher = slower/smoother)
    inline int target = 0;        // 0=head, 1=neck, 2=chest
    inline bool vis_check = true;
    inline bool show_fov = false;
}

}
