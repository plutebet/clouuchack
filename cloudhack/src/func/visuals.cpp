
// ================================================================================
// Сурс взят из бота @imguisource_bot
// Подпишись на ImGui Source: t.me/ImGui_Source_FIXNEY
// Подпишись на shit src 🧨: t.me/il2cpp_src
// ================================================================================

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
#include <fcntl.h>
#include <unistd.h>

extern ImFont* espFont;
extern ImTextureID g_weapon_atlas_tex;

static void esp_wait(const char* stage) {
    static long long last = 0;
    if (diag::every(last, 5000)) diag::log("WAIT", "ESP blocked at: %s", stage);
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
        for (auto it = map.begin(); it != map.end();) {
            if (live.count(it->first)) ++it;
            else it = map.erase(it);
        }
    }

    // ---- snapshot interpolation --------------------------------------
    // The game updates enemy transforms once per game frame; the overlay
    // samples them at the display rate. Rendering the latest sample gives
    // stepping whenever the two rates differ (90 fps game vs 120/144 Hz
    // overlay beats at 30 Hz). Keep the last two distinct samples and render
    // the position one update interval in the past, interpolated between
    // them: constant one-game-frame delay (4 cm at running speed), no steps.
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
        if (!m.has_cur) {
            m.cur = {feet, head, t_us};
            m.has_cur = true;
            return;
        }
        if (vdist(feet, m.cur.feet) < 0.0005f && vdist(head, m.cur.head) < 0.0005f) return; // game frame not advanced
        const double dt = double(t_us - m.cur.t_us);
        m.prev = m.cur;
        m.has_prev = true;
        m.cur = {feet, head, t_us};
        if (dt > 2000.0 && dt < 60000.0) m.interval_us = m.interval_us * 0.8 + dt * 0.2;
    }

    void motion_render(const motion_state& m, long long now_us, Vector3& feet, Vector3& head) {
        feet = m.cur.feet;
        head = m.cur.head;
        if (!m.has_prev) return;
        const double span = double(m.cur.t_us - m.prev.t_us);
        if (span <= 0.0 || span > 150000.0) return;            // stale pair: snap
        if (vdist(m.prev.feet, m.cur.feet) > 4.f) return;         // teleport / respawn: snap
        const double delay = std::clamp(m.interval_us, 4000.0, 34000.0);
        double alpha = (double(now_us - m.prev.t_us) - delay) / span;
        alpha = std::clamp(alpha, 0.0, 1.0);
        const float a = float(alpha);
        feet = m.prev.feet + (m.cur.feet - m.prev.feet) * a;
        head = m.prev.head + (m.cur.head - m.prev.head) * a;
    }
}

void visuals::draw() {
    if (!(g_sw >= 2.f) || !(g_sh >= 2.f)) return;
    bool any = cfg::esp::box || cfg::esp::name || cfg::esp::health || cfg::esp::weapon;
    if (!any) { esp_wait("all ESP options disabled"); return; }

    crash::stage("esp-players");
    const std::vector<player_snapshot> Players = players();
    if (Players.empty()) { esp_wait("player discovery / registry scan"); return; }
    const long long now_ms = diag::now_ms();
    const long long now_us = diag::now_us();

    // Per-object caches must not outlive the match: a new match reuses heap
    // addresses, and a stale "dead since" entry would hide a live player.
    if (now_ms - g_last_purge > 1000) {
        g_last_purge = now_ms;
        std::unordered_set<uint64_t> live;
        for (const player_snapshot& p : Players) live.insert(p.object);
        purge_missing(g_name_cache, live);
        purge_missing(g_dead_since, live);
        purge_missing(g_last_hp, live);
        purge_missing(g_motion, live);
        purge_missing(player::g_transform_cache, live);
        purge_missing(weapon::g_players, live);
    }

    // Resolve at most one name per 100 ms (dozens of reads each). Players
    // without a name come first; a failed re-read never replaces a name that
    // was already found for the same player.
    if (cfg::esp::name && now_us - g_last_name_us > 100000) {
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
    // A new local NetObject means a new match or round object: the scanner
    // drops its back-off and restarts, the camera lock is re-checked at once,
    // and per-enemy state of the old match is discarded.
    {
        static uint64_t last_local = 0;
        if (LocalPlayer != last_local) {
            if (last_local) {
                diag::log("INFO", "local player changed 0x%llx -> 0x%llx: new match",
                          (unsigned long long)last_local, (unsigned long long)LocalPlayer);
                scanner::notify_new_match();
                cam::on_new_match();
                g_dead_since.clear();
                g_motion.clear();
            }
            last_local = LocalPlayer;
        }
    }
    const int LocalTeam = Local->team;
    if (LocalTeam != 1 && LocalTeam != 2) { esp_wait("local team validation"); return; }
    const player::PlayerPose LocalPose = player::pose(*Local);
    const Vector3 LocalPosition = LocalPose.feet;
    if (!LocalPose.valid || !std::isfinite(LocalPosition.x) ||
        (LocalPosition.x == 0.f && LocalPosition.y == 0.f && LocalPosition.z == 0.f)) {
        esp_wait("local transform hierarchy"); return;
    }

    // Camera first (4 reads with a lock), then the enemy poses, then the
    // camera again: if it changed in between, the game rendered a frame
    // while we were reading and the poses are re-read so that box and
    // camera describe the same frame.
    crash::stage("esp-camera");
    cam::main_chain chain{};
    std::vector<camera_candidate> Cameras = cam::resolve(LocalPlayer, chain);
    if (Cameras.empty()) { g_camera = 0; esp_wait("camera resolution (controller / bor / transform)"); return; }

    crash::stage("esp-poses");
    int enemies = 0, alive = 0, positioned = 0, projected = 0;
    struct enemy_frame {
        uint64_t object{};
        const player_snapshot* snap{nullptr};
        Vector3 position{};
        Vector3 head{};
        bool has_head{false};
        int hp{0};
        int max_hp{100};
        int weapon{-1};
        float distance{0.f};
    };
    std::vector<enemy_frame> frame_enemies;
    auto read_enemy_pose = [&](enemy_frame& e) -> bool {
        const player::PlayerPose Shape = player::pose(*e.snap);
        if (!Shape.valid) return false;
        e.position = Shape.feet;
        e.head = Shape.has_head ? Shape.head : Shape.feet + Vector3(0.f, 1.67f, 0.f);
        e.has_head = Shape.has_head;
        return true;
    };
    for (const player_snapshot& p : Players) {
        if (p.object == LocalPlayer) continue;
        const int PlayerTeam = p.team;
        if (PlayerTeam == LocalTeam || (PlayerTeam != 1 && PlayerTeam != 2)) continue;
        ++enemies;

        const player::HealthInfo Health = player::health_info(p);
        const int hp_now = Health.valid ? Health.hp : -1;
        const auto hp_prev = g_last_hp.find(p.object);
        if (hp_prev == g_last_hp.end() || hp_prev->second != hp_now) {
            diag::log("HEALTH", "object=0x%llx hp=%d max=%d valid=%d",
                      (unsigned long long)p.object, Health.hp, Health.max_hp, Health.valid ? 1 : 0);
            g_last_hp[p.object] = hp_now;
        }
        if (!Health.valid || Health.hp <= 0) {
            g_dead_since[p.object] = now_ms;
            continue;
        }
        const auto dead = g_dead_since.find(p.object);
        if (dead != g_dead_since.end()) {
            // Hide a dead body for a short grace period even if its stats
            // object briefly reports the pre-death HP again.
            if (now_ms - dead->second < 1500) continue;
            g_dead_since.erase(dead);
        }
        ++alive;

        enemy_frame e{};
        e.object = p.object;
        e.snap = &p;
        e.hp = Health.hp;
        e.max_hp = Health.max_hp;
        if (cfg::esp::weapon) e.weapon = weapon::active_id(p);
        if (!read_enemy_pose(e)) continue;
        ++positioned;
        e.distance = calculate_distance(e.position, LocalPosition);
        if (e.distance > 500.f) continue;
        frame_enemies.push_back(e);
    }

    // Straddle check against the locked native camera.
    if (!Cameras.empty() && Cameras.front().kind == camera_kind_main_native && !frame_enemies.empty()) {
        matrix view_after{}, proj_after{};
        if (cam::reread_lock(view_after, proj_after) &&
            std::memcmp(&view_after, &Cameras.front().view, sizeof(matrix)) != 0) {
            static long long straddle_log = 0;
            static unsigned straddles = 0;
            ++straddles;
            for (enemy_frame& e : frame_enemies) read_enemy_pose(e);
            Cameras.front().view = view_after;
            Cameras.front().projection = proj_after;
            if (diag::every(straddle_log, 10000))
                diag::log("CAMERA", "frame straddle re-reads so far: %u", straddles);
        }
    }

    // Feed the interpolator with the consistent samples and replace the rendered positions by the interpolated ones.
    const long long sample_us = diag::now_us();
    for (enemy_frame& e : frame_enemies) {
        motion_state& m = g_motion[e.object];
        motion_update(m, e.position, e.head, sample_us);
        if (cfg::esp::smooth) motion_render(m, sample_us, e.position, e.head);
    }

    // Camera selection. The controller camera whose bor is
    // PlayerAnimationData.MainCamera is the camera that rendered the frame;
    // use it as is. Every other source is scored against the live player
    // coordinates so a UI/preview/arms camera cannot win.
    matrix ViewMatrix{}, ProjectionMatrix{};
    static int selected_kind_prev = camera_kind_unknown;
    uint64_t selected_camera = 0;
    int selected_kind = camera_kind_unknown;
    int selected_score = -1, selected_front = 0, selected_onscreen = 0;
    const bool has_targets = !frame_enemies.empty();
    if (Cameras.front().kind == camera_kind_main_native) {
        const camera_candidate& Camera = Cameras.front();
        selected_camera = Camera.address;
        selected_kind = Camera.kind;
        selected_score = 1000;
        ViewMatrix = Camera.view;
        ProjectionMatrix = Camera.projection;
    } else {
        static long long candidate_log = 0;
        const bool log_candidates = diag::every(candidate_log, 5000);
        for (const auto& Camera : Cameras) {
            int score = 0, front = 0, onscreen = 0;
            const float* view = reinterpret_cast<const float*>(&Camera.view);
            const Vector3 camera_position(
                -(view[0] * view[12] + view[1] * view[13] + view[2] * view[14]),
                -(view[4] * view[12] + view[5] * view[13] + view[6] * view[14]),
                -(view[8] * view[12] + view[9] * view[13] + view[10] * view[14]));
            const float camera_distance = calculate_distance(camera_position, LocalPosition);
            for (const auto& Enemy : frame_enemies) {
                ImVec2 screen_head, screen_foot;
                if (!world_to_screen(Enemy.head, Camera.view, Camera.projection, screen_head) ||
                    !world_to_screen(Enemy.position, Camera.view, Camera.projection, screen_foot)) continue;
                ++front;
                score += 10;
                const bool head_inside = screen_head.x >= -g_sw * .25f && screen_head.x <= g_sw * 1.25f &&
                                         screen_head.y >= -g_sh * .25f && screen_head.y <= g_sh * 1.25f;
                const bool foot_inside = screen_foot.x >= -g_sw * .25f && screen_foot.x <= g_sw * 1.25f &&
                                         screen_foot.y >= -g_sh * .25f && screen_foot.y <= g_sh * 1.25f;
                const float body_height = std::fabs(screen_foot.y - screen_head.y);
                if (head_inside && foot_inside) { ++onscreen; score += 50; }
                if (body_height >= 2.f && body_height <= g_sh * 1.5f) score += 5;
            }
            if (front > 0) score += 50;
            if (camera_distance < 30.f) score += 5;
            // Source quality: real matrices beat synthetic ones, the
            // PlayerAnimationData.MainCamera state beats the vqn/yjp guesses.
            switch (Camera.kind) {
                case camera_kind_kmc_native: score += 40; break;
                case camera_kind_other_native: score += 25; break;
                case camera_kind_main_state: score += 30; break;
                case camera_kind_vqn_state: score += 10; break;
                case camera_kind_ios_transform: score += 0; break;
                case camera_kind_heap_camera: score += 5; break;
                default: break;
            }
            // Strong hysteresis: the synthetic sources (main-bor / vqn-bor /
            // ios-transform) score within a few points of each other and
            // flipping between them every few frames is itself a visible jump.
            if (Camera.address == g_camera && Camera.kind == selected_kind_prev) score += 40;
            if (log_candidates)
                diag::log("CAMERA", "candidate=0x%llx kind=%s score=%d front=%d onscreen=%d localDistance=%.2f proj=(%.3f %.3f)",
                          (unsigned long long)Camera.address, camera_kind_name(Camera.kind), score, front, onscreen,
                          camera_distance, Camera.projection.m11, Camera.projection.m22);
            if (has_targets && front == 0) continue;
            if (score > selected_score || (score == selected_score && Camera.address == g_camera)) {
                selected_score = score;
                selected_front = front;
                selected_onscreen = onscreen;
                selected_camera = Camera.address;
                selected_kind = Camera.kind;
                ViewMatrix = Camera.view;
                ProjectionMatrix = Camera.projection;
            }
        }
    }
    if (!selected_camera || selected_score < 0) {
        g_camera = 0;
        STATUS_SET(camera, "camera: none (%zu candidates)", Cameras.size());
        esp_wait("camera candidates do not project players");
        return;
    }
    if (g_camera != selected_camera) {
        diag::log("CAMERA", "selected=0x%llx kind=%s score=%d front=%d onscreen=%d candidates=%zu",
                  (unsigned long long)selected_camera, camera_kind_name(selected_kind), selected_score,
                  selected_front, selected_onscreen, Cameras.size());
        STATUS_SET(camera, "camera: %s", camera_kind_name(selected_kind));
    }
    g_camera = selected_camera;
    selected_kind_prev = selected_kind;

    crash::stage("esp-draw");
    // Box from the feet to just above the head: the head point is the eye
    // pivot, so 15 cm are added to keep the top edge off the forehead.
    struct draw_item { ImVec2 min, max; float cx; const enemy_frame* e; };
    std::vector<draw_item> items;
    items.reserve(frame_enemies.size());
    for (const auto& Enemy : frame_enemies) {
        ImVec2 top, foot;
        if (!world_to_screen(Enemy.head + Vector3(0.f, 0.15f, 0.f), ViewMatrix, ProjectionMatrix, top) ||
            !world_to_screen(Enemy.position, ViewMatrix, ProjectionMatrix, foot)) continue;
        ++projected;
        float y1 = std::min(top.y, foot.y), y2 = std::max(top.y, foot.y);
        if (!(y2 - y1 >= 1.f) || y2 - y1 > g_sh * 3.f) continue;
        if (y2 - y1 < 10.f) { const float mid = (y1 + y2) * 0.5f; y1 = mid - 5.f; y2 = mid + 5.f; }
        const float half_w = std::max(3.f, (y2 - y1) * 0.21f);
        const float cx = roundf((top.x + foot.x) * 0.5f);
        const ImVec2 mn(roundf(cx - half_w), roundf(y1)), mx(roundf(cx + half_w), roundf(y2));
        if (mx.x < -64.f || mn.x > g_sw + 64.f || mx.y < -64.f || mn.y > g_sh + 64.f) continue;
        items.push_back({mn, mx, cx, &Enemy});
    }
    // Far first, near last: the closest player is never covered by another box.
    std::sort(items.begin(), items.end(),
              [](const draw_item& l, const draw_item& r) { return l.e->distance > r.e->distance; });
    for (const draw_item& it : items) {
        const enemy_frame& Enemy = *it.e;
        const float box_h = it.max.y - it.min.y;
        // Distant players fade a little so the close ones stand out.
        const float fade = 1.f - 0.4f * std::clamp((Enemy.distance - 40.f) / 110.f, 0.f, 1.f);
        const float text_sz = std::clamp(box_h * 0.11f, 11.f, 16.f);
        if (cfg::esp::box) dbox(it.min, it.max, fade);
        if (cfg::esp::health) dhp(Enemy.hp, Enemy.max_hp, it.min, it.max, fade);
        if (cfg::esp::name) {
            const auto found = g_name_cache.find(Enemy.object);
            const char* label = found != g_name_cache.end() && !found->second.name.empty()
                ? found->second.name.c_str() : "Enemy";
            dnick(label, it.min, it.cx, text_sz, fade);
        }
        if (cfg::esp::weapon) dweapon(Enemy.weapon, it.cx, it.max.y + 3.f, it.max.x - it.min.x, text_sz, fade);
    }

    // PROBE (read-only, opt-in): while /data/local/tmp/bycmd_probe exists, dumps the szi bytes
    // (Player odq+0x88 -> szi +0x10..+0x1F) of every enemy every 100 ms. A line is logged when the
    // bytes change, and every 2 s regardless. Used to find the wall/visibility flag.
    {
        static long long probe_check = 0, probe_tick = 0, probe_dump = 0;
        static bool probe_on = false;
        static std::unordered_map<uint64_t, std::array<uint8_t, 16>> probe_last;
        if (diag::every(probe_check, 1000)) probe_on = access("/data/local/tmp/bycmd_probe", F_OK) == 0;
        if (probe_on && diag::every(probe_tick, 100)) {
            const bool periodic = diag::every(probe_dump, 2000);
            for (const auto& Enemy : frame_enemies) {
                const uint64_t szi = rpm<uint64_t>(Enemy.object + 0x88);
                std::array<uint8_t, 16> cur{};
                if (!(obj_ptr(szi) && mem_read(szi + 0x10, cur.data(), cur.size()))) cur.fill(0xEE);
                const auto it = probe_last.find(Enemy.object);
                const bool changed = it == probe_last.end() || it->second != cur;
                if (changed || periodic) {
                    char hex[3 * 16 + 1];
                    for (int b = 0; b < 16; ++b) snprintf(hex + b * 3, 4, "%02X ", cur[b]);
                    diag::log("PROBE", "enemy=0x%llx szi=0x%llx dist=%.1f b[10..1F]=%s%s",
                              (unsigned long long)Enemy.object, (unsigned long long)szi, Enemy.distance,
                              hex, changed ? "CHANGED" : "");
                }
                probe_last[Enemy.object] = cur;
            }
        }
    }
    static long long frame_log = 0;
    if (diag::every(frame_log, 5000)) {
        diag::log(projected || !has_targets ? "INFO" : "WARN",
                  "ESP frame players=%zu local=0x%llx team=%d pos=(%.2f %.2f %.2f) enemies=%d alive=%d positioned=%d projected=%d camera=0x%llx kind=%s",
                  Players.size(), (unsigned long long)LocalPlayer, LocalTeam,
                  LocalPosition.x, LocalPosition.y, LocalPosition.z, enemies, alive, positioned,
                  projected, (unsigned long long)g_camera, camera_kind_name(selected_kind));
    }
}

namespace {
    ImU32 rgba(const ImVec4& c, float a) {
        return IM_COL32(int(c.x * 255.f), int(c.y * 255.f), int(c.z * 255.f), int(std::clamp(c.w * a, 0.f, 1.f) * 255.f));
    }
    ImU32 black(float a) { return IM_COL32(0, 0, 0, int(std::clamp(a, 0.f, 1.f) * 255.f)); }

    // Red (empty) -> yellow (half) -> green (full).
    ImU32 health_color(float pct, float a) {
        pct = std::clamp(pct, 0.f, 1.f);
        const ImVec4 red(0.93f, 0.25f, 0.25f, 1.f), yellow(1.f, 0.82f, 0.25f, 1.f), green(0.35f, 0.88f, 0.42f, 1.f);
        const float t = pct < 0.5f ? pct * 2.f : (pct - 0.5f) * 2.f;
        const ImVec4& lo = pct < 0.5f ? red : yellow;
        const ImVec4& hi = pct < 0.5f ? yellow : green;
        return rgba(ImVec4(lo.x + (hi.x - lo.x) * t, lo.y + (hi.y - lo.y) * t, lo.z + (hi.z - lo.z) * t, 1.f), a);
    }

    // Pixel-exact horizontal / vertical line with a 1 px dark border.
    void hline(ImDrawList* dl, float x0, float x1, float y, ImU32 col, ImU32 border) {
        dl->AddRectFilled(ImVec2(x0 - 1.f, y - 1.f), ImVec2(x1 + 1.f, y + 2.f), border);
        dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + 1.f), col);
    }
    void vline(ImDrawList* dl, float x, float y0, float y1, ImU32 col, ImU32 border) {
        dl->AddRectFilled(ImVec2(x - 1.f, y0 - 1.f), ImVec2(x + 2.f, y1 + 1.f), border);
        dl->AddRectFilled(ImVec2(x, y0), ImVec2(x + 1.f, y1), col);
    }
}

void visuals::dbox(const ImVec2& t, const ImVec2& b, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float x1 = t.x, y1 = t.y, x2 = b.x, y2 = b.y;
    const ImU32 col = rgba(cfg::esp::box_col, a);
    const ImU32 border = black(0.78f * a);

    if (cfg::esp::box_type == 0) {
        dl->AddRect(ImVec2(x1 - 1.f, y1 - 1.f), ImVec2(x2 + 2.f, y2 + 2.f), border, 0.f, 0, 1.f);
        dl->AddRect(ImVec2(x1 + 1.f, y1 + 1.f), ImVec2(x2, y2), border, 0.f, 0, 1.f);
        dl->AddRect(ImVec2(x1, y1), ImVec2(x2 + 1.f, y2 + 1.f), col, 0.f, 0, 1.f);
        return;
    }
    // Corner box: each corner a quarter of the shorter side.
    const float len = std::max(3.f, roundf(std::min(x2 - x1, y2 - y1) * 0.25f));
    hline(dl, x1, x1 + len, y1, col, border); vline(dl, x1, y1, y1 + len, col, border);
    hline(dl, x2 - len + 1.f, x2 + 1.f, y1, col, border); vline(dl, x2, y1, y1 + len, col, border);
    hline(dl, x1, x1 + len, y2, col, border); vline(dl, x1, y2 - len + 1.f, y2 + 1.f, col, border);
    hline(dl, x2 - len + 1.f, x2 + 1.f, y2, col, border); vline(dl, x2, y2 - len + 1.f, y2 + 1.f, col, border);
}

// Health bar to the left of the box, coloured by the health left; the
// number sits beside the top of the fill once some health is lost.
void visuals::dhp(int hp, int max_hp, const ImVec2& t, const ImVec2& b, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (max_hp <= 0) max_hp = 100;
    hp = std::clamp(hp, 0, max_hp);
    const float pct = float(hp) / float(max_hp);

    const float x1 = t.x - 4.f, x0 = x1 - 2.f;   // 2 px bar, 3 px gap to the box border
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

// Name centred above the box.
void visuals::dnick(const char* name, const ImVec2& box_min, float cx, float sz, float a) {
    if (!espFont || a < 0.01f || !name) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, name);
    const ImVec2 tp(roundf(cx - ts.x * 0.5f), roundf(box_min.y - ts.y - 3.f));
    draw_text_outlined(dl, espFont, sz, tp, rgba(cfg::esp::name_col, a), name);
}

// Weapon icon under the box: the atlas icon of this weapon id (weapon_icons.h), tinted by
// the ESP colour. The name when the weapon has no icon or the atlas did not upload.
void visuals::dweapon(int weapon_id, float cx, float y, float box_w, float sz, float a) {
    const char* label = weapon::name(weapon_id);
    if (!label || a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImU32 col = rgba(cfg::esp::box_col, a);
    const weapon_icon_rect* icon = nullptr;
    for (const weapon_icon_rect& r : weapon_icon_rects)
        if (r.id == weapon_id) { icon = &r; break; }
    if (icon && g_weapon_atlas_tex) {
        // Height follows the label size; the icon is then shrunk so it never
        // gets much wider than the box.
        float h = std::clamp(sz * 1.45f, 12.f, 28.f);
        float w = h * float(icon->w) / float(icon->h);
        const float max_w = std::max(box_w * 1.4f, 34.f);
        if (w > max_w) { h *= max_w / w; w = max_w; }
        const ImVec2 p0(roundf(cx - w * 0.5f), roundf(y));
        const ImVec2 p1(p0.x + roundf(w), p0.y + roundf(h));
        const ImVec2 uv0(float(icon->x) / float(weapon_icons_w), float(icon->y) / float(weapon_icons_h));
        const ImVec2 uv1(float(icon->x + icon->w) / float(weapon_icons_w), float(icon->y + icon->h) / float(weapon_icons_h));
        // Dark halo on all four sides, then the tinted icon.
        const ImU32 halo = black(0.55f * a);
        for (const ImVec2 d : {ImVec2(-1.f, 0.f), ImVec2(1.f, 0.f), ImVec2(0.f, -1.f), ImVec2(0.f, 1.f)})
            dl->AddImage(g_weapon_atlas_tex, ImVec2(p0.x + d.x, p0.y + d.y), ImVec2(p1.x + d.x, p1.y + d.y), uv0, uv1, halo);
        dl->AddImage(g_weapon_atlas_tex, p0, p1, uv0, uv1, col);
        return;
    }
    if (!espFont) return;
    const ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, label);
    draw_text_outlined(dl, espFont, sz, ImVec2(roundf(cx - ts.x * 0.5f), roundf(y)), col, label);
}

// 1 px dark outline on all sides: readable on bright and dark backgrounds.
void visuals::draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text) {
    if (!font || !dl || !text) return;
    const int a = int((color >> IM_COL32_A_SHIFT) & 0xFF);
    const ImU32 outline = IM_COL32(0, 0, 0, a * 220 / 255);
    for (const ImVec2 d : {ImVec2(-1.f, 0.f), ImVec2(1.f, 0.f), ImVec2(0.f, -1.f), ImVec2(0.f, 1.f)})
        dl->AddText(font, size, ImVec2(pos.x + d.x, pos.y + d.y), outline, text);
    dl->AddText(font, size, pos, color, text);
}
