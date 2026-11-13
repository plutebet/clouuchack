// Player hitboxes: bis.acis (HitBoxCollider[]) -> bone transform + native
// Box/CapsuleCollider shape, resolved per player and posed every frame from
// the Unity TransformHierarchy (one block read per hierarchy).
//
// Every offset comes from v100:: (game.hpp): managed fields from dump.cs 1.1.0,
// native ones from the *_Injected icall thunks of libunity.so 1.1.0.

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

    inline constexpr int kHeadBone = 10;     // HumanBodyBones.Head
    inline constexpr int kMaxBone = 55;      // HumanBodyBones.LastBone
    inline constexpr int kMaxHitboxes = 64;
    inline constexpr float kAnchorRadius = 4.f; // a hitbox further than this from its player is not his

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

    // ---- per-player cache -----------------------------------------------
    struct entry {
        int bone{-1};
        LocalShape local{};
        player::hierarchy_ref ref{};
    };
    struct model_cache {
        uint64_t model{0};
        uint64_t array{0};
        std::vector<entry> entries;
        long long built_us{0};
        bool valid{false};
    };
    inline std::unordered_map<uint64_t, model_cache> g_cache;
    inline int g_builds_left = 0;

    // Resolving a player is ~1 + 3 reads per hitbox; spread it over frames.
    inline void begin_frame() noexcept { g_builds_left = 2; }
    inline void forget(uint64_t object) { g_cache.erase(object); }

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

    // 0 = unknown, 1 = BoxCollider, 2 = CapsuleCollider. The TypeInfo slot is
    // used when the game filled it; otherwise the il2cpp class name decides.
    inline int collider_kind(uint64_t klass) noexcept {
        if (!obj_ptr(klass)) return 0;
        const uint64_t box = typeinfo(v100::boxcollider_typeinfo);
        const uint64_t capsule = typeinfo(v100::capsulecollider_typeinfo);
        if (box && klass == box) return 1;
        if (capsule && klass == capsule) return 2;
        std::string name, ns;
        if (il2cpp::klass_identity(klass, name, ns) && ns == "UnityEngine") {
            if (name == "BoxCollider") return 1;
            if (name == "CapsuleCollider") return 2;
        }
        return 0;
    }

    inline bool read_local_shape(uint64_t managed_collider, LocalShape& out) noexcept {
        const int kind = collider_kind(rpm<uint64_t>(managed_collider));
        if (!kind) return false;
        const uint64_t native = rpm<uint64_t>(managed_collider + v100::unity_cached_ptr);
        if (!obj_ptr(native)) return false;
        // Box: center@0x60 size@0x6C. Capsule: radius@0x60 height@0x64 direction@0x68 center@0x6C.
        static_assert(v100::native_box_center == v100::native_capsule_radius &&
                      v100::native_box_size == v100::native_capsule_center, "shared native block");
        uint8_t block[0x18]{};
        if (!mem_read(native + v100::native_box_center, block, sizeof(block))) return false;
        LocalShape s{};
        if (kind == 1) {
            s.shape = Shape::Box;
            std::memcpy(&s.center, block + 0x00, sizeof(Vector3));
            std::memcpy(&s.size, block + 0x0C, sizeof(Vector3));
        } else {
            s.shape = Shape::Capsule;
            std::memcpy(&s.radius, block + 0x00, 4);
            std::memcpy(&s.height, block + 0x04, 4);
            std::memcpy(&s.direction, block + 0x08, 4);
            std::memcpy(&s.center, block + 0x0C, sizeof(Vector3));
        }
        if (!local_shape_valid(s)) return false;
        out = s;
        return true;
    }

    // HitBoxCollider (MonoBehaviour) -> native Component -> GameObject -> Transform (component 0).
    inline uint64_t native_owner_transform(uint64_t managed_behaviour) noexcept {
        const uint64_t component = rpm<uint64_t>(managed_behaviour + v100::unity_cached_ptr);
        if (!obj_ptr(component)) return 0;
        const uint64_t game_object = rpm<uint64_t>(component + v100::native_component_game_object);
        if (!obj_ptr(game_object)) return 0;
        const uint64_t components = rpm<uint64_t>(game_object + v100::native_game_object_components);
        if (!obj_ptr(components)) return 0;
        const uint64_t transform = rpm<uint64_t>(components + v100::native_component_pair_ptr);
        return obj_ptr(transform) ? transform : 0;
    }

    inline bool build(uint64_t player_object, const Vector3& anchor, model_cache& cache) noexcept {
        cache.entries.clear();
        cache.valid = false;
        cache.built_us = diag::now_us();
        cache.model = find_model(player_object);
        if (!cache.model) return false;
        cache.array = rpm<uint64_t>(cache.model + v100::model_hitboxes);
        const uint64_t length = rpm<uint64_t>(cache.array + v100::array_length);
        if (!obj_ptr(cache.array) || length == 0 || length > kMaxHitboxes) return false;

        uint64_t items[kMaxHitboxes]{};
        if (!mem_read(cache.array + v100::array_data, items, size_t(length) * 8)) return false;

        uint64_t first_class = 0;
        int rejected_class = 0, rejected_shape = 0, rejected_transform = 0, rejected_far = 0;
        for (uint64_t i = 0; i < length; ++i) {
            const uint64_t hb = items[i];
            if (!obj_ptr(hb)) continue;
            uint8_t block[0x30]{};
            if (!mem_read(hb, block, sizeof(block))) continue;
            uint64_t klass = 0, collider = 0;
            int bone = -1;
            std::memcpy(&klass, block, 8);
            std::memcpy(&bone, block + v100::hitbox_bone, 4);
            std::memcpy(&collider, block + v100::hitbox_collider, 8);
            if (!first_class) first_class = klass;
            if (klass != first_class || bone < 0 || bone > kMaxBone || !obj_ptr(collider)) { ++rejected_class; continue; }

            entry e{};
            e.bone = bone;
            if (!read_local_shape(collider, e.local)) { ++rejected_shape; continue; }
            if (!player::resolve_hierarchy_native(native_owner_transform(hb), e.ref) || e.ref.chain_len < 1) {
                ++rejected_transform;
                continue;
            }
            // The owner transform must sit on this player's body, otherwise the
            // GameObject -> Transform walk landed on something else.
            Vector3 position{};
            if (!player::hierarchy_position(e.ref, position) || !finite3(position) ||
                distance3(position, anchor) > kAnchorRadius) { ++rejected_far; continue; }
            cache.entries.push_back(e);
        }

        static long long last_log = 0;
        if (diag::every(last_log, 4000) || cache.entries.empty())
            diag::log(cache.entries.empty() ? "WARN" : "HITBOX",
                      "player=0x%llx model=0x%llx array=0x%llx n=%llu built=%zu rej(class=%d shape=%d transform=%d far=%d)",
                      (unsigned long long)player_object, (unsigned long long)cache.model,
                      (unsigned long long)cache.array, (unsigned long long)length, cache.entries.size(),
                      rejected_class, rejected_shape, rejected_transform, rejected_far);
        cache.valid = !cache.entries.empty();
        return cache.valid;
    }

    // Poses every hitbox of one player for the current frame. `out` is cleared.
    inline bool read(const player_snapshot& p, const Vector3& anchor, std::vector<WorldHitbox>& out) noexcept {
        out.clear();
        model_cache& cache = g_cache[p.object];
        const long long now = diag::now_us();
        const long long age = now - cache.built_us;
        const bool stale = !cache.valid ? age > 1000000 : age > 3000000;
        if (stale && g_builds_left > 0) {
            --g_builds_left;
            build(p.object, anchor, cache);
        }
        if (!cache.valid) return false;

        // One block read per distinct hierarchy: all bones of a player share it.
        thread_local std::vector<player::TransformEntry> block;
        thread_local std::vector<player::TransformEntry> chain;
        uint64_t block_entries = 0;
        int block_max = -1;
        bool ok = true;
        for (const entry& e : cache.entries) {
            if (e.ref.entries != block_entries) {
                int max_index = 0;
                for (const entry& other : cache.entries)
                    if (other.ref.entries == e.ref.entries) max_index = std::max(max_index, other.ref.index);
                if (max_index > 4096) { ok = false; break; }
                block.resize(size_t(max_index) + 1);
                if (!mem_read(e.ref.entries, block.data(), block.size() * sizeof(player::TransformEntry))) { ok = false; break; }
                block_entries = e.ref.entries;
                block_max = max_index;
            }
            chain.resize(size_t(e.ref.chain_len));
            bool chain_ok = true;
            for (int i = 0; i < e.ref.chain_len; ++i) {
                if (e.ref.chain[i] > block_max) { chain_ok = false; break; }
                chain[size_t(i)] = block[size_t(e.ref.chain[i])];
            }
            WorldPose pose{};
            WorldHitbox world{};
            if (!chain_ok || !compose_chain(chain.data(), e.ref.chain_len, pose) ||
                !to_world(e.local, pose, e.bone, world)) { ok = false; break; }
            if (distance3(world.center, anchor) > kAnchorRadius) { ok = false; break; }
            out.push_back(world);
        }
        if (!ok) {
            // Hierarchy rebuilt (respawn / round change): resolve again next frame.
            out.clear();
            cache.valid = false;
            cache.built_us = 0;
            return false;
        }
        return !out.empty();
    }
}
