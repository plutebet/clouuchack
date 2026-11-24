
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
    if (diag::every(last, 5000)) diag::log(diag::lvl("WAIT"), "ESP blocked at: %s", stage);
}

namespace {
    // A nickname is shown exactly as read. The only rewrite: a character the
    // ESP font has no glyph for cannot be written, the name shows as "empty".
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
            if (cp > 0xFFFF) return false;   // ImWchar is 16-bit, astral glyphs do not exist in the font
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
        for (auto it = map.begin(); it != map.end();) {
            if (live.count(it->first)) ++it;
            else it = map.erase(it);
        }
    }

    // No interpolation: positions are drawn from the latest game frame as
    // read, so ESP adds no delay of its own. The present gate keeps the
    // compositor cost at zero while nothing moves.
}

namespace {
    // Debug trace: while /data/local/tmp/bycmd_espdebug exists, every ESP
    // draw element appends one record to /data/local/tmp/bycmd-esp.log (a
    // separate file, the main bycmd.log is untouched), and draws with
    // suspicious geometry also raise a throttled ANOMALY record on the main
    // log, which the debugger script routes into anomalies.log.
    int g_esp_trace_fd = -1;
    long long g_esp_trace_frame = 0;

    bool esp_trace_enabled() {
        static long long last = 0;
        static bool on = false;
        if (diag::every(last, 1000)) {
            const bool want = access("/data/local/tmp/bycmd_espdebug", F_OK) == 0;
            if (want && g_esp_trace_fd < 0) {
                g_esp_trace_fd = ::open("/data/local/tmp/bycmd-esp.log",
                                        O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_CLOEXEC, 0644);
                if (g_esp_trace_fd >= 0)
                    diag::log("INFO", "ESP trace on -> /data/local/tmp/bycmd-esp.log");
            } else if (!want && g_esp_trace_fd >= 0) {
                ::close(g_esp_trace_fd);
                g_esp_trace_fd = -1;
                diag::log("INFO", "ESP trace off");
            }
            on = want && g_esp_trace_fd >= 0;
        }
        return on;
    }

    void esp_trace(const char* format, ...) {
        if (g_esp_trace_fd < 0) return;
        char line[320];
        const long long t = diag::now_ms();
        int used = std::snprintf(line, sizeof(line), "[%lld.%03lld] ", t / 1000, t % 1000);
        va_list args;
        va_start(args, format);
        const int added = std::vsnprintf(line + used, sizeof(line) - size_t(used), format, args);
        va_end(args);
        if (added > 0) used += std::min<int>(added, int(sizeof(line) - size_t(used) - 1));
        if (used < static_cast<int>(sizeof(line)) - 1) line[used++] = '\n';
        (void)::write(g_esp_trace_fd, line, size_t(used));
    }

    // One anomaly type at most every 2 s, so a broken frame cannot flood the log.
    void esp_anomaly(const char* what, const char* format, ...) {
        static std::unordered_map<const char*, long long> last;
        if (!diag::every(last[what], 2000)) return;
        char detail[256];
        va_list args;
        va_start(args, format);
        std::vsnprintf(detail, sizeof(detail), format, args);
        va_end(args);
        diag::log("ANOMALY", "%s: %s", what, detail);
    }
}

// Red (empty) -> yellow (half) -> green (full): the HP line colour
// follows the health left, as in the original юкеш дроид build.
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
    bool any = cfg::esp::box || cfg::esp::infobar || cfg::esp::weapon;
    if (!any) { esp_wait("all ESP options disabled"); return; }

    crash::stage("esp-players");
    const long long now_ms = diag::now_ms();
    const long long now_us = diag::now_us();
    // The registry walk (list, items, one 0x98 block per NetObject) runs at
    // 10 Hz: membership, team and component pointers change on spawn/death,
    // not per frame. HP and poses below are read every frame, and a stale
    // stats/transform pointer fails its klass or TRS checks in that read.
    static std::vector<player_snapshot> s_players;
    static long long s_players_ms = 0;
    static int s_players_pid = -1;
    if (now_ms - s_players_ms >= 100 || s_players_pid != proc::pid) {
        s_players = players();
        s_players_ms = now_ms;
        s_players_pid = proc::pid;
    }
    const std::vector<player_snapshot>& Players = s_players;
    if (Players.empty()) { esp_wait("player discovery / registry scan"); return; }

    // Per-object caches must not outlive the match: a new match reuses heap
    // addresses, and a stale "dead since" entry would hide a live player.
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

    // Resolve at most one name per 100 ms (dozens of reads each). Players
    // without a name come first; a failed re-read never replaces a name that
    // was already found for the same player.
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
                reset_name_fields();
            }
            last_local = LocalPlayer;
        }
    }
    const int LocalTeam = Local->team;
    if (LocalTeam != 1 && LocalTeam != 2) { esp_wait("local team validation"); return; }

    // Camera first (one matrix read with a lock), then local + enemy poses
    // and HP in one batched read, then the camera again: if it changed in
    // between, the game rendered a frame while we were reading and the poses
    // are re-read so that box and camera describe the same frame.
    crash::stage("esp-camera");
    cam::main_chain chain{};
    std::vector<camera_candidate> Cameras = cam::resolve(LocalPlayer, chain);
    if (Cameras.empty()) { g_camera = 0; esp_wait("camera resolution (controller / bor / transform)"); return; }
    // The synthetic sources (bor with FOV 60, vqn, ios-transform) are only
    // approximations; ios-transform in particular can project far off the
    // model, and none of them follows the scope FOV. The boxes are hidden
    // instead of drawn through them:
    //  - after a lock loss in this round, for 2 s while it is re-acquired;
    //  - at the start of a round (new local NetObject) until the real
    //    camera is locked, up to 3 s (log 20261114-175419: 1.2 s of boxes
    //    through main-bor/ios-transform at FOV 60 while scoped at FOV 20).
    // After that the synthetic fallback is used as before.
    {
        static long long last_native_ms = 0;
        static uint64_t native_round = 0;
        static uint64_t round_player = 0;
        static long long round_start_ms = 0;
        if (round_player != LocalPlayer) { round_player = LocalPlayer; round_start_ms = now_ms; }
        if (Cameras.front().kind == camera_kind_main_native) {
            last_native_ms = now_ms;
            native_round = LocalPlayer;
        } else {
            const bool reacquiring = native_round == LocalPlayer && now_ms - last_native_ms < 2000;
            const bool starting = native_round != LocalPlayer && now_ms - round_start_ms < 3000;
            if (reacquiring || starting) {
                static long long hold_log = 0;
                if (diag::every(hold_log, 1000))
                    diag::log("CAMERA", "%s: ESP hidden until the native camera is locked (best fallback=%s, %lld ms)",
                              reacquiring ? "native camera lost" : "round start",
                              camera_kind_name(Cameras.front().kind),
                              reacquiring ? now_ms - last_native_ms : now_ms - round_start_ms);
                g_camera = 0;
                esp_wait("waiting for the native camera");
                return;
            }
        }
    }

    crash::stage("esp-poses");
    static std::vector<player::frame_player> s_frame;
    static player::frame_batch s_batch;
    s_frame.clear();
    s_frame.push_back({Local, false});   // [0] = local player, pose only
    for (const player_snapshot& p : Players) {
        if (p.object == LocalPlayer) continue;
        const int PlayerTeam = p.team;
        if (PlayerTeam == LocalTeam || (PlayerTeam != 1 && PlayerTeam != 2)) continue;
        s_frame.push_back({&p, true});
    }
    player::read_frame(s_frame, s_batch);

    const player::PlayerPose LocalPose = s_frame[0].pose;
    Vector3 LocalPosition = LocalPose.feet;
    if (s_frame[0].stale) { esp_wait("stale local NetObject (identity changed)"); return; }
    if (!LocalPose.valid || !std::isfinite(LocalPosition.x) ||
        (LocalPosition.x == 0.f && LocalPosition.y == 0.f && LocalPosition.z == 0.f)) {
        esp_wait("local transform hierarchy"); return;
    }

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
        bool rebuilt{false};
        int fp{0};   // index into s_frame, for late-straddle re-reads
    };
    std::vector<enemy_frame> frame_enemies;
    frame_enemies.reserve(s_frame.size());
    auto apply_pose = [](enemy_frame& e, const player::PlayerPose& Shape) -> bool {
        if (!Shape.valid) return false;
        e.position = Shape.feet;
        e.head = Shape.has_head ? Shape.head : Shape.feet + Vector3(0.f, 1.67f, 0.f);
        e.has_head = Shape.has_head;
        return true;
    };
    for (size_t fi = 1; fi < s_frame.size(); ++fi) {
        const player_snapshot& p = *s_frame[fi].snap;
        ++enemies;

        // Identity and team are re-proven inside the same batch read: a heap
        // slot reused by another NetObject, or a mid-round team change, drops
        // the player for this frame instead of drawing someone else's pose,
        // health or name.
        const player::frame_player& fp = s_frame[fi];
        if (fp.stale) {
            static long long stale_log = 0;
            if (diag::every(stale_log, 5000))
                diag::log("INFO", "NetObject 0x%llx identity changed id=%u -> %u; frame dropped",
                          (unsigned long long)p.object, p.id, fp.live_id);
            continue;
        }
        if (fp.live_team != p.team) continue;

        const player::HealthInfo Health = s_frame[fi].health;
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
        if (!apply_pose(e, s_frame[fi].pose)) continue;
        e.rebuilt = s_frame[fi].rebuilt;
        ++positioned;
        e.distance = calculate_distance(e.position, LocalPosition);
        if (e.distance > 500.f) continue;
        e.fp = int(fi);
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
            static std::vector<player::frame_player> s_reread;
            s_reread.clear();
            for (const enemy_frame& e : frame_enemies) s_reread.push_back({e.snap, false});
            player::read_frame(s_reread, s_batch);
            // A stale identity in the re-read drops the enemy: drawing the old
            // pose of a reused slot is exactly the garbage frame this pass exists
            // to prevent.
            std::vector<enemy_frame> fresh;
            fresh.reserve(frame_enemies.size());
            for (size_t k = 0; k < frame_enemies.size(); ++k) {
                if (s_reread[k].stale) continue;
                if (apply_pose(frame_enemies[k], s_reread[k].pose))
                    fresh.push_back(frame_enemies[k]);
            }
            frame_enemies.swap(fresh);
            Cameras.front().view = view_after;
            Cameras.front().projection = proj_after;
            if (diag::every(straddle_log, 10000))
                diag::log("CAMERA", "frame straddle re-reads so far: %u", straddles);
        }
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
    const bool trace = esp_trace_enabled();
    if (trace) ++g_esp_trace_frame;
    // Late straddle: between the batched pose read and this point the game
    // may have composited another frame. One bounded pass: re-verify the
    // camera and when it moved, re-read every pose so the boxes describe the
    // frame actually composited; an enemy whose identity failed in the
    // re-read is dropped, never drawn at someone else's coordinates.
    if (selected_kind == camera_kind_main_native) {
        matrix view_after{}, proj_after{};
        if (cam::reread_lock(view_after, proj_after) &&
            std::memcmp(&view_after, &Cameras.front().view, sizeof(matrix)) != 0) {
            static long long late_log = 0;
            static unsigned late_straddles = 0;
            ++late_straddles;
            player::read_frame(s_frame, s_batch);
            if (s_frame[0].stale) { esp_wait("stale local NetObject (late straddle)"); return; }
            if (s_frame[0].pose.valid) LocalPosition = s_frame[0].pose.feet;
            std::vector<enemy_frame> fresh;
            fresh.reserve(frame_enemies.size());
            for (const enemy_frame& e : frame_enemies) {
                const player::frame_player& fp2 = s_frame[size_t(e.fp)];
                if (fp2.stale) continue;
                enemy_frame copy = e;
                if (apply_pose(copy, fp2.pose)) {
                    copy.distance = calculate_distance(copy.position, LocalPosition);
                    if (copy.distance <= 500.f) fresh.push_back(copy);
                }
            }
            frame_enemies.swap(fresh);
            Cameras.front().view = view_after;
            Cameras.front().projection = proj_after;
            ViewMatrix = view_after;
            ProjectionMatrix = proj_after;
            if (diag::every(late_log, 30000))
                diag::log("INFO", "late frame straddle re-reads: %u", late_straddles);
        }
    }
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
        // Clean box: width = half the height (measured on the reference).
        const float half_w = std::max(3.f, (y2 - y1) * 0.25f);
        const float cx = roundf((top.x + foot.x) * 0.5f);
        const ImVec2 mn(roundf(cx - half_w), roundf(y1)), mx(roundf(cx + half_w), roundf(y2));
        if (mx.x < -64.f || mn.x > g_sw + 64.f || mx.y < -64.f || mn.y > g_sh + 64.f) continue;
        items.push_back({mn, mx, cx, &Enemy});
    }
    // Far first, near last: the closest player is never covered by another box.
    std::sort(items.begin(), items.end(),
              [](const draw_item& l, const draw_item& r) { return l.e->distance > r.e->distance; });
    const ImVec2 line_origin(g_sw * 0.5f, g_sh * 0.045f);
    for (const draw_item& it : items) {
        const enemy_frame& Enemy = *it.e;
        const float box_h = it.max.y - it.min.y;
        // Distant players fade a little so the close ones stand out.
        const float fade = 1.f - 0.4f * std::clamp((Enemy.distance - 40.f) / 110.f, 0.f, 1.f);
        const float text_sz = std::clamp(box_h * 0.11f, 11.f, 16.f);
        // Geometry sanity: every anomaly below is also a draw that went out
        // wrong on screen, so the debugger script can correlate it with the
        // ESP trace records of the same frame.
        if (!std::isfinite(it.min.x + it.min.y + it.max.x + it.max.y + line_origin.x + line_origin.y))
            esp_anomaly("non-finite draw coordinates", "frame=%lld object=0x%llx box=(%.1f %.1f)-(%.1f %.1f)",
                        g_esp_trace_frame, (unsigned long long)Enemy.object,
                        it.min.x, it.min.y, it.max.x, it.max.y);
        if (box_h <= 0.f || box_h > g_sh * 3.f)
            esp_anomaly("absurd box geometry", "frame=%lld object=0x%llx height=%.1f",
                        g_esp_trace_frame, (unsigned long long)Enemy.object, box_h);
        if (Enemy.hp < 0 || (Enemy.max_hp > 0 && Enemy.hp > Enemy.max_hp * 2))
            esp_anomaly("hp out of range while drawing", "frame=%lld object=0x%llx hp=%d max=%d",
                        g_esp_trace_frame, (unsigned long long)Enemy.object, Enemy.hp, Enemy.max_hp);
        // Tracer line first, so the box and the bar sit on top of it.
        if (cfg::esp::line) {
            const ImVec2 to(it.cx, it.min.y);
            dline(line_origin, to, 0.85f * fade);
            if (trace)
                esp_trace("DRAW line frame=%lld object=0x%llx from=(%.1f,%.1f) to=(%.1f,%.1f) alpha=%.2f hp=%d/%d dist=%.1f",
                          g_esp_trace_frame, (unsigned long long)Enemy.object,
                          line_origin.x, line_origin.y, to.x, to.y, 0.85f * fade, Enemy.hp, Enemy.max_hp, Enemy.distance);
        }
        if (cfg::esp::box) {
            dbox(it.min, it.max, fade);
            if (trace)
                esp_trace("DRAW box frame=%lld object=0x%llx min=(%.1f,%.1f) max=(%.1f,%.1f) hp=%d/%d dist=%.1f",
                          g_esp_trace_frame, (unsigned long long)Enemy.object,
                          it.min.x, it.min.y, it.max.x, it.max.y, Enemy.hp, Enemy.max_hp, Enemy.distance);
        }
        // Info bar: dark pill above the box with the white nickname; the HP
        // line sits inside the pill bottom, colour follows the health left.
        if (cfg::esp::infobar) {
            const auto found = g_name_cache.find(Enemy.object);
            // Exact name only: an unreadable glyph -> "empty", no name yet ->
            // the neutral "Enemy" placeholder, never a foreign name.
            const std::string shown = found != g_name_cache.end() && !found->second.name.empty()
                ? (name_writable(found->second.name) ? found->second.name : std::string("empty"))
                : std::string();
            const char* label = shown.empty() ? "Enemy" : shown.c_str();
            dnick(label, Enemy.hp, Enemy.max_hp, it.min, it.cx, text_sz, fade);
            if (trace)
                esp_trace("DRAW nick frame=%lld object=0x%llx name=%s hp=%d/%d x=%.1f y_top=%.1f",
                          g_esp_trace_frame, (unsigned long long)Enemy.object, label,
                          Enemy.hp, Enemy.max_hp, it.cx, it.min.y);
        }
        if (cfg::esp::weapon) {
            dweapon(Enemy.weapon, it.cx, it.max.y + 3.f, it.max.x - it.min.x, text_sz, fade);
            if (trace)
                esp_trace("DRAW weapon frame=%lld object=0x%llx weapon=%d x=%.1f y=%.1f",
                          g_esp_trace_frame, (unsigned long long)Enemy.object,
                          Enemy.weapon, it.cx, it.max.y + 3.f);
        }
    }
    // Locked native camera but nothing projected: the frame the ESP draws is
    // not the frame the game rendered, the matrices went stale. Logged once
    // when the streak starts and then at most every 10 s while it lasts.
    {
        static long long none_start = 0, none_logged = 0;
        const bool nothing = trace && projected == 0 && alive > 0 &&
                             Cameras.front().kind == camera_kind_main_native;
        if (nothing) {
            if (!none_start) none_start = diag::now_ms();
            const long long dur = diag::now_ms() - none_start;
            if (dur >= 4000 && diag::every(none_logged, 10000))
                esp_anomaly("native camera locked but nothing projects",
                            "frame=%lld alive=%d camera=0x%llx for_ms=%lld",
                            g_esp_trace_frame, alive, (unsigned long long)g_camera, dur);
        } else {
            none_start = 0;
            none_logged = 0;
        }
    }
    // The first fully drawn frame ends the warmup: from here on read/scan
    // misses are real anomalies, before that they were normal (BOOT level).
    diag::g_warm = true;

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
}

void visuals::dbox(const ImVec2& t, const ImVec2& b, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    // Clean box only: one thin translucent line, slightly rounded, no dark
    // outline. 2 px on a 1224 px tall screen, scaled with the display.
    const float th = std::max(1.5f, roundf(2.f * g_sh / 1224.f));
    const ImU32 thin = rgba(cfg::esp::box_col, 0.82f * a);
    dl->AddRect(t, b, thin, th * 1.5f, 0, th);
}

// Tracer line (reference look): one thin white line from the top of the
// screen to the enemy; every line starts at the same convergence point,
// thickness scales with the display like the rest of the ESP.
void visuals::dline(const ImVec2& from, const ImVec2& to, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float th = std::max(1.f, roundf(1.5f * g_sh / 1224.f));
    dl->AddLine(from, to, IM_COL32(255, 255, 255, int(std::clamp(a, 0.f, 1.f) * 255.f)), th);
}

// Info bar (reference look): a dark rounded pill above the box with the
// nickname in white text. Health is the thin line along the pill's bottom
// edge — its length follows HP and its colour goes red -> yellow -> green
// as the health drops, so one element carries both name and health.
void visuals::dnick(const char* name, int hp, int max_hp, const ImVec2& box_min, float cx, float sz, float a) {
    if (!espFont || a < 0.01f || !name) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, name);
    const float ph = std::max(sz + 9.f, 15.f);           // pill height
    const float pad_x = ph * 0.55f;
    const float pw = ts.x + pad_x * 2.f;
    const float x0 = roundf(cx - pw * 0.5f);
    const float y1 = roundf(box_min.y - 3.f);            // pill bottom
    const float y0 = y1 - ph;

    if (max_hp <= 0) max_hp = 100;
    const float pct = std::clamp(float(hp) / float(max_hp), 0.f, 1.f);
    const float r = ph * 0.5f;

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + pw, y1), black(0.78f * a), r);
    // HP line inside the bottom edge: length and colour both follow HP.
    const float line_th = std::max(2.f, roundf(2.f * g_sh / 1224.f));
    const float line_x0 = x0 + 3.f, line_w = (pw - 6.f) * pct;
    if (line_w >= 1.f)
        dl->AddRectFilled(ImVec2(line_x0, y1 - line_th), ImVec2(line_x0 + line_w, y1), health_color(pct, a));
    // White nickname, centred.
    const ImVec2 tp(roundf(cx - ts.x * 0.5f), roundf(y0 + (ph - ts.y) * 0.5f));
    dl->AddText(espFont, sz, tp, IM_COL32(255, 255, 255, int(255 * a)), name);
}

// Weapon name under the box (reference look): the weapon name in white
// with a dark outline, no icon.
void visuals::dweapon(int weapon_id, float cx, float y, float box_w, float sz, float a) {
    const char* label = weapon::name(weapon_id);
    if (!label || a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!espFont) return;
    const float fs = std::clamp(sz * 1.05f, 11.f, 17.f);
    const ImVec2 ts = espFont->CalcTextSizeA(fs, FLT_MAX, 0.f, label);
    draw_text_outlined(dl, espFont, fs, ImVec2(roundf(cx - ts.x * 0.5f), roundf(y)), IM_COL32(255, 255, 255, int(255 * a)), label);
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
