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
    inline uint64_t g_pm_singleton_klass = 0;
    inline uint64_t g_pm_dict_off = rise::pm_dict;
    inline uint64_t g_camera_native = 0;
    inline int g_camera_view_off = -1;
    inline int g_camera_proj_off = -1;
    inline long long g_camera_locked_ms = 0;
    inline long long g_last_camera_probe = 0;
    inline const char* g_pm_status = "init";

    // ---- deep diagnostic (runs once) ----
    inline void diag_typeinfo_once() {
        static bool done = false;
        if (done || !g_il2cpp_base || proc::pid <= 0) return;
        done = true;

        uint8_t magic[4]{};
        bool elf_ok = mem_read(g_il2cpp_base, magic, 4);
        diag::log("RISE-DBG", "ELF check base=0x%llx: read=%d magic=%02x%02x%02x%02x (%s)",
            (unsigned long long)g_il2cpp_base, int(elf_ok),
            magic[0], magic[1], magic[2], magic[3],
            (magic[0]==0x7f && magic[1]=='E' && magic[2]=='L' && magic[3]=='F') ? "OK" : "BAD");

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

    // ---- TypeInfo resolution ----
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
        uint64_t controller;
        uint64_t net_ctrl;
        uint64_t transform;
        uint8_t team;
        int health;
        int armor;
        bool alive;
        bool is_local;
    };

    inline bool read_health(uint64_t net_ctrl, int& hp, int& armor, bool& alive) {
        if (!obj_ptr(net_ctrl)) return false;
        struct HealthBlock {
            int32_t health;
            int32_t armor;
            uint8_t is_alive;
        };
        HealthBlock block{};
        if (!mem_read(net_ctrl + rise::ctrl_health, &block, sizeof(block))) return false;
        if (block.health < 0 || block.health > 1000) return false;
        hp = block.health;
        armor = block.armor;
        alive = block.is_alive != 0;
        return true;
    }

    // ---- String helpers for class name reading ----
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

    // ---- Try to find PM singleton on a klass's static_fields ----
    inline uint64_t try_static_fields(uint64_t target_klass, uint64_t pm_klass,
                                       uint64_t dict_klass, uint64_t sf_off) {
        uint64_t sf = rptr(target_klass + sf_off);
        if (!plausible(sf)) return 0;
        uint64_t sin = rptr(sf);
        if (!obj_ptr(sin)) return 0;
        uint64_t sk = rptr(sin);
        if (sk == pm_klass) return sin;
        // structural dict match as fallback
        if (obj_ptr(sk)) {
            for (uint64_t d = 0x10; d <= 0x78; d += 8) {
                if (looks_like_dict(rptr(sin + d), dict_klass)) return sin;
            }
        }
        return 0;
    }

    // ---- Data section scan: find any object with PM klass in il2cpp rw-p ----
    inline uint64_t scan_data_for_pm(uint64_t pm_klass) {
        if (!pm_klass || proc::pid <= 0) return 0;

        char path[64];
        std::snprintf(path, sizeof(path), "/proc/%d/maps", proc::pid);
        FILE* f = std::fopen(path, "r");
        if (!f) return 0;

        struct range { uint64_t start, end; };
        std::vector<range> ranges;
        char line[512];
        while (std::fgets(line, sizeof(line), f)) {
            if (!std::strstr(line, "libil2cpp.so")) continue;
            if (!std::strstr(line, "rw-p")) continue;
            unsigned long long s = 0, e = 0;
            if (std::sscanf(line, "%llx-%llx", &s, &e) == 2)
                ranges.push_back({s, e});
        }
        std::fclose(f);

        static constexpr size_t CHUNK = 32768;
        uint8_t buf[CHUNK];
        int checked = 0;

        for (const auto& r : ranges) {
            for (uint64_t addr = r.start; addr < r.end; addr += CHUNK) {
                size_t n = std::min((size_t)(r.end - addr), CHUNK);
                if (!mem_read(addr, buf, n)) continue;
                const uint64_t* vals = (const uint64_t*)buf;
                int cnt = n / 8;
                for (int i = 0; i < cnt; i++) {
                    uint64_t v = untag(vals[i]);
                    if (!obj_ptr(v)) continue;
                    checked++;
                    // Level 1: v → managed object with PM klass
                    uint64_t k = rptr(v);
                    if (k == pm_klass) {
                        diag::log("RISE", "DATA-SCAN L1: PM at 0x%llx data=0x%llx checked=%d",
                            (unsigned long long)v, (unsigned long long)(addr + i*8), checked);
                        return v;
                    }
                    // Level 2: v → sf_buffer → singleton with PM klass
                    uint64_t uk = untag(k);
                    if (obj_ptr(uk)) {
                        uint64_t k2 = rptr(uk);
                        if (k2 == pm_klass) {
                            diag::log("RISE", "DATA-SCAN L2: PM at 0x%llx sf=0x%llx data=0x%llx checked=%d",
                                (unsigned long long)uk, (unsigned long long)v,
                                (unsigned long long)(addr + i*8), checked);
                            return uk;
                        }
                    }
                }
            }
        }

        diag::log("RISE-DBG", "DATA-SCAN: no PM found, checked=%d ptrs", checked);
        return 0;
    }

    // ---- PlayerManager singleton resolution ----
    inline long long g_last_pm_diag = 0;

    inline uint64_t resolve_pm_singleton() {
        const uint64_t klass = typeinfo_rise(rise::PlayerManager_TypeInfo);
        if (!klass) { g_pm_status = "TypeInfo not resolved"; return 0; }

        const bool do_diag = diag::every(g_last_pm_diag, 5000);
        const uint64_t dict_klass = typeinfo_rise(rise::Dict_int_PC_TypeInfo);

        if (do_diag) {
            diag::log("RISE-DBG", "PM klass=0x%llx name='%s'",
                (unsigned long long)klass, class_name(klass).c_str());
            if (dict_klass)
                diag::log("RISE-DBG", "Dict klass=0x%llx name='%s'",
                    (unsigned long long)dict_klass, class_name(dict_klass).c_str());
        }

        // One-time klass structure dump
        static bool klass_dumped = false;
        if (!klass_dumped) {
            klass_dumped = true;
            uint64_t kd[32]{};
            mem_read(klass, kd, sizeof(kd));
            for (int i = 0; i < 32; i++) {
                uint64_t v = untag(kd[i]);
                char desc[80] = "";
                if (obj_ptr(v)) {
                    std::string n = class_name(v);
                    if (n != "?") std::snprintf(desc, sizeof(desc), " cls='%s'", n.c_str());
                }
                diag::log("RISE-DBG", "klass[+0x%02x]=0x%016llx%s%s",
                    i*8, (unsigned long long)kd[i],
                    obj_ptr(v) ? " [ptr]" : (v ? "" : " [NULL]"), desc);
            }
        }

        // Phase 1: scan static_fields offsets on PM klass itself (0x80-0xE0)
        for (uint64_t sf_off = 0x80; sf_off <= 0xE0; sf_off += 8) {
            const uint64_t static_fields = rptr(klass + sf_off);
            if (!plausible(static_fields)) continue;
            const uint64_t singleton = rptr(static_fields);
            if (!obj_ptr(singleton)) continue;
            const uint64_t obj_klass = rptr(singleton);
            if (!obj_ptr(obj_klass)) continue;

            if (obj_klass == klass) {
                if (do_diag) diag::log("RISE", "P1 exact sf_off=0x%llx", (unsigned long long)sf_off);
                g_pm_dict_off = rise::pm_dict;
                g_pm_singleton_klass = klass;
                g_pm_status = "OK (P1 exact)";
                return singleton;
            }

            // structural dict match
            for (uint64_t d = 0x10; d <= 0x78; d += 8) {
                if (looks_like_dict(rptr(singleton + d), dict_klass)) {
                    diag::log("RISE", "P1 dict sf_off=0x%llx dict_off=0x%llx name='%s'",
                        (unsigned long long)sf_off, (unsigned long long)d,
                        class_name(obj_klass).c_str());
                    g_pm_dict_off = d;
                    g_pm_singleton_klass = obj_klass;
                    g_pm_status = "OK (P1 dict)";
                    return singleton;
                }
            }

            // name match
            std::string name = class_name(obj_klass);
            if (name.find("Player") != std::string::npos && name.find("Manager") != std::string::npos) {
                diag::log("RISE", "P1 name sf_off=0x%llx name='%s'",
                    (unsigned long long)sf_off, name.c_str());
                g_pm_dict_off = rise::pm_dict;
                g_pm_singleton_klass = obj_klass;
                g_pm_status = "OK (P1 name)";
                return singleton;
            }
        }

        // Phase 2: Walk parent/ancestor class chain
        // The PM singleton might be on LazySingleton<PlayerManager>'s static_fields
        // Try multiple offsets for the 'parent' field in Il2CppClass
        static const uint64_t sf_offsets[] = {0xB0, 0xA8, 0xB8, 0xC0, 0xC8, 0x98, 0xA0};
        for (uint64_t poff : {0x58ULL, 0x50ULL, 0x48ULL, 0x60ULL, 0x40ULL, 0x68ULL, 0x70ULL}) {
            uint64_t ancestor = untag(rptr(klass + poff));
            if (!obj_ptr(ancestor) || ancestor == klass) continue;

            // Walk up to 4 levels of ancestry
            for (int depth = 0; depth < 4 && obj_ptr(ancestor) && ancestor != klass; depth++) {
                if (do_diag && depth == 0) {
                    std::string an = class_name(ancestor);
                    diag::log("RISE-DBG", "P2 poff=0x%llx ancestor[%d]=0x%llx name='%s'",
                        (unsigned long long)poff, depth, (unsigned long long)ancestor, an.c_str());
                }

                for (uint64_t sf_off : sf_offsets) {
                    uint64_t sin = try_static_fields(ancestor, klass, dict_klass, sf_off);
                    if (sin) {
                        uint64_t sk = rptr(sin);
                        diag::log("RISE", "P2 ancestor: sin=0x%llx poff=0x%llx depth=%d sf=0x%llx",
                            (unsigned long long)sin, (unsigned long long)poff, depth, (unsigned long long)sf_off);
                        g_pm_singleton_klass = (sk == klass) ? klass : sk;
                        g_pm_dict_off = rise::pm_dict;
                        for (uint64_t d = 0x10; d <= 0x78; d += 8) {
                            if (looks_like_dict(rptr(sin + d), dict_klass)) {
                                g_pm_dict_off = d;
                                break;
                            }
                        }
                        g_pm_status = "OK (P2 ancestor)";
                        return sin;
                    }
                }

                // Move to next ancestor using same parent offset
                uint64_t next = untag(rptr(ancestor + poff));
                if (!obj_ptr(next) || next == ancestor || next == klass) break;
                ancestor = next;
            }
        }

        // Phase 3: Monitor previously-NULL klass offsets for newly populated values
        static uint64_t null_offsets[16];
        static int null_count = 0;
        static bool null_scanned = false;
        if (!null_scanned) {
            null_scanned = true;
            null_count = 0;
            for (uint64_t off = 0x40; off <= 0x100; off += 8) {
                if (off >= 0x80 && off <= 0xE0) continue; // already checked in Phase 1
                uint64_t v = rptr(klass + off);
                if (v == 0 && null_count < 16)
                    null_offsets[null_count++] = off;
            }
            if (do_diag) diag::log("RISE-DBG", "P3: %d NULL offsets outside 0x80-0xE0", null_count);
        }
        for (int i = 0; i < null_count; i++) {
            uint64_t v = rptr(klass + null_offsets[i]);
            if (!plausible(v)) continue;
            uint64_t sin = rptr(v);
            if (!obj_ptr(sin)) continue;
            uint64_t sk = rptr(sin);
            if (sk == klass) {
                diag::log("RISE", "P3 null-woke: sin=0x%llx off=0x%llx",
                    (unsigned long long)sin, (unsigned long long)null_offsets[i]);
                g_pm_singleton_klass = klass;
                g_pm_dict_off = rise::pm_dict;
                for (uint64_t d = 0x10; d <= 0x78; d += 8) {
                    if (looks_like_dict(rptr(sin + d), dict_klass)) { g_pm_dict_off = d; break; }
                }
                g_pm_status = "OK (P3 null-woke)";
                return sin;
            }
        }

        // Phase 4: Brute-force data section scan (every 15s)
        static long long last_data_scan = 0;
        if (diag::every(last_data_scan, 15000)) {
            uint64_t found = scan_data_for_pm(klass);
            if (found) {
                g_pm_singleton_klass = klass;
                g_pm_dict_off = rise::pm_dict;
                for (uint64_t d = 0x10; d <= 0x78; d += 8) {
                    if (looks_like_dict(rptr(found + d), dict_klass)) { g_pm_dict_off = d; break; }
                }
                g_pm_status = "OK (P4 datascan)";
                return found;
            }
        }

        g_pm_status = "JOIN MATCH";
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
            diag::log("RISE", "PlayerManager singleton=0x%llx dict_off=0x%llx status=%s",
                (unsigned long long)g_pm_singleton, (unsigned long long)g_pm_dict_off, g_pm_status);
        }
        // Revalidate
        if (!g_pm_singleton_klass || rptr(g_pm_singleton) != g_pm_singleton_klass) {
            g_pm_singleton = 0;
            g_pm_singleton_klass = 0;
            return out;
        }
        const int64_t field_shift = (int64_t)g_pm_dict_off - (int64_t)rise::pm_dict;
        const uint64_t local_pc = rptr(g_pm_singleton + rise::pm_local_player + field_shift);
        const uint64_t dict = rptr(g_pm_singleton + g_pm_dict_off);
        std::vector<uint64_t> controllers;
        if (!read_dict_players(dict, controllers)) return out;
        const uint64_t pc_klass = typeinfo_rise(rise::PlayerController_TypeInfo);
        for (uint64_t pc : controllers) {
            uint64_t pc_k = rptr(pc);
            if (!obj_ptr(pc_k)) continue;
            if (pc_klass && pc_k != pc_klass) {
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
    inline bool resolve_camera(matrix& view, matrix& proj) {
        if (g_camera_native && g_camera_view_off >= 0 && g_camera_proj_off >= 0) {
            if (read_matrix_pair(g_camera_native, g_camera_view_off, g_camera_proj_off, view, proj))
                return true;
            g_camera_native = 0;
            g_camera_view_off = -1;
            g_camera_proj_off = -1;
        }
        const long long now = diag::now_ms();
        if (now - g_last_camera_probe < 200) return false;
        g_last_camera_probe = now;

        const uint64_t mc_klass = typeinfo_rise(rise::MainCamera_TypeInfo);
        if (!mc_klass) return false;

        // Try static_fields on MainCamera klass AND its ancestors
        auto try_mc = [&](uint64_t target_klass) -> bool {
            for (uint64_t sf_off : {0xB0ULL, 0xB8ULL, 0xA8ULL, 0xC0ULL}) {
                const uint64_t static_fields = rptr(target_klass + sf_off);
                if (!plausible(static_fields)) continue;
                const uint64_t mc_instance = rptr(static_fields + rise::main_camera_singleton);
                if (!obj_ptr(mc_instance)) continue;
                // Accept if klass matches mc_klass OR any parent matches
                uint64_t ik = rptr(mc_instance);
                if (!obj_ptr(ik)) continue;
                // Try camera resolution from this instance
                const uint64_t managed_camera = rptr(mc_instance + rise::cmc_camera);
                if (!obj_ptr(managed_camera)) continue;
                const uint64_t camera_klass = typeinfo_rise(rise::Camera_TypeInfo);
                if (camera_klass && rptr(managed_camera) != camera_klass) continue;
                const uint64_t native = rptr(managed_camera + rise::unity_cached_ptr);
                if (!obj_ptr(native) || (native & 7) != 0) continue;
                int v_off = -1, p_off = -1;
                if (detect_pair(native, view, proj, v_off, p_off, "rise-MainCamera")) {
                    g_camera_native = native;
                    g_camera_view_off = v_off;
                    g_camera_proj_off = p_off;
                    g_camera_locked_ms = now;
                    diag::log("RISE", "camera locked native=0x%llx view=+0x%x proj=+0x%x sf=0x%llx",
                              (unsigned long long)native, v_off, p_off, (unsigned long long)sf_off);
                    return true;
                }
            }
            return false;
        };

        if (try_mc(mc_klass)) return true;

        // Try ancestors of MainCamera klass
        for (uint64_t poff : {0x58ULL, 0x50ULL, 0x48ULL, 0x60ULL}) {
            uint64_t anc = untag(rptr(mc_klass + poff));
            for (int d = 0; d < 3 && obj_ptr(anc) && anc != mc_klass; d++) {
                if (try_mc(anc)) return true;
                uint64_t next = untag(rptr(anc + poff));
                if (!obj_ptr(next) || next == anc) break;
                anc = next;
            }
        }

        return false;
    }

    // ---- Transform position ----
    inline bool player_position(uint64_t managed_transform, Vector3& out) {
        if (!obj_ptr(managed_transform)) return false;
        const uint64_t native = rptr(managed_transform + rise::unity_cached_ptr);
        if (!obj_ptr(native)) return false;
        return player::native_transform_position(native, out);
    }

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
        g_pm_status = "init";
        player::g_transform_cache.clear();
    }
}
