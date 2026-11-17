// ================================================================================
// Curtis aim — Zenin internal logic, external port for Standoff 2 1.1.0.
//
// Logic ported from the Zenin internal (aimbot_predict.hpp):
//   * target angles from the world-space bone delta (atan2f, degrees)
//   * FOV selection by angular distance to the current view direction
//   * exponential smoothing toward the target angles
// External form:
//   * every pointer dereference -> one remote read (rpm/mem_read)
//   * their PlayerUpdate hook -> a polling thread (while + usleep, ~500 Hz)
// Channel (found at runtime, see the session notes):
//   * vqn razn euler angles: yjp component (comp+0x38 -> kgd) = input,
//     odq+0x78 = player camera state. Writes there move the camera.
// Safety rules learned the hard way:
//   * NEVER burst-write (a burst into a respawn-stale object corrupted the
//     game heap and crashed the game)
//   * re-read and validate the target object right before every write
//   * preserve their roll value
//   * no writes into kgd quats from the vqn path (fire accuracy has to come
//     from the camera channel)
// ================================================================================
#include "aim.hpp"
#include "aim_targets.hpp"
#include "../other/diag.hpp"
#include "../other/memory.hpp"
#include "../ui/cfg.hpp"
#include "../game/camera.hpp"
#include "../game/game.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <thread>
#include <atomic>

namespace aim {

static std::atomic<bool> s_thread_up{false};
static int s_state = 0;        // 0 off, 1 no aim state, 2 no target, 3 no camera, 4 tracking
static long long s_writes = 0;
static long long diag_silent_count = 0;

static long long now_ms() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

const char* state_text() {
    switch (s_state) {
        case 1: return "no aim state";
        case 2: return "no target";
        case 3: return "no camera";
        case 4: return "tracking";
        default: return "off";
    }
}

static float normalize_yaw(float a) {
    while (a > 180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}

// euler wraparound: the game stores angles modulo 360 (358.2 IS -1.8), so
// reads normalise to the math convention and writes keep the game's own
// representation — a jump across 0/360 whips the camera and the ESP boxes.
static float wrap_to_180(float a) {
    while (a > 180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}

// ---- kgd (their AimingData state on 1.1.0: two quaternions) ----
// Used to READ the current view angles when the vqn channel is not resolved
// yet. Never written from the vqn path.
static uint64_t s_kgd_cache = 0;
static uint64_t s_kgd_owner = 0;

static bool quats_sane(uint64_t kgd) {
    float q[8]{};
    if (!mem_read(kgd + 0x10, q, sizeof(q))) return false;
    for (float f : q)
        if (!std::isfinite(f) || std::fabs(f) > 1.2f) return false;
    // +0x20 is the yaw-only quat (0, sy, 0, cy): x and z must be ~0
    if (std::fabs(q[4]) > 0.01f || std::fabs(q[6]) > 0.01f) return false;
    auto norm = [](const float* p) { return p[0] * p[0] + p[1] * p[1] + p[2] * p[2] + p[3] * p[3]; };
    const float n1 = norm(q), n2 = norm(q + 4);
    return n1 > 0.8f && n1 < 1.2f && n2 > 0.8f && n2 < 1.2f;
}

static uint64_t resolve_kgd_structural(uint64_t local_player) {
    const uint64_t container = rpm<uint64_t>(local_player + v100::player_components);
    if (!plausible(container)) return 0;
    for (uint64_t field = 0x10; field <= 0x38; field += 8) {
        const uint64_t array = rpm<uint64_t>(container + field);
        const uint64_t count = rpm<uint64_t>(array + v100::array_length);
        if (!plausible(array) || count > 256) continue;
        for (uint64_t i = 0; i < count; ++i) {
            const uint64_t component = rpm<uint64_t>(array + v100::array_data + i * 8);
            if (!obj_ptr(component)) continue;
            const uint64_t cand = rpm<uint64_t>(component + v100::ios_aim);
            if (obj_ptr(cand) && quats_sane(cand)) return cand;
        }
    }
    return 0;
}

static uint64_t resolve_kgd(uint64_t local_player) {
    static long long last_diag = 0;
    const bool do_diag = diag::every(last_diag, 2000);
    if (!local_player) {
        if (do_diag) diag::log("AIM", "kgd: no local player");
        return 0;
    }
    if (s_kgd_cache && s_kgd_owner == local_player) {
        if (quats_sane(s_kgd_cache)) return s_kgd_cache;
        s_kgd_cache = 0;
        s_kgd_owner = 0;
        if (do_diag) diag::log("AIM", "kgd: cache dropped (probe failed)");
    }
    const uint64_t yjp_class = typeinfo(v100::ios_typeinfo);
    if (obj_ptr(yjp_class)) {
        const uint64_t yjp = find_player_component(local_player, yjp_class);
        if (obj_ptr(yjp)) {
            const uint64_t kgd = rpm<uint64_t>(yjp + v100::ios_aim);
            if (obj_ptr(kgd)) {
                s_kgd_cache = kgd;
                s_kgd_owner = local_player;
                if (do_diag) diag::log("AIM", "kgd: resolved yjp=0x%llx kgd=0x%llx",
                                    (unsigned long long)yjp, (unsigned long long)kgd);
                return kgd;
            }
        }
    }
    const uint64_t kgd = resolve_kgd_structural(local_player);
    if (kgd) {
        s_kgd_cache = kgd;
        s_kgd_owner = local_player;
        return kgd;
    }
    if (do_diag) {
        // why: the typeinfo slot can stay lazy across a transition, or the
        // local object is not a player at all — distinguish the cases
        const uint64_t yjp_class = typeinfo(v100::ios_typeinfo);
        diag::log("AIM", "kgd: not found (local=0x%llx yjpClass=0x%llx%s container=0x%llx)",
                  (unsigned long long)local_player, (unsigned long long)yjp_class,
                  (yjp_class >= 0x2000000 && yjp_class < 0x3000000) ? " LAZY" : "",
                  (unsigned long long)rpm<uint64_t>(local_player + v100::player_components));
    }
    return 0;
}

// ---- quaternion -> degrees (kgd fallback read) ----
static float quat_to_pitch_deg(const float q[4]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    if (w < 0.f) { x = -x; y = -y; z = -z; w = -w; }
    return 2.f * std::atan2(x, w) * 57.2957795f;
}

static float quat_to_yaw_deg(const float q[4]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    if (w < 0.f) { x = -x; y = -y; z = -z; w = -w; }
    return 2.f * std::atan2(y, w) * 57.2957795f;
}

// ---- kgd (AimingData) IS the write channel ----
// Structure: +0x10 full rotation quaternion, +0x20 yaw-only quaternion
// (0, sy, 0, cy — enforced by quats_sane). The vqn "camera state" is an
// OUTPUT: the game recomputes it from the aiming data every tick. Verified
// empirically — our vqn writes landed (the post-write read showed them) and
// were reset by the next tick, while the view never moved. Writing kgd is the
// only channel that moves the view AND the fire direction, because the same
// state feeds both.
static bool kgd_read_angles(uint64_t kgd, float& pitch, float& yaw) {
    if (!obj_ptr(kgd)) return false;
    float full_q[4]{}, yaw_q[4]{};
    if (!mem_read(kgd + 0x10, full_q, sizeof(full_q)) ||
        !mem_read(kgd + 0x20, yaw_q, sizeof(yaw_q))) return false;
    for (float f : full_q) if (!std::isfinite(f) || std::fabs(f) > 1.5f) return false;
    for (float f : yaw_q)  if (!std::isfinite(f) || std::fabs(f) > 1.5f) return false;
    pitch = quat_to_pitch_deg(full_q);
    yaw = quat_to_yaw_deg(yaw_q);
    return std::isfinite(pitch) && std::isfinite(yaw);
}

// Single validated write. The session notes record that a BURST write into a
// respawn-stale kgd corrupted the game heap and crashed the game, so this
// mirrors the safety contract of the old rig writer: quats_sane() immediately
// before the write, the owner re-checked by the caller, both quats written
// consistently in ONE frame, and each quat keeps the sign of the value it is
// replacing (q and -q are the same rotation, but a sign flip on write whips
// the game's interpolation).
static bool kgd_write_angles(uint64_t kgd, float pitch, float yaw) {
    if (!obj_ptr(kgd) || !quats_sane(kgd)) return false;
    pitch = std::clamp(pitch, -89.f, 89.f);
    yaw = wrap_to_180(yaw);
    const float hp = pitch * 0.01745329252f * 0.5f;
    const float hy = yaw * 0.01745329252f * 0.5f;
    float full_q[4]{}, yaw_q[4]{};
    if (!mem_read(kgd + 0x10, full_q, sizeof(full_q)) ||
        !mem_read(kgd + 0x20, yaw_q, sizeof(yaw_q))) return false;
    for (float f : full_q) if (!std::isfinite(f)) return false;
    for (float f : yaw_q)  if (!std::isfinite(f)) return false;
    // preserve each quat's stored sign
    const float fs = full_q[3] < 0.f ? -1.f : 1.f;
    const float ys = yaw_q[3] < 0.f ? -1.f : 1.f;
    const float sx = std::sin(hp), cx = std::cos(hp);
    const float sy = std::sin(hy), cy = std::cos(hy);
    // Unity Euler order (ZXY) composes yaw then pitch: full = qy * qx =
    // (cy*sx, sy*cx, -sy*sx, cy*cx). The yaw-only slot is (0, sy, 0, cy).
    // A wrong order would still be a valid unit quaternion — the view would
    // aim wrongly, nothing would crash — and the read-back telemetry would
    // show it immediately.
    const float out_full[4] = {fs * cy * sx, fs * sy * cx, -fs * sy * sx, fs * cy * cx};
    const float out_yaw[4] = {0.f, ys * sy, 0.f, ys * cy};
    return mem_write(kgd + 0x10, out_full, sizeof(out_full)) &&
           mem_write(kgd + 0x20, out_yaw, sizeof(out_yaw));
}

// ---- vqn camera states: THE live angle channel ----
// yjp component (comp+0x38 -> kgd) holds the input vqn at comp+0x48;
// odq+0x78 holds the player camera state. Both store two bor
// {Vector3 Position; Vector3 EulerAngles} and their razn (+0x10) euler
// angles move with the camera. Writes go to razn only.
static uint64_t s_vqn_input = 0;   // yjp-style input vqn
static uint64_t s_vqn_player = 0;  // odq.gjvt

static bool vqn_sane(uint64_t vqn) {
    if (!obj_ptr(vqn)) return false;
    const uint64_t bor = rpm<uint64_t>(vqn + 0x10);
    if (!obj_ptr(bor)) return false;
    float e[3]{};
    if (!mem_read(bor + v100::bor_transform + 12, e, sizeof(e))) return false;
    // modulo-360 storage: anything finite within a full circle is valid
    return std::isfinite(e[0]) && std::isfinite(e[1]) &&
           std::fabs(e[0]) <= 360.f && std::fabs(e[1]) <= 360.f;
}

static bool vqn_read_angles(uint64_t vqn, float& pitch, float& yaw) {
    if (!obj_ptr(vqn)) return false;
    const uint64_t bor = rpm<uint64_t>(vqn + 0x10);
    if (!obj_ptr(bor)) return false;
    float e[3]{};
    if (!mem_read(bor + v100::bor_transform + 12, e, sizeof(e))) return false;
    if (!std::isfinite(e[0]) || !std::isfinite(e[1])) return false;
    pitch = wrap_to_180(e[0]);
    yaw = wrap_to_180(e[1]);
    return true;
}

// NOTE: there is no vqn WRITE any more. The camera state vqn is an OUTPUT the
// game recomputes from the AimingData every tick — our writes to it were
// observed to land and be reset within one tick while the view never moved
// (and writing pooled objects the game cycles through is what produced the
// jitter). kgd_write_angles() below is the write path. The vqn objects remain
// read-only, for telemetry and for rig_find()'s reference yaw.
// Collect the write targets: the player vqn (odq+0x78) plus the yjp
// component's vqn. The yjp component (comp+0x38 -> kgd) is the ONLY trusted
// input — a random component's vqn is not a camera channel and writing
// eulers there crashed the game.
static void vqn_collect(uint64_t local_player, uint64_t kgd) {
    s_vqn_input = 0;
    s_vqn_player = 0;
    if (!local_player) return;
    const uint64_t vqn_p = rpm<uint64_t>(local_player + v100::player_camera_state);
    if (vqn_sane(vqn_p)) s_vqn_player = vqn_p;
    const uint64_t container = rpm<uint64_t>(local_player + v100::player_components);
    if (!plausible(container) || !obj_ptr(kgd)) {
        if (diag_silent_count++ % 1500 == 0)
            diag::log("AIM", "vqn: no components (container=0x%llx kgd=0x%llx)",
                      (unsigned long long)container, (unsigned long long)kgd);
        return;
    }
    for (uint64_t field = 0x10; field <= 0x38; field += 8) {
        const uint64_t array = rpm<uint64_t>(container + field);
        const uint64_t count = rpm<uint64_t>(array + v100::array_length);
        if (!plausible(array) || count > 256) continue;
        for (uint64_t i = 0; i < count; ++i) {
            const uint64_t component = rpm<uint64_t>(array + v100::array_data + i * 8);
            if (!obj_ptr(component)) continue;
            if (rpm<uint64_t>(component + v100::ios_aim) != kgd) continue;
            const uint64_t vqn = rpm<uint64_t>(component + 0x48);
            if (vqn_sane(vqn) && vqn != s_vqn_player) s_vqn_input = vqn;
            else if (diag_silent_count++ % 1500 == 0)
                diag::log("AIM", "vqn: yjp comp found, vqn=0x%llx sane=%d e=(%.1f %.1f)",
                          (unsigned long long)vqn, vqn_sane(vqn) ? 1 : 0,
                          [&] {
                              if (!obj_ptr(vqn)) return 0.f;
                              const uint64_t bor = rpm<uint64_t>(vqn + 0x10);
                              if (!obj_ptr(bor)) return 0.f;
                              float e[3]{};
                              mem_read(bor + v100::bor_transform + 12, e, sizeof(e));
                              return e[0];
                          }(),
                          [&] {
                              if (!obj_ptr(vqn)) return 0.f;
                              const uint64_t bor = rpm<uint64_t>(vqn + 0x10);
                              if (!obj_ptr(bor)) return 0.f;
                              float e[3]{};
                              mem_read(bor + v100::bor_transform + 12, e, sizeof(e));
                              return e[1];
                          }());
            return;
        }
    }
}

// ---- Aplodium TRS rig channel (their 1.0.0 write path, +8 to our 1.1.0) ----
// The yjp component holds TWO managed Transform wrappers: pivot (PITCH,
// X-quat) at +0x50 and body (YAW, Y-quat) at +0x58. Their native side:
//   wrapper+0x10 -> native; native+0x28 -> data; data+0x18 -> TRS array
//   (stride 0x30: {pos[4], quat[4], scale[4]}), data+0x20 -> parent indices,
//   native+0x30 -> this transform's index. Writing the quats into the TRS
//   slots moves the whole game chain (camera + fire + animations) — the
//   Aplodium cheat proves this channel drives the bullets too.
struct TrsSlot {
    uint64_t native{}, address{};
    // the component field this wrapper was found at. rig_find() brute-forces
    // the slot list, so the validation pass must NOT assume +0x50/+0x58.
    uint64_t slot_off{};
    int32_t index{-1}, parent{-1};
    float quat[4]{};
};
static uint64_t s_rig_comp = 0;    // the yjp component
static uint64_t s_rig_local = 0;
static TrsSlot s_rig_pivot, s_rig_body;
static bool s_rig_valid = false;

// sub-stage diagnostics for the rig slot read: 31 wrapper, 32 native,
// 33 data, 34 transforms/parents, 35 index, 36 TRS content
static int trs_fail_sub = 0;
// histogram of the above over one full scan: a single global only ever shows
// the LAST failure, which hid where the chain actually breaks
static int rig_sub_hist[6] = {0, 0, 0, 0, 0, 0};
// consecutive failed brute-force scans before we stop trying (respawn /
// a new local player resets it, see the loop)
static int s_rig_giveup = 0;
static constexpr int k_rig_giveup = 12;
static uint64_t trs_dump_wrapper = 0, trs_dump_native = 0, trs_dump_data = 0;
static int32_t trs_dump_index = -1;

// ---- the rig transform layout, measured on the live game (RIGQ probe) ----
// The camera-rig Transforms are NOT laid out like the player-skeleton bones
// the bone reader decodes. Empirically (two sessions, identical):
//   native+0x28 -> the shared Hierarchy object (iohk and lotd point to the
//                  SAME one — body and pivot live in one small hierarchy)
//   native+0x30 -> int32 index inside it (lotd=0 root/yaw, iohk=1 pitch)
//   native+0x38 -> a per-node object, NOT the hierarchy — following the bone
//                  layout here is exactly what produced the sub=34 failures
//   hier+0x90 + 0x30*index + 0x10 -> the local TRS entry's quaternion
//     (verified: lotd idx0 quat at hier+0xa0 carried the live camera yaw,
//      angle matched kgd to 0.1 degree in both sessions)
// The hierarchy stores entries INLINE (2 nodes) instead of behind the
// entries/parents pointers the big skeleton hierarchies use.
// The exact entries base / quat offset inside the entry is confirmed for
// index 0 only, so a couple of layout candidates are tried; a wrong one
// cannot pass the unit-norm check plus the x_only/y_only and ref_yaw gates
// in rig_find.
struct RigLayout {
    uint64_t hier_off;    // native -> hierarchy pointer
    uint64_t index_off;   // native -> int32 index
    uint64_t base;        // hierarchy -> first TRS entry
    uint64_t quat_off;    // entry -> quaternion
    uint64_t stride;      // entry size
};
static constexpr RigLayout k_rig_layouts[] = {
    {0x28, 0x30, 0x90, 0x10, 0x30},   // measured: idx0 quat at hier+0xa0
    {0x28, 0x30, 0x80, 0x20, 0x30},   // alternative reading of the same hit
    {0x28, 0x30, 0x90, 0x10, 0x20},   // tighter stride variant
};
static int g_rig_layout_used = -1;

static bool trs_read(uint64_t wrapper, TrsSlot& out) {
    out = {};
    if (!obj_ptr(wrapper)) { trs_fail_sub = 31; return false; }
    const uint64_t native = rpm<uint64_t>(wrapper + 0x10);
    if (!obj_ptr(native)) { trs_fail_sub = 32; trs_dump_wrapper = wrapper; return false; }
    for (size_t li = 0; li < sizeof(k_rig_layouts) / sizeof(k_rig_layouts[0]); ++li) {
        const RigLayout& L = k_rig_layouts[li];
        const uint64_t hier = rpm<uint64_t>(native + L.hier_off);
        if (!obj_ptr(hier)) { trs_fail_sub = 33; continue; }
        const int32_t index = rpm<int32_t>(native + L.index_off);
        if (index < 0 || index > 4096) { trs_fail_sub = 35; continue; }
        const uint64_t entry = hier + L.base + L.stride * uint64_t(index);
        float buf[12]{};
        if (!mem_read(entry, buf, sizeof(buf))) { trs_fail_sub = 36; continue; }
        float norm = 0.f;
        bool sane = true;
        for (int i = 4; i < 8; ++i) {
            if (!std::isfinite(buf[i])) { sane = false; break; }
            norm += buf[i] * buf[i];
        }
        if (!sane || norm < 0.8f || norm > 1.2f) { trs_fail_sub = 36; continue; }
        for (int i = 0; i < 3; ++i)
            if (!std::isfinite(buf[i])) { trs_fail_sub = 36; sane = false; break; }
        if (!sane) continue;
        out.native = native;
        out.address = entry;
        out.index = index;
        out.parent = -1;   // parents are inline too and not needed: we write
                           // LOCAL rotations directly
        std::memcpy(out.quat, buf + 4, sizeof(out.quat));
        g_rig_layout_used = int(li);
        return true;
    }
    trs_dump_wrapper = wrapper;
    trs_dump_native = native;
    return false;
}

// find the rig. The DIRECT path first: the yjp component is the one whose
// +0x38 == kgd (vqn_collect's identity), and dump.cs + disassembly pin its
// Transform iohk (PITCH, pure X quat) at +0x50 and Transform lotd (YAW, pure
// Y quat) at +0x58 — mzox derives the look Vector2 from exactly these two
// (euler.x of iohk, euler.y of lotd) and pushes it into the ubz look command
// via kCommandLook, which feeds BOTH the server fire direction and kgd/camera
// through zbjv. The brute-force scan remains only as a fallback: the earlier
// "0 valid chains" result came from probing a dead player's components while
// the local identity flapped.
static int rig_fail_stage = 0;
// brute-force counters (diagnostics): what the slot scan actually sees
static int rig_counts[5] = {0, 0, 0, 0, 0}; // comp, valid_trs, x_only, y_only, pairs

static bool rig_try_component(uint64_t component, uint64_t pivot_off, uint64_t body_off,
                               float ref_yaw);

static bool rig_find(uint64_t local_player, uint64_t kgd) {
    if (!local_player) return false;
    s_rig_valid = false;
    s_rig_comp = 0;
    rig_counts[0] = rig_counts[1] = rig_counts[2] = rig_counts[3] = rig_counts[4] = 0;
    for (int i = 0; i < 6; ++i) rig_sub_hist[i] = 0;
    const uint64_t container = rpm<uint64_t>(local_player + v100::player_components);
    if (!plausible(container)) { rig_fail_stage = 2; return false; }

    // live reference yaw (vqn), if available
    float ref_yaw = 1e9f;
    {
        const uint64_t vqn_p = rpm<uint64_t>(local_player + v100::player_camera_state);
        float p{}, y{};
        if (vqn_read_angles(vqn_p, p, y)) ref_yaw = y;
    }

    // ---- direct path: the yjp component, slots +0x50 (pitch) / +0x58 (yaw) ----
    if (obj_ptr(kgd)) {
        for (uint64_t field = 0x10; field <= 0x38; field += 8) {
            const uint64_t array = rpm<uint64_t>(container + field);
            const uint64_t count = rpm<uint64_t>(array + v100::array_length);
            if (!plausible(array) || count > 256) continue;
            for (uint64_t i = 0; i < count; ++i) {
                const uint64_t component = rpm<uint64_t>(array + v100::array_data + i * 8);
                if (!obj_ptr(component)) continue;
                if (rpm<uint64_t>(component + v100::ios_aim) != kgd) continue;
                if (rig_try_component(component, 0x50, 0x58, ref_yaw)) {
                    s_rig_local = local_player;
                    rig_fail_stage = 0;
                    diag::log("AIM", "rig(direct): comp=0x%llx pivot=+0x50(idx=%d) body=+0x58(idx=%d) "
                                     "hier=%d — the channel that drives command AND camera",
                              (unsigned long long)component, s_rig_pivot.index, s_rig_body.index,
                              s_rig_pivot.parent == s_rig_body.index ? 1 : 0);
                    return true;
                }
                // wrong slots on this build after all — fall through to the scan
                break;
            }
        }
    }

    const auto x_only = [](const TrsSlot& s) {
        return std::fabs(s.quat[1]) < 0.05f && std::fabs(s.quat[2]) < 0.05f;
    };
    const auto y_only = [](const TrsSlot& s) {
        return std::fabs(s.quat[0]) < 0.05f && std::fabs(s.quat[2]) < 0.05f;
    };

    // widened from the old 0x40..0x78 guess: that range produced ZERO valid
    // transform chains across ~70 components, so the wrappers live elsewhere
    static constexpr uint64_t k_slots[] = {
        0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48,
        0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80, 0x88,
        0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xC0, 0xC8};
    for (uint64_t field = 0x10; field <= 0x38; field += 8) {
        const uint64_t array = rpm<uint64_t>(container + field);
        const uint64_t count = rpm<uint64_t>(array + v100::array_length);
        if (!plausible(array) || count > 256) continue;
        for (uint64_t i = 0; i < count; ++i) {
            const uint64_t component = rpm<uint64_t>(array + v100::array_data + i * 8);
            if (!obj_ptr(component)) continue;
            ++rig_counts[0];
            TrsSlot pivot{}, body{};
            uint64_t pivot_slot = 0, body_slot = 0;
            bool have_pivot = false, have_body = false;
            for (uint64_t slot : k_slots) {
                TrsSlot t{};
                if (!trs_read(rpm<uint64_t>(component + slot), t)) {
                    ++rig_sub_hist[std::clamp(trs_fail_sub - 31, 0, 5)];
                    continue;
                }
                ++rig_counts[1];
                if (!have_pivot && x_only(t) && !y_only(t)) {
                    pivot = t; pivot.slot_off = slot; pivot_slot = slot;
                    have_pivot = true; ++rig_counts[2];
                } else if (!have_body && y_only(t) && !x_only(t)) {
                    body = t; body.slot_off = slot; body_slot = slot;
                    have_body = true; ++rig_counts[3];
                }
            }
            if (have_pivot && have_body) ++rig_counts[4];
            if (!have_pivot || !have_body) continue;
            if (pivot.address == body.address) continue;
            const float body_yaw = 2.f * std::atan2(body.quat[1], body.quat[3]) * 57.2957795f;
            if (ref_yaw < 1e8f) {
                const float diff = std::fabs(wrap_to_180(body_yaw - ref_yaw));
                if (diff > 8.f) continue;
            }
            s_rig_comp = component;
            s_rig_pivot = pivot;
            s_rig_body = body;
            s_rig_local = local_player;
            s_rig_valid = true;
            rig_fail_stage = 0;
            diag::log("AIM", "rig(scan): comp=0x%llx pivot=+0x%llx(idx=%d) body=+0x%llx(idx=%d) hier=%d yaw=%.1f ref=%.1f",
                      (unsigned long long)component,
                      (unsigned long long)pivot_slot, pivot.index,
                      (unsigned long long)body_slot, body.index,
                      pivot.parent == body.index ? 1 : 0, body_yaw, ref_yaw < 1e8f ? ref_yaw : 0.f);
            return true;
        }
    }
    rig_fail_stage = 6;
    static long long last_rigfail = 0;
    if (diag::every(last_rigfail, 2000))
        diag::log("AIM", "rig scan fail: comp=%d trs=%d xonly=%d yonly=%d pairs=%d ref_yaw=%.1f "
                         "subhist[31..36]=%d/%d/%d/%d/%d/%d",
                  rig_counts[0], rig_counts[1], rig_counts[2], rig_counts[3], rig_counts[4],
                  ref_yaw < 1e8f ? ref_yaw : -999.f,
                  rig_sub_hist[0], rig_sub_hist[1], rig_sub_hist[2],
                  rig_sub_hist[3], rig_sub_hist[4], rig_sub_hist[5]);
    return false;
}

// Validate one component as the camera rig at the given (pivot, body) slots.
// iohk holds a PURE X rotation (pitch, down-positive), lotd a PURE Y rotation
// (yaw) — anything else at those slots means this is not the rig (or the
// object is dead), and the caller falls back to the brute-force scan.
static bool rig_try_component(uint64_t component, uint64_t pivot_off, uint64_t body_off,
                               float ref_yaw) {
    TrsSlot pivot{}, body{};
    if (!trs_read(rpm<uint64_t>(component + pivot_off), pivot) ||
        !trs_read(rpm<uint64_t>(component + body_off), body)) {
        diag::log("AIM", "rig(direct): transform decode failed pivot(+0x%llx) sub=%d body(+0x%llx) sub=%d",
                  (unsigned long long)pivot_off, trs_fail_sub,
                  (unsigned long long)body_off, trs_fail_sub);
        return false;
    }
    const auto x_only = [](const TrsSlot& s) {
        return std::fabs(s.quat[1]) < 0.05f && std::fabs(s.quat[2]) < 0.05f &&
               std::fabs(s.quat[0]) > 0.001f;
    };
    const auto y_only = [](const TrsSlot& s) {
        return std::fabs(s.quat[0]) < 0.05f && std::fabs(s.quat[2]) < 0.05f &&
               std::fabs(s.quat[1]) > 0.001f;
    };
    if (!x_only(pivot) || !y_only(body) || pivot.address == body.address) {
        diag::log("AIM", "rig(direct): quats wrong pivot=(%.2f %.2f %.2f %.2f) body=(%.2f %.2f %.2f %.2f)",
                  pivot.quat[0], pivot.quat[1], pivot.quat[2], pivot.quat[3],
                  body.quat[0], body.quat[1], body.quat[2], body.quat[3]);
        return false;
    }
    const float body_yaw = 2.f * std::atan2(body.quat[1], body.quat[3]) * 57.2957795f;
    if (ref_yaw < 1e8f) {
        const float diff = std::fabs(wrap_to_180(body_yaw - ref_yaw));
        if (diff > 8.f) {
            diag::log("AIM", "rig(direct): yaw gate failed yaw=%.1f ref=%.1f", body_yaw, ref_yaw);
            return false;
        }
    }
    pivot.slot_off = pivot_off;
    body.slot_off = body_off;
    s_rig_comp = component;
    s_rig_pivot = pivot;
    s_rig_body = body;
    s_rig_valid = true;   // s_rig_local is set by the caller
    return true;
}

// ---- one-shot structural probe of the yjp component ----
// The 0x40..0x78 slot guess found nothing on any of the ~70 components, so
// stop guessing: walk the yjp component (the one whose +0x38 == kgd) and
// report every 8-byte slot that holds a managed object, together with how far
// the native transform chain decodes from it. verdict meaning:
//   1 = slot holds an object, native (+0x10) is a pointer
//   2 = native+0x38 (data) is a pointer
//   3 = data+0x18/+0x20 (transforms/parents) are pointers
//   4 = native+0x40 (index) is in range and the TRS quat is normalised
//       -> THIS slot is a usable Transform wrapper
// ---- one-shot rig memory search ----
// Round 1 searched for the exact expected quat VALUES and found nothing, so
// the rig quats are either transformed (angle offset) or further away. This
// round searches for the SHAPE instead: iohk holds a PURE-X unit quaternion
// and lotd a PURE-Y one — whatever their angles — because mzox reads exactly
// euler.x of one and euler.y of the other. Any hit gives the offset AND the
// stored angle, from which the kgd->rig mapping falls out. Raw dumps are
// logged for eyeball verification.
static bool quat_matches(const float q[4], float a, float b, float c, float d) {
    // q and -q are the same rotation: accept either sign
    const float cand[2][4] = {{a, b, c, d}, {-a, -b, -c, -d}};
    for (auto& e : cand) {
        bool ok = true;
        for (int i = 0; i < 4; ++i)
            if (!std::isfinite(q[i]) || std::fabs(q[i] - e[i]) > 0.02f) { ok = false; break; }
        if (ok) return true;
    }
    return false;
}

// pure-X: (x, 0, 0, w); pure-Y: (0, y, 0, w); both unit norm
static int quat_shape(const float q[4]) {
    if (!std::isfinite(q[0]) || !std::isfinite(q[1]) || !std::isfinite(q[2]) || !std::isfinite(q[3]))
        return 0;
    const float norm = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    if (norm < 0.9f || norm > 1.1f) return 0;
    const bool yz0 = std::fabs(q[1]) < 0.001f && std::fabs(q[2]) < 0.001f;
    const bool xz0 = std::fabs(q[0]) < 0.001f && std::fabs(q[2]) < 0.001f;
    if (yz0 && std::fabs(q[0]) > 0.01f) return 1;   // pure X
    if (xz0 && std::fabs(q[1]) > 0.01f) return 2;   // pure Y
    return 0;
}

static void rig_scan_region(const char* region, uint64_t base, size_t floats) {
    if (floats > 128) floats = 128;
    std::vector<float> buf(floats, 0.f);
    if (!mem_read(base, buf.data(), floats * 4)) return;
    for (size_t i = 0; i + 4 <= floats; ++i) {
        const int shape = quat_shape(&buf[i]);
        if (!shape) continue;
        const float angle = 2.f * std::atan2(shape == 1 ? buf[i] : buf[i + 1], buf[i + 3]) * 57.2957795f;
        diag::log("RIGQ", "  %s +0x%03zx: %s QUAT (%.3f %.3f %.3f %.3f) angle=%.1f  ★",
                  region, i * 4, shape == 1 ? "PITCH-X" : "YAW---Y",
                  buf[i], buf[i + 1], buf[i + 2], buf[i + 3], angle);
    }
}

static void rig_dump_hex(const char* region, uint64_t base, size_t bytes) {
    if (bytes > 0x100) bytes = 0x100;
    std::vector<uint64_t> words(bytes / 8, 0);
    if (!mem_read(base, words.data(), bytes)) { diag::log("RIGQ", "  %s: unreadable", region); return; }
    char line[160];
    int used = std::snprintf(line, sizeof(line), "  %s:", region);
    for (size_t i = 0; i < words.size(); ++i) {
        used += std::snprintf(line + used, sizeof(line) - size_t(used), " %016llx",
                              (unsigned long long)words[i]);
        if (used < 0 || used >= int(sizeof(line)) - 20) break;
    }
    diag::log("RIGQ", "%s", line);
}

static void rig_search_quats(uint64_t component, uint64_t slot_off, const char* which,
                             float pitch_deg, float yaw_deg) {
    const uint64_t v = rpm<uint64_t>(component + slot_off);
    if (!obj_ptr(v)) { diag::log("RIGQ", "%s +0x%llx: slot not an object", which, (unsigned long long)slot_off); return; }
    const uint64_t native = rpm<uint64_t>(v + 0x10);
    if (!obj_ptr(native)) { diag::log("RIGQ", "%s: native bad", which); return; }
    const float hp = pitch_deg * 0.00872664626f;
    const float hy = yaw_deg * 0.00872664626f;
    diag::log("RIGQ", "%s +0x%llx obj=0x%llx native=0x%llx kgdAngles=(p%.1f y%.1f) "
                      "wantP=(%.3f 0 0 %.3f) wantY=(0 %.3f 0 %.3f)",
              which, (unsigned long long)slot_off, (unsigned long long)v, (unsigned long long)native,
              pitch_deg, yaw_deg, std::sin(hp), std::cos(hp), std::sin(hy), std::cos(hy));
    // shape scan + raw dump of the native object
    rig_scan_region("native", native, 48);
    rig_dump_hex("native", native, 0x60);
    // the "data" object the bone layout expects at native+0x38
    const uint64_t data = rpm<uint64_t>(native + 0x38);
    if (obj_ptr(data)) {
        rig_scan_region("data", data, 96);
        rig_dump_hex("data", data, 0x100);
        // follow every pointer inside data[0..0x38)
        for (int w = 0; w < 7; ++w) {
            const uint64_t p = rpm<uint64_t>(data + w * 8);
            if (!obj_ptr(p) || p == native || p == data) continue;
            char name[32];
            std::snprintf(name, sizeof(name), "data+0x%x->ptr", w * 8);
            rig_scan_region(name, p, 48);
        }
    } else {
        diag::log("RIGQ", "  data@native+0x38 not a ptr");
    }
    // follow every pointer inside native[0..0x40) too
    for (int w = 0; w < 8; ++w) {
        const uint64_t p = rpm<uint64_t>(native + w * 8);
        if (!obj_ptr(p) || p == data) continue;
        float buf[12]{};
        if (!mem_read(p, buf, sizeof(buf))) continue;
        char name[32];
        std::snprintf(name, sizeof(name), "native+0x%x->ptr", w * 8);
        rig_scan_region(name, p, 12);
    }
}

static void rig_probe_yjp(uint64_t local_player, uint64_t kgd) {
    const uint64_t container = rpm<uint64_t>(local_player + v100::player_components);
    if (!plausible(container)) { diag::log("RIGP", "no container"); return; }
    for (uint64_t field = 0x10; field <= 0x38; field += 8) {
        const uint64_t array = rpm<uint64_t>(container + field);
        const uint64_t count = rpm<uint64_t>(array + v100::array_length);
        if (!plausible(array) || count > 256) continue;
        for (uint64_t i = 0; i < count; ++i) {
            const uint64_t component = rpm<uint64_t>(array + v100::array_data + i * 8);
            if (!obj_ptr(component)) continue;
            if (rpm<uint64_t>(component + v100::ios_aim) != kgd) continue;
            diag::log("RIGP", "yjp comp=0x%llx kgd=0x%llx slot map:",
                      (unsigned long long)component, (unsigned long long)kgd);
            int found = 0;
            for (uint64_t off = 0x08; off <= 0xC8; off += 8) {
                const uint64_t v = rpm<uint64_t>(component + off);
                if (!obj_ptr(v)) continue;
                const uint64_t native = rpm<uint64_t>(v + 0x10);
                if (!obj_ptr(native)) { 
                    diag::log("RIGP", "  +0x%02llx obj=0x%llx verdict=1 (native not a ptr)",
                              (unsigned long long)off, (unsigned long long)v);
                    continue;
                }
                const uint64_t data = rpm<uint64_t>(native + 0x38);
                if (!obj_ptr(data)) {
                    diag::log("RIGP", "  +0x%02llx obj=0x%llx native=0x%llx verdict=1 (data@+0x38 bad)",
                              (unsigned long long)off, (unsigned long long)v, (unsigned long long)native);
                    continue;
                }
                const uint64_t transforms = rpm<uint64_t>(data + 0x18);
                const uint64_t parents = rpm<uint64_t>(data + 0x20);
                if (!obj_ptr(transforms) || !obj_ptr(parents)) {
                    diag::log("RIGP", "  +0x%02llx native=0x%llx data=0x%llx verdict=2 (trs/par bad)",
                              (unsigned long long)off, (unsigned long long)native, (unsigned long long)data);
                    continue;
                }
                const int32_t index = rpm<int32_t>(native + 0x40);
                float q[4]{};
                const uint64_t trs = transforms + 0x30ull * uint64_t(index < 0 ? 0 : index);
                float buf[12]{};
                const bool read_ok = index >= 0 && index <= 65535 && mem_read(trs, buf, sizeof(buf));
                float norm = 0.f;
                if (read_ok) for (int k = 4; k < 8; ++k) norm += buf[k] * buf[k];
                const int verdict = (read_ok && norm > 0.8f && norm < 1.2f) ? 4 : 3;
                if (verdict == 4) ++found;
                std::memcpy(q, buf + 4, sizeof(q));
                diag::log("RIGP", "  +0x%02llx native=0x%llx idx=%d verdict=%d quat=(%.3f %.3f %.3f %.3f)",
                          (unsigned long long)off, (unsigned long long)native, index, verdict,
                          q[0], q[1], q[2], q[3]);
            }
            diag::log("RIGP", "done: %d usable Transform wrapper(s) on yjp", found);
            return;
        }
    }
    diag::log("RIGP", "yjp component not found (kgd=0x%llx)", (unsigned long long)kgd);
}

// full revalidation right before every write (their rig_current): a match
// transition frees the transform arrays while we are between the resolve
// and the write, and a stale slot address corrupts recycled memory
static bool trs_current(const TrsSlot& slot, uint64_t wrapper) {
    if (!obj_ptr(wrapper) || !slot.native || !slot.address) return false;
    if (g_rig_layout_used < 0) return false;
    const RigLayout& L = k_rig_layouts[size_t(g_rig_layout_used)];
    const uint64_t native = rpm<uint64_t>(wrapper + 0x10);
    if (native != slot.native) return false;
    // MUST match trs_read(): hierarchy @ native+0x28, index @ native+0x30,
    // entry @ hier+base+stride*index (the measured camera-rig layout; the
    // bone-reader's +0x38/+0x40 chain does not hold for these transforms).
    const uint64_t hier = rpm<uint64_t>(native + L.hier_off);
    if (!obj_ptr(hier)) return false;
    const int32_t index = rpm<int32_t>(native + L.index_off);
    if (index != slot.index) return false;
    return hier + L.base + L.stride * uint64_t(index) == slot.address;
}

static bool rig_current() {
    if (!s_rig_valid || !obj_ptr(s_rig_comp)) return false;
    return trs_current(s_rig_pivot, rpm<uint64_t>(s_rig_comp + s_rig_pivot.slot_off)) &&
           trs_current(s_rig_body, rpm<uint64_t>(s_rig_comp + s_rig_body.slot_off));
}

static bool rig_write(float pitch, float yaw) {
    if (!rig_current()) { s_rig_valid = false; return false; }
    pitch = std::clamp(pitch, -89.f, 89.f);
    yaw = wrap_to_180(yaw);
    const float hp = pitch * 0.01745329252f * 0.5f;
    const float hy = yaw * 0.01745329252f * 0.5f;
    // preserve the game's own quaternion sign (q and -q are the same
    // rotation, but a sign flip on write whips the interpolation)
    const float ps = s_rig_pivot.quat[3] < 0.f ? -1.f : 1.f;
    const float ys = s_rig_body.quat[3] < 0.f ? -1.f : 1.f;
    const float p[4] = {ps * std::sin(hp), 0.f, 0.f, ps * std::cos(hp)};
    const float y[4] = {0.f, ys * std::sin(hy), 0.f, ys * std::cos(hy)};
    // the quat offset comes from the layout trs_read() matched, so a write
    // can never target a different field than the one that was validated
    const uint64_t qoff = (g_rig_layout_used >= 0)
        ? k_rig_layouts[size_t(g_rig_layout_used)].quat_off : 0x10;
    return mem_write(s_rig_pivot.address + qoff, p, sizeof(p)) &&
           mem_write(s_rig_body.address + qoff, y, sizeof(y));
}

static bool rig_read(float& pitch, float& yaw) {
    if (!s_rig_valid) return false;
    const uint64_t qoff = (g_rig_layout_used >= 0)
        ? k_rig_layouts[size_t(g_rig_layout_used)].quat_off : 0x10;
    float pq[4]{}, yq[4]{};
    if (!mem_read(s_rig_pivot.address + qoff, pq, sizeof(pq))) return false;
    if (!mem_read(s_rig_body.address + qoff, yq, sizeof(yq))) return false;
    for (float f : pq)
        if (!std::isfinite(f)) return false;
    for (float f : yq)
        if (!std::isfinite(f)) return false;
    pitch = 2.f * std::atan2(pq[0], pq[3]) * 57.2957795f;
    yaw = 2.f * std::atan2(yq[1], yq[3]) * 57.2957795f;
    if (!std::isfinite(pitch) || !std::isfinite(yaw)) return false;
    // keep the fresh quats for the sign-preserving write
    std::memcpy(s_rig_pivot.quat, pq, sizeof(pq));
    std::memcpy(s_rig_body.quat, yq, sizeof(yq));
    return true;
}

// ---- camera position (their GetCameraPosition analog) ----
// The eye point: the locked native view matrix first, then the MainCamera
// bor resolved BY THIS THREAD (local -> chc -> PlayerAnimationData ->
// MainCamera) so the aim does not depend on the ESP render pass. While both
// sources are alive the matrix-vs-bor offset is learned and applied to the
// bor fallback, so a flapped lock still aims from the right height.
static Vector3 s_eye_cache{};
static long long s_eye_ms = 0;
static uint64_t s_bor_cache = 0;
static uint64_t s_bor_local = 0;
static Vector3 s_bor_offset{};
static bool s_bor_offset_valid = false;

static uint64_t resolve_main_bor(uint64_t local_player) {
    if (!local_player) return 0;
    if (s_bor_cache && s_bor_local == local_player) {
        const cam::bor_pose pose = cam::read_bor(s_bor_cache);
        if (pose.valid) return s_bor_cache;
        s_bor_cache = 0;
        s_bor_local = 0;
    }
    const uint64_t chc_class = typeinfo(v100::presentation_typeinfo);
    const uint64_t anim_class = typeinfo(v100::animation_data_typeinfo);
    if (!chc_class || !anim_class) return 0;
    const uint64_t chc = find_player_component(local_player, chc_class);
    if (!obj_ptr(chc)) return 0;
    const uint64_t animation = rpm<uint64_t>(chc + v100::presentation_animation_data);
    if (!exact_class(animation, anim_class)) return 0;
    const uint64_t bor = rpm<uint64_t>(animation + v100::animation_main_camera);
    if (!obj_ptr(bor)) return 0;
    const cam::bor_pose pose = cam::read_bor(bor);
    if (!pose.valid) return 0;
    s_bor_cache = bor;
    s_bor_local = local_player;
    return bor;
}

static Vector3 camera_position() {
    auto accept = [](const Vector3& p) {
        if (std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
            std::fabs(p.x) + std::fabs(p.y) + std::fabs(p.z) > 0.01f) {
            s_eye_cache = p;
            s_eye_ms = now_ms();
            return true;
        }
        return false;
    };
    matrix view{}, proj{};
    if (cam::reread_lock(view, proj)) {
        const Vector3 p = cam::position_from_view(view);
        if (accept(p)) {
            const uint64_t bor = resolve_main_bor(aim_targets::local());
            if (bor) {
                const cam::bor_pose pose = cam::read_bor(bor);
                if (pose.valid) {
                    s_bor_offset = p - pose.position;
                    s_bor_offset_valid = true;
                }
            }
            return p;
        }
    }
    const uint64_t bor = resolve_main_bor(aim_targets::local());
    if (bor) {
        const cam::bor_pose pose = cam::read_bor(bor);
        if (pose.valid) {
            const Vector3 p = s_bor_offset_valid ? pose.position + s_bor_offset
                                                 : pose.position;
            if (accept(p)) return p;
        }
    }
    if (s_eye_ms && now_ms() - s_eye_ms < 500) return s_eye_cache;
    return Vector3(0.f, 0.f, 0.f);
}

// ---- the aim loop (their ProcessAimbot, external form) ----
static void loop() {
    diag::log("AIM", "external loop started (polling thread, ~500 Hz)");
    uint64_t collect_local = 0;
    while (true) {
        if (!cfg::aim::enabled) {
            if (s_state != 0) diag::log("AIM", "disabled (state was %d)", s_state);
            s_state = 0;
            usleep(100000);
            continue;
        }
        {
            static long long last_hb = 0;
            if (diag::every(last_hb, 2000))
                diag::log("AIM", "on: %s", state_text());
        }

        // ---- 1. resolve the state objects; retry fast on respawn ----
        const uint64_t local = aim_targets::local();
        // ---- match/round transition: the registry swap is the signal ----
        // Every cached per-player object (kgd, vqn, rig) belongs to the
        // PREVIOUS match: on a swap they are all stale at once, and the old
        // code discovered that one failure at a time — the aim then sat in
        // "kgd: not found" for the rest of the match (observed: registry
        // changed 79 times in one session, aim dead after the first one).
        // Drop everything in one go and let the resolve run fresh.
        static uint64_t s_last_registry = 0;
        if (g_registry && g_registry != s_last_registry) {
            diag::log("AIM", "registry transition 0x%llx -> 0x%llx: dropping kgd/vqn/rig caches",
                      (unsigned long long)s_last_registry, (unsigned long long)g_registry);
            s_last_registry = g_registry;
            s_kgd_cache = 0;
            s_kgd_owner = 0;
            s_vqn_input = 0;
            s_vqn_player = 0;
            s_bor_cache = 0;
            s_bor_local = 0;
            s_rig_valid = false;
            s_rig_comp = 0;
            s_rig_giveup = 0;
            collect_local = 0;
        }
        const uint64_t kgd = resolve_kgd(local);
        if (local != collect_local || (!s_vqn_input && !s_vqn_player)) {
            if (local != collect_local) {
                diag::log("AIM", "local changed 0x%llx -> 0x%llx, re-resolving",
                          (unsigned long long)collect_local, (unsigned long long)local);
                s_rig_giveup = 0;   // a new local player may expose the rig
            }
            vqn_collect(local, kgd);
            collect_local = local;
        }
        // NOTE: there is deliberately NO periodic re-walk hunting for a
        // separate "input vqn". On this build the yjp component's +0x48 vqn IS
        // the same object as odq+0x78 (the camera state), so vqn_collect always
        // rejects it (`vqn != s_vqn_player`) and a retry could never succeed —
        // it only re-read local+0x78 every 500 ms and, while the game cycles
        // pooled vqn objects, kept swapping s_vqn_player to a different dead
        // object. The write channel is kgd now; the vqn objects are read for
        // telemetry only.
        if (!kgd) {
            // nothing resolved yet (match warm-up / respawn): light backoff,
            // the full component walk is not needed at 500 Hz
            s_state = 1;
            usleep(20000);
            continue;
        }

        // ---- 1b. the TRS rig: the ONLY channel that drives camera AND fire.
        // rig_find() is a brute-force walk (every component x every slot, each
        // a chain of remote reads), so it is throttled AND given up on: while
        // the native transform layout does not match, it fails every time and
        // would otherwise hammer process_vm_readv for nothing. On repeated
        // failure we stop scanning until the local player changes.
        if (!s_rig_valid && kgd && s_rig_giveup < k_rig_giveup) {
            static long long s_last_rig_try = 0;
            if (diag::every(s_last_rig_try, 250)) {
                if (rig_find(local, kgd)) {
                    s_rig_giveup = 0;
                } else if (++s_rig_giveup == k_rig_giveup) {
                    diag::log("AIM", "rig: giving up after %d scans (stage=%d sub=%d comp=%d "
                                     "trs=%d) — aim stays on the vqn camera channel, so the "
                                     "FIRE direction will not follow the crosshair",
                              k_rig_giveup, rig_fail_stage, trs_fail_sub,
                              rig_counts[0], rig_counts[1]);
                    rig_probe_yjp(local, kgd);   // one-shot: where ARE the wrappers?
                    // value search: kgd holds the live angles the tick just
                    // pushed from the rig, so the expected pitch/yaw quats are
                    // known — find them by pattern around the +0x50/+0x58
                    // transforms and pin the real write offset
                    float live_p = 0.f, live_y = 0.f;
                    if (kgd_read_angles(kgd, live_p, live_y)) {
                        // find the yjp component again for the search
                        const uint64_t container = rpm<uint64_t>(local + v100::player_components);
                        if (plausible(container)) {
                            for (uint64_t field = 0x10; field <= 0x38; field += 8) {
                                const uint64_t array = rpm<uint64_t>(container + field);
                                const uint64_t count = rpm<uint64_t>(array + v100::array_length);
                                if (!plausible(array) || count > 256) continue;
                                for (uint64_t i = 0; i < count; ++i) {
                                    const uint64_t comp2 = rpm<uint64_t>(array + v100::array_data + i * 8);
                                    if (!obj_ptr(comp2)) continue;
                                    if (rpm<uint64_t>(comp2 + v100::ios_aim) != kgd) continue;
                                    rig_search_quats(comp2, 0x50, "pitch(iohk)", live_p, live_y);
                                    rig_search_quats(comp2, 0x58, "yaw(lotd)", live_p, live_y);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---- 2. current angles ----
        // The control loop MUST read the object it writes. That is kgd (the
        // AimingData): reading the vqn camera state instead would put the
        // game's one-tick filtering into the loop as phase lag — the exact
        // mechanism behind the old write/read fight. The vqn objects are used
        // only by the telemetry (cam=) to verify propagation.
        float cur_pitch{}, cur_yaw{};
        bool have_state = false;
        if (s_rig_valid) {   // dead branch on 1.1.0 (no TRS rig), kept for safety
            if (rig_read(cur_pitch, cur_yaw)) {
                have_state = true;
            } else {
                s_rig_valid = false;  // stale slots, re-find next tick
            }
        }
        if (!have_state && kgd) {
            have_state = kgd_read_angles(kgd, cur_pitch, cur_yaw);
        }
        if (!have_state) { s_state = 1; usleep(5000); continue; }

        // ---- 3. eye position ----
        const Vector3 eye = camera_position();
        if (std::fabs(eye.x) + std::fabs(eye.y) + std::fabs(eye.z) < 0.01f) {
            s_state = 3;
            usleep(5000);
            continue;
        }

        // ---- 4. TARGETING + WRITE: the Aplodium model, ported 1:1 ----
        // Their pitch convention (the rig X-quaternion): POSITIVE = DOWN.
        // The rig branch runs entirely in their convention; the vqn fallback
        // keeps ours (pitch up+).
        const std::vector<aim_targets::Target> snap = aim_targets::snapshot();
        if (snap.empty()) {
            static long long last_empty = 0;
            if (diag::every(last_empty, 2000))
                diag::log("AIM", "no target: EMPTY snapshot (ESP published nothing)");
            s_state = 2;
            usleep(2000);
            continue;
        }

        const float fov_deg = std::clamp(float(cfg::aim::fov_deg), 1.f, 90.f);
        // their engagement state
        static uint64_t s_locked = 0;            // lock_target
        static uint64_t s_return_target = 0;     // back_camera
        static float s_return_pitch = 0.f, s_return_yaw = 0.f;
        static long long s_last_aim_ms = 0;
        const long long now = now_ms();
        // switching the priority mode drops the lock so the new metric takes
        // effect on the very next selection instead of being masked by the
        // sticky lock
        static int s_last_priority = -1;
        if (cfg::aim::priority != s_last_priority) {
            s_last_priority = cfg::aim::priority;
            s_locked = 0;
        }
        // ---- the user's own steering ----
        // The camera follows OUR writes, so after converging on a target the
        // crosshair metric reads ~0 for it — selection then never switches no
        // matter where the player points ("priority does not work"). The only
        // signal of where the PLAYER wants to look is the read-back minus what
        // we last wrote: a pull that big means deliberate steering. Release
        // the lock and let the CURRENT metric re-select from the player's
        // steered view. Recoil and micro-corrections stay far below the
        // threshold; a swipe is 5-30 degrees per tick.
        static float s_written_p = 0.f, s_written_y = 0.f;
        static bool s_have_written = false;
        if (s_have_written && s_locked) {
            const float pull = std::hypot(cur_pitch - s_written_p,
                                          wrap_to_180(cur_yaw - s_written_y));
            if (pull > 2.5f) {
                s_locked = 0;   // the player is steering elsewhere: follow them
            }
        }

        if (s_rig_valid) {
            // ---- their forward() and angular_error() ----
            const float pr = cur_pitch * 0.01745329252f;
            const float yr = cur_yaw * 0.01745329252f;
            const float fwdx = std::sin(yr) * std::cos(pr);
            const float fwdy = -std::sin(pr);
            const float fwdz = std::cos(yr) * std::cos(pr);

            const aim_targets::Target* best = nullptr;
            float best_dp{}, best_dy{}, best_score = 1e9f;
            for (const auto& t : snap) {
                const float dx = t.wx - eye.x;
                const float dy = t.wy - eye.y;
                const float dz = t.wz - eye.z;
                const float norm = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (!(norm > 0.01f)) continue;
                // their angles(): pitch = -asin(dy/norm)
                const float dp = -std::asin(std::clamp(dy / norm, -1.f, 1.f)) * 57.2957795f;
                const float dyaw = std::atan2(dx, dz) * 57.2957795f;
                // their angular_error(): angle between forward and the delta
                const float dotv = std::clamp((fwdx * dx + fwdy * dy + fwdz * dz) / norm, -1.f, 1.f);
                const float ang_err = std::acos(dotv) * 57.2957795f;
                if (ang_err > fov_deg) continue;
                // score: our priority modes, their lock wins over everything
                float score;
                const bool locked = cfg::aim::lock_target && s_locked == t.object;
                // The metric DECIDES; the lock is a stickiness bonus (40% off
                // the score), not an absolute veto. With the old score = -1
                // the first acquired target won forever, so switching the
                // priority mode had no visible effect at all.
                switch (cfg::aim::priority) {
                    case 1: score = t.distance; break;   // closest by range
                    case 2: score = t.hp; break;         // lowest hp
                    default: score = ang_err; break;     // closest by angle
                }
                if (locked) score *= 0.6f;
                if (score < best_score) {
                    best_score = score;
                    best = &t;
                    best_dp = dp;
                    best_dy = dyaw;
                }
            }
            if (best) {
                s_locked = cfg::aim::lock_target ? best->object : 0;
                s_last_aim_ms = now;
                // their error decomposition (their convention)
                float pe = best_dp - cur_pitch;
                float ye = wrap_to_180(best_dy - cur_yaw);
                const float err = std::hypot(pe, ye);
                // ---- speed: the user-facing scale ----
                // 30 = instant (single-write snap), lower = smoother/slower.
                // tau maps linearly: smooth 1 -> 1.2 s, smooth 30 -> 0.
                // The old formula divided by max(1, smooth/4), so settings
                // 1..4 were IDENTICAL and the whole 1..20 range spanned only
                // a 67..328 ms time constant — every value felt "maxed out".
                // Two more speed killers removed with it: the 2.5 deg full
                // snap (made the final approach instant at every setting) and
                // the 0.5 deg/tick traversal cap (a hard 125 deg/s ceiling).
                // The reversal damping is gone too: it cut every correction
                // that crossed zero by 45%, leaving a residual error on a
                // moving target — the "aims slightly off" effect. A pure
                // exponential approach cannot oscillate, so it needs no
                // artificial damping.
                const float smooth = std::clamp(float(cfg::aim::smooth), 1.f, 30.f);
                constexpr float k_slow_tau = 1.2f;   // seconds at smooth == 1
                const float tau = k_slow_tau * (30.f - smooth) / 29.f;
                float alpha = 1.f;
                if (tau > 0.0001f) {
                    // measured dt: the loop is throttled and assuming a fixed
                    // tick made the feel depend on the polling rate
                    static long long s_last_rig_write = 0;
                    const float dt = (s_last_rig_write ? std::max(1.f, float(now - s_last_rig_write)) : 4.f) * 0.001f;
                    s_last_rig_write = now;
                    alpha = std::clamp(1.f - std::exp(-dt / tau), 0.001f, 1.f);
                }
                // settle exactly: below a tenth of a degree the exponential
                // crawl is pointless — write the target directly, which also
                // kills any residual/creep error
                if (err < 0.1f) alpha = 1.f;
                const float move_p = pe * alpha;
                const float move_y = ye * alpha;
                const float next_p = std::clamp(cur_pitch + move_p, -89.f, 89.f);
                const float next_y = wrap_to_180(cur_yaw + move_y);
                // their return-after-shot bookkeeping: save the pre-aim view
                // once per engagement
                if (cfg::aim::back_camera && s_return_target != best->object) {
                    if (s_return_target == 0) {
                        s_return_pitch = cur_pitch;
                        s_return_yaw = cur_yaw;
                    }
                    s_return_target = best->object;
                }
                if (rig_write(next_p, next_y)) {
                    s_writes += 1;
                    s_state = 4;
                    // the write reference for the user-steering detector
                    s_written_p = next_p;
                    s_written_y = next_y;
                    s_have_written = true;
                } else {
                    // stale slots: drop everything, resolve fresh next tick
                    s_rig_valid = false;
                    s_locked = 0;
                    s_return_target = 0;
                    s_vqn_input = 0;
                    s_vqn_player = 0;
                    s_kgd_cache = 0;
                    s_kgd_owner = 0;
                    s_bor_cache = 0;
                    s_bor_local = 0;
                    collect_local = 0;
                    s_state = 1;
                    usleep(2000);
                    continue;
                }
                {
                    static long long last_stat = 0;
                    if (diag::every(last_stat, 1000)) {
                        diag::log("AIM", "rig state=%d err=%.2f alpha=%.2f cur=(%.1f %.1f) out=(%.1f %.1f) writes=%lld",
                                  s_state, err, alpha, cur_pitch, cur_yaw, next_p, next_y, s_writes);
                    }
                }
            } else {
                s_locked = 0;
                // their back_camera: target gone without a kill for a
                // sustained loss — give the saved angles back (occlusion
                // flicker never yanks the view: 300 ms gate)
                if (cfg::aim::back_camera && s_return_target &&
                    now >= s_last_aim_ms && now - s_last_aim_ms > 300) {
                    rig_write(s_return_pitch, s_return_yaw);
                    s_return_target = 0;
                }
                static long long last_nt = 0;
                if (diag::every(last_nt, 2000))
                    diag::log("AIM", "rig no target: snap=%zu fov=%.0f cur=(%.1f %.1f)",
                              snap.size(), fov_deg, cur_pitch, cur_yaw);
                s_state = 2;
            }
        } else if (kgd) {
            // ---- kgd write path: the AimingData IS the input ----
            // The vqn "camera state" is an output the game recomputes every
            // tick (verified: our vqn writes landed and were reset within one
            // tick while the view never moved). kgd's two quaternions are the
            // authoritative aiming state that feeds both the camera and the
            // fire direction.
            // PITCH SIGN: Unity Euler convention — POSITIVE x = looking DOWN
            // (numerically Euler(+90,0,0) * (0,0,1) = (0,-1,0); the rig branch
            // agrees, fwdy = -sin(pr)). kgd_read_angles() returns that
            // down-positive value as cur_pitch, so target_pitch is computed
            // down-positive too. It used to be atan2(dy, horiz) (up-positive):
            // the corrector drove pitch AWAY from the target and the loop
            // limit-cycled — the aim never converged and the view shook.
            const aim_targets::Target* best = nullptr;
            float best_yaw{}, best_pitch{}, best_score = 1e9f, best_ang{};
            for (const auto& t : snap) {
                const float dx = t.wx - eye.x;
                const float dy = t.wy - eye.y;
                const float dz = t.wz - eye.z;
                const float horiz = std::sqrt(dx * dx + dz * dz);
                const float target_yaw = std::atan2(dx, dz) * 57.2957795f;
                const float target_pitch = -std::atan2(dy, horiz) * 57.2957795f;
                const float d_yaw = std::fabs(normalize_yaw(target_yaw - cur_yaw));
                const float d_pitch = std::fabs(target_pitch - cur_pitch);
                if (d_yaw > fov_deg || d_pitch > fov_deg) continue;
                float score;
                const bool locked = cfg::aim::lock_target && s_locked == t.object;
                switch (cfg::aim::priority) {
                    case 1: score = t.distance; break;
                    case 2: score = t.hp; break;
                    default: score = d_yaw; break;
                }
                if (locked) score *= 0.6f;
                if (score < best_score) {
                    best_score = score;
                    best = &t;
                    best_yaw = target_yaw;
                    best_pitch = target_pitch;
                    best_ang = std::sqrt(d_yaw * d_yaw + d_pitch * d_pitch);
                }
            }
            // ---- vqn is a FEEDBACK channel: whatever we write is what we read
            // back next tick, filtered by the game's own camera pipeline. Any
            // corrector that keeps a PRIVATE angle state therefore fights that
            // filtered read-back and oscillates — that is the screen shake. A
            // 0.25 deg re-adopt tolerance made it re-trigger on every single
            // tick, i.e. the feedback loop was wide open at 125 Hz.
            // There is no private state any more. Each write is a bounded
            // proportional correction taken off the LIVE reading, issued at
            // most once per rendered frame, and gated by a hysteresis dead-zone
            // so the loop cannot limit-cycle around a single threshold. Inside
            // the dead-zone we stop writing completely and the player keeps
            // full control of the camera.
            static long long s_last_write = 0;
            static bool s_engaged = false;
            static float s_prev_mp = 0.f, s_prev_my = 0.f;   // last applied move

            constexpr long long k_frame_ms = 16;    // one write per frame (~60 Hz)
            constexpr float k_enter_deg    = 1.5f;  // start correcting above this
            constexpr float k_exit_deg     = 0.6f;  // stop correcting below this
            constexpr float k_max_step_deg = 12.f;  // never whip the camera

            if (!best) {
                s_locked = 0;
                s_engaged = false;
                s_last_write = 0;
                s_prev_mp = s_prev_my = 0.f;
                s_state = 2;
            } else {
                s_locked = cfg::aim::lock_target ? best->object : 0;
                s_engaged = s_engaged ? best_ang > k_exit_deg : best_ang > k_enter_deg;
                const bool frame_due =
                    s_last_write == 0 || now - s_last_write >= k_frame_ms;
                if (!s_engaged || !frame_due) {
                    s_state = 4;  // tracking, deliberately not writing
                } else {
                    // real elapsed time, so the feel does not depend on the
                    // polling rate or on a dropped frame
                    const float dt = (s_last_write == 0 ? float(k_frame_ms)
                                                        : float(now - s_last_write)) * 0.001f;
                    // cfg::aim::smooth is 1..20 (higher = faster) -> a time
                    // constant, then a frame-rate independent exponential step
                    const float tau = std::clamp((21.f - float(cfg::aim::smooth)) * 0.012f,
                                                 0.006f, 0.25f);
                    const float alpha = std::clamp(1.f - std::exp(-dt / tau), 0.02f, 1.f);
                    const float err_p = std::clamp(best_pitch - cur_pitch, -89.f, 89.f);
                    const float err_y = normalize_yaw(best_yaw - cur_yaw);
                    const float lim = std::max(0.5f, k_max_step_deg * (dt / 0.016f));
                    float move_p = std::clamp(err_p * alpha, -lim, lim);
                    float move_y = std::clamp(err_y * alpha, -lim, lim);
                    // Reversal suppression. A correction that flips direction
                    // against the previous one IS the visible twitch: the game
                    // filters our write, we read the filtered value back, we
                    // correct against it, and the two alternate. Small reversals
                    // are dropped instead of written, so the view only glides
                    // toward the target and then settles — no back-and-forth.
                    constexpr float k_rev_dead_deg = 2.f;
                    if (s_prev_mp * move_p < 0.f && std::fabs(move_p) < k_rev_dead_deg) move_p = 0.f;
                    if (s_prev_my * move_y < 0.f && std::fabs(move_y) < k_rev_dead_deg) move_y = 0.f;
                    s_prev_mp = move_p;
                    s_prev_my = move_y;
                    if (move_p != 0.f || move_y != 0.f) {
                        const float out_pitch = std::clamp(cur_pitch + move_p, -89.f, 89.f);
                        const float out_yaw = normalize_yaw(cur_yaw + move_y);
                        // The owner check is part of the write safety contract:
                        // resolve_kgd caches by owner, and a mismatch means the
                        // local player changed under us (respawn) — re-resolve
                        // instead of writing a recycled object.
                        if (s_kgd_owner == local && kgd_write_angles(kgd, out_pitch, out_yaw)) {
                            s_last_write = now;
                            s_writes += 1;
                            s_state = 4;
                            static long long last_vstat = 0;
                            if (diag::every(last_vstat, 500)) {
                                // `cur` is the kgd AimingData we WRITE; `cam` is
                                // the vqn camera state the game derives from it.
                                // If cam tracks cur as we write, kgd really is
                                // the source of the view — and the same state
                                // feeds the fire direction, so bullets follow
                                // the crosshair. If cam does NOT move, the
                                // quaternion composition order is wrong (it
                                // would still be a valid rotation, just aimed
                                // wrongly) and must be flipped.
                                float cam_p = 0.f, cam_y = 0.f;
                                const bool have_cam = s_vqn_player &&
                                    vqn_read_angles(s_vqn_player, cam_p, cam_y);
                                diag::log("AIM", "kgd ang=%.2f cur=(%.1f %.1f) cam=(%.1f %.1f) "
                                                 "tgt=(%.1f %.1f) move=(%.2f %.2f) alpha=%.2f writes=%lld",
                                          best_ang, cur_pitch, cur_yaw,
                                          have_cam ? cam_p : 0.f, have_cam ? cam_y : 0.f,
                                          best_pitch, best_yaw,
                                          move_p, move_y, alpha, s_writes);
                            }
                        } else {
                            s_kgd_cache = 0;
                            s_kgd_owner = 0;
                            s_vqn_input = 0;
                            s_vqn_player = 0;
                            collect_local = 0;
                            s_engaged = false;
                            s_last_write = 0;
                            s_state = 1;
                        }
                    } else {
                        s_state = 4;   // settled: nothing to correct, no write
                    }
                }
            }
        }
        // (no separate "no channel" branch: kgd is guaranteed non-zero by the
        // early backoff above, and the rig branch re-checks its own validity)

        // ~250 Hz polling. Every tick costs several remote reads (kgd quat
        // probe, view/proj matrices, the bor walk) plus a snapshot copy under
        // a mutex; at 500 Hz that process_vm_readv load is heavy enough to
        // stutter the game itself, and it bought nothing because the write
        // gate is one write per frame (~60 Hz). Still ~4x oversampled.
        usleep(4000);
    }
}

// Called from the render frame: only ensures the polling thread exists.
void tick() {
    if (cfg::aim::enabled && !s_thread_up.exchange(true)) {
        std::thread(loop).detach();
    }
}

} // namespace aim
