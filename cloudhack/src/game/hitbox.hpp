// Player hitboxes: bis.acis (HitBoxCollider[]) -> bone transform + native Box/CapsuleCollider shape,
// posed every frame from the Unity TransformHierarchy.
//
// Every offset comes from v100:: (game.hpp): managed fields from dump.cs 1.1.0, native ones from the
// *_Injected icall thunks of libunity.so 1.1.0.
//
// This runs inside the ESP frame, next to the game's own memory, so it follows hard rules:
//  * a frame costs one scatter read per player (only the hierarchy nodes the hitboxes hang on),
//    never a whole hierarchy block;
//  * resolving a player is batched (one scatter per pointer level) and done once, not per frame;
//  * a failure backs off 1 s, 2 s, 4 s, 8 s before the next resolve, nothing is retried every frame;
//  * the module only uses the Transform layout the player poses already established and never
//    probes, so it cannot corrupt the ESP's own pose reads;
//  * a per-frame time budget and a global circuit breaker cap what it can cost.

#pragma once

#include "game.hpp"
#include "player.hpp"
#include "../other/vector3.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace hitbox {
    enum class Shape : uint8_t { Box, Capsule };

    // Collider as stored by Unity: everything in the owner transform's local space.
    struct LocalShape {
        Shape shape{Shape::Box};
        Vector3 center{};
        Vector3 size{};        // Box: full extents
        float radius{0.f};     // Capsule
        float height{0.f};     // Capsule: tip to tip, includes both caps
        int direction{1};      // Capsule: 0 = X, 1 = Y, 2 = Z
    };

    struct WorldPose {
        Vector3 position{};
        Vector4 rotation{0.f, 0.f, 0.f, 1.f};
        Vector3 scale{1.f, 1.f, 1.f};
    };

    struct WorldHitbox {
        int bone{-1};          // UnityEngine.HumanBodyBones
        Shape shape{Shape::Box};
        Vector3 center{};      // world space
        Vector4 rotation{0.f, 0.f, 0.f, 1.f};
        Vector3 half{};        // Box: half extents along the local axes, scale applied
        float radius{0.f};     // Capsule, scale applied
        float half_segment{0.f}; // Capsule: half distance between the two cap centres
        int axis{1};           // Capsule axis in local space
    };

    inline constexpr int kHeadBone = 10;          // HumanBodyBones.Head
    inline constexpr int kMaxBone = 55;           // HumanBodyBones.LastBone
    inline constexpr size_t kMaxHitboxes = 64;
    inline constexpr float kAnchorRadius = 4.f;   // a hitbox further than this from its player is not his
    inline constexpr long long kFrameBudgetUs = 2500;  // hitbox work per ESP frame, all players together
    inline constexpr long long kBuildBudgetUs = 1200;  // no new resolve once this much is spent
    inline constexpr long long kBackoffBaseUs = 500000;
    inline constexpr long long kRefreshUs = 10000000;  // identity re-check of a resolved player
    inline constexpr int kBreakerFailures = 40;        // failures in 10 s that pause the module for 30 s

    static_assert(sizeof(Vector3) == 12, "Vector3 is read from remote memory as 3 floats");
    static_assert(v100::native_box_center == v100::native_capsule_radius &&
                  v100::native_capsule_height == v100::native_box_center + 4 &&
                  v100::native_capsule_direction == v100::native_box_center + 8 &&
                  v100::native_box_size == v100::native_capsule_center &&
                  v100::native_box_size == v100::native_box_center + 12,
                  "Box and Capsule collider fields are read as one 0x18 block");

    // ---- pure math -------------------------------------------------------
    inline bool finite3(const Vector3& v) noexcept {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    // chain[0] = the transform itself, chain[len-1] = hierarchy root.
    // Same composition as player::hierarchy_position, plus rotation and lossy scale.
    inline bool compose_chain(const player::TransformEntry* chain, int len, WorldPose& out) noexcept {
        if (len < 1 || !player::entry_valid(chain[0])) return false;
        Vector3 position(chain[0].position.x, chain[0].position.y, chain[0].position.z);
        Vector4 rotation = chain[0].rotation;
        Vector3 scale(chain[0].scale.x, chain[0].scale.y, chain[0].scale.z);
        for (int i = 1; i < len; ++i) {
            const player::TransformEntry& e = chain[i];
            if (!player::entry_valid(e)) return false;
            position = player::rotate(e.rotation, position * Vector3(e.scale.x, e.scale.y, e.scale.z)) +
                       Vector3(e.position.x, e.position.y, e.position.z);
            rotation = player::quaternion_multiply(e.rotation, rotation);
            scale = scale * Vector3(e.scale.x, e.scale.y, e.scale.z);
        }
        if (!player::normalize_quaternion(rotation) || !finite3(position) || !finite3(scale)) return false;
        if (std::fabs(position.x) > 100000.f || std::fabs(position.y) > 100000.f ||
            std::fabs(position.z) > 100000.f) return false;
        out = {position, rotation, scale};
        return true;
    }

    inline bool local_shape_valid(const LocalShape& s) noexcept {
        auto ok = [](float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; };
        if (!finite3(s.center) || std::fabs(s.center.x) > 5.f || std::fabs(s.center.y) > 5.f ||
            std::fabs(s.center.z) > 5.f) return false;
        if (s.shape == Shape::Box)
            return ok(s.size.x, 0.001f, 5.f) && ok(s.size.y, 0.001f, 5.f) && ok(s.size.z, 0.001f, 5.f);
        return ok(s.radius, 0.001f, 2.f) && ok(s.height, 0.001f, 5.f) && s.direction >= 0 && s.direction <= 2;
    }

    // kind: 1 = BoxCollider, 2 = CapsuleCollider. block = the 0x18 bytes at native collider + 0x60:
    //   Box:     center@0x00 size@0x0C
    //   Capsule: radius@0x00 height@0x04 direction@0x08 center@0x0C
    inline bool parse_shape(int kind, const uint8_t* block, LocalShape& out) noexcept {
        LocalShape s{};
        if (kind == 1) {
            s.shape = Shape::Box;
            std::memcpy(&s.center, block, sizeof(Vector3));
            std::memcpy(&s.size, block + 0x0C, sizeof(Vector3));
        } else if (kind == 2) {
            s.shape = Shape::Capsule;
            std::memcpy(&s.radius, block, 4);
            std::memcpy(&s.height, block + 4, 4);
            std::memcpy(&s.direction, block + 8, 4);
            std::memcpy(&s.center, block + 0x0C, sizeof(Vector3));
        } else {
            return false;
        }
        if (!local_shape_valid(s)) return false;
        out = s;
        return true;
    }

    // Range gate for reading/drawing a player's hitboxes. A non-finite or negative
    // distance (failed pose) is never in range.
    inline bool within_range(float distance, float range) noexcept {
        return std::isfinite(distance) && distance >= 0.f && distance <= range;
    }

    inline float distance3(const Vector3& a, const Vector3& b) noexcept {
        const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    inline float axis_of(const Vector3& v, int axis) noexcept { return axis == 0 ? v.x : axis == 1 ? v.y : v.z; }

    inline bool to_world(const LocalShape& s, const WorldPose& pose, int bone, WorldHitbox& out) noexcept {
        WorldHitbox h{};
        h.bone = bone;
        h.shape = s.shape;
        h.rotation = pose.rotation;
        h.center = pose.position + player::rotate(pose.rotation, s.center * pose.scale);
        const Vector3 abs_scale(std::fabs(pose.scale.x), std::fabs(pose.scale.y), std::fabs(pose.scale.z));
        if (s.shape == Shape::Box) {
            h.half = Vector3(s.size.x * abs_scale.x, s.size.y * abs_scale.y, s.size.z * abs_scale.z) * 0.5f;
        } else {
            // Unity: height follows the scale along the axis, radius the larger of the other two.
            const int a = s.direction, b = (a + 1) % 3, c = (a + 2) % 3;
            h.axis = a;
            h.radius = s.radius * std::max(axis_of(abs_scale, b), axis_of(abs_scale, c));
            const float height = s.height * axis_of(abs_scale, a);
            h.half_segment = std::max(0.f, height * 0.5f - h.radius);
        }
        if (!finite3(h.center)) return false;
        out = h;
        return true;
    }

    inline void box_corners(const WorldHitbox& h, Vector3 out[8]) noexcept {
        int n = 0;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sz = -1; sz <= 1; sz += 2)
                    out[n++] = h.center + player::rotate(h.rotation,
                        Vector3(float(sx) * h.half.x, float(sy) * h.half.y, float(sz) * h.half.z));
    }

    inline void capsule_ends(const WorldHitbox& h, Vector3& a, Vector3& b) noexcept {
        Vector3 axis(h.axis == 0 ? 1.f : 0.f, h.axis == 1 ? 1.f : 0.f, h.axis == 2 ? 1.f : 0.f);
        const Vector3 dir = player::rotate(h.rotation, axis) * h.half_segment;
        a = h.center + dir;
        b = h.center - dir;
    }

    // ---- remote reads ----------------------------------------------------
    // Reads `size` bytes at bases[i] + offset into out + i * size, 64 parts per call.
    // ok[i] says whether element i was read; a base that is not a heap pointer is skipped.
    inline void gather(const uint64_t* bases, size_t count, uint64_t offset, size_t size,
                       uint8_t* out, uint8_t* ok) noexcept {
        for (size_t first = 0; first < count; first += 64) {
            const size_t n = std::min<size_t>(64, count - first);
            read_part parts[64];
            bool part_ok[64];
            size_t index[64];
            size_t m = 0;
            for (size_t i = 0; i < n; ++i) {
                ok[first + i] = 0;
                if (!obj_ptr(bases[first + i])) continue;
                parts[m] = {bases[first + i] + offset, out + (first + i) * size, size};
                index[m++] = first + i;
            }
            if (!m) continue;
            mem_read_scatter(parts, m, part_ok);
            for (size_t k = 0; k < m; ++k) ok[index[k]] = part_ok[k] ? 1 : 0;
        }
    }

    // ---- per-player cache ------------------------------------------------
    struct group {                  // one TransformHierarchy
        uint64_t data{0};           // TransformHierarchy*
        uint64_t entries{0};        // localTransforms
        std::vector<int> nodes;     // node indices read every frame (sorted, unique)
        size_t offset{0};           // first slot of this group in the scratch buffer
    };
    struct entry {
        int bone{-1};
        LocalShape local{};
        int group{0};
        int chain_len{0};
        int slot[player::hierarchy_ref::kMaxChain]{};   // group slot of chain[i]: self first, root last
    };
    struct model_cache {
        uint64_t model{0};
        uint64_t array{0};
        std::vector<group> groups;
        std::vector<entry> entries;
        size_t slots{0};
        long long next_try_us{0};   // no (re)resolve before this
        long long refresh_us{0};    // identity re-check due
        int failures{0};
        int good_frames{0};
        bool valid{false};
    };
    struct stats_t {
        unsigned reads{0}, builds{0}, fails{0}, budget_skips{0};
        long long read_us_sum{0}, read_us_max{0}, build_us_max{0};
    };
    inline std::unordered_map<uint64_t, model_cache> g_cache;
    inline stats_t g_stats;
    inline long long g_spent_us = 0;          // hitbox time spent in the current frame
    inline int g_builds_left = 0;
    inline long long g_disabled_until_us = 0;
    inline long long g_window_start_us = 0;
    inline int g_window_fails = 0;
    inline long long g_stats_log_ms = 0;

    // Once per ESP frame, only while hitboxes are enabled.
    inline void begin_frame() noexcept {
        g_spent_us = 0;
        g_builds_left = 1;
        if (diag::every(g_stats_log_ms, 5000) && g_stats.reads) {
            int valid = 0;
            for (const auto& kv : g_cache) valid += kv.second.valid ? 1 : 0;
            diag::log("HITBOX", "stats players=%zu valid=%d reads=%u avg_us=%lld max_us=%lld builds=%u build_max_us=%lld fails=%u budget_skips=%u",
                      g_cache.size(), valid, g_stats.reads, g_stats.read_us_sum / std::max(1u, g_stats.reads),
                      g_stats.read_us_max, g_stats.builds, g_stats.build_us_max, g_stats.fails, g_stats.budget_skips);
            g_stats = {};
        }
    }
    inline void forget(uint64_t object) { g_cache.erase(object); }

    struct spend {
        long long t0{diag::now_us()};
        ~spend() { g_spent_us += diag::now_us() - t0; }
    };

    // Failure of a player: 1 s, 2 s, 4 s, 8 s (cap) before the next resolve. A storm of
    // failures anywhere pauses the whole module for 30 s.
    inline void fail(model_cache& c, long long now) noexcept {
        c.valid = false;
        c.good_frames = 0;
        c.failures = std::min(c.failures + 1, 4);
        c.next_try_us = now + (kBackoffBaseUs << c.failures);
        ++g_stats.fails;
        if (now - g_window_start_us > 10000000) { g_window_start_us = now; g_window_fails = 0; }
        if (++g_window_fails >= kBreakerFailures) {
            g_disabled_until_us = now + 30000000;
            g_window_fails = 0;
            diag::log("WARN", "hitbox: %d failures in 10 s, paused for 30 s", kBreakerFailures);
        }
    }

    inline uint64_t find_model(uint64_t player_object) noexcept {
        const uint64_t model_class = typeinfo(v100::model_typeinfo);
        const uint64_t container = rpm<uint64_t>(player_object + v100::player_parts);
        if (!model_class || !obj_ptr(container)) return 0;
        const uint64_t array = rpm<uint64_t>(container + v100::parts_array);
        const uint64_t length = rpm<uint64_t>(array + v100::array_length);
        if (!obj_ptr(array) || length == 0 || length > 64) return 0;
        for (uint64_t i = 0; i < length; ++i) {
            const uint64_t part = rpm<uint64_t>(array + v100::array_data + i * 8);
            if (obj_ptr(part) && exact_class(part, model_class)) return part;
        }
        return 0;
    }

    // 0 = unknown, 1 = BoxCollider, 2 = CapsuleCollider. The TypeInfo slots are used when the
    // game filled them; otherwise the il2cpp class name decides.
    inline int collider_kind(uint64_t klass, uint64_t box_class, uint64_t capsule_class) noexcept {
        if (!obj_ptr(klass)) return 0;
        if (box_class && klass == box_class) return 1;
        if (capsule_class && klass == capsule_class) return 2;
        std::string name, ns;
        if (il2cpp::klass_identity(klass, name, ns) && ns == "UnityEngine") {
            if (name == "BoxCollider") return 1;
            if (name == "CapsuleCollider") return 2;
        }
        return 0;
    }

    // Resolves one player: HitBoxCollider[] -> bone, collider shape, owner Transform -> hierarchy nodes.
    // Batched: one scatter per pointer level instead of a read per field per hitbox.
    inline bool build(uint64_t player_object, const Vector3& anchor, model_cache& c) noexcept {
        c.groups.clear();
        c.entries.clear();
        c.slots = 0;
        c.valid = false;
        const long long t0 = diag::now_us();
        const player::TransformLayout layout = player::g_transform_layout;
        // The pose reads decide the layout; hitboxes only follow it. During the first frames it is
        // not known yet, which is normal and not logged.
        if (layout.pid != proc::pid || layout.data_offset < 0) return false;

        c.model = find_model(player_object);
        c.array = c.model ? rpm<uint64_t>(c.model + v100::model_hitboxes) : 0;
        const uint64_t length = obj_ptr(c.array) ? rpm<uint64_t>(c.array + v100::array_length) : 0;
        if (!c.model || length == 0 || length > kMaxHitboxes) {
            diag::log("WARN", "hitbox: player=0x%llx model=0x%llx array=0x%llx length=%llu: no usable HitBoxCollider[]",
                      (unsigned long long)player_object, (unsigned long long)c.model,
                      (unsigned long long)c.array, (unsigned long long)length);
            return false;
        }
        const size_t n = size_t(length);
        uint64_t items[kMaxHitboxes]{};
        if (!mem_read(c.array + v100::array_data, items, n * 8)) return false;

        struct temp {
            bool alive{false};
            int bone{-1};
            int kind{0};
            uint64_t component{0}, collider{0}, native_collider{0}, game_object{0}, components{0}, transform{0};
            LocalShape shape{};
        };
        temp t[kMaxHitboxes];
        int rej_read = 0, rej_class = 0, rej_shape = 0, rej_transform = 0, rej_far = 0;

        // Level 1: HitBoxCollider objects: klass, m_CachedPtr, bone, managed Collider.
        constexpr size_t kHead = 0x30;
        static_assert(v100::hitbox_collider + 8 <= kHead && v100::hitbox_bone + 4 <= kHead, "header block covers the fields");
        uint8_t head[kMaxHitboxes][kHead];
        uint8_t ok[kMaxHitboxes];
        gather(items, n, 0, kHead, &head[0][0], ok);
        uint64_t first_class = 0;
        for (size_t i = 0; i < n; ++i) {
            if (!ok[i]) { ++rej_read; continue; }
            uint64_t klass = 0, component = 0, collider = 0;
            int bone = -1;
            std::memcpy(&klass, head[i], 8);
            std::memcpy(&component, head[i] + v100::unity_cached_ptr, 8);
            std::memcpy(&bone, head[i] + v100::hitbox_bone, 4);
            std::memcpy(&collider, head[i] + v100::hitbox_collider, 8);
            if (!first_class) first_class = klass;
            if (klass != first_class || bone < 0 || bone > kMaxBone || !obj_ptr(collider) || !obj_ptr(component)) {
                ++rej_class;
                continue;
            }
            t[i].alive = true;
            t[i].bone = bone;
            t[i].component = component;
            t[i].collider = collider;
        }
        {
            std::string name, ns;
            if (first_class && il2cpp::klass_identity(first_class, name, ns) && name != "HitBoxCollider") {
                diag::log("WARN", "hitbox: player=0x%llx bis+0x%llx holds %s, not HitBoxCollider[]",
                          (unsigned long long)player_object, (unsigned long long)v100::model_hitboxes, name.c_str());
                return false;
            }
        }

        // Level 2: managed Collider: klass, m_CachedPtr.
        uint64_t bases[kMaxHitboxes];
        auto collect = [&](uint64_t temp::*field) {
            for (size_t i = 0; i < n; ++i) bases[i] = t[i].alive ? t[i].*field : 0;
        };
        uint8_t block[kMaxHitboxes][0x18];
        collect(&temp::collider);
        gather(bases, n, 0, 0x18, &block[0][0], ok);
        const uint64_t box_class = typeinfo(v100::boxcollider_typeinfo);
        const uint64_t capsule_class = typeinfo(v100::capsulecollider_typeinfo);
        for (size_t i = 0; i < n; ++i) {
            if (!t[i].alive) continue;
            if (!ok[i]) { t[i].alive = false; ++rej_read; continue; }
            uint64_t klass = 0, native = 0;
            std::memcpy(&klass, block[i], 8);
            std::memcpy(&native, block[i] + v100::unity_cached_ptr, 8);
            t[i].kind = collider_kind(klass, box_class, capsule_class);
            t[i].native_collider = native;
            if (!t[i].kind || !obj_ptr(native)) { t[i].alive = false; ++rej_shape; }
        }

        // Level 3: native collider dimensions.
        collect(&temp::native_collider);
        gather(bases, n, v100::native_box_center, 0x18, &block[0][0], ok);
        for (size_t i = 0; i < n; ++i) {
            if (!t[i].alive) continue;
            if (!ok[i]) { t[i].alive = false; ++rej_read; continue; }
            if (!parse_shape(t[i].kind, block[i], t[i].shape)) { t[i].alive = false; ++rej_shape; }
        }

        // Levels 4-6: native Component -> GameObject -> component array -> Transform (component 0).
        uint64_t next[kMaxHitboxes];
        auto step = [&](uint64_t temp::*from, uint64_t temp::*to, uint64_t offset) {
            collect(from);
            gather(bases, n, offset, 8, reinterpret_cast<uint8_t*>(next), ok);
            for (size_t i = 0; i < n; ++i) {
                if (!t[i].alive) continue;
                if (!ok[i] || !obj_ptr(next[i])) { t[i].alive = false; ++rej_transform; continue; }
                t[i].*to = next[i];
            }
        };
        step(&temp::component, &temp::game_object, v100::native_component_game_object);
        step(&temp::game_object, &temp::components, v100::native_game_object_components);
        step(&temp::components, &temp::transform, v100::native_component_pair_ptr);

        // Hierarchy of each distinct Transform, on this player's body.
        struct resolved { uint64_t native{0}; player::hierarchy_ref ref{}; uint64_t data{0}; bool ok{false}; bool far{false}; };
        std::vector<resolved> done;
        auto resolve = [&](uint64_t native) -> const resolved& {
            for (const resolved& r : done) if (r.native == native) return r;
            resolved r{};
            r.native = native;
            if (player::resolve_hierarchy_native(native, r.ref) && r.ref.chain_len >= 1) {
                r.data = rpm<uint64_t>(native + uint64_t(layout.data_offset));
                Vector3 position{};
                if (obj_ptr(r.data) && player::hierarchy_position(r.ref, position) && finite3(position)) {
                    r.far = distance3(position, anchor) > kAnchorRadius;
                    r.ok = !r.far;
                }
            }
            done.push_back(r);
            return done.back();
        };
        for (size_t i = 0; i < n; ++i) {
            if (!t[i].alive) continue;
            const resolved& r = resolve(t[i].transform);
            if (!r.ok) { t[i].alive = false; r.far ? ++rej_far : ++rej_transform; continue; }

            size_t gi = 0;
            while (gi < c.groups.size() && !(c.groups[gi].entries == r.ref.entries && c.groups[gi].data == r.data)) ++gi;
            if (gi == c.groups.size()) {
                group g;
                g.data = r.data;
                g.entries = r.ref.entries;
                c.groups.push_back(std::move(g));
            }
            entry e;
            e.bone = t[i].bone;
            e.local = t[i].shape;
            e.group = int(gi);
            e.chain_len = r.ref.chain_len;
            for (int k = 0; k < r.ref.chain_len; ++k) {
                e.slot[k] = r.ref.chain[k];      // node index for now, remapped to a slot below
                c.groups[gi].nodes.push_back(r.ref.chain[k]);
            }
            c.entries.push_back(e);
        }
        for (group& g : c.groups) {
            std::sort(g.nodes.begin(), g.nodes.end());
            g.nodes.erase(std::unique(g.nodes.begin(), g.nodes.end()), g.nodes.end());
            g.offset = c.slots;
            c.slots += g.nodes.size();
        }
        for (entry& e : c.entries) {
            const group& g = c.groups[size_t(e.group)];
            for (int k = 0; k < e.chain_len; ++k)
                e.slot[k] = int(std::lower_bound(g.nodes.begin(), g.nodes.end(), e.slot[k]) - g.nodes.begin());
        }

        const long long took = diag::now_us() - t0;
        ++g_stats.builds;
        g_stats.build_us_max = std::max(g_stats.build_us_max, took);
        diag::log(c.entries.empty() ? "WARN" : "HITBOX",
                  "player=0x%llx model=0x%llx n=%zu built=%zu groups=%zu nodes=%zu build_us=%lld rej(read=%d class=%d shape=%d transform=%d far=%d)",
                  (unsigned long long)player_object, (unsigned long long)c.model, n, c.entries.size(),
                  c.groups.size(), c.slots, took, rej_read, rej_class, rej_shape, rej_transform, rej_far);

        // The first resolved player is dumped once, so a log shows which shapes the game really has.
        static bool dumped = false;
        if (!dumped && !c.entries.empty()) {
            dumped = true;
            int shown = 0;
            for (const entry& e : c.entries) {
                if (++shown > 24) break;
                if (e.local.shape == Shape::Box)
                    diag::log("HITBOX", "  bone=%d box center=(%.3f %.3f %.3f) size=(%.3f %.3f %.3f) chain=%d",
                              e.bone, e.local.center.x, e.local.center.y, e.local.center.z,
                              e.local.size.x, e.local.size.y, e.local.size.z, e.chain_len);
                else
                    diag::log("HITBOX", "  bone=%d capsule center=(%.3f %.3f %.3f) radius=%.3f height=%.3f dir=%d chain=%d",
                              e.bone, e.local.center.x, e.local.center.y, e.local.center.z,
                              e.local.radius, e.local.height, e.local.direction, e.chain_len);
            }
        }
        return !c.entries.empty();
    }

    // Poses every hitbox of one player for the current frame. `out` is cleared.
    inline bool read(const player_snapshot& p, const Vector3& anchor, std::vector<WorldHitbox>& out) noexcept {
        out.clear();
        const long long now = diag::now_us();
        if (now < g_disabled_until_us) return false;
        if (g_spent_us > kFrameBudgetUs) { ++g_stats.budget_skips; return false; }
        spend timer;
        model_cache& c = g_cache[p.object];

        if (c.valid && now >= c.refresh_us) {
            c.refresh_us = now + kRefreshUs;
            const uint64_t model = find_model(p.object);
            const uint64_t array = model ? rpm<uint64_t>(model + v100::model_hitboxes) : 0;
            if (model != c.model || array != c.array) { c.valid = false; c.failures = 0; c.next_try_us = 0; }
        }
        if (!c.valid) {
            if (now < c.next_try_us || g_builds_left <= 0 || g_spent_us > kBuildBudgetUs) return false;
            --g_builds_left;
            if (!build(p.object, anchor, c)) {
                // A missing Transform layout is not the player's fault: no backoff, try next frame.
                if (player::g_transform_layout.pid == proc::pid && player::g_transform_layout.data_offset >= 0)
                    fail(c, now);
                return false;
            }
            c.valid = true;
            c.refresh_us = now + kRefreshUs;
        }

        const player::TransformLayout layout = player::g_transform_layout;
        if (layout.pid != proc::pid || layout.data_offset < 0) return false;
        thread_local std::vector<player::TransformEntry> scratch;
        scratch.resize(c.slots);
        for (const group& g : c.groups) {
            // First part of the first call: the hierarchy's entries pointer, to notice a reallocation.
            uint64_t current_entries = 0;
            size_t done = 0;
            bool first = true;
            while (first || done < g.nodes.size()) {
                read_part parts[64];
                size_t count = 0;
                if (first)
                    parts[count++] = {g.data + uint64_t(layout.entries_offset), &current_entries, 8};
                while (count < 64 && done < g.nodes.size()) {
                    parts[count++] = {g.entries + uint64_t(g.nodes[done]) * sizeof(player::TransformEntry),
                                      &scratch[g.offset + done], sizeof(player::TransformEntry)};
                    ++done;
                }
                first = false;
                if (!mem_read_scatter(parts, count)) { fail(c, now); return false; }
            }
            if (current_entries != g.entries) { fail(c, now); return false; }
        }

        out.reserve(c.entries.size());
        for (const entry& e : c.entries) {
            player::TransformEntry chain[player::hierarchy_ref::kMaxChain];
            const group& g = c.groups[size_t(e.group)];
            for (int i = 0; i < e.chain_len; ++i) chain[i] = scratch[g.offset + size_t(e.slot[i])];
            WorldPose pose;
            WorldHitbox world;
            // One bad hitbox (hidden object, scale 0) must not take the others with it.
            if (!compose_chain(chain, e.chain_len, pose) || !to_world(e.local, pose, e.bone, world) ||
                distance3(world.center, anchor) > kAnchorRadius) continue;
            out.push_back(world);
        }
        if (out.empty()) { fail(c, now); return false; }
        if (++c.good_frames >= 60) c.failures = 0;

        const long long took = diag::now_us() - now;
        ++g_stats.reads;
        g_stats.read_us_sum += took;
        g_stats.read_us_max = std::max(g_stats.read_us_max, took);
        return true;
    }
}
