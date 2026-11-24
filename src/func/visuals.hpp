// ================================================================================
// -----------------[ TELEGRAM SOURCE ]-----------------
// * BOT:     t.me/imguisource_bot
// * CHANNEL: t.me/ImGui_Source_FIXNEY (ImGui Source)
// * CH 2:    t.me/il2cpp_src (shit src 🧨)
// -----------------------------------------------------
// ================================================================================

#pragma once

#include "imgui.h"

namespace visuals {
    void draw();
    void dbox(const ImVec2& t, const ImVec2& b, float a);
    // Info bar: dark pill above the box, white nickname, HP line inside the
    // pill bottom (length and colour both follow HP).
    void dnick(const char* name, int hp, int max_hp, const ImVec2& box_min, float cx, float sz, float a);
    void dweapon(int weapon_id, float cx, float y, float box_w, float sz, float a);
    // Tracer line from the top of the screen to the enemy (reference look):
    // one thin white line per player, all converging at the same point.
    void dline(const ImVec2& from, const ImVec2& to, float a);
    void draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text);
}
