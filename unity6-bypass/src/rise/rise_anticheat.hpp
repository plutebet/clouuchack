#pragma once

#include "rise_offsets.hpp"
#include "rise_game.hpp"
#include "../other/memory.hpp"
#include "../other/diag.hpp"
#include "../protect/oxorany.hpp"
#include <cstdio>
#include <cstring>

namespace rise_ac {

    inline uint64_t g_libcac_base = 0;
    inline bool g_cac_patched = false;
    inline bool g_singleton_nulled = false;
    inline long long g_last_tick = 0;
    inline long long g_last_cac_scan = 0;
    inline int g_patch_count = 0;
    inline int g_null_count = 0;

    // ARM64 instruction constants
    static constexpr uint32_t ARM64_RET       = 0xD65F03C0; // ret x30
    static constexpr uint32_t ARM64_MOV_W0_0  = 0x52800000; // movz w0, #0
    static constexpr uint32_t ARM64_MOV_W0_1  = 0x52800020; // movz w0, #1
    static constexpr uint32_t ARM64_NOP        = 0xD503201F; // nop

    // libcac.so function offsets (from ELF symbol table)
    static constexpr uint64_t CAC_START            = 0xCD2B0;
    static constexpr uint64_t CAC_GET_VERDICT      = 0xCD2B4;
    static constexpr uint64_t CAC_STATUS           = 0xCD3CC;
    static constexpr uint64_t CAC_SERVER_OK        = 0xCD3E0;
    static constexpr uint64_t CAC_SET_PLAYER       = 0xCD3F8;
    static constexpr uint64_t CAC_POLL_REPORT      = 0xCD3FC;
    static constexpr uint64_t CAC_REPORT_MANAGED   = 0xCD410;
    static constexpr uint64_t CAC_SET_RULES        = 0xCD628;

    // libcac.so state variables (.data section)
    static constexpr uint64_t CAC_START_S          = 0x10B600;
    static constexpr uint64_t CAC_GET_VERDICT_S    = 0x10B604;
    static constexpr uint64_t CAC_STATUS_S         = 0x10B608;
    static constexpr uint64_t CAC_SET_PLAYER_S     = 0x10B60C;
    static constexpr uint64_t CAC_REPORT_MANAGED_S = 0x10B610;
    static constexpr uint64_t CAC_POLL_REPORT_S    = 0x10B614;
    static constexpr uint64_t CAC_SERVER_OK_S      = 0x10B618;
    static constexpr uint64_t CAC_SET_RULES_S      = 0x10B61C;

    // Find libcac.so load base in the game process by scanning /proc/<pid>/maps.
    inline uint64_t find_libcac() {
        if (proc::pid <= 0) return 0;
        char path[64];
        std::snprintf(path, sizeof(path), "/proc/%d/maps", proc::pid);
        FILE* f = std::fopen(path, "r");
        if (!f) return 0;
        char line[512];
        uint64_t base = 0;
        while (std::fgets(line, sizeof(line), f)) {
            if (!std::strstr(line, oxorany("libcac.so"))) continue;
            unsigned long long begin = 0, end = 0;
            char perms[8] = {};
            unsigned long long offset = 0;
            if (std::sscanf(line, "%llx-%llx %4s %llx", &begin, &end, perms, &offset) == 4) {
                if (perms[2] == 'x' && offset == 0) {
                    base = uint64_t(begin);
                    break;
                }
                if (base == 0 && offset == 0) base = uint64_t(begin);
            }
        }
        std::fclose(f);
        return base;
    }

    // Patch a function to RET immediately.
    inline bool patch_ret(uint64_t addr) {
        return wpm<uint32_t>(addr, ARM64_RET);
    }

    // Patch a function to: MOV W0, #imm16; RET
    inline bool patch_mov_ret(uint64_t addr, uint32_t mov_insn) {
        bool ok = wpm<uint32_t>(addr, mov_insn);
        ok = wpm<uint32_t>(addr + 4, ARM64_RET) && ok;
        return ok;
    }

    // Verify a patch by reading back.
    inline bool verify_patch(uint64_t addr, uint32_t expected) {
        return rpm<uint32_t>(addr) == expected;
    }

    // Patch libcac.so functions to neutralize the AC engine.
    // cac_get_verdict → return 0 (clean)
    // cac_report_managed, cac_poll_report, cac_set_rules → RET (nop)
    // cac_server_ok → return 1 (always ok)
    // cac_start → RET (prevent initialization of scan threads)
    inline bool patch_libcac(uint64_t base) {
        if (!base) return false;
        int patched = 0;

        // cac_start → RET (prevent scan thread creation)
        if (patch_ret(base + CAC_START)) patched++;

        // cac_get_verdict → MOV W0, #0; RET (return "clean")
        if (patch_mov_ret(base + CAC_GET_VERDICT, ARM64_MOV_W0_0)) patched++;

        // cac_report_managed → RET (suppress HTTP reports)
        if (patch_ret(base + CAC_REPORT_MANAGED)) patched++;

        // cac_poll_report → RET
        if (patch_ret(base + CAC_POLL_REPORT)) patched++;

        // cac_set_rules → RET (ignore server-pushed rules)
        if (patch_ret(base + CAC_SET_RULES)) patched++;

        // cac_server_ok → MOV W0, #1; RET (always reachable)
        if (patch_mov_ret(base + CAC_SERVER_OK, ARM64_MOV_W0_1)) patched++;

        // cac_status → MOV W0, #1; RET (always "running")
        if (patch_mov_ret(base + CAC_STATUS, ARM64_MOV_W0_1)) patched++;

        // Spoof state variables in .data: set status/verdict to "clean/ok"
        wpm<int32_t>(base + CAC_START_S, 1);           // started
        wpm<int32_t>(base + CAC_STATUS_S, 1);          // running
        wpm<int32_t>(base + CAC_SERVER_OK_S, 1);       // server ok
        wpm<int32_t>(base + CAC_GET_VERDICT_S, 0);     // verdict: clean

        const bool ok = patched >= 5;
        if (ok) {
            g_patch_count++;
            diag::log("RISE-AC", "libcac.so patched (%d/7 functions) base=0x%llx",
                      patched, (unsigned long long)base);
        }
        return ok;
    }

    // Null the CoreAcClient managed singleton via its TypeInfo.
    // LazySingleton: klass → static_fields → +0x0 = instance ptr.
    inline bool null_core_ac_singleton() {
        const uint64_t klass = rise_game::typeinfo_rise(rise::CoreAcClient_TypeInfo);
        if (!klass) return false;

        for (uint64_t sf_off : {uint64_t(0xB8), uint64_t(0xB0), uint64_t(0xC0)}) {
            const uint64_t static_fields = rptr(klass + sf_off);
            if (!plausible(static_fields)) continue;
            const uint64_t singleton = rptr(static_fields);
            if (!singleton) continue;
            const uint64_t obj_klass = rptr(singleton);
            if (obj_klass != klass) continue;

            const uint64_t zero = 0;
            if (mem_write(static_fields, &zero, sizeof(zero))) {
                g_null_count++;
                diag::log("RISE-AC", "CoreAcClient singleton nulled (was 0x%llx, attempt #%d)",
                          (unsigned long long)singleton, g_null_count);
                return true;
            }
        }
        return false;
    }

    // Block AC network endpoints via iptables (belt-and-suspenders).
    // Runs once at startup. Drops outbound to game.standrise.cc:2284.
    inline void block_ac_network() {
        static bool done = false;
        if (done) return;
        done = true;
        // Drop outbound TCP to the AC reporting port.
        // The game still connects to the game server on other ports.
        system(oxorany("iptables -C OUTPUT -p tcp --dport 2284 -j DROP 2>/dev/null || "
                       "iptables -A OUTPUT -p tcp --dport 2284 -j DROP 2>/dev/null"));
        diag::log("RISE-AC", "iptables: blocked outbound TCP :2284");
    }

    // Main tick — called from the render loop every frame.
    // Actual work is throttled to every 2 seconds.
    inline void tick() {
        const long long now = diag::now_ms();
        if (now - g_last_tick < 2000) return;
        g_last_tick = now;

        if (proc::pid <= 0 || proc::lib == 0) return;

        // Layer 1: find and patch libcac.so (once, re-verify periodically)
        if (!g_cac_patched || now - g_last_cac_scan > 30000) {
            g_last_cac_scan = now;
            if (!g_libcac_base) {
                g_libcac_base = find_libcac();
                if (g_libcac_base)
                    diag::log("RISE-AC", "libcac.so found at 0x%llx", (unsigned long long)g_libcac_base);
            }
            if (g_libcac_base) {
                if (!g_cac_patched) {
                    g_cac_patched = patch_libcac(g_libcac_base);
                } else {
                    // Re-verify patches haven't been restored by integrity checks
                    if (!verify_patch(g_libcac_base + CAC_GET_VERDICT, ARM64_MOV_W0_0)) {
                        diag::log("RISE-AC", "patch integrity lost, re-patching");
                        g_cac_patched = false;
                        patch_libcac(g_libcac_base);
                        g_cac_patched = true;
                    }
                }
            }
        }

        // Layer 2: null CoreAcClient singleton (continuous, it may be recreated)
        if (rise_game::g_il2cpp_base) {
            null_core_ac_singleton();
        }
    }

    // Reset state on process change.
    inline void reset() {
        g_libcac_base = 0;
        g_cac_patched = false;
        g_singleton_nulled = false;
        g_last_tick = 0;
        g_last_cac_scan = 0;
    }
}
