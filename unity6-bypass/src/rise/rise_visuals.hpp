#pragma once

#include "imgui.h"

namespace rise_visuals {
    void draw();
    void dbox(const ImVec2& t, const ImVec2& b, float a);
    void dhp(int hp, int max_hp, const ImVec2& t, const ImVec2& b, float a);
    void dnick(const char* name, const ImVec2& box_min, float cx, float sz, float a);
    void draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text);
}
