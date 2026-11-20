#pragma once
#include "imgui.h"

namespace cfg {

namespace esp {
    inline bool box = true;
    inline bool name = false;
    inline bool health = true;
    inline bool weapon = true;
    inline float box_rounding = 0.f;
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
    inline float fov = 1.0f;
    inline int delay_ms = 0;
    // interval/confirm/region removed — TriggerFire/TriggerDelay state machines handle timing
    inline float fire_x = 0.f;
    inline float fire_y = 0.f;
    inline bool fire_set = false;
    inline bool picking = false;
    inline bool vis_check = true;
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
