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

namespace aim {
    inline bool enabled = false;
    inline int fov_deg = 15;
    inline int smooth = 15;
    inline int priority = 0;
    inline bool lock_target = true;
    inline bool back_camera = false;
}

}
