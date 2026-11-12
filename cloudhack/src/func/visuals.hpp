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
    // Render thread (main loop): draws the newest sample published by the reader.
    void draw();

    // Reader thread: reads the game at 144 Hz and publishes one sample per tick. Started once the
    // game layer is up, stopped at shutdown.
    void start();
    void stop();

    // The game went away or the match changed: the reader drops its cached state.
    void request_reset();

    void dbox(const ImVec2& t, const ImVec2& b, float a);
    void dhp(int hp, int max_hp, const ImVec2& t, const ImVec2& b, float box_h, float a);
    void dnick(const char* name, const ImVec2& box_max, float cx, float sz, float a);
    void dweapon(int weapon_id, float cx, float y, float box_w, float sz, float a);
    void draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text);
}
