
// ================================================================================
// -----------------[ TELEGRAM SOURCE ]-----------------
// * BOT:     t.me/imguisource_bot
// * CHANNEL: t.me/ImGui_Source_FIXNEY (ImGui Source)
// * CH 2:    t.me/il2cpp_src (shit src 🧨)
// -----------------------------------------------------
// ================================================================================

#pragma once

#include "../other/memory.hpp"
#include "../other/status.hpp"
#include "../other/crash.hpp"
#include "../protect/oxorany.hpp"
#include <stdint.h>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>
#include <cctype>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <sys/resource.h>
#include <sys/syscall.h>

struct matrix {
    float m11, m12, m13, m14;
    float m21, m22, m23, m24;
    float m31, m32, m33, m34;
    float m41, m42, m43, m44;
};

// ---------------------------------------------------------------------------
// Build profile: Standoff 2 1.1.0 (Halloween 2026, Unity 6000.3.13f1), ARM64.
// Every value below is tied to this exact libunity.so. Sources:
//   * TypeInfo RVAs      -> script.json  (dump x64 1.1.0, Il2CppDumper)
//   * field offsets      -> dump.cs      (same dump)
//   * build-id / sha256  -> remap_final/offsets_remap.json (not re-derived here)
// Anything that is only a hypothesis is marked as such.
// ---------------------------------------------------------------------------
namespace v100 {
    inline constexpr char profile_name[] = "Standoff2-1.1.0-Halloween2026-Unity6-arm64-v17";
    // GNU build-id note of libunity.so (first 8 bytes, lld "fast" style).
    // Taken from remap_final; game::verify_build() refuses to run ESP when the
    // mapped library reports a different id.
    inline constexpr char libunity_build_id[] = "69815f2befe135c2";
    inline constexpr char libunity_sha256[] = "ba0966976204fa3f7a2755c9588d32299fe7eb794b712c9f1a444e046cbe7c13";

    // ---- TypeInfo slots (RVA), script.json ----
    inline constexpr uint64_t registry_typeinfo = 0xCD32028;      // nba
    inline constexpr uint64_t store_typeinfo = 0xCD303E8;         // ibc
    inline constexpr uint64_t player_typeinfo = 0xCD32658;        // odq : wwv
    inline constexpr uint64_t transforms_typeinfo = 0xCD31E68;    // mtc : yvt (player transform component)
    inline constexpr uint64_t model_typeinfo = 0xCD2DDE8;         // bis
    inline constexpr uint64_t stats_typeinfo = 0xCD360F0;         // ydm
    inline constexpr uint64_t camera_typeinfo = 0xCD1F798;        // UnityEngine.Camera
    inline constexpr uint64_t ios_typeinfo = 0xCD36248;           // yjp : urz (kgd + ydm + vqn + 2x Transform)
    inline constexpr uint64_t presentation_typeinfo = 0xCD2E320;  // chc : pjy (vqn, uyg, kgd, yvt, szi, PlayerAnimationData)
    inline constexpr uint64_t animation_data_typeinfo = 0xCD28488;// Axlebolt...PlayerAnimation.PlayerAnimationData
    inline constexpr uint64_t camera_main_typeinfo = 0xCD311F8;   // kmc : cgy, etx (world camera controller, hypothesis)
    inline constexpr uint64_t camera_arms_typeinfo = 0xCD357B0;   // wot : cgy, nri (arms camera controller, hypothesis)
    inline constexpr uint64_t camera_state_typeinfo = 0xCD35080;  // vqn (two bor)
    inline constexpr uint64_t bor_typeinfo = 0xCD2DEF0;           // bor (ValueTransform camera state)
    inline constexpr uint64_t kgd_typeinfo = 0xCD31080;           // kgd (two quaternions)
    inline constexpr uint64_t player_string_literal = 0xCDADA30;  // ScriptString index 0 (""), slot may be null until used
    inline constexpr uint64_t data_relro_begin = 0xB886F80;
    inline constexpr uint64_t data_end = 0xC46CF80;
    inline constexpr uint64_t text_begin = 0x535F940;
    inline constexpr uint64_t text_end = 0xB882E00;
    inline constexpr uint64_t native_camera_get_main = 0x6BE9604; // UnityEngine.Camera.get_main (reference only)
    // UnityEngine.Physics.Linecast(Vector3, Vector3, out RaycastHit, int layerMask) - script.json 1.1.0.
    // Trigger visible check. RaycastHit.distance sits at +0x1C (dump.cs).
    inline constexpr uint64_t physics_linecast_hit = 0x92DA5F0;

    // ---- field offsets, dump.cs 1.1.0 ----
    inline constexpr uint64_t registry_store = 0x30;        // nba.fume : ibc
    inline constexpr uint64_t store_objects = 0x18;         // ibc.bwol (List<ohn>, validated at runtime)
    inline constexpr uint64_t list_items = 0x10;
    inline constexpr uint64_t list_count = 0x18;
    inline constexpr uint64_t array_length = 0x18;
    inline constexpr uint64_t array_data = 0x20;
    inline constexpr uint64_t net_object_id = 0x10;         // wwv.sumb
    inline constexpr uint64_t player_world = 0x18;          // wwv.vuxu : qyn
    inline constexpr uint64_t player_components = 0x20;     // wwv.yvjn : cpa (six component arrays +0x10..+0x38)
    inline constexpr uint64_t player_parts = 0x28;          // wwv.bvvg : nzk
    inline constexpr uint64_t player_local = 0x44;          // wwv +0x44/+0x45/+0x46 bool flags
    inline constexpr uint64_t player_transform = 0x68;      // odq.aiyh : yvt (mtc)
    inline constexpr uint64_t player_stats = 0x70;          // odq.djka : ydm
    inline constexpr uint64_t player_camera_state = 0x78;   // odq.gjvt : vqn
    inline constexpr uint64_t player_view_mode = 0x80;      // odq.jkhm : dvp (0 third person, 1 first person)
    inline constexpr uint64_t player_team = 0x90;           // odq.<pley> byte
    inline constexpr uint64_t stats_hp = 0x10;              // ydm.espa (ushort), getter bdqp
    inline constexpr uint64_t stats_max_hp = 0x14;          // ydm.ktmm (ushort), getter nfln
    inline constexpr uint64_t parts_array = 0x20;           // nzk.kevw : cew[]
    inline constexpr uint64_t transform_a = 0x28;           // mtc.<jpkx> Transform
    inline constexpr uint64_t transform_b = 0x30;           // mtc.<mpwq> Transform
    inline constexpr uint64_t model_transform = 0x20;       // bis.<dcul> Transform
    inline constexpr uint64_t unity_cached_ptr = 0x10;      // UnityEngine.Object.m_CachedPtr
    // Native Transform -> TransformHierarchy (Unity 6000.3, this libunity).
    // Measured, not from dump.cs: the runtime layout search found exactly
    // these values in 5 device sessions on Infinix X6873 (logs 20261114-151113,
    // -155757, -175231/default, -175419 and an earlier default log), each
    // with plausible world positions. The older 0x38/0x40 layout never
    // decoded on this build.
    inline constexpr uint64_t transform_hierarchy = 0x28;   // native Transform: TransformHierarchy*
    inline constexpr uint64_t transform_index = 0x10;       // native Transform: index in the hierarchy (int)
    inline constexpr uint64_t hierarchy_entries = 0x18;     // TransformHierarchy: TRS entries (48 bytes each)
    inline constexpr uint64_t hierarchy_parents = 0x20;     // TransformHierarchy: parent indices (int32)
    inline constexpr uint64_t camera_view = 0x70;           // native Camera worldToCamera (observed, pattern-verified at runtime)
    inline constexpr uint64_t camera_projection = 0xB0;     // native Camera projection (observed, pattern-verified at runtime)
    inline constexpr uint64_t presentation_animation_data = 0xA0; // chc.whjb : PlayerAnimationData
    inline constexpr uint64_t presentation_camera_state = 0x60;   // chc.ydtf : vqn
    inline constexpr uint64_t animation_arms_camera = 0x90;       // PlayerAnimationData.<ArmsCamera> : bor
    inline constexpr uint64_t animation_main_camera = 0x100;      // PlayerAnimationData.<MainCamera> : bor
    inline constexpr uint64_t animation_is_first_person = 0xA4;   // PlayerAnimationData.<IsFirstPerson>
    inline constexpr uint64_t controller_state = 0x10;      // cgy.elgy : bor
    inline constexpr uint64_t controller_transform = 0x18;  // cgy.hlsr : Transform
    inline constexpr uint64_t controller_camera = 0x20;     // cgy.<kmek> : UnityEngine.Camera
    inline constexpr uint64_t bor_transform = 0x10;         // bor.cowu : ValueTransform {Vector3 Position; Vector3 EulerAngles}
    inline constexpr uint64_t bor_float_a = 0x48;           // bor.<rrdl> float (FOV candidate, const zolb = 60)
    inline constexpr uint64_t bor_float_b = 0x4C;           // bor.<urpe> float (FOV candidate)
    inline constexpr uint64_t camera_state_a = 0x10;        // vqn.<razn> : bor
    inline constexpr uint64_t camera_state_b = 0x18;        // vqn.<ublg> : bor
    inline constexpr uint64_t ios_aim = 0x38;               // yjp.zmyf : kgd
    inline constexpr uint64_t ios_transform_a = 0x50;       // yjp.iohk : Transform
    inline constexpr uint64_t ios_transform_b = 0x58;       // yjp.lotd : Transform
    inline constexpr uint64_t kgd_rotation_a = 0x10;        // kgd.hzqi : Quaternion
    inline constexpr uint64_t kgd_rotation_b = 0x20;        // kgd.lacb : Quaternion

    // ---- weapon (dump.cs + script.json 1.1.0, libunity build-id 69815f2befe135c2) ----
    inline constexpr uint64_t weapon_part_typeinfo = 0xCD34CE0;   // uyg_TypeInfo (uyg : cew, WeaponPart)
    inline constexpr uint64_t weapon_typeinfo = 0xCD30D00;        // jrx_TypeInfo (active weapon instance)
    inline constexpr uint64_t weapon_part_owner = 0x30;           // uyg.kbdx WeaponContext @0x30 -> Owner NetObjectId._id (uint) @+0x0
    inline constexpr uint64_t weapon_part_team = 0x40;            // WeaponContext.Team (byte) @+0x10
    inline constexpr uint64_t weapon_part_active = 0x80;          // WeaponContext.ActiveWeapon : jrx @+0x50
    inline constexpr uint64_t weapon_id = 0x10;                   // jrx.<oqrg> WeaponId { byte Raw }
}

struct remote_map { uint64_t begin{}, end{}; bool writable{}; };
inline std::vector<remote_map> g_scan_maps;
inline uint64_t g_registry = 0, g_camera = 0;
inline int g_scan_pid = -1;
inline uint64_t g_scan_lib = 0;
// NetObject has three adjacent byte flags at +0x44/+0x45/+0x46. Their
// obfuscated build order is not stable between updates; resolve which one is
// the local-player flag from the live set instead of trusting the old order.
inline int g_local_flag_offset = int(v100::player_local);
struct player_cache_entry { uint32_t id{}; uint64_t object{}; long long seen_ms{}; };
inline std::vector<player_cache_entry> g_player_cache;

enum camera_kind : int {
    camera_kind_unknown = 0,
    camera_kind_main_native = 1,   // cgy whose bor == PlayerAnimationData.MainCamera -> UnityEngine.Camera
    camera_kind_kmc_native = 2,    // kmc controller camera, bor identity not confirmed
    camera_kind_other_native = 3,  // wot / unclassified controller camera
    camera_kind_main_state = 4,    // synthetic from PlayerAnimationData.MainCamera bor
    camera_kind_vqn_state = 5,     // synthetic from odq.vqn bor
    camera_kind_ios_transform = 6, // synthetic from yjp transform + kgd quaternion
    camera_kind_heap_camera = 7,   // any UnityEngine.Camera found on the heap
};
inline const char* camera_kind_name(int kind) {
    switch (kind) {
        case camera_kind_main_native: return "main-native";
        case camera_kind_kmc_native: return "kmc-native";
        case camera_kind_other_native: return "other-native";
        case camera_kind_main_state: return "main-bor";
        case camera_kind_vqn_state: return "vqn-bor";
        case camera_kind_ios_transform: return "ios-transform";
        case camera_kind_heap_camera: return "heap-camera";
        default: return "unknown";
    }
}
struct camera_candidate {
    uint64_t address{};
    matrix view{};
    matrix projection{};
    int view_off{-1};
    int proj_off{-1};
    int kind{camera_kind_unknown};
};

inline void reset() {
    g_registry = g_camera = 0;
    g_scan_maps.clear();
    g_player_cache.clear();
}

inline void ensure_scan_context() {
    if (g_scan_pid != proc::pid || g_scan_lib != proc::lib) {
        reset();
        g_scan_pid = proc::pid;
        g_scan_lib = proc::lib;
        diag::log("INFO", "scan context reset pid=%d libunity=0x%llx", proc::pid,
                  (unsigned long long)proc::lib);
    }
}

inline bool plausible(uint64_t p) { return p >= 0x10000 && p < 0x0001000000000000ULL; }
inline bool obj_ptr(uint64_t p);
// Objects live above 4 GiB on this runtime; anything else is a field value,
// not an object, and must not be read (the scanner's false registry matches
// used to read junk such as 0x1d0000 on every pass).
inline bool exact_class(uint64_t object, uint64_t klass) {
    return klass && obj_ptr(object) && rpm<uint64_t>(object) == klass;
}

// Heap object pointers on this 64-bit runtime are always above 4 GiB; field
// values also contain floats/ints which must not be probed as pointers.
inline bool obj_ptr(uint64_t p) { return p >= 0x100000000ULL && p < 0x800000000000ULL; }

// Il2CppClass* from a TypeInfo slot. On this il2cpp the slots are lazy: until
// generated code touches the class the slot holds an encoded metadata index
// (observed 0x2002xxxx for kmc/wot during a whole match, because they are
// constructed through generic/DI paths that never initialise the global
// slot). Such a value must never be compared against object headers.
// An initialised slot never changes for the life of the process, so it is
// read once per thread and library; a lazy slot is re-read until it fills.
inline uint64_t typeinfo(uint64_t rva, uint64_t lib = 0) {
    const uint64_t base = lib ? lib : proc::lib;
    if (!base) return 0;
    struct slot { uint64_t rva, value; };
    thread_local uint64_t cache_lib = 0;
    thread_local int cache_pid = -1;
    thread_local std::vector<slot> cache;
    if (cache_lib != base || cache_pid != proc::pid) {
        cache.clear();
        cache_lib = base;
        cache_pid = proc::pid;
    }
    for (const slot& s : cache)
        if (s.rva == rva) return s.value;
    const uint64_t value = rpm<uint64_t>(base + rva);
    if (!obj_ptr(value)) return 0;
    cache.push_back({rva, value});
    return value;
}

// ---- il2cpp klass name resolution (sanity checks only) ----
// This build uses a newer il2cpp metadata layout than the old v24
// `name@+0x10` scheme. Observed on a live 1.0.0 instance by the previous
// maintainer: klass + 0x198 points at the class string table whose first
// NUL-terminated entry is the class name. Used here only to double-check
// that Camera_TypeInfo really resolves to "Camera" on the running build.
namespace il2cpp {
    inline const uint64_t k_name_off = 0x198;
    inline const uint64_t k_ns_hint_off = 0x0d8;
    inline int g_layout = 0;  // 0 unknown, 5 = new-metadata layout verified
    inline int g_cache_pid = -1;
    inline uint64_t g_cache_lib = 0;
    struct klass_names { std::string name, ns; };
    inline std::unordered_map<uint64_t, klass_names> g_names;

    inline void invalidate_if_context_changed() {
        if (g_cache_pid != proc::pid || g_cache_lib != proc::lib) {
            g_layout = 0;
            g_names.clear();
            g_cache_pid = proc::pid;
            g_cache_lib = proc::lib;
        }
    }

    inline bool read_ascii(uint64_t ptr, std::string& out, bool allow_empty) {
        out.clear();
        if (!plausible(ptr)) return false;
        char buf[129]{};
        if (!mem_read(ptr, buf, 64)) return false;
        size_t len = 0;
        while (len < 64 && buf[len]) ++len;
        if (len == 64) {
            if (!mem_read(ptr + 64, buf + 64, 64)) return false;
            while (len < 128 && buf[len]) ++len;
            if (len == 128) return false;
        }
        for (size_t i = 0; i < len; ++i) {
            const unsigned char c = static_cast<unsigned char>(buf[i]);
            if (c < 0x20 || c > 0x7E) return false;
        }
        if (len == 0 && !allow_empty) return false;
        out.assign(buf, len);
        return true;
    }

    inline bool looks_like_identifier(const std::string& s) {
        if (s.empty()) return false;
        for (char c : s) {
            const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                            (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '<' ||
                            c == '>' || c == '`' || c == '+' || c == '[' || c == ']' ||
                            c == ',' || c == '/';
            if (!ok) return false;
        }
        return true;
    }

    inline bool ensure_layout(bool force = false) {
        invalidate_if_context_changed();
        if (g_layout) return true;
        static long long last_probe = 0;
        if (!force && !diag::every(last_probe, 1000)) return false;
        const uint64_t known = typeinfo(v100::registry_typeinfo);
        if (!plausible(known)) return false;
        std::string name;
        if (read_ascii(rpm<uint64_t>(known + k_name_off), name, false) && looks_like_identifier(name)) {
            g_layout = 5;
            diag::log("KLIB", "il2cpp klass layout=new(0x198) klass=0x%llx name=%s",
                      (unsigned long long)known, name.c_str());
            return true;
        }
        for (uint64_t off = 0x0d8; off <= 0x1d8; off += 8) {
            if (!read_ascii(rpm<uint64_t>(known + off), name, false)) continue;
            if (!looks_like_identifier(name)) continue;
            g_layout = 5;
            diag::log("KLIB", "il2cpp klass layout=heur off=0x%llx klass=0x%llx name=%s",
                      (unsigned long long)off, (unsigned long long)known, name.c_str());
            return true;
        }
        return false;
    }

    inline bool klass_identity(uint64_t klass, std::string& name, std::string& ns, bool force = false) {
        name.clear();
        ns.clear();
        invalidate_if_context_changed();
        if (!plausible(klass)) return false;
        auto cached = g_names.find(klass);
        if (cached != g_names.end()) {
            name = cached->second.name;
            ns = cached->second.ns;
            return !name.empty();
        }
        if (!ensure_layout(force)) return false;
        if (!read_ascii(rpm<uint64_t>(klass + k_name_off), name, false)) return false;
        if (!read_ascii(rpm<uint64_t>(klass + k_ns_hint_off), ns, true)) ns.clear();
        g_names[klass] = {name, ns};
        return true;
    }

    inline std::string klass_label(uint64_t klass) {
        std::string name, ns;
        if (!klass_identity(klass, name, ns)) return "?";
        if (ns.empty()) return name;
        return ns + "." + name;
    }
}

// ---------------------------------------------------------------------------
// Build verification. ESP never runs against an unverified libunity.so:
// a stale profile would otherwise read garbage through every TypeInfo slot.
// ---------------------------------------------------------------------------
namespace game {
    inline int build_attempts = 0;

    inline bool verify_build() {
        if (proc::build_checked) return proc::build_ok;
        if (proc::pid <= 0 || proc::lib == 0) return false;
        static long long last_attempt = 0;
        if (!diag::every(last_attempt, 500)) return false;
        ++build_attempts;

        char id[48]{};
        const bool have_id = elf_build_id(proc::lib, id, sizeof(id));
        if (have_id) std::snprintf(proc::build_id, sizeof(proc::build_id), "%s", id);
        const size_t expected_len = std::strlen(v100::libunity_build_id);
        const bool id_match = have_id && std::strncmp(id, v100::libunity_build_id, expected_len) == 0;

        // Secondary check: the Camera TypeInfo slot must name "Camera" on the
        // live il2cpp metadata. Needs the klass layout probe to succeed.
        const uint64_t camera_klass = typeinfo(v100::camera_typeinfo);
        std::string name, ns;
        const bool name_readable = il2cpp::klass_identity(camera_klass, name, ns, true);
        const bool camera_ok = name_readable && name == "Camera";

        if (have_id) {
            proc::build_checked = true;
            proc::build_ok = id_match;
            diag::log(id_match ? "INFO" : "ERROR",
                      "libunity build-id=%s expected=%s match=%d camera-klass=%s profile=%s",
                      id, v100::libunity_build_id, id_match ? 1 : 0,
                      name_readable ? name.c_str() : "?", v100::profile_name);
        } else if (name_readable) {
            proc::build_checked = true;
            proc::build_ok = camera_ok;
            diag::log(camera_ok ? "WARN" : "ERROR",
                      "libunity build-id note unreadable; Camera TypeInfo resolves to '%s' (%s)",
                      name.c_str(), camera_ok ? "accepting build by klass name" : "profile mismatch");
        } else if (build_attempts >= 20) {
            proc::build_checked = true;
            proc::build_ok = false;
            diag::log("ERROR", "cannot verify libunity build: no build-id note and klass names unreadable");
        } else {
            return false;
        }

        if (!proc::build_ok && proc::skip_build_check) {
            diag::log("WARN", "--skip-build-check: running against an UNVERIFIED build, offsets may be wrong");
            proc::build_ok = true;
        }
        STATUS_SET(build, "build: %s (%s)", proc::build_ok ? "OK" : "MISMATCH",
                   proc::build_id[0] ? proc::build_id : "no build-id");
        return proc::build_ok;
    }
}

inline void refresh_scan_maps() {
    g_scan_maps.clear();
    if (proc::pid <= 0) return;
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/maps", proc::pid);
    FILE* file = std::fopen(path, "r");
    if (!file) {
        diag::log("ERROR", "cannot open %s errno=%d", path, errno);
        return;
    }
    char line[1024];
    while (std::fgets(line, sizeof(line), file)) {
        unsigned long long begin = 0, end = 0;
        char perms[5]{};
        if (std::sscanf(line, "%llx-%llx %4s", &begin, &end, perms) != 3) continue;
        if (perms[0] != 'r' || perms[1] != 'w' || end <= begin) continue;
        const uint64_t size = end - begin;
        if (size < 0x1000 || size > 4096ULL * 1024 * 1024) continue;
        // Managed/native objects live in private writable mappings. Avoid files and stacks.
        if (std::strchr(line, '/') || std::strstr(line, "[stack")) continue;
        g_scan_maps.push_back({begin, end, true});
    }
    std::fclose(file);
}

inline bool writable_scan_address(uint64_t address) {
    return std::any_of(g_scan_maps.begin(), g_scan_maps.end(),
        [address](const remote_map& map) { return address >= map.begin && address < map.end; });
}

inline bool validate_registry(uint64_t candidate, uint64_t registry_class, uint64_t store_class) {
    if (!exact_class(candidate, registry_class)) return false;
    uint64_t store = rpm<uint64_t>(candidate + v100::registry_store);
    if (!exact_class(store, store_class)) return false;
    uint64_t list = rpm<uint64_t>(store + v100::store_objects);
    if (!obj_ptr(list)) return false;
    int count = rpm<int>(list + v100::list_count);
    uint64_t array = rpm<uint64_t>(list + v100::list_items);
    if (count < 0 || count > 4096 || !obj_ptr(array)) return false;
    uint64_t length = rpm<uint64_t>(array + v100::array_length);
    return length >= unsigned(count) && length <= 8192;
}

// ---------------------------------------------------------------------------
// Background heap scanner. Finds the few singleton-like managed objects the
// ESP needs (NetObject registry, camera controllers, PlayerAnimationData,
// UnityEngine.Camera wrappers) by their klass pointer. Runs on its own
// thread so a multi-hundred-MiB GC heap never stalls the render loop; the
// render thread only validates and uses the published addresses.
// ---------------------------------------------------------------------------
namespace scanner {
    struct results {
        int pid{-1};
        uint64_t lib{0};
        uint64_t registry{0};
        // Every NetObject registry seen in the heap. Between matches the old
        // one stays alive and still validates, so the render thread picks the
        // one that holds the local player instead of trusting the first hit.
        std::vector<uint64_t> registries;
        std::vector<uint64_t> controllers; // kmc / wot instances
        std::vector<uint64_t> animation;   // PlayerAnimationData instances
        std::vector<uint64_t> cameras;     // UnityEngine.Camera managed wrappers
        unsigned passes{0};
        long long last_pass_ms{0};
        double last_pass_mib{0};
        long long last_pass_duration_ms{0};
    };
    inline std::mutex g_mutex;
    inline results g_results;
    inline std::atomic<bool> g_run{false};
    inline std::atomic<bool> g_rescan{true};
    inline std::atomic<bool> g_busy{false};
    // Set by the camera resolver while it has no locked camera. Without it
    // the scanner only re-passes when the registry is missing: a full pass
    // reads ~4 GiB on this game and must not run every 15 s for nothing.
    inline std::atomic<bool> g_want_more{true};
    // Set by the render thread while it has no usable registry (invalid, or
    // no local player for a long time); the scanner's own result list can
    // still hold the stale registry, so it cannot tell by itself.
    inline std::atomic<bool> g_need_registry{false};
    // Bumped by the render thread when the local player changes (new match,
    // new round object). Resets the pass back-off and restarts a running pass
    // so the new match's camera and registry are found from the start.
    inline std::atomic<unsigned> g_match_epoch{0};
    inline void notify_new_match() { g_match_epoch.fetch_add(1); }
    inline std::thread g_thread;

    // Mappings that can never hold il2cpp objects. Names come from
    // prctl(PR_SET_VMA_ANON_NAME) on Android 11+.
    inline bool skip_mapping(const char* line) {
        static const char* skip[] = {"[anon:dalvik", "[anon:stack_and_tls", "[anon:linker_alloc",
            "[anon:thread signal stack", "[anon:bionic_alloc", "[anon:cfi shadow",
            "[anon:arc4random", "[anon:atexit", "[anon:System property", "[anon:.bss]",
            "[vvar]", "[vdso]", "[heap]"};
        for (const char* name : skip)
            if (std::strstr(line, name)) return true;
        return false;
    }

    inline results snapshot() {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_results;
    }

    inline void request_rescan() { g_rescan = true; }

    inline void sleep_ms(long long ms) {
        for (long long slept = 0; slept < ms && g_run; slept += 50) usleep(50000);
    }

    inline std::vector<remote_map> read_maps(int pid) {
        std::vector<remote_map> maps;
        char path[64];
        std::snprintf(path, sizeof(path), "/proc/%d/maps", pid);
        FILE* file = std::fopen(path, "r");
        if (!file) return maps;
        char line[1024];
        while (std::fgets(line, sizeof(line), file)) {
            unsigned long long begin = 0, end = 0;
            char perms[5]{};
            if (std::sscanf(line, "%llx-%llx %4s", &begin, &end, perms) != 3) continue;
            if (perms[0] != 'r' || perms[1] != 'w' || end <= begin) continue;
            const uint64_t size = end - begin;
            if (size < 0x1000 || size > 4096ULL * 1024 * 1024) continue;
            if (std::strchr(line, '/') || std::strstr(line, "[stack") || skip_mapping(line)) continue;
            maps.push_back({begin, end, true});
        }
        std::fclose(file);
        return maps;
    }

    inline void pass(int pid, uint64_t lib) {
        const uint64_t registry_class = typeinfo(v100::registry_typeinfo, lib);
        const uint64_t store_class = typeinfo(v100::store_typeinfo, lib);
        const uint64_t kmc_class = typeinfo(v100::camera_main_typeinfo, lib);
        const uint64_t wot_class = typeinfo(v100::camera_arms_typeinfo, lib);
        const uint64_t animation_class = typeinfo(v100::animation_data_typeinfo, lib);
        const uint64_t camera_class = typeinfo(v100::camera_typeinfo, lib);
        if (!registry_class || !store_class) {
            diag::log("SCAN", "TypeInfo registry=0x%llx store=0x%llx not initialised yet (lazy slots)",
                      (unsigned long long)rpm<uint64_t>(lib + v100::registry_typeinfo),
                      (unsigned long long)rpm<uint64_t>(lib + v100::store_typeinfo));
            return;
        }
        const std::vector<remote_map> maps = read_maps(pid);
        results local;
        local.pid = pid;
        local.lib = lib;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            local.passes = g_results.passes;
            if (g_results.pid == pid && g_results.lib == lib &&
                validate_registry(g_results.registry, registry_class, store_class))
                local.registry = g_results.registry;
        }
        const unsigned epoch = g_match_epoch.load();
        // Found objects go out as soon as they are seen: the camera resolver
        // probes the published list every 250 ms, so a match does not wait
        // for the end of a multi-GiB pass.
        auto publish = [&](std::vector<uint64_t> results::*list, uint64_t object, size_t cap) {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_results.pid != pid || g_results.lib != lib) return;
            std::vector<uint64_t>& v = g_results.*list;
            if (v.size() < cap && std::find(v.begin(), v.end(), object) == v.end()) v.push_back(object);
        };
        const long long started = diag::now_ms();
        uint64_t scanned = 0;
        std::vector<uint64_t> words;
        for (const remote_map& map : maps) {
            uint64_t cursor = map.begin;
            while (cursor < map.end) {
                if (!g_run || proc::pid != pid || g_match_epoch.load() != epoch) return;
                size_t bytes = size_t(std::min<uint64_t>(map.end - cursor, 1024 * 1024));
                bytes &= ~size_t(7);
                if (bytes < 16) break;
                words.resize(bytes / 8);
                if (mem_read(cursor, words.data(), bytes)) {
                    for (size_t i = 0; i < words.size(); ++i) {
                        const uint64_t word = words[i];
                        if (word < 0x10000) continue;
                        const uint64_t object = cursor + i * 8;
                        if (word == registry_class) {
                            if (local.registries.size() < 8 && validate_registry(object, registry_class, store_class)) {
                                local.registries.push_back(object);
                                publish(&results::registries, object, 8);
                                if (!local.registry) local.registry = object;
                                std::lock_guard<std::mutex> lock(g_mutex);
                                g_results.pid = pid;
                                g_results.lib = lib;
                                if (!g_results.registry) g_results.registry = object;
                                diag::log("SCAN", "NetObject registry candidate 0x%llx after %.1f MiB",
                                          (unsigned long long)object, double(scanned) / 1048576.0);
                            }
                        } else if ((word == kmc_class && kmc_class) || (word == wot_class && wot_class)) {
                            if (local.controllers.size() < 32) { local.controllers.push_back(object); publish(&results::controllers, object, 32); }
                        } else if (word == animation_class && animation_class) {
                            if (local.animation.size() < 32) { local.animation.push_back(object); publish(&results::animation, object, 32); }
                        } else if (word == camera_class && camera_class) {
                            if (local.cameras.size() < 128 && obj_ptr(rpm<uint64_t>(object + v100::unity_cached_ptr))) {
                                local.cameras.push_back(object);
                                publish(&results::cameras, object, 128);
                            }
                        }
                    }
                }
                cursor += bytes;
                scanned += bytes;
                // ~400 MiB/s ceiling keeps one core from being pinned on
                // weaker devices while the game is rendering.
                usleep(1500);
            }
        }
        local.passes += 1;
        local.last_pass_ms = diag::now_ms();
        local.last_pass_mib = double(scanned) / 1048576.0;
        local.last_pass_duration_ms = local.last_pass_ms - started;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_results = local;
        }
        diag::log("SCAN", "pass %u done: %.1f MiB in %lld ms registry=0x%llx controllers=%zu animation=%zu cameras=%zu maps=%zu",
                  local.passes, local.last_pass_mib, local.last_pass_duration_ms,
                  (unsigned long long)local.registry, local.controllers.size(),
                  local.animation.size(), local.cameras.size(), maps.size());
    }

    inline void loop() {
        // Background priority: the scan must never steal a core from the game.
        ::setpriority(PRIO_PROCESS, static_cast<id_t>(::syscall(SYS_gettid)), 19);
        unsigned idle_passes = 0; // passes that found nothing new while the registry is known
        unsigned seen_epoch = g_match_epoch.load();
        // The local NetObject changes every round, not only every match. The
        // camera resolver re-locks from the existing camera list within a
        // frame or two, so a round change only warrants a pass if something
        // is still missing 1.5 s later. Before, every round (~20 s) started a
        // 1.3 GiB pass: ~30% of a core and ~1300 process_vm_readv of 1 MiB
        // against the game, with nothing new found.
        bool settle_pending = false;
        long long settle_until = 0;
        while (g_run) {
            const unsigned epoch = g_match_epoch.load();
            if (epoch != seen_epoch) {
                seen_epoch = epoch;
                idle_passes = 0;
                settle_pending = true;
                settle_until = diag::now_ms() + 1500;
                diag::log("SCAN", "new round/match (local player changed): back-off reset, re-check in 1.5 s");
            }
            bool new_match = false;
            if (settle_pending && diag::now_ms() >= settle_until) {
                settle_pending = false;
                new_match = true;
            }
            const int pid = proc::pid;
            const uint64_t lib = proc::lib;
            if (pid <= 0 || lib == 0 || !proc::build_ok) {
                sleep_ms(200);
                continue;
            }
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                if (g_results.pid != pid || g_results.lib != lib) {
                    g_results = results{};
                    g_results.pid = pid;
                    g_results.lib = lib;
                    g_rescan = true;
                }
            }
            bool essential_missing = false;
            long long last_pass = 0;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                essential_missing = g_results.registry == 0 || g_results.cameras.empty();
                last_pass = g_results.last_pass_ms;
            }
            const long long since_pass = diag::now_ms() - last_pass;
            // A pass is worth its CPU only while something is still missing:
            // the registry (before/after a match) or a camera for the
            // resolver. Once everything is locked the thread idles. Inside a
            // match (registry known) repeated fruitless passes back off up to
            // two minutes so a layout mismatch does not cost a 4 GiB scan
            // every 15 s while the player is in a round.
            const bool registry_known = !essential_missing || g_results.registry != 0;
            long long interval = 15000;
            if (registry_known && idle_passes > 0) interval = std::min<long long>(30000, 15000LL << std::min<unsigned>(idle_passes, 1));
            // A new match with no camera lock scans at once; a requested
            // rescan (lock lost, registry gone) waits 3 s after the last pass.
            // A rescan request is dropped once its reason is gone: requests
            // made while a pass was running used to start one more full pass
            // after the camera and registry were already found (log
            // 20261114-175419, pass 5: 7 s, 1.46 GiB for nothing).
            const bool needed = essential_missing || g_want_more || g_need_registry;
            if (g_rescan && !needed) g_rescan = false;
            const bool due = (new_match && needed) ||
                             (g_rescan && since_pass > 3000) ||
                             (needed && since_pass > interval);
            if (!due) {
                sleep_ms(100);
                continue;
            }
            const bool wanted_more = g_want_more;
            g_rescan = false;
            g_busy = true;
            pass(pid, lib);
            g_busy = false;
            (void)crash::g_frame; // the scanner thread shares the crash handler; its stage is implicit
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                if (g_results.registry && wanted_more) ++idle_passes; else idle_passes = 0;
            }
            // Short rest, cut short by a new match.
            for (int i = 0; i < 30 && g_run && g_match_epoch.load() == seen_epoch; ++i) sleep_ms(100);
        }
    }

    inline void start() {
        if (g_run) return;
        g_run = true;
        g_thread = std::thread(loop);
    }

    inline void stop() {
        if (!g_run) return;
        g_run = false;
        if (g_thread.joinable()) g_thread.join();
    }
}

inline void cache_player(uint64_t object, uint32_t id, long long now) {
    if (!id) id = uint32_t((object >> 4) ^ object);
    auto same_object = std::find_if(g_player_cache.begin(), g_player_cache.end(),
        [object](const player_cache_entry& entry) { return entry.object == object; });
    if (same_object != g_player_cache.end()) {
        same_object->id = id;
        same_object->seen_ms = now;
        return;
    }
    auto found = std::find_if(g_player_cache.begin(), g_player_cache.end(),
                              [id](const player_cache_entry& entry) { return entry.id == id; });
    if (found == g_player_cache.end()) g_player_cache.push_back({id, object, now});
    else { found->object = object; found->seen_ms = now; }
}

inline bool registry_ready() {
    ensure_scan_context();
    // The registry pointer is revalidated (5 reads) four times a second;
    // players() still checks count/array on every frame.
    static long long last_validation = 0;
    if (g_registry && !diag::every(last_validation, 250)) return true;
    const uint64_t registry_class = typeinfo(v100::registry_typeinfo);
    const uint64_t store_class = typeinfo(v100::store_typeinfo);
    static long long type_log = 0;
    if (diag::every(type_log, 5000))
        diag::log(registry_class && store_class ? "INFO" : "ERROR",
                  "TypeInfo registry=0x%llx store=0x%llx player=0x%llx transforms=0x%llx kmc=0x%llx wot=0x%llx (0x2xxxxxxx = lazy slot, class not initialised by generated code)",
                  (unsigned long long)registry_class, (unsigned long long)store_class,
                  (unsigned long long)rpm<uint64_t>(proc::lib + v100::player_typeinfo),
                  (unsigned long long)rpm<uint64_t>(proc::lib + v100::transforms_typeinfo),
                  (unsigned long long)rpm<uint64_t>(proc::lib + v100::camera_main_typeinfo),
                  (unsigned long long)rpm<uint64_t>(proc::lib + v100::camera_arms_typeinfo));
    if (!registry_class || !store_class) return false;
    if (validate_registry(g_registry, registry_class, store_class)) return true;
    g_registry = 0;
    scanner::g_need_registry = true;
    const scanner::results snap = scanner::snapshot();
    if (snap.pid == proc::pid && snap.lib == proc::lib &&
        validate_registry(snap.registry, registry_class, store_class)) {
        g_registry = snap.registry;
        diag::log("INFO", "NetObject registry in use: 0x%llx", (unsigned long long)g_registry);
        return true;
    }
    scanner::request_rescan();
    STATUS_SET(scan, "scan: %s pass=%u %.0f MiB", scanner::g_busy ? "running" : "waiting",
               snap.passes, snap.last_pass_mib);
    return false;
}

// One block read per NetObject (odq): wwv header, flags, mtc/ydm/vqn pointers
// and the team byte. Everything the frame needs from the player object itself.
struct player_snapshot {
    uint64_t object{0};
    uint32_t id{0};
    uint64_t world{0};
    uint8_t flags[3]{0, 0, 0};   // +0x44 +0x45 +0x46
    uint64_t transform{0};       // odq+0x68 mtc
    uint64_t stats{0};           // odq+0x70 ydm
    uint64_t camera_state{0};    // odq+0x78 vqn
    uint8_t view_mode{0};        // odq+0x80 dvp
    uint8_t team{0};             // odq+0x90
    bool is_local() const { return flags[g_local_flag_offset - int(v100::player_local)] != 0; }
};

inline bool decode_player_snapshot(uint64_t object, const uint8_t* block, uint64_t player_class, player_snapshot& out) {
    uint64_t klass = 0;
    std::memcpy(&klass, block, 8);
    if (klass != player_class) return false;
    out.object = object;
    std::memcpy(&out.id, block + v100::net_object_id, 4);
    std::memcpy(&out.world, block + v100::player_world, 8);
    std::memcpy(out.flags, block + v100::player_local, 3);
    std::memcpy(&out.transform, block + v100::player_transform, 8);
    std::memcpy(&out.stats, block + v100::player_stats, 8);
    std::memcpy(&out.camera_state, block + v100::player_camera_state, 8);
    out.view_mode = block[v100::player_view_mode];
    out.team = block[v100::player_team];
    return out.world != 0;
}

inline bool read_player_snapshot(uint64_t object, uint64_t player_class, player_snapshot& out) {
    if (!obj_ptr(object)) return false;
    uint8_t block[0x98]{};
    if (!mem_read(object, block, sizeof(block))) return false;
    return decode_player_snapshot(object, block, player_class, out);
}

inline int resolve_local_flag(const std::vector<player_snapshot>& objects) {
    if (objects.empty()) return g_local_flag_offset;
    for (int offset : {0x44, 0x45, 0x46}) {
        int true_count = 0;
        for (const player_snapshot& p : objects)
            if (p.flags[offset - 0x44] != 0) ++true_count;
        // A live match has exactly one local NetObject. If a flag is true for
        // zero or several objects it is IsServer/IsLocalOwned, not IsLocal.
        if (true_count == 1) {
            if (g_local_flag_offset != offset) {
                diag::log("INFO", "local NetObject flag resolved at +0x%x (old +0x%x)",
                          offset, g_local_flag_offset);
                g_local_flag_offset = offset;
            }
            return offset;
        }
    }
    return g_local_flag_offset;
}

// Decodes every player NetObject of one registry. False when the registry
// structure itself is unreadable.
inline bool read_registry_players(uint64_t registry, uint64_t player_class, std::vector<player_snapshot>& current, int& count) {
    current.clear();
    count = 0;
    const uint64_t store = obj_ptr(registry) ? rpm<uint64_t>(registry + v100::registry_store) : 0;
    const uint64_t list = obj_ptr(store) ? rpm<uint64_t>(store + v100::store_objects) : 0;
    if (!obj_ptr(list)) return false;
    count = rpm<int>(list + v100::list_count);
    const uint64_t array = rpm<uint64_t>(list + v100::list_items);
    if (!player_class || count < 0 || count > 4096 || (count > 0 && !obj_ptr(array))) return false;
    if (count == 0) return true;
    std::vector<uint64_t> items(size_t(count), 0);
    if (!mem_read(array + v100::array_data, items.data(), items.size() * 8)) return false;
    // Player blocks are fetched in scatter batches and decoded locally; an
    // unreadable object only drops itself (per-part retry in the scatter).
    constexpr size_t kBlock = 0x98;
    std::vector<uint8_t> blocks(size_t(count) * kBlock, 0);
    std::vector<uint8_t> fetched(size_t(count), 0);
    for (int start = 0; start < count; start += 64) {
        read_part parts[64];
        int index[64];
        size_t n = 0;
        for (int i = start; i < count && i < start + 64; ++i) {
            if (!obj_ptr(items[size_t(i)])) continue;
            parts[n] = {items[size_t(i)], blocks.data() + size_t(i) * kBlock, kBlock};
            index[n++] = i;
        }
        bool ok[64];
        if (n) mem_read_scatter(parts, n, ok);
        for (size_t k = 0; k < n; ++k) if (ok[k]) fetched[size_t(index[k])] = 1;
    }
    for (int i = 0; i < count; ++i) {
        player_snapshot snap{};
        if (fetched[size_t(i)] &&
            decode_player_snapshot(items[size_t(i)], blocks.data() + size_t(i) * kBlock, player_class, snap))
            current.push_back(snap);
    }
    return true;
}

// A live match has exactly one local NetObject on one of the three flags.
inline bool has_local_player(const std::vector<player_snapshot>& objects) {
    for (int slot = 0; slot < 3; ++slot) {
        int n = 0;
        for (const player_snapshot& p : objects) if (p.flags[slot]) ++n;
        if (n == 1) return true;
    }
    return false;
}

// The registry in use holds no local player (old match kept alive, next
// match started without going through the menu): switch to a registry the
// scanner found that does hold one, or ask for a pass to find it.
inline void switch_to_live_registry(uint64_t player_class, long long no_local_ms) {
    static long long last_try = 0, last_request = 0;
    if (!diag::every(last_try, 1000)) return;
    const scanner::results snap = scanner::snapshot();
    if (snap.pid == proc::pid && snap.lib == proc::lib) {
        const uint64_t registry_class = typeinfo(v100::registry_typeinfo);
        const uint64_t store_class = typeinfo(v100::store_typeinfo);
        for (uint64_t candidate : snap.registries) {
            if (candidate == g_registry || !validate_registry(candidate, registry_class, store_class)) continue;
            std::vector<player_snapshot> objects;
            int count = 0;
            if (!read_registry_players(candidate, player_class, objects, count) || !has_local_player(objects)) continue;
            diag::log("INFO", "registry 0x%llx has no local player, switching to 0x%llx (%zu players)",
                      (unsigned long long)g_registry, (unsigned long long)candidate, objects.size());
            g_registry = candidate;
            g_player_cache.clear();
            return;
        }
    }
    // Between rounds the registry has no local player for 1-2 s (log
    // 20261114-155757: rescan at +0 s, new local player at +1.5 s, same
    // registry). Only a long absence means the registry itself is stale.
    if (no_local_ms >= 8000) scanner::g_need_registry = true;
    if (no_local_ms >= 8000 && diag::every(last_request, 30000)) {
        diag::log("INFO", "registry 0x%llx has no local player for %lld ms and no other candidate holds one: rescan",
                  (unsigned long long)g_registry, no_local_ms);
        scanner::request_rescan();
    }
}

inline std::vector<player_snapshot> players() {
    std::vector<player_snapshot> out;
    if (!registry_ready()) return out;
    const uint64_t player_class = typeinfo(v100::player_typeinfo);
    std::vector<player_snapshot> current;
    int count = 0;
    if (!read_registry_players(g_registry, player_class, current, count)) {
        diag::log("WARN", "registry became invalid playerClass=0x%llx count=%d",
                  (unsigned long long)player_class, count);
        g_registry = 0;
        scanner::g_need_registry = true;
        scanner::request_rescan();
        return out;
    }
    static long long no_local_since = 0;
    const long long now = diag::now_ms();
    if (has_local_player(current)) {
        no_local_since = 0;
        scanner::g_need_registry = false;
    } else {
        if (!no_local_since) no_local_since = now;
        if (now - no_local_since > 1000) {
            const uint64_t before = g_registry;
            switch_to_live_registry(player_class, now - no_local_since);
            if (g_registry != before && read_registry_players(g_registry, player_class, current, count) &&
                has_local_player(current))
                no_local_since = 0;
        }
    }
    const size_t raw_exact = current.size();
    uint64_t active_world = 0;
    for (const player_snapshot& snap : current) {
        if (!active_world || snap.is_local()) active_world = snap.world;
        cache_player(snap.object, snap.id, now);
    }
    // Cache validation costs three reads per entry; once every 500 ms is enough.
    static long long last_cache_check = 0;
    if (diag::every(last_cache_check, 500)) {
        g_player_cache.erase(std::remove_if(g_player_cache.begin(), g_player_cache.end(),
            [now, player_class, active_world](const player_cache_entry& entry) {
                return now - entry.seen_ms > 2500 || !exact_class(entry.object, player_class) ||
                       !rpm<uint64_t>(entry.object + v100::player_world) ||
                       (active_world && rpm<uint64_t>(entry.object + v100::player_world) != active_world);
            }), g_player_cache.end());
    }
    constexpr size_t kMaxPlayers = 32;
    for (const player_snapshot& snap : current) {
        if (out.size() >= kMaxPlayers) break;
        out.push_back(snap);
    }
    // A dead player disappears from the registry but can remain in the cache
    // for a couple of seconds, still carrying the last pre-death stats. Once
    // the registry has a real match snapshot, do not resurrect cache-only objects.
    if (current.size() < 2) {
        for (const auto& entry : g_player_cache) {
            if (out.size() >= kMaxPlayers) break;
            bool present = false;
            for (const player_snapshot& snap : out) if (snap.object == entry.object) { present = true; break; }
            if (present) continue;
            player_snapshot snap{};
            if (read_player_snapshot(entry.object, player_class, snap)) out.push_back(snap);
        }
    }
    if (!out.empty()) resolve_local_flag(out);
    static long long player_log = 0;
    if (diag::every(player_log, 5000))
        diag::log("INFO", "NetObjects=%d exactNow=%zu cachedPlayers=%zu registry=0x%llx",
                  count, raw_exact, out.size(), (unsigned long long)g_registry);
    return out;
}

inline std::string read_managed_string(uint64_t string_object, uint64_t string_class) {
    if (string_object < 0x100000 || !plausible(string_object) || !string_class ||
        rpm<uint64_t>(string_object) != string_class) return {};
    const int32_t length = rpm<int32_t>(string_object + 0x10);
    if (length <= 0 || length > 40) return {};
    std::vector<uint16_t> chars(static_cast<size_t>(length), uint16_t{});
    if (!mem_read(string_object + 0x14, chars.data(), chars.size() * sizeof(uint16_t))) return {};
    std::string out;
    for (uint16_t ch : chars) {
        if (ch < 0x20 || ch == 0x7f) return {};
        if (ch < 0x80) out.push_back(char(ch));
        else if (ch < 0x800) {
            out.push_back(char(0xC0 | (ch >> 6)));
            out.push_back(char(0x80 | (ch & 0x3F)));
        } else {
            out.push_back(char(0xE0 | (ch >> 12)));
            out.push_back(char(0x80 | ((ch >> 6) & 0x3F)));
            out.push_back(char(0x80 | (ch & 0x3F)));
        }
    }
    return out;
}

// Walk the six component arrays of wwv.yvjn (cpa +0x10..+0x38) and return the
// first component whose klass matches.
inline uint64_t find_player_component(uint64_t player, uint64_t component_class) {
    if (!player || !component_class) return 0;
    const uint64_t container = rpm<uint64_t>(player + v100::player_components);
    if (!plausible(container)) return 0;
    for (uint64_t field = 0x10; field <= 0x38; field += 8) {
        const uint64_t array = rpm<uint64_t>(container + field);
        const uint64_t count = rpm<uint64_t>(array + v100::array_length);
        if (!plausible(array) || count > 256) continue;
        for (uint64_t i = 0; i < count; ++i) {
            const uint64_t component = rpm<uint64_t>(array + v100::array_data + i * 8);
            if (exact_class(component, component_class)) return component;
        }
    }
    return 0;
}

inline bool noisy_player_name(const std::string& value) {
    if (value.empty() || value == "Player" || value.front() == '_' ||
        value.find('/') != std::string::npos || value.find('<') != std::string::npos ||
        value.find('>') != std::string::npos || value.find("http") != std::string::npos ||
        value.find(" SDF") != std::string::npos || value.find('.') != std::string::npos ||
        value.find('"') != std::string::npos) return true;
    bool hex = value.size() >= 16;
    bool short_lower = value.size() <= 4;
    bool repeated = value.size() >= 8;
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (!std::isxdigit(ch)) hex = false;
        if (!(ch >= 'a' && ch <= 'z')) short_lower = false;
        if (i && value[i] != value[0]) repeated = false;
    }
    static const char* ui_words[] = {"Normal", "Highlighted", "Pressed", "Selected",
        "Crashlytics", "Diffuse"};
    for (const char* word : ui_words) if (value == word) return true;
    return hex || short_lower || repeated;
}

// System.String klass. There is no String TypeInfo slot in this build and a
// single literal slot stays null until the game touches that literal, so the
// klass is taken from the string-literal table: the klass word shared by at
// least three initialised literals.
inline uint64_t string_klass() {
    static uint64_t cached = 0;
    static int cached_pid = -1;
    static long long last_try = 0;
    if (cached && cached_pid == proc::pid) return cached;
    if (!diag::every(last_try, 1000)) return 0;
    uint64_t slots[512]{};
    if (!mem_read(proc::lib + v100::player_string_literal, slots, sizeof(slots))) return 0;
    std::unordered_map<uint64_t, int> votes;
    for (uint64_t slot : slots) {
        if (!obj_ptr(slot)) continue;
        const uint64_t klass = rpm<uint64_t>(slot);
        if (!obj_ptr(klass)) continue;
        if (++votes[klass] >= 3) {
            cached = klass;
            cached_pid = proc::pid;
            diag::log("NAME", "string klass 0x%llx (literal table)", (unsigned long long)klass);
            return cached;
        }
    }
    return 0;
}

inline bool printable_name(const std::string& value) {
    return !value.empty() && value.size() <= 96 && value.find('<') == std::string::npos &&
           value.find("http") == std::string::npos;
}

// Where the nickname lives: component class + field offset. Found by the
// heuristic below and locked once two different players agree on the place
// with two different names; after that every name is read from there
// directly, so it no longer depends on what else the components hold.
struct name_location { uint64_t klass{0}; uint32_t offset{0}; };
inline name_location g_name_location;
inline int g_name_location_pid = -1;
struct name_vote { uint64_t klass; uint32_t offset; uint32_t player_id; std::string value; };
inline std::vector<name_vote> g_name_votes;

inline void vote_name_location(uint64_t klass, uint32_t offset, uint32_t player_id, const std::string& value) {
    for (const name_vote& v : g_name_votes) {
        if (v.klass != klass || v.offset != offset || v.player_id == player_id || v.value == value) continue;
        g_name_location = {klass, offset};
        g_name_votes.clear();
        diag::log("NAME", "nickname field locked: component klass=0x%llx +0x%x ('%s', '%s')",
                  (unsigned long long)klass, offset, v.value.c_str(), value.c_str());
        return;
    }
    for (const name_vote& v : g_name_votes)
        if (v.klass == klass && v.offset == offset && v.player_id == player_id) return;
    if (g_name_votes.size() >= 64) g_name_votes.erase(g_name_votes.begin());
    g_name_votes.push_back({klass, offset, player_id, value});
}

inline std::string player_name(uint64_t player, uint32_t player_id) {
    if (g_name_location_pid != proc::pid) {
        g_name_location = {};
        g_name_votes.clear();
        g_name_location_pid = proc::pid;
    }
    const uint64_t string_class = string_klass();
    const uint64_t container = rpm<uint64_t>(player + v100::player_components);
    if (!string_class || !plausible(container)) return {};
    uint64_t arrays[6]{};
    if (!mem_read(container + 0x10, arrays, sizeof(arrays))) return {};
    for (uint64_t array : arrays) {
        if (!plausible(array)) continue;
        const uint64_t count = rpm<uint64_t>(array + v100::array_length);
        if (count == 0 || count > 256) continue;
        std::vector<uint64_t> components(static_cast<size_t>(count));
        if (!mem_read(array + v100::array_data, components.data(), components.size() * 8)) continue;
        for (uint64_t component : components) {
            if (!plausible(component)) continue;
            uint64_t words[32]{};   // klass, monitor, then 30 field words from +0x10
            if (!mem_read(component, words, sizeof(words))) continue;
            const uint64_t klass = words[0];
            if (g_name_location.klass) {
                if (klass != g_name_location.klass) continue;
                const uint64_t value = rpm<uint64_t>(component + g_name_location.offset);
                std::string name = obj_ptr(value) ? read_managed_string(value, string_class) : std::string();
                return printable_name(name) ? name : std::string();
            }
            for (int i = 2; i < 32; ++i) {
                if (!obj_ptr(words[i])) continue;
                std::string candidate = read_managed_string(words[i], string_class);
                if (candidate.empty() || candidate.size() > 96 || noisy_player_name(candidate)) continue;
                vote_name_location(klass, uint32_t(i * 8), player_id, candidate);
                return candidate;
            }
        }
    }
    return {};
}

inline bool finite_matrix(const matrix& m) {
    const float* p = reinterpret_cast<const float*>(&m);
    float sum = 0.f;
    for (int i = 0; i < 16; ++i) { if (!std::isfinite(p[i]) || std::fabs(p[i]) > 1000000.f) return false; sum += std::fabs(p[i]); }
    return sum > .01f;
}

inline bool matrix_window_sane(const float* m, bool want_view) {
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(m[i]) || std::fabs(m[i]) > 1000000.f) return false;
    if (want_view) {
        // View last row ~= (0,0,0,1).
        return std::fabs(m[12]) < 1000.f && std::fabs(m[3]) < .001f &&
               std::fabs(m[7]) < .001f && std::fabs(m[11]) < .001f &&
               std::fabs(m[15] - 1.f) < .01f;
    }
    // Projection: perspective markers + sane focal lengths.
    const bool persp = std::fabs(m[11]) > .5f && std::fabs(m[15]) < .1f;
    if (!persp) return false;
    if (std::fabs(m[0]) < .05f || std::fabs(m[0]) > 20.f) return false;
    if (std::fabs(m[5]) < .05f || std::fabs(m[5]) > 20.f) return false;
    return true;
}

inline bool validate_camera(uint64_t camera, matrix& view, matrix& projection) {
    uint64_t vtable = rpm<uint64_t>(camera);
    uint64_t game_object = rpm<uint64_t>(camera + 0x20);
    if (vtable < proc::lib + v100::data_relro_begin || vtable >= proc::lib + v100::data_end ||
        !plausible(game_object)) return false;
    if (!mem_read(camera + v100::camera_view, &view, sizeof(view)) || !mem_read(camera + v100::camera_projection, &projection, sizeof(projection))) return false;
    const float* v = reinterpret_cast<const float*>(&view);
    const float* p = reinterpret_cast<const float*>(&projection);
    return finite_matrix(view) && finite_matrix(projection) && std::fabs(v[3]) < .001f && std::fabs(v[7]) < .001f &&
           std::fabs(v[11]) < .001f && std::fabs(v[15] - 1.f) < .01f && std::fabs(p[0]) > .001f && std::fabs(p[5]) > .001f;
}

// Snapshot of a native Camera object. Unity keeps worldToCamera, projection,
// implicit/skybox/clip variants inline; the object is larger than 0x300, so
// read up to 0x800 and fall back to shorter windows when the tail is unmapped.
inline constexpr size_t k_camera_block = 0x800;
inline size_t read_camera_block(uint64_t native, uint8_t* buf) {
    for (size_t size : {k_camera_block, size_t(0x400), size_t(0x300)}) {
        if (mem_read(native, buf, size)) return size;
    }
    return 0;
}

// Every 16-byte aligned window that looks like a view or a projection matrix.
inline void scan_matrix_windows(const uint8_t* buf, size_t size, std::vector<int>& views, std::vector<int>& projs) {
    views.clear();
    projs.clear();
    for (size_t off = 0x40; off + sizeof(matrix) <= size; off += 0x10) {
        const float* m = reinterpret_cast<const float*>(buf + off);
        if (matrix_window_sane(m, true)) views.push_back(int(off));
        else if (matrix_window_sane(m, false)) projs.push_back(int(off));
    }
}

// Generic pattern scan: find (view,proj) matrix pair inside a native Camera
// by float patterns. The known Unity layout (+0x70/+0xB0) is tried first.
inline bool detect_pair(uint64_t obj, matrix& view, matrix& projection,
                        int& view_off, int& proj_off, const char* tag) {
    uint8_t buf[k_camera_block]{};
    const size_t size = read_camera_block(obj, buf);
    if (!size) return false;
    auto at = [&](int off) -> const float* {
        return reinterpret_cast<const float*>(buf + off);
    };
    if (matrix_window_sane(at(int(v100::camera_view)), true) &&
        matrix_window_sane(at(int(v100::camera_projection)), false)) {
        view_off = int(v100::camera_view);
        proj_off = int(v100::camera_projection);
    } else {
        std::vector<int> views, projs;
        scan_matrix_windows(buf, size, views, projs);
        if (views.empty() || projs.empty()) return false;
        view_off = views.front();
        proj_off = projs.front();
    }
    std::memcpy(&view, at(view_off), sizeof(view));
    std::memcpy(&projection, at(proj_off), sizeof(projection));
    const float* p = at(proj_off);
    diag::log("CAMERA", "%s pair obj=0x%llx view=+0x%x proj=+0x%x fov=%.1f proj=(%.3f %.3f)",
              tag, (unsigned long long)obj, view_off, proj_off,
              2.f * std::atan(1.f / p[5]) * 57.29578f, p[0], p[5]);
    return true;
}

// Re-read a pattern-located matrix pair. Two syscalls per frame.
inline bool read_matrix_pair(uint64_t native, int view_off, int proj_off, matrix& view, matrix& projection) {
    if (!native || view_off < 0 || proj_off < 0) return false;
    const read_part parts[] = {
        {native + uint64_t(view_off), &view, sizeof(view)},
        {native + uint64_t(proj_off), &projection, sizeof(projection)},
    };
    if (!mem_read_scatter(parts, 2)) return false;
    return matrix_window_sane(reinterpret_cast<const float*>(&view), true) &&
           matrix_window_sane(reinterpret_cast<const float*>(&projection), false);
}
