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
    inline uint64_t g_pm_singleton_klass = 0;  // actual runtime klass of PM singleton
    inline uint64_t g_pm_dict_off = rise::pm_dict; // detected dict offset within PM object
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
        const uint64_t raw = rpm<uint64_t>(g_il2cpp_base + rva);
        const uint64_t value = untag(raw);
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

    // ---- String reading for class name diagnostics ----
    inline std::string read_cstr(uint64_t addr, int max_len = 48) {
        addr = untag(addr);
        if (!plausible(addr)) return "?";
        char buf[64]{};
        if (!mem_read(addr, buf, std::min(max_len, (int)sizeof(buf) - 1))) return "?";
        for (int i = 0; buf[i]; i++)
            if ((unsigned char)buf[i] < 0x20 || (unsigned char)buf[i] > 0x7e) { buf[i] = 0; break; }
        return buf[0] ? buf : "?";
    }

    inline std::string class_name(uint64_t klass) {
        if (!obj_ptr(klass)) return "?";
        return read_cstr(rptr(klass + 0x10));
    }

    // Check if a pointer looks like a Dictionary<int, T> by internal structure
    inline bool looks_like_dict(uint64_t ptr, uint64_t dict_klass) {
        if (!obj_ptr(ptr)) return false;
        uint64_t k = rptr(ptr);
        if (!obj_ptr(k)) return false;
        if (dict_klass && k != dict_klass) return false;
        uint64_t entries = rptr(ptr + rise::dict_entries);
        if (!obj_ptr(entries)) return false;
        int count = rpm<int>(ptr + rise::dict_count);
        if (count < 0 || count > 1000) return false;
        uint64_t arr_len = rpm<uint64_t>(entries + rise::array_length);
        return arr_len >= (uint64_t)count && arr_len <= 10000;
    }

    // ---- PlayerManager singleton resolution ----
    inline long long g_last_pm_diag = 0;

    inline uint64_t resolve_pm_singleton() {
        const uint64_t klass = typeinfo_rise(rise::PlayerManager_TypeInfo);
        if (!klass) return 0;

        const bool do_diag = diag::every(g_last_pm_diag, 5000);
        const uint64_t dict_klass = typeinfo_rise(rise::Dict_int_PC_TypeInfo);

        if (do_diag) {
            diag::log("RISE-DBG", "PM klass=0x%llx name='%s'",
                (unsigned long long)klass, class_name(klass).c_str());
            if (dict_klass)
                diag::log("RISE-DBG", "Dict klass=0x%llx name='%s'",
                    (unsigned long long)dict_klass, class_name(dict_klass).c_str());
        }

        // Scan all plausible static_fields offsets in klass structure
        for (uint64_t sf_off = 0x80; sf_off <= 0xE0; sf_off += 8) {
            const uint64_t static_fields = rptr(klass + sf_off);
            if (!plausible(static_fields)) continue;
            const uint64_t singleton = rptr(static_fields + 0x0);
            if (!obj_ptr(singleton)) continue;
            const uint64_t obj_klass = rptr(singleton);
            if (!obj_ptr(obj_klass)) continue;

            // Method 1: exact klass match
            if (obj_klass == klass) {
                if (do_diag) diag::log("RISE", "PM exact match sf_off=0x%llx", (unsigned long long)sf_off);
                g_pm_dict_off = rise::pm_dict;
                g_pm_singleton_klass = obj_klass;
                return singleton;
            }

            // Method 2: structural scan — find a Dictionary field inside the singleton
            uint64_t fields[16]{};
            mem_read(singleton, fields, sizeof(fields));

            if (do_diag) {
                diag::log("RISE-DBG", "sf_off=0x%llx sin=0x%llx klass='%s'",
                    (unsigned long long)sf_off, (unsigned long long)singleton,
                    class_name(obj_klass).c_str());
                for (int d = 0; d < 16; d++) {
                    uint64_t v = untag(fields[d]);
                    if (v) diag::log("RISE-DBG", "  +0x%02x: 0x%llx obj=%d",
                        d * 8, (unsigned long long)v, int(obj_ptr(v)));
                }
            }

            for (int d = 2; d < 16; d++) {
                uint64_t f = untag(fields[d]);
                if (looks_like_dict(f, dict_klass)) {
                    uint64_t found_off = (uint64_t)(d * 8);
                    diag::log("RISE", "PM dict-scan: sin=0x%llx dict_off=0x%llx sf_off=0x%llx name='%s'",
                        (unsigned long long)singleton, (unsigned long long)found_off,
                        (unsigned long long)sf_off, class_name(obj_klass).c_str());
                    g_pm_dict_off = found_off;
                    g_pm_singleton_klass = obj_klass;
                    return singleton;
                }
            }

            // Method 3: class name contains "PlayerManager"
            std::string name = class_name(obj_klass);
            if (name.find("Player") != std::string::npos && name.find("Manager") != std::string::npos) {
                diag::log("RISE", "PM name-match: sin=0x%llx sf_off=0x%llx name='%s'",
                    (unsigned long long)singleton, (unsigned long long)sf_off, name.c_str());
                g_pm_dict_off = rise::pm_dict;
                g_pm_singleton_klass = obj_klass;
                return singleton;
            }

            // Method 4: relaxed dict scan without klass requirement
            if (!dict_klass) {
                for (int d = 2; d < 16; d++) {
                    uint64_t f = untag(fields[d]);
                    if (looks_like_dict(f, 0)) {
                        uint64_t found_off = (uint64_t)(d * 8);
                        diag::log("RISE", "PM dict-notype: sin=0x%llx dict_off=0x%llx sf_off=0x%llx",
                            (unsigned long long)singleton, (unsigned long long)found_off,
                            (unsigned long long)sf_off);
                        g_pm_dict_off = found_off;
                        g_pm_singleton_klass = obj_klass;
                        return singleton;
                    }
                }
            }
        }

        return 0;
    }

    // ---- Dictionary<int, PlayerController> iteration ----
    inline bool read_dict_players(uint64_t dict, std::vector<uint64_t>& controllers) {
        controllers.clear();
        if (!obj_ptr(dict)) return false;
        const int count = rpm<int>(dict + rise::dict_count);
        if (count <= 0 || count > 64) return false;
        const uint64_t entries_arr = rptr(dict + rise::dict_entries);
        if (!obj_ptr(entries_arr)) return false;
        const uint64_t arr_len = rpm<uint64_t>(entries_arr + rise::array_length);
        if (arr_len < uint64_t(count) || arr_len > 4096) return false;
        const size_t entry_count = size_t(std::min(int(arr_len), 128));
        const size_t block_size = entry_count * size_t(rise::dict_entry_size);
        std::vector<uint8_t> buf(block_size, 0);
        if (!mem_read(entries_arr + rise::array_data, buf.data(), block_size)) return false;
        for (size_t i = 0; i < entry_count; ++i) {
            const uint8_t* entry = buf.data() + i * size_t(rise::dict_entry_size);
            int32_t hashCode = 0;
            std::memcpy(&hashCode, entry, 4);
            if (hashCode < 0) continue;
            uint64_t value = 0;
            std::memcpy(&value, entry + rise::dict_entry_value, 8);
            value = untag(value);
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
        // Revalidate singleton using detected runtime klass
        if (!g_pm_singleton_klass || rptr(g_pm_singleton) != g_pm_singleton_klass) {
            g_pm_singleton = 0;
            g_pm_singleton_klass = 0;
            return out;
        }
        // Compute field shift from detected dict offset vs expected
        const int64_t field_shift = (int64_t)g_pm_dict_off - (int64_t)rise::pm_dict;
        // Read local player (apply same shift)
        const uint64_t local_pc = rptr(g_pm_singleton + rise::pm_local_player + field_shift);
        // Read dictionary using detected offset
        const uint64_t dict = rptr(g_pm_singleton + g_pm_dict_off);
        std::vector<uint64_t> controllers;
        if (!read_dict_players(dict, controllers)) return out;
        // Build snapshots
        const uint64_t pc_klass = typeinfo_rise(rise::PlayerController_TypeInfo);
        for (uint64_t pc : controllers) {
            uint64_t pc_k = rptr(pc);
            if (!obj_ptr(pc_k)) continue;
            // Exact match preferred; skip only if klass is clearly invalid
            if (pc_klass && pc_k != pc_klass) {
                // Relaxed: accept if class name contains "Player"
                static bool warned = false;
                if (!warned) {
                    diag::log("RISE-DBG", "PC klass mismatch: got=0x%llx expect=0x%llx name='%s'",
                        (unsigned long long)pc_k, (unsigned long long)pc_klass, class_name(pc_k).c_str());
                    warned = true;
                }
            }
            rise_player p{};
            p.controller = pc;
            p.is_local = (pc == local_pc);
            p.team = rpm<uint8_t>(pc + rise::pc_team);
            p.net_ctrl = rptr(pc + rise::pc_net_ctrl);
            p.transform = rptr(pc + rise::pc_transform);
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
            const uint64_t static_fields = rptr(mc_klass + sf_off);
            if (!plausible(static_fields)) continue;
            const uint64_t mc_instance = rptr(static_fields + rise::main_camera_singleton);
            if (!obj_ptr(mc_instance)) continue;
            if (rptr(mc_instance) != mc_klass) continue;
            // CameraMovementController.Camera at +0x98
            const uint64_t managed_camera = rptr(mc_instance + rise::cmc_camera);
            if (!obj_ptr(managed_camera)) continue;
            const uint64_t camera_klass = typeinfo_rise(rise::Camera_TypeInfo);
            if (!camera_klass || rptr(managed_camera) != camera_klass) continue;
            const uint64_t native = rptr(managed_camera + rise::unity_cached_ptr);
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
        const uint64_t native = rptr(managed_transform + rise::unity_cached_ptr);
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
        g_pm_singleton_klass = 0;
        g_pm_dict_off = rise::pm_dict;
        g_camera_native = 0;
        g_camera_view_off = -1;
        g_camera_proj_off = -1;
        g_il2cpp_base = 0;
        player::g_transform_cache.clear();
    }
}
