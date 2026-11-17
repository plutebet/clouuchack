#include "rise_visuals.hpp"
#include "rise_game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../ui/theme/theme.hpp"
#include "../ui/cfg.hpp"
#include "../other/crash.hpp"
#include "../other/diag.hpp"
#include "../protect/oxorany.hpp"
#include "imgui.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cfloat>
#include <cstdio>

extern ImFont* espFont;

namespace {

    std::unordered_map<uint64_t, long long> g_dead_since;
    long long g_last_purge = 0;

    template<typename Map>
    void purge_missing(Map& map, const std::unordered_set<uint64_t>& live) {
        for (auto it = map.begin(); it != map.end();)
            if (live.count(it->first)) ++it; else it = map.erase(it);
    }

    // ---- motion interpolation (identical to SO2) ----
    struct motion_sample { Vector3 feet{}; Vector3 head{}; long long t_us{0}; };
    struct motion_state {
        motion_sample prev{}, cur{};
        bool has_prev{false}, has_cur{false};
        double interval_us{8333.0};
    };
    std::unordered_map<uint64_t, motion_state> g_motion;

    float vdist(const Vector3& a, const Vector3& b) {
        const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    void motion_update(motion_state& m, const Vector3& feet, const Vector3& head, long long t_us) {
        if (!m.has_cur) { m.cur = {feet, head, t_us}; m.has_cur = true; return; }
        if (vdist(feet, m.cur.feet) < 0.0005f && vdist(head, m.cur.head) < 0.0005f) return;
        const double dt = double(t_us - m.cur.t_us);
        m.prev = m.cur; m.has_prev = true;
        m.cur = {feet, head, t_us};
        if (dt > 2000.0 && dt < 60000.0) m.interval_us = m.interval_us * 0.8 + dt * 0.2;
    }

    void motion_render(const motion_state& m, long long now_us, Vector3& feet, Vector3& head) {
        feet = m.cur.feet; head = m.cur.head;
        if (!m.has_prev) return;
        const double span = double(m.cur.t_us - m.prev.t_us);
        if (span <= 0.0 || span > 150000.0) return;
        if (vdist(m.prev.feet, m.cur.feet) > 4.f) return;
        const double delay = std::clamp(m.interval_us, 4000.0, 34000.0);
        double alpha = (double(now_us - m.prev.t_us) - delay) / span;
        alpha = std::clamp(alpha, 0.0, 1.0);
        const float a = float(alpha);
        feet = m.prev.feet + (m.cur.feet - m.prev.feet) * a;
        head = m.prev.head + (m.cur.head - m.prev.head) * a;
    }

    // ---- color helpers ----
    ImU32 rgba(const ImVec4& c, float a) {
        return IM_COL32(int(c.x * 255.f), int(c.y * 255.f), int(c.z * 255.f), int(std::clamp(c.w * a, 0.f, 1.f) * 255.f));
    }
    ImU32 black(float a) { return IM_COL32(0, 0, 0, int(std::clamp(a, 0.f, 1.f) * 255.f)); }

    ImU32 health_color(float pct, float a) {
        pct = std::clamp(pct, 0.f, 1.f);
        const ImVec4 red(0.93f, 0.25f, 0.25f, 1.f), yellow(1.f, 0.82f, 0.25f, 1.f), green(0.35f, 0.88f, 0.42f, 1.f);
        const float t = pct < 0.5f ? pct * 2.f : (pct - 0.5f) * 2.f;
        const ImVec4& lo = pct < 0.5f ? red : yellow;
        const ImVec4& hi = pct < 0.5f ? yellow : green;
        return rgba(ImVec4(lo.x + (hi.x - lo.x) * t, lo.y + (hi.y - lo.y) * t, lo.z + (hi.z - lo.z) * t, 1.f), a);
    }
}

static void draw_dbg(const char* text, int line, ImU32 col = IM_COL32(0,255,0,220)) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    const float fs = 13.f;
    const ImVec2 pos(10.f, 60.f + line * 16.f);
    dl->AddText(nullptr, fs, ImVec2(pos.x+1,pos.y+1), IM_COL32(0,0,0,180), text);
    dl->AddText(nullptr, fs, pos, col, text);
}

void rise_visuals::draw() {
    if (!(g_sw >= 2.f) || !(g_sh >= 2.f)) return;

    static long long hb = 0;
    if (diag::every(hb, 5000))
        diag::log("RISE-DBG", "draw() heartbeat sw=%.0f sh=%.0f", g_sw, g_sh);

    const bool any = cfg::esp::box || cfg::esp::name || cfg::esp::health;

    int dbg_line = 0;
    char dbg[128];

    std::snprintf(dbg, sizeof(dbg), "il2cpp=0x%llx pid=%d",
        (unsigned long long)rise_game::g_il2cpp_base, rise_game::g_pid);
    draw_dbg(dbg, dbg_line++);

    if (!any) {
        draw_dbg("ESP: all toggles OFF", dbg_line++, IM_COL32(255,255,0,220));
        static long long toff = 0;
        if (diag::every(toff, 3000)) diag::log("RISE-DBG", "ESP toggles off box=%d name=%d hp=%d",
            int(cfg::esp::box), int(cfg::esp::name), int(cfg::esp::health));
        return;
    }

    crash::stage("rise-render-p");
    const long long now_ms = diag::now_ms();
    const long long now_us = diag::now_us();

    static std::vector<rise_game::rise_player> s_players;
    static long long s_players_ms = 0;
    if (now_ms - s_players_ms >= 100) {
        s_players = rise_game::players();
        s_players_ms = now_ms;
    }

    // TypeInfo diagnostic
    {
        const uint64_t pm_klass = rise_game::typeinfo_rise(rise::PlayerManager_TypeInfo);
        std::snprintf(dbg, sizeof(dbg), "PM klass=0x%llx pm=0x%llx players=%zu",
            (unsigned long long)pm_klass,
            (unsigned long long)rise_game::g_pm_singleton,
            s_players.size());
        draw_dbg(dbg, dbg_line++);
    }

    if (s_players.empty()) {
        draw_dbg("ESP: no players from PM", dbg_line++, IM_COL32(255,80,80,220));
        static long long tpm = 0;
        if (diag::every(tpm, 3000)) {
            const uint64_t pk = rise_game::typeinfo_rise(rise::PlayerManager_TypeInfo);
            diag::log("RISE-DBG", "no players: PM_klass=0x%llx pm_singleton=0x%llx il2cpp=0x%llx",
                (unsigned long long)pk, (unsigned long long)rise_game::g_pm_singleton,
                (unsigned long long)rise_game::g_il2cpp_base);
        }
        return;
    }

    if (now_ms - g_last_purge > 1000) {
        g_last_purge = now_ms;
        std::unordered_set<uint64_t> live;
        for (const auto& p : s_players) live.insert(p.controller);
        purge_missing(g_dead_since, live);
        purge_missing(g_motion, live);
        purge_missing(player::g_transform_cache, live);
    }

    const rise_game::rise_player* Local = nullptr;
    for (const auto& p : s_players) if (p.is_local) { Local = &p; break; }
    if (!Local) {
        draw_dbg("ESP: no local player", dbg_line++, IM_COL32(255,80,80,220));
        static long long tlp = 0;
        if (diag::every(tlp, 3000)) diag::log("RISE-DBG", "no local player in %zu entries", s_players.size());
        return;
    }
    const uint8_t LocalTeam = Local->team;
    std::snprintf(dbg, sizeof(dbg), "local team=%d hp=%d alive=%d",
        int(LocalTeam), Local->health, int(Local->alive));
    draw_dbg(dbg, dbg_line++);
    if (LocalTeam != 1 && LocalTeam != 2) {
        draw_dbg("ESP: bad local team", dbg_line++, IM_COL32(255,80,80,220));
        return;
    }

    crash::stage("rise-render-cam");
    matrix ViewMatrix{}, ProjectionMatrix{};
    const bool cam_ok = rise_game::resolve_camera(ViewMatrix, ProjectionMatrix);
    std::snprintf(dbg, sizeof(dbg), "camera=%s vm11=%.2f pm22=%.2f",
        cam_ok ? "OK" : "FAIL", ViewMatrix.m11, ProjectionMatrix.m22);
    draw_dbg(dbg, dbg_line++, cam_ok ? IM_COL32(0,255,0,220) : IM_COL32(255,80,80,220));
    if (!cam_ok) {
        static long long tcam = 0;
        if (diag::every(tcam, 3000))
            diag::log("RISE-DBG", "camera FAIL native=0x%llx view_off=%d proj_off=%d",
                (unsigned long long)rise_game::g_camera_native,
                rise_game::g_camera_view_off, rise_game::g_camera_proj_off);
        return;
    }

    Vector3 LocalPosition{};
    const bool pos_ok = rise_game::player_position(Local->transform, LocalPosition);
    std::snprintf(dbg, sizeof(dbg), "localpos=%s (%.1f,%.1f,%.1f)",
        pos_ok ? "OK" : "FAIL", LocalPosition.x, LocalPosition.y, LocalPosition.z);
    draw_dbg(dbg, dbg_line++, pos_ok ? IM_COL32(0,255,0,220) : IM_COL32(255,80,80,220));
    if (!pos_ok) {
        static long long tpos = 0;
        if (diag::every(tpos, 3000))
            diag::log("RISE-DBG", "localpos FAIL transform=0x%llx",
                (unsigned long long)Local->transform);
        return;
    }
    if (!std::isfinite(LocalPosition.x) ||
        (LocalPosition.x == 0.f && LocalPosition.y == 0.f && LocalPosition.z == 0.f)) return;

    struct enemy_frame {
        uint64_t controller{};
        Vector3 position{};
        Vector3 head{};
        int hp{0};
        int armor{0};
        uint8_t team{0};
        float distance{0.f};
    };
    std::vector<enemy_frame> frame_enemies;
    frame_enemies.reserve(s_players.size());

    for (const auto& p : s_players) {
        if (p.is_local) continue;
        if (!cfg::esp::teammates && p.team == LocalTeam) continue;
        if (p.team != 1 && p.team != 2) continue;

        if (!p.alive || p.health <= 0) {
            g_dead_since[p.controller] = now_ms;
            continue;
        }
        const auto dead = g_dead_since.find(p.controller);
        if (dead != g_dead_since.end()) {
            if (now_ms - dead->second < 1500) continue;
            g_dead_since.erase(dead);
        }

        player::PlayerPose pose = rise_game::player_pose(p);
        if (!pose.valid) continue;

        enemy_frame e{};
        e.controller = p.controller;
        e.position = pose.feet;
        e.head = pose.head;
        e.hp = p.health;
        e.armor = p.armor;
        e.team = p.team;
        e.distance = calculate_distance(e.position, LocalPosition);
        if (e.distance > 500.f) continue;
        frame_enemies.push_back(e);
    }

    const long long sample_us = diag::now_us();
    for (auto& e : frame_enemies) {
        motion_state& m = g_motion[e.controller];
        motion_update(m, e.position, e.head, sample_us);
        if (cfg::esp::smooth) motion_render(m, sample_us, e.position, e.head);
    }

    // Matrix fallback: when no enemy projects, reuse last good matrices.
    {
        static matrix s_good_view{}, s_good_proj{};
        static bool s_has_good = false;
        static int s_fallback_streak = 0;
        if (!frame_enemies.empty()) {
            bool any_projects = false;
            for (const auto& e : frame_enemies) {
                ImVec2 tmp;
                if (world_to_screen(e.head, ViewMatrix, ProjectionMatrix, tmp)) { any_projects = true; break; }
            }
            if (any_projects) {
                s_good_view = ViewMatrix; s_good_proj = ProjectionMatrix;
                s_has_good = true; s_fallback_streak = 0;
            } else if (s_has_good && s_fallback_streak < 3) {
                ++s_fallback_streak;
                ViewMatrix = s_good_view; ProjectionMatrix = s_good_proj;
            }
        }
    }

    crash::stage("rise-render-draw");
    struct draw_item { ImVec2 min, max; float cx; const enemy_frame* e; };
    std::vector<draw_item> items;
    items.reserve(frame_enemies.size());
    for (const auto& Enemy : frame_enemies) {
        ImVec2 top, foot;
        if (!world_to_screen(Enemy.head + Vector3(0.f, 0.15f, 0.f), ViewMatrix, ProjectionMatrix, top) ||
            !world_to_screen(Enemy.position, ViewMatrix, ProjectionMatrix, foot)) continue;
        float y1 = std::min(top.y, foot.y), y2 = std::max(top.y, foot.y);
        if (!(y2 - y1 >= 1.f) || y2 - y1 > g_sh * 3.f) continue;
        if (y2 - y1 < 10.f) { const float mid = (y1 + y2) * 0.5f; y1 = mid - 5.f; y2 = mid + 5.f; }
        const float half_w = std::max(3.f, (y2 - y1) * 0.25f);
        const float cx = roundf((top.x + foot.x) * 0.5f);
        const ImVec2 mn(roundf(cx - half_w), roundf(y1)), mx(roundf(cx + half_w), roundf(y2));
        if (mx.x < -64.f || mn.x > g_sw + 64.f || mx.y < -64.f || mn.y > g_sh + 64.f) continue;
        items.push_back({mn, mx, cx, &Enemy});
    }

    std::snprintf(dbg, sizeof(dbg), "enemies=%zu projected=%zu", frame_enemies.size(), items.size());
    draw_dbg(dbg, dbg_line++, items.empty() ? IM_COL32(255,255,0,220) : IM_COL32(0,255,0,220));

    std::sort(items.begin(), items.end(),
              [](const draw_item& l, const draw_item& r) { return l.e->distance > r.e->distance; });

    for (const draw_item& it : items) {
        const enemy_frame& Enemy = *it.e;
        const float box_h = it.max.y - it.min.y;
        const float fade = 1.f - 0.4f * std::clamp((Enemy.distance - 40.f) / 110.f, 0.f, 1.f);
        const float text_sz = std::clamp(box_h * 0.11f, 11.f, 16.f);
        const bool is_teammate = Enemy.team == LocalTeam;
        if (cfg::esp::box) dbox(it.min, it.max, fade);
        if (cfg::esp::health) dhp(Enemy.hp, 100, it.min, it.max, fade);
        if (cfg::esp::name) {
            const char* label = is_teammate ? "Team" : "Enemy";
            dnick(label, it.min, it.cx, text_sz, fade);
        }
    }

    static long long frame_log = 0;
    if (diag::every(frame_log, 5000))
        diag::log("RISE", "frame players=%zu enemies=%zu projected=%zu local_team=%d",
                  s_players.size(), frame_enemies.size(), items.size(), int(LocalTeam));
}

void rise_visuals::dbox(const ImVec2& t, const ImVec2& b, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float th = std::max(1.5f, roundf(2.f * g_sh / 1224.f));
    const ImU32 thin = rgba(cfg::esp::box_col, 0.82f * a);
    dl->AddRect(ImVec2(t.x, t.y), ImVec2(b.x, b.y), thin, th * 1.5f, 0, th);
}

void rise_visuals::dhp(int hp, int max_hp, const ImVec2& t, const ImVec2& b, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (max_hp <= 0) max_hp = 100;
    hp = std::clamp(hp, 0, max_hp);
    const float pct = float(hp) / float(max_hp);
    const float x1 = t.x - 4.f, x0 = x1 - 2.f;
    const float y0 = t.y, y1 = b.y + 1.f;
    const float fill_top = roundf(y1 - (y1 - y0) * pct);
    dl->AddRectFilled(ImVec2(x0 - 1.f, y0 - 1.f), ImVec2(x1 + 1.f, y1 + 1.f), black(0.75f * a));
    if (y1 - fill_top >= 1.f) dl->AddRectFilled(ImVec2(x0, fill_top), ImVec2(x1, y1), health_color(pct, a));
    if (hp < max_hp && espFont && y1 - y0 >= 28.f) {
        char txt[8];
        std::snprintf(txt, sizeof(txt), "%d", hp);
        const float fs = 11.f;
        const ImVec2 ts = espFont->CalcTextSizeA(fs, FLT_MAX, 0.f, txt);
        const float ty = std::clamp(roundf(fill_top - ts.y * 0.5f), y0 - ts.y * 0.5f, y1 - ts.y * 0.5f);
        draw_text_outlined(dl, espFont, fs, ImVec2(roundf(x0 - 3.f - ts.x), ty), IM_COL32(255, 255, 255, int(255 * a)), txt);
    }
}

void rise_visuals::dnick(const char* name, const ImVec2& box_min, float cx, float sz, float a) {
    if (!espFont || a < 0.01f || !name) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, name);
    const ImVec2 tp(roundf(cx - ts.x * 0.5f), roundf(box_min.y - ts.y - 3.f));
    draw_text_outlined(dl, espFont, sz, tp, rgba(cfg::esp::name_col, a), name);
}

void rise_visuals::draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text) {
    if (!font || !dl || !text) return;
    const int a = int((color >> IM_COL32_A_SHIFT) & 0xFF);
    const ImU32 outline = IM_COL32(0, 0, 0, a * 220 / 255);
    for (const ImVec2 d : {ImVec2(-1.f, 0.f), ImVec2(1.f, 0.f), ImVec2(0.f, -1.f), ImVec2(0.f, 1.f)})
        dl->AddText(font, size, ImVec2(pos.x + d.x, pos.y + d.y), outline, text);
    dl->AddText(font, size, pos, color, text);
}
