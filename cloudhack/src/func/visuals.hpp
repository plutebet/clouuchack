
// ================================================================================
// -----------------[ TELEGRAM SOURCE ]-----------------
// * BOT:     t.me/imguisource_bot
// * CHANNEL: t.me/ImGui_Source_FIXNEY (ImGui Source)
// * CH 2:    t.me/il2cpp_src (shit src 🧨)
// -----------------------------------------------------
// ================================================================================

#pragma once

#include "imgui.h"
#include <cstdint>

struct Vector3;
struct matrix;

namespace visuals {
    void draw();

    // ---- one enemy on the screen ------------------------------------------
    enum class box_state {
        none,         // head or feet cannot be projected (behind the camera, far outside the view)
        degenerate,   // projected, but the box is empty or taller than three screens
        off_screen,   // a valid box that lies completely outside the screen
        visible,
    };
    struct box_rect { ImVec2 min, max; float cx; };   // pixel-snapped, max is the last pixel of the outline
    struct target {
        box_rect box;
        uint64_t key;        // per-enemy animation state (health trail)
        float distance;      // metres
        int hp, max_hp;
        int weapon;          // WeaponId, -1 unknown
        const char* name;    // null: no name row
    };
    box_state project_box(const Vector3& feet, const Vector3& head, const matrix& view, const matrix& projection, box_rect& out);
    void draw_target(const target& t, long long now_ms, float dt_s);

    // ---- arrows for enemies outside the view -------------------------------
    struct arrow { ImVec2 pos, dir; };   // dir: unit vector on the screen, y down
    bool offscreen_arrow(const matrix& view, const Vector3& world, arrow& out);
    void draw_arrow(const arrow& ar, float distance, float a);

    // ---- elements ------------------------------------------------------------
    void dbox(const ImVec2& t, const ImVec2& b, float a);
    void dhp(int hp, int max_hp, float trail_hp, const ImVec2& t, const ImVec2& b, float a, float number_sz);
    void dnick(const char* name, const ImVec2& box_min, float cx, float sz, float a);
    float dweapon(int weapon_id, float cx, float y, float box_w, float sz, float a);   // returns the height used
    float ddist(float metres, float cx, float y, float sz, float a);                    // returns the height used
    void draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text);
}
