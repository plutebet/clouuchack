#pragma once

#include "rise_offsets.hpp"
#include "../other/memory.hpp"
#include "../other/status.hpp"
#include "../other/crash.hpp"
#include "../protect/oxorany.hpp"
#include "../game/game.hpp"
#include "../game/player.hpp"
#include "../game/math.hpp"
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace rise_game {

    // ---- state ----
    inline uint64_t g_il2cpp_base = 0;
    inline int g_pid = -1;
    inline uint64_t g_pm_singleton = 0;
    inline uint64_t g_camera_native = 0;
    inline int g_camera_view_off = -1;
    inline int g_camera_proj_off = -1;
    inline long long g_camera_locked_ms = 0;
    inline long long g_last_camera_probe = 0;

    // ---- deep diagnostic (runs once) ----
    inline void diag_typeinfo_once() {
        static bool done = false;
        if (done || !g_il2cpp_base || proc::pid <= 0) return;
        done = true;

        // 1) Verify ELF magic at base
        uint8_t magic[4]{};
        bool elf_ok = mem_read(g_il2cpp_base, magic, 4);
        diag::log("RISE-DBG", "ELF check base=0x%llx: read=%d magic=%02x%02x%02x%02x (%s)",
            (unsigned long long)g_il2cpp_base, int(elf_ok),
            magic[0], magic[1], magic[2], magic[3],
            (magic[0]==0x7f && magic[1]=='E' && magic[2]=='L' && magic[3]=='F') ? "OK" : "BAD");

        // 2) Read raw TypeInfo slot values (before obj_ptr filter)
        const uint64_t rvas[] = {
            rise::PlayerManager_TypeInfo,
            rise::PlayerController_TypeInfo,
            rise::MainCamera_TypeInfo,
            rise::Camera_TypeInfo,
            rise::CoreAcClient_TypeInfo
        };
        const char* names[] = {"PM", "PC", "MainCam", "Cam", "CoreAC"};
        for (int i = 0; i < 5; i++) {
            const uint64_t addr = g_il2cpp_base + rvas[i];
            const uint64_t raw = rpm<uint64_t>(addr);
            diag::log("RISE-DBG", "TypeInfo[%s] addr=0x%llx rva=0x%llx raw=0x%llx obj_ptr=%d",
                names[i], (unsigned long long)addr, (unsigned long long)rvas[i],
                (unsigned long long)raw, int(obj_ptr(raw)));
        }

        // 3) Dump maps for il2cpp and unity
        char path[64];
        std::snprintf(path, sizeof(path), "/proc/%d/maps", proc::pid);
        FILE* f = std::fopen(path, "r");
        if (!f) { diag::log("RISE-DBG", "cannot open %s", path); return; }
        char line[512];
        int count = 0;
        while (std::fgets(line, sizeof(line), f) && count < 20) {
            size_t len = std::strlen(line);
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
            if (std::strstr(line, "il2cpp") || std::strstr(line, "libunity")) {
                diag::log("RISE-DBG", "maps: %s", line);
                count++;
            }
        }
        std::fclose(f);
        if (count == 0) diag::log("RISE-DBG", "maps: NO il2cpp/libunity entries found!");
    }

    // ---- TypeInfo resolution (same mechanism as SO2 typeinfo()) ----
    inline uint64_t typeinfo_rise(uint64_t rva) {
        if (!g_il2cpp_base) return 0;
        thread_local uint64_t cache_base = 0;
        thread_local int cache_pid = -1;
        struct slot { uint64_t rva, value; };
        thread_local std::vector<slot> cache;
        if (cache_base != g_il2cpp_base || cache_pid != proc::pid) {
            cache.clear();
            cache_base = g_il2cpp_base;
            cache_pid = proc::pid;
        }
        for (const slot& s : cache)
            if (s.rva == rva) return s.value;
        const uint64_t value = rpm<uint64_t>(g_il2cpp_base + rva);
        if (!obj_ptr(value)) return 0;
        cache.push_back({rva, value});
        return value;
    }

    // ---- player snapshot ----
    struct rise_player {
        uint64_t controller;      // PlayerController*
        uint64_t net_ctrl;        // NetworkController* (for health)
        uint64_t transform;       // managed Transform (from PlayerController.CBCBGACGGECGEAA @0xD8)
        uint8_t team;             // from PlayerController @0x59
        int health;
        int armor;
        bool alive;
        bool is_local;
    };

    // Read health/armor/alive from NetworkController (which extends Controller base).
    inline bool read_health(uint64_t net_ctrl, int& hp, int& armor, bool& alive) {
        if (!obj_ptr(net_ctrl)) return false;
        struct HealthBlock {
            int32_t health;   // +0x40
            int32_t armor;    // +0x44
            uint8_t is_alive; // +0x48
        };
        HealthBlock block{};
        if (!mem_read(net_ctrl + rise::ctrl_health, &block, sizeof(block))) return false;
        if (block.health < 0 || block.health > 1000) return false;
        hp = block.health;
        armor = block.armor;
        alive = block.is_alive != 0;
        return true;
    }

    // ---- PlayerManager singleton resolution ----
    // LazySingleton: class static fields area has the singleton at offset 0x0.
    // Il2CppClass -> staticFields pointer at a known offset, then read +0x0.
    // For il2cpp, klass->static_fields is at a version-dependent offset.
    // We try the common il2cpp offsets for static_fields: 0xB8, 0xB0.
    inline uint64_t resolve_pm_singleton() {
        const uint64_t klass = typeinfo_rise(rise::PlayerManager_TypeInfo);
        if (!klass) return 0;
        for (uint64_t sf_off : {uint64_t(0xB8), uint64_t(0xB0), uint64_t(0xC0)}) {
            const uint64_t static_fields = rpm<uint64_t>(klass + sf_off);
            if (!plausible(static_fields)) continue;
            const uint64_t singleton = rpm<uint64_t>(static_fields + rise::main_camera_singleton);
            if (!obj_ptr(singleton)) continue;
            const uint64_t obj_klass = rpm<uint64_t>(singleton);
            if (obj_klass == klass) return singleton;
        }
        return 0;
    }

    // ---- Dictionary<int, PlayerController> iteration ----
    inline bool read_dict_players(uint64_t dict, std::vector<uint64_t>& controllers) {
        controllers.clear();
        if (!obj_ptr(dict)) return false;
        const int count = rpm<int>(dict + rise::dict_count);
        if (count <= 0 || count > 64) return false;
        const uint64_t entries_arr = rpm<uint64_t>(dict + rise::dict_entries);
        if (!obj_ptr(entries_arr)) return false;
        const uint64_t arr_len = rpm<uint64_t>(entries_arr + rise::array_length);
        if (arr_len < uint64_t(count) || arr_len > 4096) return false;
        // Read all entries in bulk
        const size_t entry_count = size_t(std::min(int(arr_len), 128));
        const size_t block_size = entry_count * size_t(rise::dict_entry_size);
        std::vector<uint8_t> buf(block_size, 0);
        if (!mem_read(entries_arr + rise::array_data, buf.data(), block_size)) return false;
        for (size_t i = 0; i < entry_count; ++i) {
            const uint8_t* entry = buf.data() + i * size_t(rise::dict_entry_size);
            int32_t hashCode = 0;
            std::memcpy(&hashCode, entry, 4);
            if (hashCode < 0) continue; // unused slot
            uint64_t value = 0;
            std::memcpy(&value, entry + rise::dict_entry_value, 8);
            if (obj_ptr(value)) controllers.push_back(value);
        }
        return true;
    }

    // ---- enumerate players ----
    inline std::vector<rise_player> players() {
        std::vector<rise_player> out;
        if (!g_pm_singleton) {
            g_pm_singleton = resolve_pm_singleton();
            if (!g_pm_singleton) return out;
            diag::log("RISE", "PlayerManager singleton=0x%llx", (unsigned long long)g_pm_singleton);
        }
        // Revalidate singleton
        const uint64_t pm_klass = typeinfo_rise(rise::PlayerManager_TypeInfo);
        if (!pm_klass || rpm<uint64_t>(g_pm_singleton) != pm_klass) {
            g_pm_singleton = 0;
            return out;
        }
        // Read local player
        const uint64_t local_pc = rpm<uint64_t>(g_pm_singleton + rise::pm_local_player);
        // Read dictionary
        const uint64_t dict = rpm<uint64_t>(g_pm_singleton + rise::pm_dict);
        std::vector<uint64_t> controllers;
        if (!read_dict_players(dict, controllers)) return out;
        // Build snapshots
        const uint64_t pc_klass = typeinfo_rise(rise::PlayerController_TypeInfo);
        for (uint64_t pc : controllers) {
            if (!pc_klass || rpm<uint64_t>(pc) != pc_klass) continue;
            rise_player p{};
            p.controller = pc;
            p.is_local = (pc == local_pc);
            p.team = rpm<uint8_t>(pc + rise::pc_team);
            p.net_ctrl = rpm<uint64_t>(pc + rise::pc_net_ctrl);
            p.transform = rpm<uint64_t>(pc + rise::pc_transform);
            if (!read_health(p.net_ctrl, p.health, p.armor, p.alive)) {
                p.health = 100;
                p.armor = 0;
                p.alive = true;
            }
            out.push_back(p);
        }
        return out;
    }

    // ---- Camera resolution via MainCamera singleton ----
    // MainCamera -> CameraMovementController.Camera (0x98) -> m_CachedPtr -> native Camera
    inline bool resolve_camera(matrix& view, matrix& proj) {
        // Fast path: re-read locked camera
        if (g_camera_native && g_camera_view_off >= 0 && g_camera_proj_off >= 0) {
            if (read_matrix_pair(g_camera_native, g_camera_view_off, g_camera_proj_off, view, proj))
                return true;
            g_camera_native = 0;
            g_camera_view_off = -1;
            g_camera_proj_off = -1;
        }
        // Throttle probing
        const long long now = diag::now_ms();
        if (now - g_last_camera_probe < 200) return false;
        g_last_camera_probe = now;
        // Resolve MainCamera singleton
        const uint64_t mc_klass = typeinfo_rise(rise::MainCamera_TypeInfo);
        if (!mc_klass) return false;
        // Static fields -> singleton
        for (uint64_t sf_off : {uint64_t(0xB8), uint64_t(0xB0), uint64_t(0xC0)}) {
            const uint64_t static_fields = rpm<uint64_t>(mc_klass + sf_off);
            if (!plausible(static_fields)) continue;
            const uint64_t mc_instance = rpm<uint64_t>(static_fields + rise::main_camera_singleton);
            if (!obj_ptr(mc_instance)) continue;
            if (rpm<uint64_t>(mc_instance) != mc_klass) continue;
            // CameraMovementController.Camera at +0x98
            const uint64_t managed_camera = rpm<uint64_t>(mc_instance + rise::cmc_camera);
            if (!obj_ptr(managed_camera)) continue;
            const uint64_t camera_klass = typeinfo_rise(rise::Camera_TypeInfo);
            if (!camera_klass || rpm<uint64_t>(managed_camera) != camera_klass) continue;
            const uint64_t native = rpm<uint64_t>(managed_camera + rise::unity_cached_ptr);
            if (!obj_ptr(native) || (native & 7) != 0) continue;
            // Try to detect matrix pair
            int v_off = -1, p_off = -1;
            if (detect_pair(native, view, proj, v_off, p_off, "rise-MainCamera")) {
                g_camera_native = native;
                g_camera_view_off = v_off;
                g_camera_proj_off = p_off;
                g_camera_locked_ms = now;
                diag::log("RISE", "camera locked native=0x%llx view=+0x%x proj=+0x%x",
                          (unsigned long long)native, v_off, p_off);
                return true;
            }
        }
        return false;
    }

    // ---- Transform position (reuses SO2's Unity transform walking) ----
    inline bool player_position(uint64_t managed_transform, Vector3& out) {
        if (!obj_ptr(managed_transform)) return false;
        const uint64_t native = rpm<uint64_t>(managed_transform + rise::unity_cached_ptr);
        if (!obj_ptr(native)) return false;
        return player::native_transform_position(native, out);
    }

    // Player pose: feet + head estimate
    inline player::PlayerPose player_pose(const rise_player& p) {
        player::PlayerPose pose{};
        Vector3 pos{};
        if (!player_position(p.transform, pos)) return pose;
        pose.feet = pos;
        pose.head = pos + Vector3(0.f, 1.67f, 0.f);
        pose.valid = true;
        pose.has_head = true;
        return pose;
    }

    // ---- Reset on process change ----
    inline void reset() {
        g_pm_singleton = 0;
        g_camera_native = 0;
        g_camera_view_off = -1;
        g_camera_proj_off = -1;
        g_il2cpp_base = 0;
        player::g_transform_cache.clear();
    }
}
