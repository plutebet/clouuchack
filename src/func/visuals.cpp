
#include "visuals.hpp"
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../game/camera.hpp"
#include "../game/weapon.hpp"
#include "esp/weapon_icons.h"
#include "../ui/theme/theme.hpp"
#include "../ui/cfg.hpp"
#include "../other/crash.hpp"
#include "../protect/oxorany.hpp"
#include "imgui.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <cfloat>
#include <cstdio>

extern ImFont* espFont;
extern ImTextureID g_weapon_atlas_tex;

static void esp_wait(const char* stage) {
    static long long last = 0;
    if (diag::every(last, 5000)) diag::log(diag::lvl("WAIT"), "ESP blocked at: %s", stage);
}

namespace {
    bool name_writable(const std::string& s) {
        if (!espFont || s.empty()) return false;
        size_t i = 0;
        while (i < s.size()) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            uint32_t cp; int len;
            if (c < 0x80) { cp = c; len = 1; }
            else if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; len = 2; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; len = 3; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; len = 4; }
            else return false;
            if (i + size_t(len) > s.size()) return false;
            for (int k = 1; k < len; ++k) {
                if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
                cp = (cp << 6) | uint32_t(static_cast<unsigned char>(s[i + k]) & 0x3F);
            }
            i += size_t(len);
            if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0)) return false;
            if (cp > 0xFFFF) return false;
            if (!espFont->FindGlyphNoFallback(ImWchar(cp))) return false;
        }
        return true;
    }
}

namespace {
    struct name_cache_entry { uint32_t id{}; std::string name; long long refreshed{}; };
    std::unordered_map<uint64_t, name_cache_entry> g_name_cache;
    std::unordered_map<uint64_t, long long> g_dead_since;
    std::unordered_map<uint64_t, int> g_last_hp;
    size_t g_name_cursor = 0;
    long long g_last_purge = 0;
    long long g_last_name_us = 0;

    template<typename Map>
    void purge_missing(Map& map, const std::unordered_set<uint64_t>& live) {
        for (auto it = map.begin(); it != map.end();)
            if (live.count(it->first)) ++it; else it = map.erase(it);
    }
}

ImU32 health_color(float pct, float a) {
    pct = std::clamp(pct, 0.f, 1.f);
    const ImVec4 red(0.93f, 0.25f, 0.25f, 1.f), yellow(1.f, 0.82f, 0.25f, 1.f), green(0.35f, 0.88f, 0.42f, 1.f);
    const float t = pct < 0.5f ? pct * 2.f : (pct - 0.5f) * 2.f;
    const ImVec4& lo = pct < 0.5f ? red : yellow;
    const ImVec4& hi = pct < 0.5f ? yellow : green;
    return IM_COL32(int((lo.x + (hi.x - lo.x) * t) * 255.f), int((lo.y + (hi.y - lo.y) * t) * 255.f),
                    int((lo.z + (hi.z - lo.z) * t) * 255.f), int(std::clamp(a, 0.f, 1.f) * 255.f));
}

void visuals::draw() {
    if (!(g_sw >= 2.f) || !(g_sh >= 2.f)) return;
    if (!cfg::esp::visuals) { esp_wait("ESP master switch off"); return; }
    if (!(cfg::esp::box || cfg::esp::infobar || cfg::esp::weapon)) { esp_wait("all ESP options disabled"); return; }

    crash::stage("esp-players");
    const long long now_ms = diag::now_ms();
    const long long now_us = diag::now_us();

    static std::vector<player_snapshot> s_players;
    static long long s_players_ms = 0;
    static int s_players_pid = -1;
    if (now_ms - s_players_ms >= 100 || s_players_pid != proc::pid) {
        s_players = players();
        s_players_ms = now_ms;
        s_players_pid = proc::pid;
    }
    const std::vector<player_snapshot>& Players = s_players;
    if (Players.empty()) { esp_wait("player discovery"); return; }

    if (now_ms - g_last_purge > 1000) {
        g_last_purge = now_ms;
        std::unordered_set<uint64_t> live;
        for (const player_snapshot& p : Players) live.insert(p.object);
        purge_missing(g_name_cache, live);
        purge_missing(g_dead_since, live);
        purge_missing(g_last_hp, live);
        purge_missing(player::g_transform_cache, live);
        purge_missing(weapon::g_players, live);
    }

    // Direct-path name resolve: one player per 100ms, no scanning
    if (cfg::esp::infobar && now_us - g_last_name_us > 100000) {
        g_last_name_us = now_us;
        const size_t n = Players.size();
        const player_snapshot* pick = nullptr;
        for (size_t k = 0; k < n && !pick; ++k) {
            const player_snapshot& p = Players[(g_name_cursor + k) % n];
            const auto it = g_name_cache.find(p.object);
            if (it == g_name_cache.end() || it->second.id != p.id || it->second.name.empty()) {
                pick = &p;
                g_name_cursor += k + 1;
            }
        }
        if (!pick) pick = &Players[g_name_cursor++ % n];
        name_cache_entry& entry = g_name_cache[pick->object];
        if (entry.id != pick->id || entry.name.empty() || now_ms - entry.refreshed > 5000) {
            std::string name = player_name(pick->object, pick->id);
            if (!name.empty() || entry.id != pick->id) entry.name = std::move(name);
            entry.id = pick->id;
            entry.refreshed = now_ms;
        }
    }

    const player_snapshot* Local = nullptr;
    for (const player_snapshot& p : Players) if (p.is_local()) { Local = &p; break; }
    if (!Local) { esp_wait("local player flag"); return; }
    const uint64_t LocalPlayer = Local->object;

    {
        static uint64_t last_local = 0;
        if (LocalPlayer != last_local) {
            if (last_local) {
                scanner::notify_new_match();
                cam::on_new_match();
                g_dead_since.clear();
                reset_name_fields();
            }
            last_local = LocalPlayer;
        }
    }
    const int LocalTeam = Local->team;
    if (LocalTeam != 1 && LocalTeam != 2) { esp_wait("local team validation"); return; }

    crash::stage("esp-camera");
    cam::main_chain chain{};
    std::vector<camera_candidate> Cameras = cam::resolve(LocalPlayer, chain);
    if (Cameras.empty()) { g_camera = 0; esp_wait("camera resolution"); return; }

    crash::stage("esp-poses");
    static std::vector<player::frame_player> s_frame;
    static player::frame_batch s_batch;
    s_frame.clear();
    s_frame.push_back({Local, false});
    for (const player_snapshot& p : Players) {
        if (p.object == LocalPlayer) continue;
        if (p.team == LocalTeam || (p.team != 1 && p.team != 2)) continue;
        s_frame.push_back({&p, true});
    }
    player::read_frame(s_frame, s_batch);

    const player::PlayerPose LocalPose = s_frame[0].pose;
    Vector3 LocalPosition = LocalPose.feet;
    if (s_frame[0].stale || !LocalPose.valid) { esp_wait("local transform"); return; }
    if (LocalPosition.x == 0.f && LocalPosition.y == 0.f && LocalPosition.z == 0.f) {
        esp_wait("local position zero"); return;
    }

    // Camera selection: main native first, otherwise best score
    matrix ViewMatrix{}, ProjectionMatrix{};
    uint64_t selected_camera = 0;
    int selected_kind = camera_kind_unknown;
    if (Cameras.front().kind == camera_kind_main_native) {
        selected_camera = Cameras.front().address;
        selected_kind = Cameras.front().kind;
        ViewMatrix = Cameras.front().view;
        ProjectionMatrix = Cameras.front().projection;
    } else {
        int best_score = -1;
        for (const auto& cam : Cameras) {
            int score = 0;
            switch (cam.kind) {
                case camera_kind_main_native: score = 100; break;
                case camera_kind_kmc_native: score = 80; break;
                case camera_kind_main_state: score = 60; break;
                case camera_kind_other_native: score = 50; break;
                case camera_kind_vqn_state: score = 30; break;
                case camera_kind_heap_camera: score = 20; break;
                case camera_kind_ios_transform: score = 10; break;
                default: break;
            }
            if (cam.address == g_camera) score += 40;
            if (score > best_score) {
                best_score = score;
                selected_camera = cam.address;
                selected_kind = cam.kind;
                ViewMatrix = cam.view;
                ProjectionMatrix = cam.projection;
            }
        }
    }
    if (!selected_camera) { g_camera = 0; esp_wait("no camera"); return; }
    g_camera = selected_camera;

    crash::stage("esp-draw");

    struct draw_enemy {
        uint64_t object;
        const player_snapshot* snap;
        Vector3 position;
        Vector3 head;
        int hp, max_hp, weapon_id;
        float distance;
    };
    std::vector<draw_enemy> enemies;
    enemies.reserve(s_frame.size());

    for (size_t fi = 1; fi < s_frame.size(); ++fi) {
        const player::frame_player& fp = s_frame[fi];
        if (fp.stale || fp.live_team != fp.snap->team) continue;

        const player::HealthInfo Health = fp.health;
        if (!Health.valid || Health.hp <= 0) {
            g_dead_since[fp.snap->object] = now_ms;
            continue;
        }
        const auto dead = g_dead_since.find(fp.snap->object);
        if (dead != g_dead_since.end()) {
            if (now_ms - dead->second < 1500) continue;
            g_dead_since.erase(dead);
        }
        if (!fp.pose.valid) continue;

        draw_enemy e{};
        e.object = fp.snap->object;
        e.snap = fp.snap;
        e.position = fp.pose.feet;
        e.head = fp.pose.has_head ? fp.pose.head : fp.pose.feet + Vector3(0.f, 1.67f, 0.f);
        e.hp = Health.hp;
        e.max_hp = Health.max_hp;
        e.weapon_id = cfg::esp::weapon ? weapon::active_id(*fp.snap) : -1;
        e.distance = calculate_distance(e.position, LocalPosition);
        if (e.distance > 500.f) continue;
        enemies.push_back(e);
    }

    // Project and build draw items
    struct draw_item { ImVec2 mn, mx; float cx; const draw_enemy* e; };
    std::vector<draw_item> items;
    items.reserve(enemies.size());

    for (const auto& e : enemies) {
        ImVec2 top, foot;
        if (!world_to_screen(e.head + Vector3(0.f, 0.15f, 0.f), ViewMatrix, ProjectionMatrix, top) ||
            !world_to_screen(e.position, ViewMatrix, ProjectionMatrix, foot)) continue;

        float y1 = std::min(top.y, foot.y), y2 = std::max(top.y, foot.y);
        if (y2 - y1 < 1.f || y2 - y1 > g_sh * 3.f) continue;
        if (y2 - y1 < 10.f) { float mid = (y1 + y2) * .5f; y1 = mid - 5.f; y2 = mid + 5.f; }

        const float half_w = std::max(3.f, (y2 - y1) * 0.25f);
        const float cx = roundf((top.x + foot.x) * 0.5f);
        const ImVec2 mn(roundf(cx - half_w), roundf(y1));
        const ImVec2 mx(roundf(cx + half_w), roundf(y2));
        if (mx.x < -64.f || mn.x > g_sw + 64.f || mx.y < -64.f || mn.y > g_sh + 64.f) continue;
        items.push_back({mn, mx, cx, &e});
    }

    // Far first, near last
    std::sort(items.begin(), items.end(),
              [](const draw_item& a, const draw_item& b) { return a.e->distance > b.e->distance; });

    const ImVec2 line_origin(g_sw * 0.5f, g_sh * 0.045f);

    for (const draw_item& it : items) {
        const draw_enemy& e = *it.e;
        const float box_h = it.mx.y - it.mn.y;
        const float fade = 1.f - 0.4f * std::clamp((e.distance - 40.f) / 110.f, 0.f, 1.f);
        const float text_sz = std::clamp(box_h * 0.11f, 11.f, 16.f);

        if (cfg::esp::line)
            dline(line_origin, ImVec2(it.cx, it.mn.y), 0.85f * fade);

        if (cfg::esp::box)
            dbox(it.mn, it.mx, fade);

        if (cfg::esp::infobar) {
            const auto found = g_name_cache.find(e.object);
            const std::string shown = found != g_name_cache.end() && !found->second.name.empty()
                ? (name_writable(found->second.name) ? found->second.name : std::string("empty"))
                : std::string();
            const char* label = shown.empty() ? "Enemy" : shown.c_str();
            dnick(label, e.hp, e.max_hp, it.mn, it.cx, text_sz, fade);
        }

        if (cfg::esp::weapon)
            dweapon(e.weapon_id, it.cx, it.mx.y + 3.f, it.mx.x - it.mn.x, text_sz, fade);
    }

    diag::g_warm = true;
    static long long frame_log = 0;
    if (diag::every(frame_log, 5000))
        diag::log("INFO", "ESP frame players=%zu enemies=%zu projected=%zu camera=%s",
                  Players.size(), enemies.size(), items.size(), camera_kind_name(selected_kind));
}

namespace {
    ImU32 rgba(const ImVec4& c, float a) {
        return IM_COL32(int(c.x * 255.f), int(c.y * 255.f), int(c.z * 255.f), int(std::clamp(c.w * a, 0.f, 1.f) * 255.f));
    }
    ImU32 black(float a) { return IM_COL32(0, 0, 0, int(std::clamp(a, 0.f, 1.f) * 255.f)); }
}

void visuals::dbox(const ImVec2& t, const ImVec2& b, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float th = std::max(1.5f, roundf(2.f * g_sh / 1224.f));
    dl->AddRect(t, b, rgba(cfg::esp::box_col, 0.82f * a), th * 1.5f, 0, th);
}

void visuals::dline(const ImVec2& from, const ImVec2& to, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float th = std::max(1.f, roundf(1.5f * g_sh / 1224.f));
    dl->AddLine(from, to, IM_COL32(255, 255, 255, int(std::clamp(a, 0.f, 1.f) * 255.f)), th);
}

void visuals::dnick(const char* name, int hp, int max_hp, const ImVec2& box_min, float cx, float sz, float a) {
    if (!espFont || a < 0.01f || !name) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, name);
    const float ph = std::max(sz + 9.f, 15.f);
    const float pad_x = ph * 0.55f;
    const float pw = ts.x + pad_x * 2.f;
    const float x0 = roundf(cx - pw * 0.5f);
    const float y1 = roundf(box_min.y - 3.f);
    const float y0 = y1 - ph;

    if (max_hp <= 0) max_hp = 100;
    const float pct = std::clamp(float(hp) / float(max_hp), 0.f, 1.f);
    const float r = ph * 0.5f;

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + pw, y1), black(0.78f * a), r);
    const float line_th = std::max(2.f, roundf(2.f * g_sh / 1224.f));
    const float line_x0 = x0 + 3.f, line_w = (pw - 6.f) * pct;
    if (line_w >= 1.f)
        dl->AddRectFilled(ImVec2(line_x0, y1 - line_th), ImVec2(line_x0 + line_w, y1), health_color(pct, a));
    const ImVec2 tp(roundf(cx - ts.x * 0.5f), roundf(y0 + (ph - ts.y) * 0.5f));
    dl->AddText(espFont, sz, tp, IM_COL32(255, 255, 255, int(255 * a)), name);
}

void visuals::dweapon(int weapon_id, float cx, float y, float box_w, float sz, float a) {
    const char* label = weapon::name(weapon_id);
    if (!label || a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!espFont) return;
    const float fs = std::clamp(sz * 1.05f, 11.f, 17.f);
    const ImVec2 ts = espFont->CalcTextSizeA(fs, FLT_MAX, 0.f, label);
    draw_text_outlined(dl, espFont, fs, ImVec2(roundf(cx - ts.x * 0.5f), roundf(y)), IM_COL32(255, 255, 255, int(255 * a)), label);
}

void visuals::draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text) {
    if (!font || !dl || !text) return;
    const int a = int((color >> IM_COL32_A_SHIFT) & 0xFF);
    const ImU32 outline = IM_COL32(0, 0, 0, a * 220 / 255);
    for (const ImVec2 d : {ImVec2(-1.f, 0.f), ImVec2(1.f, 0.f), ImVec2(0.f, -1.f), ImVec2(0.f, 1.f)})
        dl->AddText(font, size, ImVec2(pos.x + d.x, pos.y + d.y), outline, text);
    dl->AddText(font, size, pos, color, text);
}
