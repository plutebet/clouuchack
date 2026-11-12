// ================================================================================
// Skeleton of a player, read-only.
//
// Chain (dump.cs 1.1.0, see v100 in game.hpp):
//   odq (wwv) +0x28 nzk -> +0x20 cew[] -> element of class bis (character model)
//   bis +0x30 CharacterBindings -> +0x20 BonesDictionary
//     (SerializableDictionary<HumanBodyBones, Transform>)
//   Transform -> m_CachedPtr -> native TransformHierarchy (layout probed in player.hpp)
//
// Per frame one character costs two remote reads: all bones of a character
// live in one TransformHierarchy, so its local transforms and parent indices
// are read as two blocks and every bone is composed locally.
// ================================================================================

#pragma once

#include "game.hpp"
#include "player.hpp"
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace skeleton {
    // UnityEngine.HumanBodyBones values (enum constants in dump.cs).
    enum : int {
        Hips = 0, LeftUpperLeg = 1, RightUpperLeg = 2, LeftLowerLeg = 3, RightLowerLeg = 4,
        LeftFoot = 5, RightFoot = 6, Spine = 7, Chest = 8, Neck = 9, Head = 10,
        LeftShoulder = 11, RightShoulder = 12, LeftUpperArm = 13, RightUpperArm = 14,
        LeftLowerArm = 15, RightLowerArm = 16, LeftHand = 17, RightHand = 18,
        UpperChest = 54, BoneCount = 55,
    };

    // Lines are drawn between consecutive bones of a chain that are present;
    // a missing bone (e.g. no UpperChest on a rig) is simply skipped.
    inline constexpr int k_chain_spine[] = {Hips, Spine, Chest, UpperChest, Neck, Head};
    inline constexpr int k_chain_left_arm[] = {UpperChest, LeftShoulder, LeftUpperArm, LeftLowerArm, LeftHand};
    inline constexpr int k_chain_right_arm[] = {UpperChest, RightShoulder, RightUpperArm, RightLowerArm, RightHand};
    inline constexpr int k_chain_left_leg[] = {Hips, LeftUpperLeg, LeftLowerLeg, LeftFoot};
    inline constexpr int k_chain_right_leg[] = {Hips, RightUpperLeg, RightLowerLeg, RightFoot};

    inline bool used(int bone) {
        return (bone >= Hips && bone <= RightHand) || bone == UpperChest;
    }

    struct pose {
        std::array<Vector3, BoneCount> point{};
        uint64_t present{0};   // bit = HumanBodyBones value (UpperChest uses bit 54)
        bool has(int bone) const { return bone >= 0 && bone < BoneCount && (present >> bone) & 1ULL; }
    };

    struct bone_ref { int bone{-1}; player::hierarchy_ref ref{}; };
    struct character {
        uint64_t model{0};
        std::vector<bone_ref> bones;
        long long resolved_ms{0};
    };
    inline std::unordered_map<uint64_t, character> g_players; // key: odq object
    inline uint64_t g_model_class = 0;      // bis klass (slot or learned)
    inline uint64_t g_transform_class = 0;  // UnityEngine.Transform klass
    inline int g_context_pid = -1;
    inline uint64_t g_context_lib = 0;

    inline void reset_if_context_changed() {
        if (g_context_pid == proc::pid && g_context_lib == proc::lib) return;
        g_players.clear();
        g_model_class = g_transform_class = 0;
        g_context_pid = proc::pid;
        g_context_lib = proc::lib;
    }

    inline bool is_transform(uint64_t object) {
        return g_transform_class && obj_ptr(object) && rpm<uint64_t>(object) == g_transform_class;
    }

    // BonesDictionary -> (bone, managed Transform). The serialized arrays are
    // used when Unity kept them; otherwise the live Dictionary entries.
    inline bool read_bone_map(uint64_t model, std::vector<std::pair<int, uint64_t>>& out) {
        out.clear();
        const uint64_t bindings = rpm<uint64_t>(model + v100::model_bindings);
        if (!obj_ptr(bindings)) return false;
        const uint64_t bones = rpm<uint64_t>(bindings + v100::bindings_bones);
        if (!obj_ptr(bones)) return false;

        uint64_t heads[3]{};   // m_dict, m_keys, m_values
        if (!mem_read(bones + v100::sdict_dict, heads, sizeof(heads))) return false;
        const uint64_t keys = heads[1], values = heads[2];
        if (obj_ptr(keys) && obj_ptr(values)) {
            const uint64_t n = rpm<uint64_t>(keys + v100::array_length);
            if (n > 0 && n <= 64 && rpm<uint64_t>(values + v100::array_length) == n) {
                int32_t key_data[64]{};
                uint64_t value_data[64]{};
                if (mem_read(keys + v100::array_data, key_data, size_t(n) * 4) &&
                    mem_read(values + v100::array_data, value_data, size_t(n) * 8)) {
                    for (uint64_t i = 0; i < n; ++i)
                        if (used(key_data[i]) && is_transform(value_data[i]))
                            out.emplace_back(key_data[i], value_data[i]);
                }
            }
            if (!out.empty()) return true;
        }

        const uint64_t dict = heads[0];
        if (!obj_ptr(dict)) return false;
        const uint64_t entries = rpm<uint64_t>(dict + v100::dict_entries);
        const int32_t count = rpm<int32_t>(dict + v100::dict_count);
        if (!obj_ptr(entries) || count <= 0 || count > 128) return false;
        const uint64_t capacity = rpm<uint64_t>(entries + v100::array_length);
        if (capacity < uint64_t(count) || capacity > 512) return false;
        std::vector<uint8_t> raw(size_t(count) * v100::dict_entry_size);
        if (!mem_read(entries + v100::array_data, raw.data(), raw.size())) return false;
        for (int32_t i = 0; i < count; ++i) {
            const uint8_t* e = raw.data() + size_t(i) * v100::dict_entry_size;
            int32_t hash = 0, key = 0;
            uint64_t value = 0;
            std::memcpy(&hash, e, 4);
            std::memcpy(&key, e + v100::dict_entry_key, 4);
            std::memcpy(&value, e + v100::dict_entry_value, 8);
            if (hash < 0) continue; // free slot
            if (used(key) && is_transform(value)) out.emplace_back(key, value);
        }
        return !out.empty();
    }

    inline void log_bone_map_failure(uint64_t model) {
        static long long last = 0;
        if (!diag::every(last, 5000)) return;
        const uint64_t bindings = rpm<uint64_t>(model + v100::model_bindings);
        const uint64_t bones = obj_ptr(bindings) ? rpm<uint64_t>(bindings + v100::bindings_bones) : 0;
        uint64_t words[6]{};
        if (obj_ptr(bones)) mem_read(bones, words, sizeof(words));
        const uint64_t dict = words[2];
        uint64_t dict_words[6]{};
        if (obj_ptr(dict)) mem_read(dict, dict_words, sizeof(dict_words));
        diag::log("SKELETON", "bone map unreadable model=0x%llx bindings=0x%llx bones=0x%llx "
                  "bones[0..5]=%llx %llx %llx %llx %llx %llx dict[0..5]=%llx %llx %llx %llx %llx %llx transformClass=0x%llx",
                  (unsigned long long)model, (unsigned long long)bindings, (unsigned long long)bones,
                  (unsigned long long)words[0], (unsigned long long)words[1], (unsigned long long)words[2],
                  (unsigned long long)words[3], (unsigned long long)words[4], (unsigned long long)words[5],
                  (unsigned long long)dict_words[0], (unsigned long long)dict_words[1], (unsigned long long)dict_words[2],
                  (unsigned long long)dict_words[3], (unsigned long long)dict_words[4], (unsigned long long)dict_words[5],
                  (unsigned long long)g_transform_class);
    }

    inline uint64_t find_model(const player_snapshot& p, std::vector<std::pair<int, uint64_t>>& map) {
        const uint64_t container = rpm<uint64_t>(p.object + v100::player_parts);
        if (!obj_ptr(container)) return 0;
        const uint64_t array = rpm<uint64_t>(container + v100::parts_array);
        if (!obj_ptr(array)) return 0;
        const uint64_t length = rpm<uint64_t>(array + v100::array_length);
        if (length == 0 || length > 64) return 0;
        uint64_t parts[64]{};
        if (!mem_read(array + v100::array_data, parts, size_t(length) * 8)) return 0;
        for (uint64_t i = 0; i < length; ++i) {
            const uint64_t part = parts[i];
            if (!obj_ptr(part)) continue;
            const uint64_t klass = rpm<uint64_t>(part);
            if (g_model_class) {
                if (klass != g_model_class) continue;
                if (!read_bone_map(part, map)) { log_bone_map_failure(part); return 0; }
                return part;
            }
            // Slot lazy: accept the part whose root is a Transform and whose
            // bindings yield a plausible humanoid bone map, then keep its class.
            if (!is_transform(rpm<uint64_t>(part + v100::model_root))) continue;
            if (!read_bone_map(part, map) || map.size() < 8) continue;
            g_model_class = klass;
            diag::log("SKELETON", "bis klass learned 0x%llx (slot lazy), %zu bones",
                      (unsigned long long)klass, map.size());
            return part;
        }
        return 0;
    }

    inline character& resolve(const player_snapshot& p) {
        reset_if_context_changed();
        if (!g_model_class) g_model_class = typeinfo(v100::model_typeinfo);
        if (!g_transform_class) g_transform_class = typeinfo(v100::transform_typeinfo);
        character& c = g_players[p.object];
        const long long now = diag::now_ms();
        if (!c.bones.empty() && now - c.resolved_ms < 1000) return c;
        c.resolved_ms = now;
        c.bones.clear();
        if (!g_transform_class) return c;
        std::vector<std::pair<int, uint64_t>> map;
        c.model = find_model(p, map);
        if (!c.model) return c;
        for (const auto& [bone, managed] : map) {
            bone_ref r{bone, {}};
            if (player::resolve_hierarchy(managed, r.ref)) c.bones.push_back(r);
        }
        static long long last_log = 0;
        if (diag::every(last_log, 10000)) {
            // Once per session: the raw words around the first bone's native Transform,
            // so the index field can be confirmed from the log.
            static bool words_logged = false;
            if (!words_logged && !map.empty()) {
                words_logged = true;
                const uint64_t n0 = rpm<uint64_t>(map.front().second + v100::unity_cached_ptr);
                uint64_t w[9]{};
                if (obj_ptr(n0)) mem_read(n0 + 0x10, w, sizeof(w));
                diag::log("SKELETON", "first bone native words +0x10..+0x50: %llx %llx %llx %llx %llx %llx %llx %llx %llx",
                          (unsigned long long)w[0], (unsigned long long)w[1], (unsigned long long)w[2],
                          (unsigned long long)w[3], (unsigned long long)w[4], (unsigned long long)w[5],
                          (unsigned long long)w[6], (unsigned long long)w[7], (unsigned long long)w[8]);
            }
            diag::log("SKELETON", "player=0x%llx model=0x%llx bones=%zu/%zu",
                      (unsigned long long)p.object, (unsigned long long)c.model, c.bones.size(), map.size());

            // Why bones fail: managed -> native (m_CachedPtr), then the hierarchy reads, counted per bone.
            const auto& L = player::g_transform_layout;
            int native_null = 0, data_bad = 0, index_bad = 0, entries_bad = 0, ok = 0;
            uint64_t s_managed = 0, s_native = 0, s_data = 0;
            int s_index = -1;
            for (const auto& [bone, managed] : map) {
                const uint64_t native = rpm<uint64_t>(managed + v100::unity_cached_ptr);
                if (!obj_ptr(native)) {
                    if (native_null == 0) s_managed = managed;
                    ++native_null;
                    continue;
                }
                const uint64_t data = rpm<uint64_t>(native + uint64_t(L.data_offset));
                const int index = rpm<int>(native + uint64_t(L.index_offset));
                if (!plausible(data)) {
                    if (data_bad == 0) s_native = native;
                    ++data_bad;
                    continue;
                }
                if (index < 0 || index > 8192) {
                    if (index_bad == 0) { s_native = native; s_data = data; s_index = index; }
                    ++index_bad;
                    continue;
                }
                const uint64_t entries = rpm<uint64_t>(data + uint64_t(L.entries_offset));
                const uint64_t parents = rpm<uint64_t>(data + uint64_t(L.parents_offset));
                if (!plausible(entries) || !plausible(parents)) { ++entries_bad; continue; }
                ++ok;
            }
            const uint64_t root_managed = rpm<uint64_t>(c.model + v100::model_root);
            const uint64_t root_native = obj_ptr(root_managed) ? rpm<uint64_t>(root_managed + v100::unity_cached_ptr) : 0;
            diag::log("SKELETON", "diag native_null=%d data_bad=%d index_bad=%d entries_bad=%d ok=%d "
                      "layout data=%d index=%d entries=%d parents=%d "
                      "root_native=0x%llx s_managed=0x%llx s_native=0x%llx s_data=0x%llx s_index=%d",
                      native_null, data_bad, index_bad, entries_bad, ok,
                      (int)L.data_offset, (int)L.index_offset, (int)L.entries_offset, (int)L.parents_offset,
                      (unsigned long long)root_native, (unsigned long long)s_managed,
                      (unsigned long long)s_native, (unsigned long long)s_data, s_index);
        }
        return c;
    }

    // World position of `index` inside one hierarchy block that was read
    // locally (entries/parents cover indices 0..count-1).
    inline bool compose(const std::vector<player::TransformEntry>& entries, const std::vector<int32_t>& parents,
                        int index, Vector3& out) {
        const int count = int(entries.size());
        if (index < 0 || index >= count || !player::entry_valid(entries[size_t(index)])) return false;
        const player::TransformEntry& self = entries[size_t(index)];
        Vector3 result(self.position.x, self.position.y, self.position.z);
        int parent = parents[size_t(index)];
        for (int guard = 0; parent >= 0; ++guard) {
            if (guard >= 128 || parent >= count || parent == index) return false;
            const player::TransformEntry& e = entries[size_t(parent)];
            if (!player::entry_valid(e)) return false;
            const Vector3 scaled(result.x * e.scale.x, result.y * e.scale.y, result.z * e.scale.z);
            result = player::rotate(e.rotation, scaled) + Vector3(e.position.x, e.position.y, e.position.z);
            const int next = parents[size_t(parent)];
            if (next == parent) return false;
            parent = next;
        }
        if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z) ||
            std::fabs(result.x) > 100000.f || std::fabs(result.y) > 100000.f || std::fabs(result.z) > 100000.f)
            return false;
        out = result;
        return true;
    }

    inline bool read(const player_snapshot& p, pose& out) {
        out.present = 0;
        character& c = resolve(p);
        if (c.bones.empty()) return false;

        // Unity keeps a hierarchy in depth-first order, so ancestors have
        // lower indices; reading 0..max covers every parent. A parent index
        // outside the block fails compose() instead of reading garbage.
        const uint64_t entries = c.bones.front().ref.entries;
        const uint64_t parents = c.bones.front().ref.parents;
        int max_index = 0;
        for (const bone_ref& b : c.bones)
            if (b.ref.entries == entries && b.ref.index > max_index) max_index = b.ref.index;
        if (max_index > 2047) return false;
        std::vector<player::TransformEntry> block(size_t(max_index) + 1);
        std::vector<int32_t> parent_block(size_t(max_index) + 1);
        if (!mem_read(entries, block.data(), block.size() * sizeof(player::TransformEntry)) ||
            !mem_read(parents, parent_block.data(), parent_block.size() * sizeof(int32_t))) {
            c.resolved_ms = 0; // hierarchy moved: re-resolve next frame
            return false;
        }
        for (const bone_ref& b : c.bones) {
            Vector3 point{};
            const bool ok = b.ref.entries == entries
                ? compose(block, parent_block, b.ref.index, point)
                : player::hierarchy_position(b.ref, point);
            if (!ok) continue;
            out.point[size_t(b.bone)] = point;
            out.present |= 1ULL << b.bone;
        }
        return out.present != 0;
    }
}
