
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

    // ---- health bar: recently lost health -------------------------------
    // After a hit the lost part of the bar stays as a pale segment for a
    // moment and then drains, so the damage just dealt is visible at a glance.
    struct hp_view { float shown{0.f}; int last{-1}; long long hold_until_ms{0}; };
    std::unordered_map<uint64_t, hp_view> g_hp_view;
    constexpr long long kTrailHoldMs = 350;
    constexpr float kTrailDrain = 9.f;   // exponential rate, 1/s

    // Displayed health (>= hp). First sight and healing show no trail; every
    // hit restarts the hold; after it the value falls towards the real one.
    float hp_trail_step(hp_view& v, int hp, int max_hp, long long now_ms, float dt_s) {
        const float target = float(hp);
        if (v.last < 0 || hp > v.last) v.shown = target;
        else if (hp < v.last) v.hold_until_ms = now_ms + kTrailHoldMs;
        v.last = hp;
        if (v.shown < target) v.shown = target;
        if (now_ms >= v.hold_until_ms && v.shown > target) {
            v.shown += (target - v.shown) * (1.f - std::exp(-kTrailDrain * dt_s));
            if (v.shown - target < 0.5f) v.shown = target;
        }
        return std::min(v.shown, float(std::max(max_hp, 1)));
    }

    // Distant players fade a little so the close ones stand out.
    float distance_fade(float metres) {
        return 1.f - 0.4f * std::clamp((metres - 40.f) / 110.f, 0.f, 1.f);
    }
}

void visuals::draw() {
    if (!(g_sw >= 2.f) || !(g_sh >= 2.f)) return;
    bool any = cfg::esp::box || cfg::esp::name || cfg::esp::health || cfg::esp::weapon ||
               cfg::esp::distance || cfg::esp::arrows;
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
        purge_missing(g_hp_view, live);
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
                g_hp_view.clear();
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
    // Box geometry is visuals::project_box. An enemy without a box on the
    // screen (behind the camera, or a box completely outside) becomes a
    // candidate for a direction arrow.
    std::vector<visuals::target> targets;
    targets.reserve(frame_enemies.size());
    struct arrow_item { visuals::arrow arrow; float distance; };
    std::vector<arrow_item> arrow_items;
    for (const auto& Enemy : frame_enemies) {
        visuals::box_rect box{};
        const visuals::box_state state = visuals::project_box(Enemy.position, Enemy.head, ViewMatrix, ProjectionMatrix, box);
        if (state == visuals::box_state::off_screen || state == visuals::box_state::visible) ++projected;
        if (state == visuals::box_state::visible) {
            const char* label = nullptr;
            if (cfg::esp::name) {
                const auto found = g_name_cache.find(Enemy.object);
                label = found != g_name_cache.end() && !found->second.name.empty() ? found->second.name.c_str() : "Enemy";
            }
            targets.push_back({box, Enemy.object, Enemy.distance, Enemy.hp, Enemy.max_hp, Enemy.weapon, label});
        } else if (cfg::esp::arrows && (state == visuals::box_state::none || state == visuals::box_state::off_screen)) {
            visuals::arrow ar{};
            if (visuals::offscreen_arrow(ViewMatrix, Enemy.position, ar)) arrow_items.push_back({ar, Enemy.distance});
        }
    }
    // Far first, near last: the closest player is never covered by another box.
    std::sort(targets.begin(), targets.end(),
              [](const visuals::target& l, const visuals::target& r) { return l.distance > r.distance; });
    static long long last_draw_us = 0;
    const float dt_s = last_draw_us ? std::clamp(float(now_us - last_draw_us) * 1e-6f, 0.f, 0.1f) : 0.016f;
    last_draw_us = now_us;
    for (const visuals::target& t : targets) visuals::draw_target(t, now_ms, dt_s);
    if (!arrow_items.empty()) {
        // The eight nearest, the nearest drawn last.
        std::sort(arrow_items.begin(), arrow_items.end(),
                  [](const arrow_item& l, const arrow_item& r) { return l.distance > r.distance; });
        const size_t first = arrow_items.size() > 8 ? arrow_items.size() - 8 : 0;
        for (size_t i = first; i < arrow_items.size(); ++i)
            visuals::draw_arrow(arrow_items[i].arrow, arrow_items[i].distance, distance_fade(arrow_items[i].distance));
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
}

void visuals::dbox(const ImVec2& t, const ImVec2& b, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float x1 = t.x, y1 = t.y, x2 = b.x, y2 = b.y;
    const ImU32 col = rgba(cfg::esp::box_col, a);
    const ImU32 border = black(0.78f * a);

    if (!cfg::esp::corner) {
        dl->AddRect(ImVec2(x1 - 1.f, y1 - 1.f), ImVec2(x2 + 2.f, y2 + 2.f), border, 0.f, 0, 1.f);
        dl->AddRect(ImVec2(x1 + 1.f, y1 + 1.f), ImVec2(x2, y2), border, 0.f, 0, 1.f);
        dl->AddRect(ImVec2(x1, y1), ImVec2(x2 + 1.f, y2 + 1.f), col, 0.f, 0, 1.f);
        return;
    }
    // Corner box: the arms are 30 % of the width and 18 % of the height. All
    // dark borders go down first and the coloured arms after them, so the
    // border of one arm never cuts into its neighbour at the corner.
    const float lx = std::max(3.f, roundf((x2 - x1) * 0.30f));
    const float ly = std::max(3.f, roundf((y2 - y1) * 0.18f));
    struct arm { float x0, y0, x1, y1; };   // filled pixels [x0,x1) x [y0,y1)
    const arm arms[8] = {
        {x1, y1, x1 + lx, y1 + 1.f},              {x1, y1, x1 + 1.f, y1 + ly},                    // top left
        {x2 - lx + 1.f, y1, x2 + 1.f, y1 + 1.f},  {x2, y1, x2 + 1.f, y1 + ly},                    // top right
        {x1, y2, x1 + lx, y2 + 1.f},              {x1, y2 - ly + 1.f, x1 + 1.f, y2 + 1.f},        // bottom left
        {x2 - lx + 1.f, y2, x2 + 1.f, y2 + 1.f},  {x2, y2 - ly + 1.f, x2 + 1.f, y2 + 1.f},        // bottom right
    };
    for (const arm& r : arms) dl->AddRectFilled(ImVec2(r.x0 - 1.f, r.y0 - 1.f), ImVec2(r.x1 + 1.f, r.y1 + 1.f), border);
    for (const arm& r : arms) dl->AddRectFilled(ImVec2(r.x0, r.y0), ImVec2(r.x1, r.y1), col);
}

// Health bar to the left of the box, coloured by the health left. The bar is
// 2 px wide on small boxes and grows to 4 px on big ones. The part lost to
// the last hit (trail_hp > hp) stays pale above the fill until it drains;
// quarter marks show up once the bar is tall enough to tell them apart; the
// number sits beside the top of the fill once some health is lost.
void visuals::dhp(int hp, int max_hp, float trail_hp, const ImVec2& t, const ImVec2& b, float a, float number_sz) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (max_hp <= 0) max_hp = 100;
    hp = std::clamp(hp, 0, max_hp);
    const float pct = float(hp) / float(max_hp);
    const float trail_pct = std::clamp(trail_hp / float(max_hp), pct, 1.f);

    const float bw = std::clamp(roundf((b.y - t.y) / 140.f), 2.f, 4.f);
    const float x1 = t.x - 4.f, x0 = x1 - bw;   // 3 px gap to the box border
    const float y0 = t.y, y1 = b.y + 1.f;
    const float fill_top = roundf(y1 - (y1 - y0) * pct);
    const float trail_top = roundf(y1 - (y1 - y0) * trail_pct);
    dl->AddRectFilled(ImVec2(x0 - 1.f, y0 - 1.f), ImVec2(x1 + 1.f, y1 + 1.f), black(0.75f * a));
    if (fill_top - trail_top >= 1.f)
        dl->AddRectFilled(ImVec2(x0, trail_top), ImVec2(x1, fill_top), IM_COL32(255, 246, 220, int(235.f * a)));
    if (y1 - fill_top >= 1.f) dl->AddRectFilled(ImVec2(x0, fill_top), ImVec2(x1, y1), health_color(pct, a));
    if (y1 - y0 >= 72.f) {
        for (int quarter = 1; quarter <= 3; ++quarter) {
            const float ty = roundf(y1 - (y1 - y0) * 0.25f * float(quarter));
            dl->AddRectFilled(ImVec2(x0, ty), ImVec2(x1, ty + 1.f), black(0.55f * a));
        }
    }

    if (hp < max_hp && espFont && y1 - y0 >= 28.f) {
        char txt[8];
        std::snprintf(txt, sizeof(txt), "%d", hp);
        const ImVec2 ts = espFont->CalcTextSizeA(number_sz, FLT_MAX, 0.f, txt);
        const float ty = std::clamp(roundf(fill_top - ts.y * 0.5f), y0 - ts.y * 0.5f, y1 - ts.y * 0.5f);
        draw_text_outlined(dl, espFont, number_sz, ImVec2(roundf(x0 - 3.f - ts.x), ty), IM_COL32(255, 255, 255, int(255 * a)), txt);
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
float visuals::dweapon(int weapon_id, float cx, float y, float box_w, float sz, float a) {
    const char* label = weapon::name(weapon_id);
    if (!label || a < 0.01f) return 0.f;
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
        return (p1.y - p0.y) + 2.f;
    }
    if (!espFont) return 0.f;
    const ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, label);
    draw_text_outlined(dl, espFont, sz, ImVec2(roundf(cx - ts.x * 0.5f), roundf(y)), col, label);
    return ts.y + 2.f;
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

// Distance in whole metres, centred under the box (or under the weapon icon).
float visuals::ddist(float metres, float cx, float y, float sz, float a) {
    if (!espFont || a < 0.01f || !std::isfinite(metres)) return 0.f;
    char txt[16];
    std::snprintf(txt, sizeof(txt), "%dm", int(metres + 0.5f));
    const ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, txt);
    draw_text_outlined(ImGui::GetBackgroundDrawList(), espFont, sz, ImVec2(roundf(cx - ts.x * 0.5f), roundf(y)),
                       IM_COL32(235, 235, 235, int(255.f * a)), txt);
    return ts.y + 2.f;
}

// Box from the feet to just above the head: the head point is the eye pivot,
// so 15 cm are added to keep the top edge off the forehead. The width does
// not follow the height (a crouching player is as wide as a standing one):
// it is measured 0.38 m to each side of the feet along the camera's right
// axis, which is what 0.21 * height gave a standing player before.
visuals::box_state visuals::project_box(const Vector3& feet, const Vector3& head, const matrix& view,
                                        const matrix& projection, box_rect& out) {
    ImVec2 top, foot;
    if (!world_to_screen(head + Vector3(0.f, 0.15f, 0.f), view, projection, top) ||
        !world_to_screen(feet, view, projection, foot)) return box_state::none;
    float y1 = std::min(top.y, foot.y), y2 = std::max(top.y, foot.y);
    if (!(y2 - y1 >= 1.f) || y2 - y1 > g_sh * 3.f) return box_state::degenerate;
    if (y2 - y1 < 10.f) { const float mid = (y1 + y2) * 0.5f; y1 = mid - 5.f; y2 = mid + 5.f; }
    const float h = y2 - y1;
    float half_w = std::max(3.f, h * 0.21f);
    // Row 0 of a view matrix is the camera's x axis in world space. A matrix
    // whose rows are not unit length is not trusted: the ratio above stays.
    const float* m = reinterpret_cast<const float*>(&view);
    const float row = std::sqrt(m[0] * m[0] + m[4] * m[4] + m[8] * m[8]);
    if (row > 0.5f && row < 2.f) {
        const Vector3 right(m[0] / row, m[4] / row, m[8] / row);
        ImVec2 left_px, right_px;
        if (world_to_screen(feet - right * 0.38f, view, projection, left_px) &&
            world_to_screen(feet + right * 0.38f, view, projection, right_px))
            half_w = std::clamp(std::fabs(right_px.x - left_px.x) * 0.5f, std::max(3.f, h * 0.14f), h * 0.55f);
    }
    const float cx = roundf((top.x + foot.x) * 0.5f);
    out.min = ImVec2(roundf(cx - half_w), roundf(y1));
    out.max = ImVec2(roundf(cx + half_w), roundf(y2));
    out.cx = cx;
    if (out.max.x < -64.f || out.min.x > g_sw + 64.f || out.max.y < -64.f || out.min.y > g_sh + 64.f)
        return box_state::off_screen;
    return box_state::visible;
}

// One enemy: box, health bar, name above, weapon icon and distance below.
void visuals::draw_target(const target& t, long long now_ms, float dt_s) {
    const box_rect& r = t.box;
    const float k = ui::style::k();
    const float box_h = r.max.y - r.min.y;
    const float fade = distance_fade(t.distance);
    const float text_sz = std::clamp(box_h * 0.11f, 11.f * k, 16.f * k);
    if (cfg::esp::box) dbox(r.min, r.max, fade);
    if (cfg::esp::health) {
        const float trail = hp_trail_step(g_hp_view[t.key], t.hp, t.max_hp, now_ms, dt_s);
        dhp(t.hp, t.max_hp, trail, r.min, r.max, fade, std::clamp(text_sz * 0.85f, 11.f * k, 14.f * k));
    }
    if (cfg::esp::name && t.name) dnick(t.name, r.min, r.cx, text_sz, fade);
    float y = r.max.y + 3.f;
    if (cfg::esp::weapon) y += dweapon(t.weapon, r.cx, y, r.max.x - r.min.x, text_sz, fade);
    if (cfg::esp::distance) ddist(t.distance, r.cx, y, std::max(10.f * k, text_sz * 0.9f), fade);
}

// Direction of an enemy as seen from the camera, for one that has no box on
// the screen. View space looks down -Z with +X to the right, so (x, z) of the
// enemy is already the screen direction: ahead (z < 0) points up, right
// points right, behind (z > 0) points down. The arrow sits on a ring around
// the screen centre.
bool visuals::offscreen_arrow(const matrix& view, const Vector3& world, arrow& out) {
    const Vector4 c = matrix_multiply(view, Vector4(world.x, world.y, world.z, 1.f));
    const float h = std::sqrt(c.x * c.x + c.z * c.z);
    if (!std::isfinite(h) || h < 1e-3f) return false;
    out.dir = ImVec2(c.x / h, c.z / h);
    const float radius = 0.38f * g_sh;
    out.pos = ImVec2(g_sw * 0.5f + out.dir.x * radius, g_sh * 0.5f + out.dir.y * radius);
    return true;
}

// Elongated triangle pointing outwards (about twice as long as it is wide,
// so the direction reads even when it is tiny), in the ESP colour on a dark
// halo; nearer enemies get a bigger one (18 px up to 15 m, 10 px from 80 m on).
void visuals::draw_arrow(const arrow& ar, float distance, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float k = ui::style::k();
    const float s = (10.f + 8.f * (1.f - std::clamp((distance - 15.f) / 65.f, 0.f, 1.f))) * k;
    const ImVec2 d = ar.dir, n(-d.y, d.x);
    const auto at = [&](float along, float side) {
        return ImVec2(ar.pos.x + (d.x * along + n.x * side) * s, ar.pos.y + (d.y * along + n.y * side) * s);
    };
    const ImVec2 tip = at(1.1f, 0.f);
    ImVec2 base_a = at(-0.8f, 0.5f), base_b = at(-0.8f, -0.5f);
    // Filled shapes need clockwise winding (y down: positive cross product), otherwise the AA fringe goes inwards.
    if ((base_a.x - tip.x) * (base_b.y - tip.y) - (base_a.y - tip.y) * (base_b.x - tip.x) < 0.f) std::swap(base_a, base_b);
    const ImVec2 mid = at(-0.17f, 0.f);   // centroid
    const auto grown = [&](const ImVec2& p) { return ImVec2(mid.x + (p.x - mid.x) * 1.3f, mid.y + (p.y - mid.y) * 1.3f); };
    dl->AddTriangleFilled(grown(tip), grown(base_a), grown(base_b), black(0.7f * a));
    dl->AddTriangleFilled(tip, base_a, base_b, rgba(cfg::esp::box_col, a));
    if (cfg::esp::distance && espFont && std::isfinite(distance)) {
        char txt[16];
        std::snprintf(txt, sizeof(txt), "%dm", int(distance + 0.5f));
        const float fs = 12.f * k;
        const ImVec2 ts = espFont->CalcTextSizeA(fs, FLT_MAX, 0.f, txt);
        // Inside the ring, along the arrow's axis, clear of the triangle and its halo.
        const float inset = s * 1.f + 4.f * k + (std::fabs(d.x) * ts.x + std::fabs(d.y) * ts.y) * 0.5f;
        draw_text_outlined(dl, espFont, fs, ImVec2(roundf(ar.pos.x - d.x * inset - ts.x * 0.5f), roundf(ar.pos.y - d.y * inset - ts.y * 0.5f)),
                           IM_COL32(235, 235, 235, int(255.f * a)), txt);
    }
}
