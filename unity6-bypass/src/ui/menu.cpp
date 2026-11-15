// ================================================================================
// cloudhack menu: sidebar layout.
//   left:  butterfly logo (tap = close the menu), "Visuals" tab,
//          "Unload" (tap = unload at once through status::g_quit_request)
//   right: Visuals - Box / Name / Health / Weapon / Smooth switches,
//                    Box Style (Full / Corner), Color (HSV popup)
// Opens centred on the auto-detected screen centre (ui::bar). Press on the
// panel and move = drag; a control fires on release without moving.
// Everything is drawn on the foreground draw list; no ImGui windows.
// Geometry is in base px for a 1224 px tall screen, scaled by style::k().
// ================================================================================

#define IMGUI_DEFINE_MATH_OPERATORS
#include "menu.hpp"
#include "bar.hpp"
#include "cfg.hpp"
#include "theme/theme.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "Android_draw/draw.h"
#include "fonts/menu/butterfly.h"
#include "../other/status.hpp"
#include <cmath>
#include <cfloat>
#include <string>

namespace ui::menu {
    using namespace style;

    namespace {
        // Palette of the reference design: neutral greys, green accent.
        const ImVec4 c_panel(0x1E / 255.f, 0x1E / 255.f, 0x1E / 255.f, 0.97f);
        const ImVec4 c_sidebar(0x26 / 255.f, 0x26 / 255.f, 0x26 / 255.f, 1.f);
        const ImVec4 c_tab_sel(0x3A / 255.f, 0x3A / 255.f, 0x3A / 255.f, 1.f);
        const ImVec4 c_widget(0x2E / 255.f, 0x2E / 255.f, 0x2E / 255.f, 1.f);
        const ImVec4 c_track_off(0xD6 / 255.f, 0xD6 / 255.f, 0xD6 / 255.f, 1.f);
        const ImVec4 c_green(0x3B / 255.f, 0xC4 / 255.f, 0x6F / 255.f, 1.f);
        const ImVec4 c_text(0xE8 / 255.f, 0xE8 / 255.f, 0xE8 / 255.f, 1.f);
        const ImVec4 c_text_dim(0x9A / 255.f, 0x9A / 255.f, 0x9A / 255.f, 1.f);
        const ImVec4 c_white(1.f, 1.f, 1.f, 1.f);
        const ImVec4 c_border(1.f, 1.f, 1.f, 0.08f);

        // Base sizes (px at k = 1).
        constexpr float k_panel_w = 720.f, k_panel_h = 636.f, k_sidebar_w = 200.f;
        constexpr float k_logo_h = 150.f, k_tab_h = 88.f, k_row = 76.f, k_pad = 28.f;

        float g_ma = 0.f;                 // menu alpha 0..1
        bool g_was_open = false;
        ImVec2 g_pos;                     // panel top-left
        bool g_color_open = false;        // colour popup
        ImVec4 g_hsv{0.f, 1.f, 1.f, 1.f}; // popup state (hue, sat, val)
        bool g_hsv_init = false;
        int g_picker = 0;                 // 0 none, 1 sat/val square, 2 hue bar
        bool g_panel_pressed = false;
        bool g_panel_dragging = false;
        ImVec2 g_panel_press;
        ImVec2 g_panel_grab;

        float lrp(float a, float b, float t) { return a + (b - a) * t; }
        ImVec4 lrp_col(const ImVec4& a, const ImVec4& b, float t) {
            return ImVec4(lrp(a.x, b.x, t), lrp(a.y, b.y, t), lrp(a.z, b.z, t), lrp(a.w, b.w, t));
        }
        bool inside(const ImVec2& p, const ImVec2& mn, const ImVec2& mx) {
            return p.x >= mn.x && p.x <= mx.x && p.y >= mn.y && p.y <= mx.y;
        }
        // A control fires on release: finger went down inside the rect and came
        // up inside it without travelling more than the drag threshold.
        bool tapped(const ImVec2& mn, const ImVec2& mx) {
            ImGuiIO& io = ImGui::GetIO();
            if (!ImGui::IsMouseReleased(0) || g_panel_dragging) return false;
            const ImVec2 down = io.MouseClickedPos[0];
            const ImVec2 up = io.MousePos;
            if (!inside(down, mn, mx) || !inside(up, mn, mx)) return false;
            const float thr = 10.f * style::k();
            const float dx = up.x - down.x, dy = up.y - down.y;
            return dx * dx + dy * dy <= thr * thr;
        }
        bool held(const ImVec2& mn, const ImVec2& mx) {
            ImGuiIO& io = ImGui::GetIO();
            return io.MouseDown[0] && !g_panel_dragging && inside(io.MouseClickedPos[0], mn, mx) && inside(io.MousePos, mn, mx);
        }
        ImVec2 clamp_panel(ImVec2 q, float w, float h, float k) {
            q.x = ImClamp(q.x, 8.f * k, ImMax(8.f * k, g_sw - 8.f * k - w));
            q.y = ImClamp(q.y, 8.f * k, ImMax(8.f * k, g_sh - 8.f * k - h));
            return q;
        }

        ImFont* menu_font() { return fontMenu ? fontMenu : (fontMedium ? fontMedium : ImGui::GetFont()); }

        void text_at(ImDrawList* dl, const ImVec2& pos, float h, const ImVec4& c, float a, const char* s) {
            ImFont* f = menu_font();
            dl->AddText(f, f->FontSize, ImVec2(pos.x, pos.y + (h - f->FontSize) * 0.5f), col(c, a), s);
        }

        // Filled ellipse rotated by rot radians (convex, so AddConvexPolyFilled).
        void ellipse(ImDrawList* dl, const ImVec2& c, float rx, float ry, float rot, ImU32 color) {
            constexpr int n = 32;
            ImVec2 pts[n];
            const float cr = cosf(rot), sr = sinf(rot);
            for (int i = 0; i < n; ++i) {
                const float t = 6.2831853f * float(i) / float(n);
                const float x = rx * cosf(t), y = ry * sinf(t);
                pts[i] = ImVec2(c.x + x * cr - y * sr, c.y + x * sr + y * cr);
            }
            dl->AddConvexPolyFilled(pts, n, color);
        }

        // Fallback logo when the butterfly texture is unavailable: white wings
        // with dark veins, body and antennae.
        void butterfly_vector(ImDrawList* dl, const ImVec2& c, float s, float a, float k) {
            const ImU32 wing = col(c_white, a);
            const ImU32 vein = col(c_sidebar, a);
            for (int side = -1; side <= 1; side += 2) {
                const float sx = float(side);
                const ImVec2 upper(c.x + sx * 0.25f * s, c.y - 0.12f * s);
                const ImVec2 lower(c.x + sx * 0.19f * s, c.y + 0.19f * s);
                ellipse(dl, upper, 0.25f * s, 0.19f * s, sx * -0.55f, wing);
                ellipse(dl, lower, 0.16f * s, 0.13f * s, sx * 0.65f, wing);
                const float vt = ImMax(1.f, 2.2f * k);
                const ImVec2 root(c.x + sx * 0.04f * s, c.y - 0.02f * s);
                dl->AddLine(root, ImVec2(c.x + sx * 0.44f * s, c.y - 0.30f * s), vein, vt);
                dl->AddLine(root, ImVec2(c.x + sx * 0.46f * s, c.y - 0.08f * s), vein, vt);
                dl->AddLine(root, ImVec2(c.x + sx * 0.22f * s, c.y - 0.30f * s), vein, vt);
                dl->AddLine(ImVec2(c.x + sx * 0.04f * s, c.y + 0.05f * s), ImVec2(c.x + sx * 0.30f * s, c.y + 0.27f * s), vein, vt);
                dl->AddLine(ImVec2(c.x + sx * 0.04f * s, c.y + 0.05f * s), ImVec2(c.x + sx * 0.14f * s, c.y + 0.31f * s), vein, vt);
                ellipse(dl, ImVec2(c.x + sx * 0.33f * s, c.y - 0.17f * s), 0.045f * s, 0.035f * s, 0.f, vein);
                dl->AddBezierCubic(ImVec2(c.x + sx * 0.015f * s, c.y - 0.22f * s),
                                   ImVec2(c.x + sx * 0.04f * s, c.y - 0.34f * s),
                                   ImVec2(c.x + sx * 0.10f * s, c.y - 0.42f * s),
                                   ImVec2(c.x + sx * 0.17f * s, c.y - 0.46f * s), wing, ImMax(1.f, 2.f * k));
            }
            ellipse(dl, ImVec2(c.x, c.y + 0.02f * s), 0.035f * s, 0.25f * s, 0.f, wing);
            ellipse(dl, ImVec2(c.x, c.y + 0.02f * s), 0.012f * s, 0.21f * s, 0.f, vein);
        }

        // Label on the left, pill switch on the right. Returns true on tap.
        bool toggle_row(ImDrawList* dl, const char* label, bool* v, const ImVec2& pos, float w, float h, float a, float k, bool live) {
            bool tap = false;
            if (live && !g_color_open && tapped(pos, ImVec2(pos.x + w, pos.y + h))) { *v = !*v; tap = true; }
            text_at(dl, pos, h, c_text, a, label);
            const float tw = 104.f * k, th = 54.f * k;
            const ImVec2 tmin(pos.x + w - tw, pos.y + (h - th) * 0.5f);
            const ImVec2 tmax(tmin.x + tw, tmin.y + th);
            const float t = anim(std::string("tg_") + label, *v ? 1.f : 0.f, 16.f);
            dl->AddRectFilled(tmin, tmax, col(lrp_col(c_track_off, c_green, t), a), th * 0.5f);
            const float kr = th * 0.5f - 6.f * k;
            const ImVec2 kc(tmin.x + th * 0.5f + (tw - th) * t, tmin.y + th * 0.5f);
            dl->AddCircleFilled(ImVec2(kc.x, kc.y + 1.5f * k), kr, IM_COL32(0, 0, 0, int(50 * a)), 24);
            dl->AddCircleFilled(kc, kr, col(c_white, a), 24);
            return tap;
        }

        // Row of equal buttons, the selected one green. Returns true when it changed.
        bool segmented(ImDrawList* dl, const char* const* names, int count, int* v, const ImVec2& pos, float w, float h, float a, float k, bool live) {
            const float gap = 10.f * k;
            const float bw = (w - gap * float(count - 1)) / float(count);
            const int before = *v;
            ImFont* f = menu_font();
            for (int i = 0; i < count; ++i) {
                const ImVec2 mn(pos.x + (bw + gap) * float(i), pos.y), mx(mn.x + bw, pos.y + h);
                if (live && !g_color_open && tapped(mn, mx)) *v = i;
                const float t = anim(std::string("sg_") + names[i], *v == i ? 1.f : 0.f, 16.f);
                dl->AddRectFilled(mn, mx, col(lrp_col(c_widget, c_green, t), a), 12.f * k);
                const ImVec2 ts = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.f, names[i]);
                dl->AddText(f, f->FontSize, ImVec2(mn.x + (bw - ts.x) * 0.5f, mn.y + (h - ts.y) * 0.5f),
                            col(lrp_col(c_text, c_white, t), a), names[i]);
            }
            return *v != before;
        }

        // Label + rounded colour swatch. Tap toggles the popup.
        void color_row(ImDrawList* dl, const char* label, ImVec4* v, const ImVec2& pos, float w, float h, float a, float k, bool live) {
            text_at(dl, pos, h, c_text, a, label);
            const float s = 54.f * k;
            const ImVec2 smin(pos.x + w - s * 1.9f, pos.y + (h - s) * 0.5f);
            const ImVec2 smax(pos.x + w, smin.y + s);
            if (live && tapped(smin, smax)) {
                g_color_open = !g_color_open;
                if (g_color_open) {
                    ImGui::ColorConvertRGBtoHSV(v->x, v->y, v->z, g_hsv.x, g_hsv.y, g_hsv.z);
                    g_hsv_init = true;
                }
            }
            dl->AddRectFilled(smin, smax, IM_COL32(int(v->x * 255), int(v->y * 255), int(v->z * 255), int(255 * a)), 14.f * k);
            dl->AddRect(smin, smax, IM_COL32(255, 255, 255, int((g_color_open ? 110 : 40) * a)), 14.f * k, 0, 1.5f * k);
        }

        // HSV picker over the content area at the colour row's height.
        void color_popup(ImDrawList* dl, ImVec4* v, const ImVec2& amin, const ImVec2& amax, float anchor_y, float a, float k) {
            if (!g_color_open) return;
            if (!g_hsv_init) { ImGui::ColorConvertRGBtoHSV(v->x, v->y, v->z, g_hsv.x, g_hsv.y, g_hsv.z); g_hsv_init = true; }

            const float pad = 16.f * k, sv = 220.f * k, hue_w = 28.f * k, gap = 14.f * k;
            const float pw = pad + sv + gap + hue_w + pad;
            const float ph = pad + sv + pad;
            const ImVec2 o(amax.x - pw - 8.f * k,
                           ImClamp(anchor_y - ph - 8.f * k, amin.y + 8.f * k, ImMax(amin.y + 8.f * k, amax.y - ph - 8.f * k)));
            const ImVec2 omax(o.x + pw, o.y + ph);

            dl->AddRectFilled(o, omax, col(c_sidebar, a), 18.f * k);
            dl->AddRect(o, omax, col(c_border, a), 18.f * k, 0, 1.5f * k);

            const ImVec2 sv_min(o.x + pad, o.y + pad), sv_max(sv_min.x + sv, sv_min.y + sv);
            const ImVec2 hue_min(sv_max.x + gap, sv_min.y), hue_max(hue_min.x + hue_w, sv_max.y);

            ImVec4 hc;
            ImGui::ColorConvertHSVtoRGB(g_hsv.x, 1.f, 1.f, hc.x, hc.y, hc.z);
            const ImU32 hu = IM_COL32(int(hc.x * 255), int(hc.y * 255), int(hc.z * 255), int(255 * a));
            dl->AddRectFilledMultiColor(sv_min, sv_max, IM_COL32(255, 255, 255, int(255 * a)), hu, hu, IM_COL32(255, 255, 255, int(255 * a)));
            dl->AddRectFilledMultiColor(sv_min, sv_max, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, int(255 * a)), IM_COL32(0, 0, 0, int(255 * a)));

            const float seg = (hue_max.y - hue_min.y) / 6.f;
            for (int i = 0; i < 6; ++i) {
                ImVec4 c1, c2;
                ImGui::ColorConvertHSVtoRGB(i / 6.f, 1.f, 1.f, c1.x, c1.y, c1.z);
                ImGui::ColorConvertHSVtoRGB((i + 1) / 6.f, 1.f, 1.f, c2.x, c2.y, c2.z);
                const ImU32 u1 = IM_COL32(int(c1.x * 255), int(c1.y * 255), int(c1.z * 255), int(255 * a));
                const ImU32 u2 = IM_COL32(int(c2.x * 255), int(c2.y * 255), int(c2.z * 255), int(255 * a));
                dl->AddRectFilledMultiColor(ImVec2(hue_min.x, hue_min.y + seg * i), ImVec2(hue_max.x, hue_min.y + seg * (i + 1)), u1, u1, u2, u2);
            }

            ImGuiIO& io = ImGui::GetIO();
            const ImVec2 mp = io.MousePos;
            if (io.MouseDown[0]) {
                if (g_picker == 0 && ImGui::IsMouseClicked(0)) {
                    if (inside(mp, sv_min, sv_max)) g_picker = 1;
                    else if (inside(mp, hue_min, hue_max)) g_picker = 2;
                    else if (!inside(mp, o, omax)) g_color_open = false; // tap outside closes
                }
                if (g_picker == 1) {
                    g_hsv.y = ImClamp((mp.x - sv_min.x) / (sv_max.x - sv_min.x), 0.f, 1.f);
                    g_hsv.z = 1.f - ImClamp((mp.y - sv_min.y) / (sv_max.y - sv_min.y), 0.f, 1.f);
                } else if (g_picker == 2) {
                    g_hsv.x = ImClamp((mp.y - hue_min.y) / (hue_max.y - hue_min.y), 0.f, 1.f);
                }
                if (g_picker) ImGui::ColorConvertHSVtoRGB(g_hsv.x, g_hsv.y, g_hsv.z, v->x, v->y, v->z);
            } else {
                g_picker = 0;
            }

            const float cx = sv_min.x + g_hsv.y * sv, cy = sv_min.y + (1.f - g_hsv.z) * sv;
            dl->AddCircle(ImVec2(cx, cy), 7.f * k, IM_COL32(0, 0, 0, int(200 * a)), 16, 3.f * k);
            dl->AddCircle(ImVec2(cx, cy), 7.f * k, IM_COL32(255, 255, 255, int(255 * a)), 16, 1.5f * k);
            const float hy = hue_min.y + g_hsv.x * (hue_max.y - hue_min.y);
            dl->AddRectFilled(ImVec2(hue_min.x - 3.f * k, hy - 2.f * k), ImVec2(hue_max.x + 3.f * k, hy + 2.f * k), IM_COL32(255, 255, 255, int(255 * a)), 2.f * k);
        }

        void visuals_tab(ImDrawList* dl, const ImVec2& amin, const ImVec2& amax, float a, float k, bool live) {
            const float pad = k_pad * k, row = k_row * k;
            const float cw = amax.x - amin.x - pad * 2.f;
            ImVec2 c(amin.x + pad, amin.y + pad * 0.5f);
            struct item { const char* name; bool* value; };
            const item items[] = {
                {"Box", &cfg::esp::box},
                {"Name", &cfg::esp::name},
                {"Health", &cfg::esp::health},
                {"Weapon", &cfg::esp::weapon},
                {"Smooth", &cfg::esp::smooth},
            };
            for (const item& it : items) {
                toggle_row(dl, it.name, it.value, c, cw, row, a, k, live);
                c.y += row;
            }
            text_at(dl, c, 48.f * k, c_text, a, "Box Style");
            c.y += 48.f * k;
            static const char* const styles[] = {"Full", "Corner", "Clean"};
            segmented(dl, styles, 3, &cfg::esp::box_type, c, cw, 58.f * k, a, k, live);
            c.y += 58.f * k + 12.f * k;
            const float color_y = c.y;
            color_row(dl, "Color", &cfg::esp::box_col, c, cw, row, a, k, live);
            color_popup(dl, &cfg::esp::box_col, amin, amax, color_y, a, k);
        }

    }

    void render() {
        bar::render();

        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime;
        if (dt <= 0.f || dt > 0.1f) dt = 0.016f;
        g_ma = lrp(g_ma, bar::g_open ? 1.f : 0.f, ImClamp(12.f * dt, 0.f, 1.f));
        const float k = style::k();
        const float w = ImMin(k_panel_w * k, g_sw - 16.f * k);
        const float ph = ImMin(k_panel_h * k, g_sh - 16.f * k);

        // Every open starts centred on the detected screen centre.
        if (bar::g_open && !g_was_open) {
            const ImVec2 c = bar::g_center_valid ? bar::g_center : ImVec2(g_sw * 0.5f, g_sh * 0.5f);
            g_pos = ImVec2(c.x - w * 0.5f, c.y - ph * 0.5f);
            g_color_open = false;
            g_panel_pressed = g_panel_dragging = false;
        }
        g_was_open = bar::g_open;
        if (g_ma < 0.01f) { g_color_open = false; g_picker = 0; bar::g_panel_visible = false; return; }

        tick();
        const float a = g_ma * bar::game_alpha();
        const bool live = g_ma > 0.6f && bar::g_open;
        cfg::esp::health_col = cfg::esp::box_col;   // one colour for box and health bar

        g_pos = clamp_panel(g_pos, w, ph, k);
        ImVec2 p = g_pos;

        // ---- panel drag (press anywhere on the panel, move > threshold) ----
        {
            const ImVec2 mp = io.MousePos;
            const float thr = 10.f * k;
            if (!g_panel_pressed && live && ImGui::IsMouseClicked(0) && inside(mp, p, ImVec2(p.x + w, p.y + ph))) {
                g_panel_pressed = true;
                g_panel_dragging = false;
                g_panel_press = mp;
                g_panel_grab = ImVec2(mp.x - p.x, mp.y - p.y);
            }
            if (g_panel_pressed) {
                if (io.MouseDown[0]) {
                    const float dx = mp.x - g_panel_press.x, dy = mp.y - g_panel_press.y;
                    if (!g_panel_dragging && g_picker == 0 && dx * dx + dy * dy > thr * thr) g_panel_dragging = true;
                    if (g_panel_dragging) {
                        g_pos = clamp_panel(ImVec2(mp.x - g_panel_grab.x, mp.y - g_panel_grab.y), w, ph, k);
                        p = g_pos;
                    }
                } else {
                    g_panel_pressed = false;   // g_panel_dragging stays set this frame so the release is not a tap
                }
            } else {
                g_panel_dragging = false;
            }
        }

        // Open/close animation: slide up a little while fading.
        p.y += (1.f - g_ma) * 24.f * k;
        const ImVec2 pmax(p.x + w, p.y + ph);
        bar::g_panel_visible = true;
        bar::g_panel_min = p;
        bar::g_panel_max = pmax;

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const float r = 30.f * k;
        const float sbw = k_sidebar_w * k;
        dl->AddRectFilled(ImVec2(p.x, p.y + 4.f * k), ImVec2(pmax.x, pmax.y + 6.f * k), IM_COL32(0, 0, 0, int(70 * a)), r);
        dl->AddRectFilled(p, pmax, col(c_panel, a), r);
        dl->AddRectFilled(p, ImVec2(p.x + sbw, pmax.y), col(c_sidebar, a), r, ImDrawFlags_RoundCornersLeft);
        dl->PushClipRect(p, pmax, true);

        // ---- sidebar: logo (close) + tabs ----
        const ImVec2 logo_min(p.x, p.y), logo_max(p.x + sbw, p.y + k_logo_h * k);
        const ImVec2 logo_c((logo_min.x + logo_max.x) * 0.5f, (logo_min.y + logo_max.y) * 0.5f);
        const float press_t = anim("logo_press", held(logo_min, logo_max) ? 1.f : 0.f, 18.f);
        if (press_t > 0.01f) dl->AddCircleFilled(logo_c, 56.f * k, col(c_tab_sel, a * press_t), 40);
        const float logo_scale = 1.f - 0.06f * press_t;
        if (g_logo_tex) {
            // Texture cropped from the reference screenshot (butterfly.h), aspect kept.
            const float lh = 100.f * k * logo_scale;
            const float lw = lh * float(butterfly_w) / float(butterfly_h);
            dl->AddImage(g_logo_tex, ImVec2(logo_c.x - lw * 0.5f, logo_c.y - lh * 0.5f),
                         ImVec2(logo_c.x + lw * 0.5f, logo_c.y + lh * 0.5f), ImVec2(0, 0), ImVec2(1, 1), col(c_white, a));
        } else {
            butterfly_vector(dl, logo_c, 104.f * k * logo_scale, a, k);
        }
        if (live && tapped(logo_min, logo_max)) {
            bar::close();
            g_color_open = false;
        }

        // "Visuals" is the only page; "Unload" is an action: one tap unloads.
        const char* tabs[] = {"Visuals", "Unload"};
        for (int i = 0; i < 2; ++i) {
            const ImVec2 tmin(p.x, logo_max.y + k_tab_h * k * float(i)), tmax(p.x + sbw, tmin.y + k_tab_h * k);
            if (i == 1 && live && tapped(tmin, tmax)) status::g_quit_request = true;
            const float sel = i == 0 ? 1.f : (held(tmin, tmax) ? 1.f : 0.f);
            const float t = anim(std::string("tab_") + tabs[i], sel, 16.f);
            if (t > 0.01f) dl->AddRectFilled(tmin, tmax, col(c_tab_sel, a * t));
            ImFont* f = menu_font();
            const ImVec2 ts = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.f, tabs[i]);
            dl->AddText(f, f->FontSize, ImVec2(tmin.x + (sbw - ts.x) * 0.5f, tmin.y + (k_tab_h * k - ts.y) * 0.5f),
                        col(lrp_col(c_text_dim, c_text, t), a), tabs[i]);
        }

        // ---- content ----
        const ImVec2 amin(p.x + sbw, p.y), amax(pmax.x, pmax.y);
        dl->PushClipRect(amin, amax, true);
        visuals_tab(dl, amin, amax, a, k, live);
        dl->PopClipRect();

        dl->PopClipRect();
        dl->AddRect(p, pmax, col(c_border, a), r, 0, 1.5f * k);
        if (!g_panel_pressed) g_panel_dragging = false;
    }
}
