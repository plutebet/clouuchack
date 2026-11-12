
// ================================================================================
// Сурс взят из бота @imguisource_bot
// Подпишись на ImGui Source: t.me/ImGui_Source_FIXNEY
// Подпишись на shit src 🧨: t.me/il2cpp_src
// ================================================================================
//
// Two threads share the ESP:
//  * reader (visuals::start): every remote read of the ESP (players, transforms, health,
//    weapons, camera, names) happens here, at 144 Hz, and each tick publishes one sample;
//  * render (visuals::draw, from the main loop): interpolates the newest sample and draws it.
// The render thread never waits for the game's memory: a slow read can only make the boxes a
// few milliseconds old, it cannot stall the frame.
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
#include "imgui.h"
#include <array>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <fcntl.h>
#include <unistd.h>

extern ImFont* espFont;
extern ImTextureID g_weapon_atlas_tex;

namespace {
    constexpr int kMaxEnemies = 10;
    constexpr long long kReaderPeriodUs = 6944;   // 144 Hz while a match is running
    constexpr long long kReaderIdleUs = 100000;   // 10 Hz while there is nothing to read
    constexpr long long kStaleUs = 400000;        // samples older than this are not drawn

    ImU32 scaled_color(const ImVec4& c, float a, float k = 1.f) {
        return IM_COL32(int(c.x * 255.f * k), int(c.y * 255.f * k), int(c.z * 255.f * k), int(c.w * 255.f * a));
    }

    template<typename Map>
    void purge_missing(Map& map, const std::unordered_set<uint64_t>& live) {
        for (auto it = map.begin(); it != map.end();) {
            if (live.count(it->first)) ++it;
            else it = map.erase(it);
        }
    }

    float vdist(const Vector3& a, const Vector3& b) {
        const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    // ---- published sample (reader -> render) ---------------------------------
    struct enemy_sample {
        uint64_t object{0};
        int hp{0};
        int max_hp{100};
        int weapon{-1};
        Vector3 feet{};
        Vector3 head{};
        float distance{0.f};
        bool has_name{false};
        char name[40]{};
    };
    struct frame_sample {
        unsigned long long seq{0};
        long long sample_us{0};
        bool valid{false};
        int enemy_count{0};
        matrix view{};
        matrix projection{};
        enemy_sample enemies[kMaxEnemies]{};
    };
    std::mutex g_sample_mutex;
    frame_sample g_sample;                       // guarded by g_sample_mutex
    std::atomic<bool> g_reader_run{false};
    std::atomic<bool> g_reset_request{false};
    std::thread g_reader;

    // ---- reader-owned state ----------------------------------------------------
    struct name_entry {
        uint64_t source{0};          // managed string the name came from (fast path)
        uint32_t id{0};
        long long refreshed_ms{0};
        bool has_text{false};
        char text[40]{};
    };
    std::unordered_map<uint64_t, name_entry> g_names;
    std::unordered_map<uint64_t, long long> g_dead_since;
    std::unordered_map<uint64_t, int> g_last_hp;
    long long g_last_purge_ms = 0;
    long long g_last_full_name_ms = 0;

    struct enemy_frame {
        uint64_t object{0};
        int snap_index{-1};
        Vector3 position{};
        Vector3 head{};
        bool has_head{false};
        int hp{0};
        int max_hp{100};
        int weapon{-1};
        float distance{0.f};
    };

    void esp_wait(const char* stage) {
        static long long last = 0;
        if (diag::every(last, 5000)) diag::log("WAIT", "ESP blocked at: %s", stage);
    }

    // Names. The cheap path re-reads the managed string found last time (three reads). The
    // component walk of player_name() is expensive: it runs at most once per second, and a player
    // whose name cannot be found is retried every 10 s.
    void refresh_names(const std::vector<player_snapshot>& list, long long now_ms) {
        const uint64_t string_class = managed_string_class();
        for (const player_snapshot& p : list) {
            name_entry& e = g_names[p.object];
            const bool id_changed = e.id != p.id;
            const bool stale = now_ms - e.refreshed_ms > 5000;
            const bool missing = !e.has_text && now_ms - e.refreshed_ms > 10000;
            if (!id_changed && !stale && !missing) continue;
            std::string found;
            if (e.source && string_class) found = read_managed_string(e.source, string_class);
            if (found.empty() || noisy_player_name(found)) {
                if (now_ms - g_last_full_name_ms < 1000) continue;
                g_last_full_name_ms = now_ms;
                uint64_t source = 0;
                found = player_name(p.object, &source);
                e.source = source;
            }
            e.id = p.id;
            e.refreshed_ms = now_ms;
            e.has_text = !found.empty();
            std::snprintf(e.text, sizeof(e.text), "%s", found.c_str());
        }
    }

    void reset_reader_state() {
        g_names.clear();
        g_dead_since.clear();
        g_last_hp.clear();
        g_last_full_name_ms = 0;
        g_camera = 0;
        cam::reset();
    }

    // One reader tick. Returns false when there is nothing to publish.
    bool read_tick(frame_sample& out) {
        if (!(g_sw >= 2.f) || !(g_sh >= 2.f)) return false;
        const bool any = cfg::esp::box || cfg::esp::name || cfg::esp::health || cfg::esp::weapon;
        if (!any) { esp_wait("all ESP options disabled"); return false; }
        if (!proc::build_ok || proc::lib == 0 || proc::pid <= 0) return false;

        crash::stage("esp-players");
        const std::vector<player_snapshot> Players = players();
        if (Players.empty()) { esp_wait("player discovery / registry scan"); return false; }
        const long long now_ms = diag::now_ms();

        // Per-object caches must not outlive the match: a new match reuses heap addresses.
        if (now_ms - g_last_purge_ms > 1000) {
            g_last_purge_ms = now_ms;
            std::unordered_set<uint64_t> live;
            for (const player_snapshot& p : Players) live.insert(p.object);
            purge_missing(g_names, live);
            purge_missing(g_dead_since, live);
            purge_missing(g_last_hp, live);
            purge_missing(player::g_transform_cache, live);
            purge_missing(weapon::g_players, live);
        }
        if (cfg::esp::name) refresh_names(Players, now_ms);

        const player_snapshot* Local = nullptr;
        for (const player_snapshot& p : Players) if (p.is_local()) { Local = &p; break; }
        if (!Local) { esp_wait("local player flag"); return false; }
        const uint64_t LocalPlayer = Local->object;
        const int LocalTeam = Local->team;
        if (LocalTeam != 1 && LocalTeam != 2) { esp_wait("local team validation"); return false; }
        const player::PlayerPose LocalPose = player::pose(*Local);
        const Vector3 LocalPosition = LocalPose.feet;
        if (!LocalPose.valid || !std::isfinite(LocalPosition.x) ||
            (LocalPosition.x == 0.f && LocalPosition.y == 0.f && LocalPosition.z == 0.f)) {
            esp_wait("local transform hierarchy");
            return false;
        }

        // Camera first, then the enemy poses, then the camera again (straddle check below).
        crash::stage("esp-camera");
        cam::main_chain chain{};
        std::vector<camera_candidate> Cameras = cam::resolve(LocalPlayer, chain);
        if (Cameras.empty()) { g_camera = 0; esp_wait("camera resolution (controller / bor / transform)"); return false; }

        // Enemies: other team. Their health is read in one scatter request.
        crash::stage("esp-poses");
        std::array<player_snapshot, kMaxEnemies> enemy_snap{};
        size_t ne = 0;
        for (const player_snapshot& p : Players) {
            if (p.object == LocalPlayer) continue;
            if (p.team == LocalTeam || (p.team != 1 && p.team != 2)) continue;
            if (ne < enemy_snap.size()) enemy_snap[ne++] = p;
        }
        std::array<player::HealthInfo, kMaxEnemies> health{};
        player::health_batch(enemy_snap.data(), ne, health.data());

        std::array<enemy_frame, kMaxEnemies> frame_enemies{};
        size_t nf = 0;
        int enemies = 0, alive = 0, positioned = 0, projected = 0;
        for (size_t i = 0; i < ne; ++i) {
            const player_snapshot& p = enemy_snap[i];
            ++enemies;
            const player::HealthInfo& Health = health[i];
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
                // Hide a dead body for a short grace period even if its stats object briefly
                // reports the pre-death HP again.
                if (now_ms - dead->second < 1500) continue;
                g_dead_since.erase(dead);
            }
            ++alive;

            enemy_frame e{};
            e.object = p.object;
            e.snap_index = int(i);
            e.hp = Health.hp;
            e.max_hp = Health.max_hp;
            if (cfg::esp::weapon) e.weapon = weapon::active_id(p);
            const player::PlayerPose Shape = player::pose(p);
            if (!Shape.valid) continue;
            e.position = Shape.feet;
            e.head = Shape.has_head ? Shape.head : Shape.feet + Vector3(0.f, 1.67f, 0.f);
            e.has_head = Shape.has_head;
            ++positioned;
            e.distance = calculate_distance(e.position, LocalPosition);
            if (e.distance > 500.f) continue;
            frame_enemies[nf++] = e;
        }

        // Straddle check: if the camera moved while the poses were read, the poses are read again,
        // so that box and camera describe the same game frame.
        if (!Cameras.empty() && Cameras.front().kind == camera_kind_main_native && nf > 0) {
            matrix view_after{}, proj_after{};
            if (cam::reread_lock(view_after, proj_after) &&
                std::memcmp(&view_after, &Cameras.front().view, sizeof(matrix)) != 0) {
                static long long straddle_log = 0;
                static unsigned straddles = 0;
                ++straddles;
                for (size_t i = 0; i < nf; ++i) {
                    const player::PlayerPose Shape = player::pose(enemy_snap[size_t(frame_enemies[i].snap_index)]);
                    if (!Shape.valid) continue;
                    frame_enemies[i].position = Shape.feet;
                    frame_enemies[i].head = Shape.has_head ? Shape.head : Shape.feet + Vector3(0.f, 1.67f, 0.f);
                    frame_enemies[i].has_head = Shape.has_head;
                }
                Cameras.front().view = view_after;
                Cameras.front().projection = proj_after;
                if (diag::every(straddle_log, 10000))
                    diag::log("CAMERA", "frame straddle re-reads so far: %u", straddles);
            }
        }

        // Camera selection. The controller camera whose bor is PlayerAnimationData.MainCamera is the
        // camera that rendered the frame; use it as is. Every other source is scored against the
        // live player coordinates so a UI/preview/arms camera cannot win.
        matrix ViewMatrix{}, ProjectionMatrix{};
        static int selected_kind_prev = camera_kind_unknown;
        uint64_t selected_camera = 0;
        int selected_kind = camera_kind_unknown;
        int selected_score = -1, selected_front = 0, selected_onscreen = 0;
        const bool has_targets = nf > 0;
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
                for (size_t k = 0; k < nf; ++k) {
                    const enemy_frame& Enemy = frame_enemies[k];
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
                // Source quality: real matrices beat synthetic ones, the PlayerAnimationData.MainCamera
                // state beats the vqn/yjp guesses.
                switch (Camera.kind) {
                    case camera_kind_kmc_native: score += 40; break;
                    case camera_kind_other_native: score += 25; break;
                    case camera_kind_main_state: score += 30; break;
                    case camera_kind_vqn_state: score += 10; break;
                    case camera_kind_ios_transform: score += 0; break;
                    case camera_kind_heap_camera: score += 5; break;
                    default: break;
                }
                // Strong hysteresis: the synthetic sources score within a few points of each other, and
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
            return false;
        }
        if (g_camera != selected_camera) {
            diag::log("CAMERA", "selected=0x%llx kind=%s score=%d front=%d onscreen=%d candidates=%zu",
                      (unsigned long long)selected_camera, camera_kind_name(selected_kind), selected_score,
                      selected_front, selected_onscreen, Cameras.size());
            STATUS_SET(camera, "camera: %s", camera_kind_name(selected_kind));
        }
        g_camera = selected_camera;
        selected_kind_prev = selected_kind;

        for (size_t k = 0; k < nf; ++k) {
            ImVec2 sh, sf;
            if (world_to_screen(frame_enemies[k].head, ViewMatrix, ProjectionMatrix, sh) &&
                world_to_screen(frame_enemies[k].position, ViewMatrix, ProjectionMatrix, sf))
                ++projected;
        }

        // PROBE (read-only, opt-in): while /data/local/tmp/bycmd_probe exists, dumps the szi bytes of
        // every enemy every 100 ms. A line is logged when the bytes change, and every 2 s regardless.
        {
            static long long probe_check = 0, probe_tick = 0, probe_dump = 0;
            static bool probe_on = false;
            static std::unordered_map<uint64_t, std::array<uint8_t, 16>> probe_last;
            if (diag::every(probe_check, 1000)) probe_on = access("/data/local/tmp/bycmd_probe", F_OK) == 0;
            if (probe_on && diag::every(probe_tick, 100)) {
                const bool periodic = diag::every(probe_dump, 2000);
                for (size_t k = 0; k < nf; ++k) {
                    const enemy_frame& Enemy = frame_enemies[k];
                    const uint64_t szi = rpm<uint64_t>(Enemy.object + 0x88);
                    std::array<uint8_t, 16> cur{};
                    if (!(obj_ptr(szi) && mem_read(szi + 0x10, cur.data(), cur.size()))) cur.fill(0xEE);
                    const auto it = probe_last.find(Enemy.object);
                    const bool changed = it == probe_last.end() || it->second != cur;
                    if (changed || periodic) {
                        char hex[3 * 16 + 1];
                        for (int b = 0; b < 16; ++b) std::snprintf(hex + b * 3, 4, "%02X ", cur[b]);
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

        // Publish. Positions are raw game-frame samples; the render thread interpolates them.
        out = frame_sample{};
        out.valid = true;
        out.view = ViewMatrix;
        out.projection = ProjectionMatrix;
        out.enemy_count = int(nf);
        for (size_t k = 0; k < nf; ++k) {
            const enemy_frame& e = frame_enemies[k];
            enemy_sample& s = out.enemies[k];
            s.object = e.object;
            s.hp = e.hp;
            s.max_hp = e.max_hp;
            s.weapon = e.weapon;
            s.feet = e.position;
            s.head = e.head;
            s.distance = e.distance;
            if (cfg::esp::name) {
                const auto found = g_names.find(e.object);
                if (found != g_names.end() && found->second.has_text) {
                    s.has_name = true;
                    std::snprintf(s.name, sizeof(s.name), "%s", found->second.text);
                }
            }
        }
        return true;
    }

    // Reader thread body: one tick per period, the sample is published under the mutex.
    void reader_loop() {
        unsigned long long seq = 0;
        unsigned ticks = 0;
        double sum_us = 0.0, max_us = 0.0;
        unsigned long long calls_at_window = g_remote_calls.load(std::memory_order_relaxed);
        long long window_ms = diag::now_ms();
        long long next_us = diag::now_us();
        int enemies_now = 0;
        while (g_reader_run.load()) {
            if (g_reset_request.exchange(false)) reset_reader_state();
            const long long t0 = diag::now_us();
            frame_sample frame{};
            const bool ok = read_tick(frame);
            const long long t1 = diag::now_us();
            {
                std::lock_guard<std::mutex> lock(g_sample_mutex);
                if (ok) {
                    frame.sample_us = t1;
                    frame.seq = ++seq;
                    g_sample = frame;
                } else {
                    g_sample.valid = false;
                }
            }
            if (ok) enemies_now = frame.enemy_count;
            const double tick_us = double(t1 - t0);
            ++ticks;
            sum_us += tick_us;
            if (tick_us > max_us) max_us = tick_us;

            const long long now_ms = diag::now_ms();
            if (now_ms - window_ms >= 5000 && ticks > 0) {
                const unsigned long long calls = g_remote_calls.load(std::memory_order_relaxed);
                diag::log("PERF", "ESP reader ticks=%u avg=%.2fms max=%.2fms remote_calls/tick=%.1f enemies=%d",
                          ticks, sum_us / ticks / 1000.0, max_us / 1000.0,
                          double(calls - calls_at_window) / ticks, enemies_now);
                ticks = 0;
                sum_us = 0.0;
                max_us = 0.0;
                calls_at_window = calls;
                window_ms = now_ms;
            }

            // Pace to the next tick in short slices, so stop() stays prompt.
            next_us += ok ? kReaderPeriodUs : kReaderIdleUs;
            const long long resync_now = diag::now_us();
            if (next_us < resync_now - 50000) next_us = resync_now;   // far behind (stall): resync
            while (g_reader_run.load()) {
                const long long rest = next_us - diag::now_us();
                if (rest <= 0) break;
                usleep(useconds_t(std::min<long long>(rest, 5000)));
            }
        }
    }

    // ---- render side (render thread only) --------------------------------------
    struct motion_sample { Vector3 feet{}; Vector3 head{}; long long t_us{0}; };
    struct motion_state {
        motion_sample prev{}, cur{};
        bool has_prev{false}, has_cur{false};
        double interval_us{8333.0};
        long long seen_us{0};
    };
    std::unordered_map<uint64_t, motion_state> g_motion;
    unsigned long long g_rendered_seq = 0;

    void motion_update(motion_state& m, const Vector3& feet, const Vector3& head, long long t_us) {
        m.seen_us = t_us;
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

    // Position one game-frame interval in the past, interpolated between the last two samples.
    void motion_render(const motion_state& m, long long now_us, Vector3& feet, Vector3& head) {
        feet = m.cur.feet;
        head = m.cur.head;
        if (!m.has_prev) return;
        const double span = double(m.cur.t_us - m.prev.t_us);
        if (span <= 0.0 || span > 150000.0) return;            // stale pair: snap
        if (vdist(m.prev.feet, m.cur.feet) > 4.f) return;      // teleport / respawn: snap
        const double delay = std::clamp(m.interval_us, 4000.0, 34000.0);
        double alpha = (double(now_us - m.prev.t_us) - delay) / span;
        alpha = std::clamp(alpha, 0.0, 1.0);
        const float a = float(alpha);
        feet = m.prev.feet + (m.cur.feet - m.prev.feet) * a;
        head = m.prev.head + (m.cur.head - m.prev.head) * a;
    }

    void render_stats(long long t0, long long t1) {
        static unsigned frames = 0;
        static double sum_us = 0.0, max_us = 0.0;
        static long long window_ms = 0;
        if (window_ms == 0) window_ms = diag::now_ms();
        ++frames;
        const double us = double(t1 - t0);
        sum_us += us;
        if (us > max_us) max_us = us;
        const long long now_ms = diag::now_ms();
        if (now_ms - window_ms >= 5000) {
            diag::log("PERF", "ESP render frames=%u avg=%.2fms max=%.2fms", frames, sum_us / frames / 1000.0, max_us / 1000.0);
            frames = 0;
            sum_us = 0.0;
            max_us = 0.0;
            window_ms = now_ms;
        }
    }

    void draw_frame() {
        if (!(g_sw >= 2.f) || !(g_sh >= 2.f)) return;
        frame_sample snap;
        {
            std::lock_guard<std::mutex> lock(g_sample_mutex);
            snap = g_sample;
        }
        if (!snap.valid) return;
        const long long now_us = diag::now_us();
        if (now_us - snap.sample_us > kStaleUs) return;
        crash::stage("esp-draw");

        if (snap.seq != g_rendered_seq) {
            g_rendered_seq = snap.seq;
            for (int i = 0; i < snap.enemy_count; ++i) {
                const enemy_sample& e = snap.enemies[i];
                motion_update(g_motion[e.object], e.feet, e.head, snap.sample_us);
            }
            for (auto it = g_motion.begin(); it != g_motion.end();) {
                if (snap.sample_us - it->second.seen_us > 2000000) it = g_motion.erase(it);
                else ++it;
            }
        }

        for (int i = 0; i < snap.enemy_count; ++i) {
            const enemy_sample& e = snap.enemies[i];
            Vector3 feet = e.feet;
            Vector3 head = e.head;
            if (cfg::esp::smooth) {
                const auto it = g_motion.find(e.object);
                if (it != g_motion.end()) motion_render(it->second, now_us, feet, head);
            }
            ImVec2 ScreenHead, ScreenFoot;
            const bool HeadVisible = world_to_screen(head, snap.view, snap.projection, ScreenHead);
            const bool FootVisible = world_to_screen(feet, snap.view, snap.projection, ScreenFoot);
            if (!HeadVisible || !FootVisible) continue;

            const float x1 = roundf(ScreenHead.x);
            const float y1 = roundf(fminf(ScreenHead.y, ScreenFoot.y));
            const float x2 = roundf(ScreenFoot.x);
            const float y2 = roundf(fmaxf(ScreenHead.y, ScreenFoot.y));
            const float bh = fabsf(y2 - y1);
            const float bw = roundf(bh * 0.21f);   // total width ~0.42 * height
            const float cx = roundf((x1 + x2) * 0.5f);
            const ImVec2 BoxMin(cx - bw, y1);
            const ImVec2 BoxMax(cx + bw, y2);

            const float inv_dist = 1.f / (e.distance + 0.1f);
            const float font_sz = std::clamp(400.f * inv_dist, 10.f, 18.f);
            // Distant players are drawn a little quieter, so the near ones read first.
            const float fade = std::clamp(1.2f - e.distance / 450.f, 0.45f, 1.f);

            if (cfg::esp::box) visuals::dbox(BoxMin, BoxMax, fade);
            if (cfg::esp::health) visuals::dhp(e.hp, e.max_hp, BoxMin, BoxMax, bh, fade);
            if (cfg::esp::name) visuals::dnick(e.has_name ? e.name : "Enemy", BoxMax, cx, font_sz, fade);
            if (cfg::esp::weapon) {
                const float y = BoxMax.y + 4.f + (cfg::esp::name && espFont ? font_sz + 4.f : 0.f);
                visuals::dweapon(e.weapon, cx, y, BoxMax.x - BoxMin.x, font_sz, fade);
            }
        }
    }
} // namespace

void visuals::start() {
    bool expected = false;
    if (!g_reader_run.compare_exchange_strong(expected, true)) return;
    g_reader = std::thread(reader_loop);
}

void visuals::stop() {
    if (!g_reader_run.exchange(false)) return;
    if (g_reader.joinable()) g_reader.join();
}

void visuals::request_reset() {
    g_reset_request.store(true);
}

void visuals::draw() {
    const long long t0 = diag::now_us();
    draw_frame();
    render_stats(t0, diag::now_us());
}

void visuals::dbox(const ImVec2& t, const ImVec2& b, float a) {
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    // 1 px lines centred on pixel centres: crisp, no shimmer from half-covered pixels.
    const float x1 = floorf(t.x) + 0.5f, y1 = floorf(t.y) + 0.5f;
    const float x2 = floorf(b.x) + 0.5f, y2 = floorf(b.y) + 0.5f;
    const ImU32 col = scaled_color(cfg::esp::box_col, a);
    const ImU32 halo = IM_COL32(0, 0, 0, int(150 * a));
    const float r = cfg::esp::box_rounding;
    if (cfg::esp::box_type == 0) {
        dl->AddRect(ImVec2(x1 - 1.f, y1 - 1.f), ImVec2(x2 + 1.f, y2 + 1.f), halo, r, 0, 3.f);
        dl->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), col, r, 0, 1.f);
        return;
    }
    // Corner brackets: the arm length follows the box, clamped so small and large boxes both read well.
    const float L = std::clamp(std::min(x2 - x1, y2 - y1) * 0.24f, 6.f, 48.f);
    auto bracket = [&](float cx, float cy, float dx, float dy) {
        dl->AddLine(ImVec2(cx, cy), ImVec2(cx + dx * L, cy), halo, 3.f);
        dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + dy * L), halo, 3.f);
        dl->AddLine(ImVec2(cx, cy), ImVec2(cx + dx * L, cy), col, 1.f);
        dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + dy * L), col, 1.f);
    };
    bracket(x1, y1, 1.f, 1.f);
    bracket(x2, y1, -1.f, 1.f);
    bracket(x1, y2, 1.f, -1.f);
    bracket(x2, y2, -1.f, -1.f);
}

void visuals::dhp(int hp, int max_hp, const ImVec2& t, const ImVec2& b, float box_h, float a) {
    if (a < 0.01f) return;
    (void)box_h;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (max_hp <= 0) max_hp = 100;
    hp = std::clamp(hp, 0, max_hp);
    const float pct = float(hp) / float(max_hp);
    const float top = floorf(t.y), bot = floorf(b.y);
    const float h = bot - top;
    if (h < 2.f) return;
    const float bx = floorf(t.x) - 6.f;   // bar: x in [bx, bx + 3]
    const ImVec2 track_min(bx - 1.f, top - 1.f), track_max(bx + 4.f, bot + 1.f);
    dl->AddRectFilled(track_min, track_max, IM_COL32(0, 0, 0, int(140 * a)), 2.f);
    dl->AddRect(track_min, track_max, IM_COL32(255, 255, 255, int(30 * a)), 2.f, 0, 1.f);

    // Fill from the bottom: health colour at the top, a darker tone towards the foot of the bar.
    const float fill_top = roundf(bot - h * pct);
    if (hp > 0 && bot - fill_top >= 1.f) {
        const ImU32 col_top = scaled_color(cfg::esp::health_col, a);
        const ImU32 col_bot = scaled_color(cfg::esp::health_col, a, 0.7f);
        dl->AddRectFilledMultiColor(ImVec2(bx, fill_top), ImVec2(bx + 3.f, bot), col_top, col_top, col_bot, col_bot);
    }
    // Quarter marks, so the level reads at a glance.
    for (int q = 1; q <= 3; ++q) {
        const float y = roundf(bot - h * (float(q) * 0.25f)) + 0.5f;
        dl->AddLine(ImVec2(bx - 1.f, y), ImVec2(bx + 4.f, y), IM_COL32(0, 0, 0, int(170 * a)), 1.f);
    }
    // The number sits left of the bar, only when the player is hurt and the bar is tall enough.
    if (hp < max_hp && espFont && h > 24.f) {
        char txt[8];
        std::snprintf(txt, sizeof(txt), "%d", hp);
        const float fs = 10.f;
        const ImVec2 ts = espFont->CalcTextSizeA(fs, FLT_MAX, 0.f, txt);
        const ImVec2 tp(roundf(bx - 3.f - ts.x), roundf(top + h * 0.5f - ts.y * 0.5f));
        draw_text_outlined(dl, espFont, fs, tp, IM_COL32(255, 255, 255, int(235 * a)), txt);
    }
}

void visuals::dnick(const char* name, const ImVec2& box_max, float cx, float sz, float a) {
    if (!espFont || !name || !*name || a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, name);
    const ImVec2 tp(roundf(cx - ts.x * 0.5f), roundf(box_max.y + 4.f));
    // Translucent plate behind the name: readable on bright ground, quiet on dark.
    dl->AddRectFilled(ImVec2(tp.x - 4.f, tp.y - 2.f), ImVec2(tp.x + ts.x + 4.f, tp.y + ts.y + 2.f),
                      IM_COL32(0, 0, 0, int(110 * a)), 4.f);
    draw_text_outlined(dl, espFont, sz, tp, scaled_color(cfg::esp::name_col, a), name);
}

// Weapon icon under the box: the atlas icon of this weapon id (weapon_icons.h), tinted by the ESP
// colour. The name when the weapon has no icon or the atlas did not upload.
void visuals::dweapon(int weapon_id, float cx, float y, float box_w, float sz, float a) {
    const char* label = weapon::name(weapon_id);
    if (!label || a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImU32 col = scaled_color(cfg::esp::box_col, a);
    const weapon_icon_rect* icon = nullptr;
    for (const weapon_icon_rect& r : weapon_icon_rects)
        if (r.id == weapon_id) { icon = &r; break; }
    if (icon && g_weapon_atlas_tex) {
        // Height follows the distance-scaled label size; the icon is shrunk so it never gets much
        // wider than the box.
        float h = std::clamp(sz * 1.6f, 16.f, 34.f);
        float w = h * float(icon->w) / float(icon->h);
        const float max_w = std::max(box_w * 1.2f, 36.f);
        if (w > max_w) { h *= max_w / w; w = max_w; }
        const ImVec2 p0(roundf(cx - w * 0.5f), roundf(y));
        const ImVec2 p1(roundf(cx + w * 0.5f), roundf(y + h));
        const ImVec2 uv0(float(icon->x) / float(weapon_icons_w), float(icon->y) / float(weapon_icons_h));
        const ImVec2 uv1(float(icon->x + icon->w) / float(weapon_icons_w), float(icon->y + icon->h) / float(weapon_icons_h));
        const ImU32 shadow = IM_COL32(0, 0, 0, int(160 * a));
        dl->AddImage(g_weapon_atlas_tex, ImVec2(p0.x + 1.f, p0.y + 1.f), ImVec2(p1.x + 1.f, p1.y + 1.f), uv0, uv1, shadow);
        dl->AddImage(g_weapon_atlas_tex, p0, p1, uv0, uv1, col);
        return;
    }
    if (!espFont) return;
    const ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, label);
    draw_text_outlined(dl, espFont, sz, ImVec2(roundf(cx - ts.x * 0.5f), roundf(y)), col, label);
}

void visuals::draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text) {
    if (!font || !dl || !text) return;
    const int a = int((color >> IM_COL32_A_SHIFT) & 0xFF);
    // One soft shadow and the text: two draws instead of three, the ESP issues many of them.
    dl->AddText(font, size, ImVec2(pos.x + 1.f, pos.y + 1.f), IM_COL32(0, 0, 0, a * 4 / 5), text);
    dl->AddText(font, size, pos, color, text);
}
