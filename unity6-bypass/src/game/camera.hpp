
// ================================================================================
// Gameplay camera resolution for Standoff 2 1.1.0.
//
// Chain (dump.cs 1.1.0):
//   local odq -> wwv.yvjn (cpa, +0x20) -> pjy[] -> chc (presentation component)
//     chc+0xA0  PlayerAnimationData
//       +0x100  bor MainCamera   (camera state of the world camera)
//       +0x90   bor ArmsCamera   (camera state of the first-person arms camera)
//
// The bor state is written by the simulation tick: no recoil kick, FOV fixed
// at 60 even when scoped, and it only moves at tick rate. It is therefore
// used as an *identity*, not as the camera: among all UnityEngine.Camera
// wrappers the background scanner finds, the one whose native worldToCamera
// matrix puts the camera at the bor position looking along the bor forward
// is the camera that renders the world. Its native matrices are what the GPU
// used for the frame (recoil, crouch, scope FOV, per-frame interpolation).
//
// kmc/wot (cgy controllers, +0x10 bor, +0x20 Camera) are only usable when
// their TypeInfo slots are initialised; observed lazy (never touched) in a
// live match, so they are a secondary source.
// ================================================================================

#pragma once

#include "game.hpp"
#include "player.hpp"

namespace cam {
    struct main_chain {
        uint64_t presentation{0};  // chc
        uint64_t animation{0};     // PlayerAnimationData
        uint64_t main_bor{0};
        uint64_t arms_bor{0};
        bool first_person{false};
    };
    struct bor_pose {
        Vector3 position{};
        Vector3 forward{};
        float fov{60.f};
        bool valid{false};
    };
    struct native_lock {
        uint64_t managed{0};
        uint64_t native{0};
        int view_off{-1};
        int proj_off{-1};
        int kind{camera_kind_unknown};
        int pid{-1};
        int mismatch_frames{0};
        long long locked_ms{0};
        long long fov_mismatch_since_ms{0};  // projection FOV != MainCamera FOV since
    };
    struct projection_info {
        float fov{0.f};
        float aspect{0.f};
        float near_plane{0.f};
        float far_plane{0.f};
    };
    inline native_lock g_lock;
    inline uint64_t g_chain_player = 0;
    inline uint64_t g_chain_presentation = 0;
    // The chc -> PlayerAnimationData -> bor pointers live as long as the
    // local player object; re-reading them (6 reads) every frame bought
    // nothing. The bor *contents* are still read fresh where they are used.
    inline main_chain g_chain_cache{};
    inline long long g_chain_cache_ms = 0;
    inline long long g_last_probe = 0;
    inline long long g_unlocked_since_ms = 0;  // first frame without a native lock, 0 = locked
    inline long long g_last_chain_log = 0;
    inline long long g_last_lock_log = 0;

    inline void reset() {
        g_lock = {};
        g_chain_player = 0;
        g_chain_presentation = 0;
        g_chain_cache = {};
        g_chain_cache_ms = 0;
        player::g_transform_cache.clear();
    }

    inline Vector3 position_from_view(const matrix& m) {
        const float* v = reinterpret_cast<const float*>(&m);
        return Vector3(
            -(v[0] * v[12] + v[1] * v[13] + v[2] * v[14]),
            -(v[4] * v[12] + v[5] * v[13] + v[6] * v[14]),
            -(v[8] * v[12] + v[9] * v[13] + v[10] * v[14]));
    }

    // Unity view space looks down -Z: world forward is minus the third row.
    inline Vector3 forward_from_view(const matrix& m) {
        const float* v = reinterpret_cast<const float*>(&m);
        return Vector3(-v[2], -v[6], -v[10]);
    }

    inline projection_info projection_params(const matrix& m) {
        const float* p = reinterpret_cast<const float*>(&m);
        projection_info info;
        if (std::fabs(p[5]) > .001f) info.fov = 2.f * std::atan(1.f / p[5]) * 57.29578f;
        if (std::fabs(p[0]) > .001f) info.aspect = p[5] / p[0];
        // OpenGL-style perspective: p[10] = -(f+n)/(f-n), p[14] = -2fn/(f-n).
        const float a = p[10], b = p[14];
        if (std::fabs(a - 1.f) > .0001f && std::fabs(a + 1.f) > .0001f) {
            info.near_plane = b / (a - 1.f);
            info.far_plane = b / (a + 1.f);
        }
        return info;
    }

    inline float distance(const Vector3& a, const Vector3& b) {
        const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    inline float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    inline bor_pose read_bor(uint64_t bor) {
        bor_pose pose;
        if (!obj_ptr(bor)) return pose;
        struct Raw { Vector3 position; Vector3 euler; };
        Raw raw{};
        if (!mem_read(bor + v100::bor_transform, &raw, sizeof(raw))) return pose;
        const float values[] = {raw.position.x, raw.position.y, raw.position.z, raw.euler.x, raw.euler.y, raw.euler.z};
        for (float value : values)
            if (!std::isfinite(value) || std::fabs(value) > 100000.f) return pose;
        if (std::fabs(raw.position.x) < .001f && std::fabs(raw.position.y) < .001f && std::fabs(raw.position.z) < .001f)
            return pose;
        Vector4 rotation = player::euler_to_quaternion(raw.euler);
        if (!player::normalize_quaternion(rotation)) return pose;
        pose.position = raw.position;
        pose.forward = player::rotate(rotation, Vector3(0.f, 0.f, 1.f));
        float fovs[2]{};
        if (mem_read(bor + v100::bor_float_a, fovs, sizeof(fovs))) {
            if (fovs[0] >= 20.f && fovs[0] <= 120.f) pose.fov = fovs[0];
            else if (fovs[1] >= 20.f && fovs[1] <= 120.f) pose.fov = fovs[1];
        }
        pose.valid = true;
        return pose;
    }

    // chc lookup is cached per local player and revalidated with one read.
    inline main_chain resolve_chain(uint64_t local_player) {
        const long long now = diag::now_ms();
        if (g_chain_player == local_player && g_chain_cache.animation && now - g_chain_cache_ms < 250)
            return g_chain_cache;
        main_chain chain{};
        const uint64_t chc_class = typeinfo(v100::presentation_typeinfo);
        const uint64_t animation_class = typeinfo(v100::animation_data_typeinfo);
        if (!chc_class || !animation_class || !local_player) return chain;
        if (g_chain_player != local_player || !exact_class(g_chain_presentation, chc_class)) {
            g_chain_player = local_player;
            g_chain_presentation = find_player_component(local_player, chc_class);
        }
        chain.presentation = g_chain_presentation;
        if (!chain.presentation) return chain;
        const uint64_t animation = rpm<uint64_t>(chain.presentation + v100::presentation_animation_data);
        if (!exact_class(animation, animation_class)) return chain;
        chain.animation = animation;
        chain.main_bor = rpm<uint64_t>(animation + v100::animation_main_camera);
        chain.arms_bor = rpm<uint64_t>(animation + v100::animation_arms_camera);
        chain.first_person = rpm<uint8_t>(animation + v100::animation_is_first_person) != 0;
        if (!obj_ptr(chain.main_bor)) chain.main_bor = 0;
        if (!obj_ptr(chain.arms_bor)) chain.arms_bor = 0;
        g_chain_cache = chain;
        g_chain_cache_ms = now;
        if (diag::every(g_last_chain_log, 5000))
            diag::log("CAMERA", "chain local=0x%llx chc=0x%llx animation=0x%llx mainBor=0x%llx armsBor=0x%llx firstPerson=%d",
                      (unsigned long long)local_player, (unsigned long long)chain.presentation,
                      (unsigned long long)chain.animation, (unsigned long long)chain.main_bor,
                      (unsigned long long)chain.arms_bor, chain.first_person ? 1 : 0);
        return chain;
    }

    inline bool managed_to_native(uint64_t managed, uint64_t camera_class, uint64_t& native) {
        if (!exact_class(managed, camera_class)) return false;
        native = rpm<uint64_t>(managed + v100::unity_cached_ptr);
        // Native objects are heap allocations; an odd m_CachedPtr
        // (0x...763 in log 20261114-193524) is a stale or reused wrapper.
        return obj_ptr(native) && (native & 7) == 0;
    }

    // Candidate managed Camera wrappers: scanner heap list plus, when the
    // kmc/wot slots are initialised, the controllers' +0x20 cameras.
    inline std::vector<uint64_t> managed_candidates(const scanner::results& snap, const main_chain& chain) {
        std::vector<uint64_t> out;
        const uint64_t kmc_class = typeinfo(v100::camera_main_typeinfo);
        const uint64_t wot_class = typeinfo(v100::camera_arms_typeinfo);
        for (uint64_t controller : snap.controllers) {
            const uint64_t klass = rpm<uint64_t>(controller);
            if ((kmc_class && klass == kmc_class) || (wot_class && klass == wot_class)) {
                const uint64_t state = rpm<uint64_t>(controller + v100::controller_state);
                if (chain.arms_bor && state == chain.arms_bor) continue;
                const uint64_t managed = rpm<uint64_t>(controller + v100::controller_camera);
                if (obj_ptr(managed)) out.push_back(managed);
            }
        }
        for (uint64_t managed : snap.cameras)
            if (std::find(out.begin(), out.end(), managed) == out.end()) out.push_back(managed);
        return out;
    }

    struct match {
        uint64_t managed{0};
        uint64_t native{0};
        int view_off{-1};
        int proj_off{-1};
        matrix view{};
        matrix projection{};
        projection_info info{};
        float dist{0.f};
        float align{0.f};
        float score{0.f};
    };

    // The world camera and the first-person arms camera sit at the same eye
    // pose; only the projection tells them apart. Measured on 1.1.0: the
    // MainCamera bor FOV follows the world camera (60, 20 when scoped) and
    // the ArmsCamera bor holds the arms camera's constant 40. Pure.
    //   rejects a projection more than 10 degrees off the main FOV or with the
    //   arms FOV while the main FOV differs, otherwise returns a penalty
    //   growing with the distance to the main FOV.
    inline bool projection_fov_ok(float proj_fov, float main_fov, float arms_fov, float& penalty) {
        penalty = std::fabs(proj_fov - main_fov) * 2.f;
        // A camera more than 10 degrees off the MainCamera FOV is not locked
        // at all (log 20261114-155757: a FOV 90 camera at the eye pose was
        // locked after a round start and dropped by the watchdog 600 ms later).
        // During a scope animation this only delays the lock by a 250 ms probe.
        if (std::fabs(proj_fov - main_fov) > 10.f) return false;
        const bool arms_known = arms_fov > 1.f && std::fabs(main_fov - arms_fov) > 2.f;
        if (arms_known && std::fabs(proj_fov - arms_fov) < 1.f && std::fabs(proj_fov - main_fov) > 1.f)
            return false;
        return true;
    }

    // Pick the view/projection windows of one native Camera that agree with
    // the bor pose. Returns false when no view window matches.
    inline bool match_native(uint64_t managed, uint64_t native, const bor_pose& pose, float arms_fov, match& out) {
        uint8_t buf[k_camera_block]{};
        const size_t size = read_camera_block(native, buf);
        if (!size) return false;
        std::vector<int> views, projs;
        scan_matrix_windows(buf, size, views, projs);
        if (views.empty() || projs.empty()) return false;
        int best_view = -1;
        float best_dist = 1.0e9f, best_align = -2.f;
        for (int off : views) {
            matrix m{};
            std::memcpy(&m, buf + off, sizeof(m));
            const float d = distance(position_from_view(m), pose.position);
            const float a = dot(forward_from_view(m), pose.forward);
            // Eye position: the bor sits at eye height, the camera may add
            // bob/recoil offsets below ~0.5 m. Direction: recoil kicks a few
            // degrees, so accept down to ~25 degrees of divergence.
            if (d > 1.5f || a < 0.9f) continue;
            if (d < best_dist) { best_dist = d; best_align = a; best_view = off; }
        }
        if (best_view < 0) return false;
        int best_proj = -1;
        float best_proj_score = -1.0e9f;
        projection_info best_info{};
        for (int off : projs) {
            matrix m{};
            std::memcpy(&m, buf + off, sizeof(m));
            const projection_info info = projection_params(m);
            if (info.fov < 5.f || info.fov > 150.f) continue;
            // A gameplay perspective has a positive aspect and 0 < near < far
            // (log 20261114-175419 matched aspect=-1.50 far=-0 at +0x5f0).
            if (!(info.aspect > 0.5f && info.near_plane > 0.f && info.far_plane > info.near_plane)) continue;
            float fov_penalty = 0.f;
            if (!projection_fov_ok(info.fov, pose.fov, arms_fov, fov_penalty)) continue; // arms camera
            float score = 0.f;
            if (info.far_plane > 50.f) score += 100.f;           // world camera, not UI
            if (info.near_plane > 0.f && info.near_plane < 1.f) score += 10.f;
            const float screen_aspect = g_sw / std::max(g_sh, 1.f);
            if (std::fabs(info.aspect - screen_aspect) < 0.15f) score += 20.f;
            // Strong: the bor FOV tracks the world camera, scope included.
            // The old 0.2 weight left 4 points between the world camera and
            // the arms camera (FOV 40), and a lock taken while the two FOVs
            // diverged (scope animation, round start) kept the arms camera:
            // boxes too small in scope, too large without it.
            score -= fov_penalty;
            if (off == int(v100::camera_projection)) score += 5.f;
            if (score > best_proj_score) { best_proj_score = score; best_proj = off; best_info = info; }
        }
        if (best_proj < 0) return false;
        out.managed = managed;
        out.native = native;
        out.view_off = best_view;
        out.proj_off = best_proj;
        std::memcpy(&out.view, buf + best_view, sizeof(matrix));
        std::memcpy(&out.projection, buf + best_proj, sizeof(matrix));
        out.info = best_info;
        out.dist = best_dist;
        out.align = best_align;
        out.score = best_proj_score - best_dist * 20.f + best_align * 10.f;
        return true;
    }

    inline uint64_t g_lock_camera_class = 0;
    inline long long g_last_bor_check = 0;
    // After a match change the locked camera may belong to the old scene:
    // for a few seconds it is checked against the new MainCamera state every
    // frame and dropped on the first mismatch instead of after 2 s.
    inline long long g_strict_until_ms = 0;
    inline void on_new_match() {
        g_strict_until_ms = diag::now_ms() + 3000;
        g_last_probe = 0;
    }

    // Re-read the locked matrices only (2 reads). Used by the straddle check
    // in visuals: if the camera advanced while player poses were being read,
    // the poses are re-read so box and camera come from the same game frame.
    inline bool reread_lock(matrix& view, matrix& projection) {
        if (!g_lock.managed || g_lock.pid != proc::pid) return false;
        return read_matrix_pair(g_lock.native, g_lock.view_off, g_lock.proj_off, view, projection);
    }

    // Re-read the locked matrices: one scatter read per frame. The wrapper
    // (klass + m_CachedPtr, 2 reads) is re-validated every 250 ms and the
    // matrices pass the view/projection sanity check every frame, so a
    // destroyed camera is dropped by whichever notices first. bor divergence
    // check every 250 ms (every frame for 3 s after a new match).
    inline bool refresh_lock(const main_chain& chain, camera_candidate& candidate) {
        if (g_lock.pid != proc::pid) g_lock = {};
        if (!g_lock.managed) return false;
        const bool strict = diag::now_ms() < g_strict_until_ms;
        // Wrapper klass, m_CachedPtr and both matrices in one call, every
        // frame: a destroyed or recycled Camera is dropped on the frame it
        // happens instead of drawing through foreign matrices.
        uint64_t klass = 0, native = 0;
        const read_part parts[] = {
            {g_lock.managed, &klass, sizeof(klass)},
            {g_lock.managed + v100::unity_cached_ptr, &native, sizeof(native)},
            {g_lock.native + uint64_t(g_lock.view_off), &candidate.view, sizeof(candidate.view)},
            {g_lock.native + uint64_t(g_lock.proj_off), &candidate.projection, sizeof(candidate.projection)},
        };
        const char* why = nullptr;
        if (!mem_read_scatter(parts, 4)) why = "read failed";
        else if (!g_lock_camera_class || klass != g_lock_camera_class) why = "wrapper klass changed";
        else if (native != g_lock.native) why = "m_CachedPtr changed";
        else if (!matrix_window_sane(reinterpret_cast<const float*>(&candidate.view), true) ||
                 !matrix_window_sane(reinterpret_cast<const float*>(&candidate.projection), false)) why = "matrices not sane";
        if (why) {
            diag::log("CAMERA", "camera lock lost managed=0x%llx native=0x%llx (%s)",
                      (unsigned long long)g_lock.managed, (unsigned long long)g_lock.native, why);
            g_lock = {};
            g_last_probe = 0;   // try to re-acquire on this very frame
            return false;
        }
        // A camera 3 m away from the MainCamera state is not the gameplay
        // camera (the bor lags at most a tick, ~0.2 m at running speed).
        // Checked every 100 ms and dropped after 3 checks in a row (~300 ms);
        // the old 8 checks at 250 ms drew through a wrong camera for 2 s.
        if (strict || diag::every(g_last_bor_check, 100)) {
            const bor_pose pose = read_bor(chain.main_bor);
            if (pose.valid) {
                const float d = distance(position_from_view(candidate.view), pose.position);
                if (d > 3.0f) {
                    if (++g_lock.mismatch_frames >= (strict ? 1 : 3)) {
                        diag::log("CAMERA", "camera lock dropped: native camera %.1f m away from MainCamera bor for %d checks",
                                  d, g_lock.mismatch_frames);
                        g_lock = {};
                        g_last_probe = 0;
                        return false;
                    }
                } else {
                    g_lock.mismatch_frames = 0;
                }
                // The gameplay camera's FOV follows the MainCamera bor FOV
                // (60 -> 20 in scope on 1.1.0). A locked projection that
                // disagrees for 600 ms (longer than a scope animation) is
                // another camera, e.g. the arms camera with its fixed 40.
                const float locked_fov = projection_params(candidate.projection).fov;
                const long long now_ms = diag::now_ms();
                if (std::fabs(locked_fov - pose.fov) > 3.f) {
                    if (!g_lock.fov_mismatch_since_ms) g_lock.fov_mismatch_since_ms = now_ms;
                    else if (now_ms - g_lock.fov_mismatch_since_ms >= 600) {
                        diag::log("CAMERA", "camera lock dropped: projection FOV %.1f != MainCamera FOV %.1f for %lld ms (not the world camera)",
                                  locked_fov, pose.fov, now_ms - g_lock.fov_mismatch_since_ms);
                        g_lock = {};
                        g_last_probe = 0;
                        return false;
                    }
                } else {
                    g_lock.fov_mismatch_since_ms = 0;
                }
                if (diag::every(g_last_lock_log, 5000)) {
                    const projection_info info = projection_params(candidate.projection);
                    diag::log("CAMERA", "locked native=0x%llx fov=%.1f aspect=%.2f near=%.2f far=%.0f borFov=%.1f borDist=%.2f",
                              (unsigned long long)native, info.fov, info.aspect, info.near_plane, info.far_plane,
                              pose.fov, d);
                }
            }
        }
        candidate.address = native;
        candidate.view_off = g_lock.view_off;
        candidate.proj_off = g_lock.proj_off;
        candidate.kind = g_lock.kind;
        return true;
    }

    // Probe every candidate wrapper against the bor pose (throttled).
    inline bool acquire_lock(const main_chain& chain, const bor_pose& pose, camera_candidate& candidate) {
        if (!pose.valid) return false;
        const long long now = diag::now_ms();
        if (now - g_last_probe < 250) return false;
        g_last_probe = now;
        const uint64_t camera_class = typeinfo(v100::camera_typeinfo);
        if (!camera_class) return false;
        const scanner::results snap = scanner::snapshot();
        if (snap.pid != proc::pid || snap.lib != proc::lib) return false;
        const bor_pose arms = read_bor(chain.arms_bor);
        const float arms_fov = arms.valid ? arms.fov : 0.f;
        const std::vector<uint64_t> managed_list = managed_candidates(snap, chain);
        match best{};
        bool have_best = false;
        int probed = 0, matched = 0;
        static long long last_summary = 0;
        const bool detail = diag::every(last_summary, 5000);
        for (uint64_t managed : managed_list) {
            uint64_t native = 0;
            if (!managed_to_native(managed, camera_class, native)) continue;
            ++probed;
            match m{};
            if (!match_native(managed, native, pose, arms_fov, m)) {
                if (detail && probed <= 24) {
                    // Why did this wrapper not match: dump its first view window pose.
                    uint8_t buf[k_camera_block]{};
                    const size_t size = read_camera_block(native, buf);
                    std::vector<int> views, projs;
                    if (size) scan_matrix_windows(buf, size, views, projs);
                    Vector3 p{}, f{};
                    if (!views.empty()) {
                        matrix v{};
                        std::memcpy(&v, buf + views.front(), sizeof(v));
                        p = position_from_view(v);
                        f = forward_from_view(v);
                    }
                    diag::log("CAMERA", "wrapper=0x%llx native=0x%llx block=0x%zx views=%zu projs=%zu first=+0x%x pos=(%.2f %.2f %.2f) fwd=(%.2f %.2f %.2f)",
                              (unsigned long long)managed, (unsigned long long)native, size, views.size(), projs.size(),
                              views.empty() ? -1 : views.front(), p.x, p.y, p.z, f.x, f.y, f.z);
                }
                continue;
            }
            ++matched;
            diag::log("CAMERA", "match managed=0x%llx native=0x%llx view=+0x%x proj=+0x%x dist=%.2f align=%.3f fov=%.1f aspect=%.2f near=%.3f far=%.0f score=%.1f",
                      (unsigned long long)managed, (unsigned long long)native, m.view_off, m.proj_off,
                      m.dist, m.align, m.info.fov, m.info.aspect, m.info.near_plane, m.info.far_plane, m.score);
            if (!have_best || m.score > best.score) { best = m; have_best = true; }
        }
        if (!have_best) {
            if (detail)
                diag::log("CAMERA", "no native camera matches MainCamera bor pos=(%.2f %.2f %.2f) fwd=(%.2f %.2f %.2f): wrappers=%zu probed=%d (scanner pass=%u cameras=%zu controllers=%zu)",
                          pose.position.x, pose.position.y, pose.position.z, pose.forward.x, pose.forward.y, pose.forward.z,
                          managed_list.size(), probed, snap.passes, snap.cameras.size(), snap.controllers.size());
            return false;
        }
        g_lock = {best.managed, best.native, best.view_off, best.proj_off, camera_kind_main_native, proc::pid, 0, now};
        g_lock_camera_class = camera_class;
        confirm_local_flag(best.dist);
        diag::log("CAMERA", "camera locked managed=0x%llx native=0x%llx view=+0x%x proj=+0x%x fov=%.1f far=%.0f (matched=%d of %d)",
                  (unsigned long long)best.managed, (unsigned long long)best.native, best.view_off, best.proj_off,
                  best.info.fov, best.info.far_plane, matched, probed);
        candidate.address = best.native;
        candidate.view = best.view;
        candidate.projection = best.projection;
        candidate.view_off = best.view_off;
        candidate.proj_off = best.proj_off;
        candidate.kind = camera_kind_main_native;
        return true;
    }

    // Full candidate list, best first.
    inline std::vector<camera_candidate> resolve(uint64_t local_player, main_chain& chain) {
        std::vector<camera_candidate> out;
        chain = resolve_chain(local_player);

        camera_candidate native{};
        if (refresh_lock(chain, native)) {
            g_unlocked_since_ms = 0;
            scanner::g_want_more = false;
            out.push_back(native);
            return out;
        }
        const bor_pose pose = read_bor(chain.main_bor);
        if (acquire_lock(chain, pose, native)) {
            g_unlocked_since_ms = 0;
            scanner::g_want_more = false;
            out.push_back(native);
            return out;
        }
        // A dropped lock is re-acquired from the camera list the scanner
        // already published (round start, scope, recoil). Only when that keeps
        // failing for 1.5 s is the list stale (map change, recreated camera)
        // and worth a heap pass.
        const long long now_ms = diag::now_ms();
        if (!g_unlocked_since_ms) g_unlocked_since_ms = now_ms;
        if (now_ms - g_unlocked_since_ms >= 1500) {
            scanner::g_want_more = true;
            static long long last_rescan_request = 0;
            if (diag::every(last_rescan_request, 10000)) {
                diag::log("CAMERA", "no native camera for %lld ms: requesting a heap pass", now_ms - g_unlocked_since_ms);
                scanner::request_rescan();
            }
        }

        camera_candidate synthetic{};
        if (chain.main_bor && player::synthetic_from_bor(chain.main_bor, camera_kind_main_state, synthetic))
            out.push_back(synthetic);
        const std::vector<camera_candidate> fallback = player::camera_fallback_candidates(local_player);
        // vqn holds both bors; the ArmsCamera one (fixed FOV 40) projects the
        // first-person arms, not the world.
        for (const camera_candidate& c : fallback)
            if (!(chain.arms_bor && c.kind == camera_kind_vqn_state && c.address == chain.arms_bor)) out.push_back(c);
        return out;
    }
}
