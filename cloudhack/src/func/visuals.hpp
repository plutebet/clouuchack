
// ================================================================================
// -----------------[ TELEGRAM SOURCE ]-----------------
// * BOT:     t.me/imguisource_bot
// * CHANNEL: t.me/ImGui_Source_FIXNEY (ImGui Source)
// * CH 2:    t.me/il2cpp_src (shit src 🧨)
// -----------------------------------------------------
// ================================================================================

#pragma once

#include "imgui.h"
#include "../game/game.hpp"
#include "../other/vector3.h"

namespace visuals {
    void draw();
    void dbox(const ImVec2& t, const ImVec2& b, float a, float flash);
    void dhp(float shown, float trail, int hp, int max_hp, const ImVec2& t, const ImVec2& b, float a);
    void dnick(const char* name, const ImVec2& box_min, float cx, float sz, float a);
    void dstance(int stance, const ImVec2& box_min, const ImVec2& box_max, float a, float sz);
    float dweapon(int weapon_id, float cx, float y, float box_w, float sz, float a);
    void ddistance(float meters, float cx, float y, float a, float sz);
    void darrow(const Vector3& world, float meters, const matrix& view);
    void draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text);
}
