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
}

namespace trigger {
    inline bool enabled = false;
    inline float fov = 1.0f;
    inline int delay_ms = 50;
    inline int interval_ms = 100;
    inline int confirm = 2;
    inline int region = 0;
    inline float fire_x = 0.f;
    inline float fire_y = 0.f;
    inline bool fire_set = false;
    inline bool picking = false;
}

}
