
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

    // ---- per-enemy animation ------------------------------------------
    // Fade-in when an enemy appears, a health bar that slides down with a
    // short "damage trail", and a brief flash of the box on a hit.
    struct anim_state {
        float alpha{0.f};       // fade-in 0..1
        float shown_hp{-1.f};   // health drawn (eases toward the real value)
        float trail_hp{-1.f};   // health before the last hits (fades after a pause)
        int last_hp{-1};
        long long hit_ms{0};
    };
    std::unordered_map<uint64_t, anim_state> g_anim;

    void anim_update(anim_state& s, int hp, float dt, long long now_ms) {
        s.alpha = std::min(1.f, s.alpha + dt * 7.f);
        if (s.shown_hp < 0.f || float(hp) > s.shown_hp) {   // first sample, heal or respawn: snap
            s.shown_hp = s.trail_hp = float(hp);
        } else {
            if (s.last_hp >= 0 && hp < s.last_hp) {
                s.trail_hp = std::max(s.trail_hp, s.shown_hp);
                s.hit_ms = now_ms;
            }
            s.shown_hp += (float(hp) - s.shown_hp) * std::min(1.f, dt * 14.f);
            if (now_ms - s.hit_ms > 350) s.trail_hp += (s.shown_hp - s.trail_hp) * std::min(1.f, dt * 6.f);
            s.trail_hp = std::max(s.trail_hp, s.shown_hp);
        }
        s.last_hp = hp;
    }

    // At most `max_glyphs` UTF-8 characters, ".." appended when cut.
    std::string clip_utf8(const char* text, size_t max_glyphs) {
        std::string out;
        size_t glyphs = 0;
        for (const char* p = text; *p; ) {
            const unsigned char c = static_cast<unsigned char>(*p);
            const size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
            if (glyphs == max_glyphs) { out += ".."; break; }
            for (size_t i = 0; i < len && p[i]; ++i) out.push_back(p[i]);
            p += len;
            ++glyphs;
        }
        return out;
    }

    // Half width of a player's box in pixels. The width does not follow the
    // height (a crouching player is as wide as a standing one): it is measured
    // 0.38 m to each side of the feet along the camera's right axis, which is
    // what 0.21 x height gave a standing player. A view matrix whose rows are
    // not unit length is not trusted and keeps that ratio.
    float box_half_width(const Vector3& feet, const matrix& view, const matrix& projection, float height_px) {
        float half = std::max(3.f, height_px * 0.21f);
        const float* m = reinterpret_cast<const float*>(&view);
        const float row = std::sqrt(m[0] * m[0] + m[4] * m[4] + m[8] * m[8]);   // camera x axis in world space
        if (row > 0.5f && row < 2.f) {
            const Vector3 right(m[0] / row, m[4] / row, m[8] / row);
            ImVec2 left_px, right_px;
            if (world_to_screen(feet - right * 0.38f, view, projection, left_px) &&
                world_to_screen(feet + right * 0.38f, view, projection, right_px))
                half = std::clamp(std::fabs(right_px.x - left_px.x) * 0.5f, std::max(3.f, height_px * 0.14f), height_px * 0.55f);
        }
        return half;
    }

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
    bool any = cfg::esp::box || cfg::esp::name || cfg::esp::health || cfg::esp::weapon ||
               cfg::esp::distance || cfg::esp::stance || cfg::esp::arrows;
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
        purge_missing(g_anim, live);
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
        int stance{-1};
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
        if (cfg::esp::weapon || cfg::esp::stance) {
            e.weapon = weapon::active_id(p);
            e.stance = weapon::stance(p.object);
        }
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
    const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.001f, 0.1f);
    // Box from the feet to just above the head: the head point is the eye
    // pivot, so 15 cm are added to keep the top edge off the forehead.
    struct draw_item { ImVec2 min, max; float cx; const enemy_frame* e; };
    std::vector<draw_item> items;
    items.reserve(frame_enemies.size());
    std::vector<const enemy_frame*> off_screen;
    for (const auto& Enemy : frame_enemies) {
        ImVec2 top, foot;
        const bool projected_ok = world_to_screen(Enemy.head + Vector3(0.f, 0.15f, 0.f), ViewMatrix, ProjectionMatrix, top) &&
                                  world_to_screen(Enemy.position, ViewMatrix, ProjectionMatrix, foot);
        float y1 = std::min(top.y, foot.y), y2 = std::max(top.y, foot.y);
        const float cx = roundf((top.x + foot.x) * 0.5f);
        if (!projected_ok || !std::isfinite(cx) || !std::isfinite(y1) || !std::isfinite(y2) ||
            !(y2 - y1 >= 1.f) || y2 - y1 > g_sh * 3.f) {
            off_screen.push_back(&Enemy);
            continue;
        }
        ++projected;
        if (y2 - y1 < 10.f) { const float mid = (y1 + y2) * 0.5f; y1 = mid - 5.f; y2 = mid + 5.f; }
        const float half_w = box_half_width(Enemy.position, ViewMatrix, ProjectionMatrix, y2 - y1);
        const ImVec2 mn(roundf(cx - half_w), roundf(y1)), mx(roundf(cx + half_w), roundf(y2));
        if (mx.x < 0.f || mn.x > g_sw || mx.y < 0.f || mn.y > g_sh) {
            off_screen.push_back(&Enemy);
            continue;
        }
        items.push_back({mn, mx, cx, &Enemy});
    }
    // Far first, near last: the closest player is never covered by another box.
    std::sort(items.begin(), items.end(),
              [](const draw_item& l, const draw_item& r) { return l.e->distance > r.e->distance; });
    for (const draw_item& it : items) {
        const enemy_frame& Enemy = *it.e;
        anim_state& anim = g_anim[Enemy.object];
        anim_update(anim, Enemy.hp, dt, now_ms);
        const float box_h = it.max.y - it.min.y;
        // Distant players fade a little so the close ones stand out.
        const float fade = (1.f - 0.4f * std::clamp((Enemy.distance - 40.f) / 110.f, 0.f, 1.f)) * anim.alpha;
        const float flash = std::clamp(1.f - float(now_ms - anim.hit_ms) / 160.f, 0.f, 1.f);
        const float text_sz = std::clamp(box_h * 0.11f, 11.f, 16.f);
        if (cfg::esp::box) dbox(it.min, it.max, fade, flash);
        if (cfg::esp::health) dhp(anim.shown_hp, anim.trail_hp, Enemy.hp, Enemy.max_hp, it.min, it.max, fade);
        if (cfg::esp::name) {
            const auto found = g_name_cache.find(Enemy.object);
            const bool known = found != g_name_cache.end() && !found->second.name.empty();
            const std::string label = clip_utf8(known ? found->second.name.c_str() : "Enemy", 16);
            dnick(label.c_str(), it.min, it.cx, text_sz, known ? fade : fade * 0.7f);
        }
        if (cfg::esp::stance && (Enemy.stance == 1 || Enemy.stance == 2))
            dstance(Enemy.stance, it.min, it.max, fade, text_sz);
        float below = it.max.y + 3.f;
        if (cfg::esp::weapon) below = dweapon(Enemy.weapon, it.cx, below, it.max.x - it.min.x, text_sz, fade);
        if (cfg::esp::distance) ddistance(Enemy.distance, it.cx, below, fade, text_sz);
    }
    if (cfg::esp::arrows)
        for (const enemy_frame* e : off_screen) darrow(e->position + Vector3(0.f, 0.9f, 0.f), e->distance, ViewMatrix);

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

namespace {
    // Keeps a label of size `ts` inside the screen with a 2 px margin.
    ImVec2 clamp_label(ImVec2 pos, const ImVec2& ts) {
        pos.x = std::clamp(pos.x, 2.f, std::max(2.f, g_sw - ts.x - 2.f));
        pos.y = std::clamp(pos.y, 2.f, std::max(2.f, g_sh - ts.y - 2.f));
        return ImVec2(roundf(pos.x), roundf(pos.y));
    }
    ImU32 mix_white(const ImVec4& c, float t, float a) {
        return rgba(ImVec4(c.x + (1.f - c.x) * t, c.y + (1.f - c.y) * t, c.z + (1.f - c.z) * t, c.w), a);
    }
}

void visuals::dbox(const ImVec2& t, const ImVec2& b, float a, float flash) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float x1 = t.x, y1 = t.y, x2 = b.x, y2 = b.y;
    // A hit flashes the box towards white for a moment.
    const ImU32 col = mix_white(cfg::esp::box_col, std::clamp(flash, 0.f, 1.f) * 0.85f, a);
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

// Health bar to the left of the box. `shown` eases toward the real health,
// `trail` keeps the health before the last hits for a moment (light part
// above the fill); the number is the real value.
void visuals::dhp(float shown, float trail, int hp, int max_hp, const ImVec2& t, const ImVec2& b, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (max_hp <= 0) max_hp = 100;
    hp = std::clamp(hp, 0, max_hp);
    const float fmax = float(max_hp);
    const float pct = std::clamp(shown / fmax, 0.f, 1.f);
    const float trail_pct = std::clamp(std::max(trail, shown) / fmax, 0.f, 1.f);

    const float x1 = t.x - 4.f, x0 = x1 - 2.f;   // 2 px bar, 3 px gap to the box border
    const float y0 = t.y, y1 = b.y + 1.f;
    const float fill_top = roundf(y1 - (y1 - y0) * pct);
    const float trail_top = roundf(y1 - (y1 - y0) * trail_pct);
    dl->AddRectFilled(ImVec2(x0 - 1.f, y0 - 1.f), ImVec2(x1 + 1.f, y1 + 1.f), black(0.75f * a));
    if (fill_top - trail_top >= 1.f)
        dl->AddRectFilled(ImVec2(x0, trail_top), ImVec2(x1, fill_top), IM_COL32(255, 255, 255, int(170 * a)));
    if (y1 - fill_top >= 1.f) dl->AddRectFilled(ImVec2(x0, fill_top), ImVec2(x1, y1), health_color(pct, a));

    if (hp < max_hp && espFont && y1 - y0 >= 28.f) {
        char txt[8];
        std::snprintf(txt, sizeof(txt), "%d", hp);
        const float fs = 11.f;
        const ImVec2 ts = espFont->CalcTextSizeA(fs, FLT_MAX, 0.f, txt);
        const float ty = std::clamp(fill_top - ts.y * 0.5f, y0 - ts.y * 0.5f, y1 - ts.y * 0.5f);
        draw_text_outlined(dl, espFont, fs, clamp_label(ImVec2(x0 - 3.f - ts.x, ty), ts), IM_COL32(255, 255, 255, int(255 * a)), txt);
    }
}

// Name centred above the box, kept on screen.
void visuals::dnick(const char* name, const ImVec2& box_min, float cx, float sz, float a) {
    if (!espFont || a < 0.01f || !name || !*name) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, name);
    const ImVec2 tp = clamp_label(ImVec2(cx - ts.x * 0.5f, box_min.y - ts.y - 3.f), ts);
    draw_text_outlined(dl, espFont, sz, tp, rgba(cfg::esp::name_col, a), name);
}

// CROUCH / JUMP beside the top right corner of the box.
void visuals::dstance(int stance, const ImVec2& box_min, const ImVec2& box_max, float a, float sz) {
    if (!espFont || a < 0.01f) return;
    const char* label = stance == 2 ? "JUMP" : "CROUCH";
    const float fs = std::clamp(sz * 0.85f, 10.f, 14.f);   // 10 px on small boxes, grows with the label size
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 ts = espFont->CalcTextSizeA(fs, FLT_MAX, 0.f, label);
    const ImU32 col = stance == 2 ? IM_COL32(120, 200, 255, int(255 * a)) : IM_COL32(255, 210, 90, int(255 * a));
    draw_text_outlined(dl, espFont, fs, clamp_label(ImVec2(box_max.x + 4.f, box_min.y), ts), col, label);
}

// Weapon icon under the box: the atlas icon of this weapon id (weapon_icons.h), tinted by
// the ESP colour. The name when the weapon has no icon or the atlas did not upload.
// Returns the y just below what was drawn (y itself when nothing was).
float visuals::dweapon(int weapon_id, float cx, float y, float box_w, float sz, float a) {
    const char* label = weapon::name(weapon_id);
    if (!label || a < 0.01f) return y;
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
        const ImVec2 p0 = clamp_label(ImVec2(cx - w * 0.5f, y), ImVec2(roundf(w), roundf(h)));
        const ImVec2 p1(p0.x + roundf(w), p0.y + roundf(h));
        const ImVec2 uv0(float(icon->x) / float(weapon_icons_w), float(icon->y) / float(weapon_icons_h));
        const ImVec2 uv1(float(icon->x + icon->w) / float(weapon_icons_w), float(icon->y + icon->h) / float(weapon_icons_h));
        // Dark halo on all four sides, then the tinted icon.
        const ImU32 halo = black(0.55f * a);
        for (const ImVec2 d : {ImVec2(-1.f, 0.f), ImVec2(1.f, 0.f), ImVec2(0.f, -1.f), ImVec2(0.f, 1.f)})
            dl->AddImage(g_weapon_atlas_tex, ImVec2(p0.x + d.x, p0.y + d.y), ImVec2(p1.x + d.x, p1.y + d.y), uv0, uv1, halo);
        dl->AddImage(g_weapon_atlas_tex, p0, p1, uv0, uv1, col);
        return p1.y + 2.f;
    }
    if (!espFont) return y;
    const ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, label);
    const ImVec2 tp = clamp_label(ImVec2(cx - ts.x * 0.5f, y), ts);
    draw_text_outlined(dl, espFont, sz, tp, col, label);
    return tp.y + ts.y + 1.f;
}

// "24m" centred under the weapon.
void visuals::ddistance(float meters, float cx, float y, float a, float sz) {
    if (!espFont || a < 0.01f || !std::isfinite(meters)) return;
    char txt[16];
    std::snprintf(txt, sizeof(txt), "%dm", int(meters + 0.5f));
    const float fs = std::clamp(sz * 0.85f, 10.f, 14.f);   // 10 px on small boxes, grows with the label size
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 ts = espFont->CalcTextSizeA(fs, FLT_MAX, 0.f, txt);
    draw_text_outlined(dl, espFont, fs, clamp_label(ImVec2(cx - ts.x * 0.5f, y), ts), IM_COL32(220, 220, 220, int(230 * a)), txt);
}

// Arrow on a circle around the screen centre pointing at an enemy that is
// out of view: up = ahead, down = behind. Closer enemies get bigger, more
// opaque arrows.
void visuals::darrow(const Vector3& world, float meters, const matrix& view) {
    if (!std::isfinite(meters)) return;
    const Vector4 cam = matrix_multiply(view, Vector4(world.x, world.y, world.z, 1.f));
    const float right = cam.x, forward = -cam.z;   // Unity view space looks down -Z
    if (!std::isfinite(right) || !std::isfinite(forward) || (std::fabs(right) < 1e-4f && std::fabs(forward) < 1e-4f)) return;
    const float angle = std::atan2(right, forward);
    const float sn = std::sin(angle), cs = std::cos(angle);
    const ImVec2 c(g_sw * 0.5f, g_sh * 0.5f);
    const float radius = std::min(g_sw, g_sh) * 0.38f;
    const float near_t = 1.f - std::clamp((meters - 5.f) / 60.f, 0.f, 1.f);
    const float size = 9.f + 6.f * near_t;
    const float a = 0.55f + 0.45f * near_t;
    // Twice as long as it is wide: an arrow that is wider than it is long
    // reads as a plain triangle, and its direction is lost when it is small.
    const float len = size * 2.f, half_w = size * 0.5f;
    const ImVec2 dir(sn, -cs), side(cs, sn);
    const auto at = [&](float along, float across) {
        return ImVec2(c.x + dir.x * (radius + along) + side.x * across, c.y + dir.y * (radius + along) + side.y * across);
    };
    const ImVec2 tip = at(len * 0.55f, 0.f), l = at(-len * 0.45f, half_w), r = at(-len * 0.45f, -half_w);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddTriangleFilled(tip, l, r, rgba(cfg::esp::box_col, a));
    dl->AddTriangle(tip, l, r, black(0.8f * a), 1.f);
    if (cfg::esp::distance && espFont) {
        char txt[16];
        std::snprintf(txt, sizeof(txt), "%dm", int(meters + 0.5f));
        const float fs = 11.f;
        const ImVec2 ts = espFont->CalcTextSizeA(fs, FLT_MAX, 0.f, txt);
        // Inside the ring, along the arrow's axis, clear of the triangle.
        const float inset = len * 0.45f + 4.f + (std::fabs(dir.x) * ts.x + std::fabs(dir.y) * ts.y) * 0.5f;
        const ImVec2 tp = clamp_label(ImVec2(c.x + dir.x * (radius - inset) - ts.x * 0.5f, c.y + dir.y * (radius - inset) - ts.y * 0.5f), ts);
        draw_text_outlined(dl, espFont, fs, tp, IM_COL32(220, 220, 220, int(230 * a)), txt);
    }
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
