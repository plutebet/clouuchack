
// ================================================================================
// Бот с сурсами: @imguisource_bot
// Основной канал (ImGui Source): @ImGui_Source_FIXNEY
// Дополнительный канал (shit src 🧨): @il2cpp_src
// ================================================================================

#pragma once
#include "World2Screen.h"
#include "Math/Math.h"
#include "../../GetEntities/Entities.h"
#include "../Threading/Threading.h"
#include "../Diag/Profiler.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

uintptr_t W2S::g_CachedMatrixAddr = 0;
Matrix4x4 W2S::g_ViewMatrix = {};
Vector3 W2S::g_CameraPos = {};
bool W2S::g_HaveCameraPos = false;
std::chrono::steady_clock::time_point W2S::g_LastUpdateTime =
    std::chrono::steady_clock::now();

float W2S::g_ScreenW = (float)GetSystemMetrics(SM_CXSCREEN);
float W2S::g_ScreenH = (float)GetSystemMetrics(SM_CYSCREEN);

namespace {
// ── published view*projection (seqlock) ─────────────────────────────────────
// The updater thread writes the whole matrix behind an odd/even sequence
// counter; the render thread reads it without ever blocking.
std::atomic<uint32_t> g_w2sSeq{0};
Matrix4x4 g_w2sMatrix{};
Vector3 g_w2sCamPos{};
bool g_w2sHavePos = false;
std::atomic<bool> g_w2sReady{false};
std::atomic<uint64_t> g_matrixPublishUs{0};

// Writer-side staging filled in by UpdateMatrixAddress().
Vector3 s_camPos{};
bool s_havePos = false;

PollingThread g_w2sThread;

// Recover the camera's WORLD position straight from the view*projection matrix
// M. WorldToScreen() computes
//     x = (M11*x + M21*y + M31*z + M41) / w
//     y = (M12*x + M22*y + M32*z + M42) / w
//     w =  M14*x + M24*y + M34*z + M44
// The camera's own position is the point where the projection is degenerate -
// it maps to the screen centre AND has w = 0 - i.e. the solution of
//     M11*Cx + M21*Cy + M31*Cz = -M41
//     M12*Cx + M22*Cy + M32*Cz = -M42
//     M14*Cx + M24*Cy + M34*Cz = -M44
// A 3x3 system solved with Cramer's rule. This guarantees a distance origin
// whenever the matrix is usable (i.e. whenever the ESP draws boxes at all),
// even if the separate Camera.position read is unavailable.
bool DeriveCameraPos(const Matrix4x4 &m, Vector3 &out) {
  const double a11 = m._11, a12 = m._21, a13 = m._31;
  const double a21 = m._12, a22 = m._22, a23 = m._32;
  const double a31 = m._14, a32 = m._24, a33 = m._34;
  const double b1 = -(double)m._41, b2 = -(double)m._42, b3 = -(double)m._44;

  const double det = a11 * (a22 * a33 - a23 * a32) -
                     a12 * (a21 * a33 - a23 * a31) +
                     a13 * (a21 * a32 - a22 * a31);
  if (!std::isfinite(det) || fabs(det) < 1e-9)
    return false;

  const double d1 = b1 * (a22 * a33 - a23 * a32) -
                    a12 * (b2 * a33 - a23 * b3) + a13 * (b2 * a32 - a22 * b3);
  const double d2 = a11 * (b2 * a33 - a23 * b3) -
                    b1 * (a21 * a33 - a23 * a31) + a13 * (a21 * b3 - b2 * a31);
  const double d3 = a11 * (a22 * b3 - b2 * a32) -
                    a12 * (a21 * b3 - b2 * a31) + b1 * (a21 * a32 - a22 * a31);

  const double cx = d1 / det, cy = d2 / det, cz = d3 / det;
  if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cz))
    return false;

  out = Vector3((float)cx, (float)cy, (float)cz);
  return true;
}

void PublishMatrix(const Matrix4x4 &m) {
  // How often the game actually changes the camera matrix. If this is ~30/s
  // while the overlay renders at 1000/s, the ESP cannot look faster than the
  // game camera regardless of overlay FPS.
  static Matrix4x4 lastMatrix{};
  static bool haveLastMatrix = false;
  if (!haveLastMatrix || std::memcmp(&lastMatrix, &m, sizeof(Matrix4x4)) != 0) {
    lastMatrix = m;
    haveLastMatrix = true;
    prof::matrixChanges.fetch_add(1, std::memory_order_relaxed);
  }

  // Prefer the position derived from the matrix: it is always available while
  // the matrix is, whereas s_camPos (Camera.position @0x444) is often zero.
  Vector3 camPos = s_camPos;
  bool havePos = s_havePos;
  Vector3 derived;
  if (DeriveCameraPos(m, derived)) {
    camPos = derived;
    havePos = !camPos.Empty() && camPos.IsValid();
  }

  g_w2sSeq.fetch_add(1, std::memory_order_acq_rel); // odd  -> writing
  g_w2sMatrix = m;
  g_w2sCamPos = camPos;
  g_w2sHavePos = havePos;
  g_w2sSeq.fetch_add(1, std::memory_order_acq_rel); // even -> stable
  g_w2sReady.store(true, std::memory_order_release);
  g_matrixPublishUs.store(prof::NowUs(), std::memory_order_release);
}

bool ReadPublished(Matrix4x4 &m, Vector3 &camPos, bool &havePos) {
  for (int attempt = 0; attempt < 8; ++attempt) {
    const uint32_t s1 = g_w2sSeq.load(std::memory_order_acquire);
    if (s1 & 1u)
      continue; // writer mid-update
    const Matrix4x4 mm = g_w2sMatrix;
    const Vector3 cp = g_w2sCamPos;
    const bool hp = g_w2sHavePos;
    const uint32_t s2 = g_w2sSeq.load(std::memory_order_acquire);
    if (s1 == s2) {
      m = mm;
      camPos = cp;
      havePos = hp;
      return true;
    }
  }
  return false;
}
} // namespace

extern uint64_t g_gameBase;
extern uint64_t g_unityBase;
extern uint64_t g_camKlass;  // found in ResolveClasses()
extern uint64_t g_camField;  // static slot of the Camera ptr (dump: 0x48)
extern bool g_camFieldLocked;
extern BaseNetworkable *g_entities;

// Pointers that live inside a PE module are code/metadata, never the native
// peer of a managed Unity object.
static bool InModuleRange(uint64_t p) {
  if (p >= g_gameBase && p < g_gameBase + 0x14000000ull)
    return true;  // GameAssembly.dll (live size ~0x12489000)
  if (g_unityBase && p >= g_unityBase && p < g_unityBase + 0x8000000ull)
    return true;  // UnityPlayer.dll
  return false;
}

static bool AllFinite(const Matrix4x4 &m) {
  const float v[16] = {m._11, m._12, m._13, m._14, m._21, m._22, m._23,
                       m._24, m._31, m._32, m._33, m._34, m._41, m._42,
                       m._43, m._44};
  for (float f : v)
    if (!std::isfinite(f))
      return false;
  return true;
}

// The x/y numerators WorldToScreen() divides by w (same field convention).
static inline void ProjNumerators(const Matrix4x4 &m, const Vector3 &p,
                                  float &xn, float &yn) {
  xn = m._11 * p.x + m._21 * p.y + m._31 * p.z + m._41;
  yn = m._12 * p.x + m._22 * p.y + m._32 * p.z + m._42;
}

// A matrix that projects WORLD positions must satisfy two things the bare
// projection matrix does not:
//   * it sends the camera's own world position to clip (0,0) -> screen centre
//   * it carries a real translation (a projection matrix has none)
// This is what separates Camera.view*projection (what W2S needs) from
// Camera.projectionMatrix (which also has a valid-looking (0,0,-1,0) w row -
// the old signature check latched THAT at native+0xB0 and produced no boxes).
static bool IsUsableViewProj(const Matrix4x4 &m, const Vector3 &camPos) {
  if (!AllFinite(m))
    return false;
  if (fabsf(m._14) + fabsf(m._24) + fabsf(m._34) + fabsf(m._44) < 0.5f)
    return false;  // degenerate w row
  if (fabsf(m._41) + fabsf(m._42) + fabsf(m._43) + fabsf(m._44) < 1.0f)
    return false;  // no translation -> projection matrix, not view*projection
  float xn, yn;
  ProjNumerators(m, camPos, xn, yn);
  // Loose enough for ADS/scope camera offsets, still rejects a bare projection
  // matrix (which has no translation row and is rejected above).
  return fabsf(xn) < 0.5f && fabsf(yn) < 0.5f;
}

// Weaker test for when the camera position is unknown.
static bool IsAnyViewProj(const Matrix4x4 &m) {
  if (!AllFinite(m))
    return false;
  if (fabsf(m._14) + fabsf(m._24) + fabsf(m._34) + fabsf(m._44) < 0.5f)
    return false;
  if (fabsf(m._41) + fabsf(m._42) + fabsf(m._43) + fabsf(m._44) < 1.0f)
    return false;
  return true;
}

// ── strong validity oracle for a view*projection ───────────────────────────
// IsAnyViewProj (finite + "carries a translation") is far too weak to trust on
// its own: when the native Camera object is recreated by a scene change or a
// respawn, the cached matrix address can be reused by unrelated data whose
// floats still look finite. If that garbage happens to have a huge w row, every
// world point divides out to (0,0) - which is exactly the "after a while all the
// boxes and ESP collapse into the middle of my screen" bug. These extra checks
// reject it and force the address to be re-resolved:
//   * the w row of a real view*projection is a camera basis axis (~unit length),
//     never a scaled-up garbage row;
//   * the camera position recovered from the matrix must sit inside the map;
//   * when the local player is known, the derived camera must be right next to
//     it (the render camera IS the local player's eye).
static bool DerivedCameraPlausible(const Matrix4x4 &m, Vector3 &camOut) {
  if (!AllFinite(m))
    return false;

  const float wmag = sqrtf(m._14 * m._14 + m._24 * m._24 + m._34 * m._34);
  if (!(wmag > 0.05f && wmag < 50.f))
    return false;  // scaled/garbage w row -> everything would collapse to centre

  if (fabsf(m._41) + fabsf(m._42) + fabsf(m._43) + fabsf(m._44) < 1.0f)
    return false;  // no translation -> projection matrix, not view*projection

  Vector3 cam;
  if (!DeriveCameraPos(m, cam) || !cam.IsValid() || cam.Empty())
    return false;

  // Rust maps live well inside these generous bounds; a derived camera outside
  // them is not a real viewpoint.
  if (cam.x < -20000.f || cam.x > 20000.f || cam.y < -6000.f ||
      cam.y > 8000.f || cam.z < -20000.f || cam.z > 20000.f)
    return false;

  camOut = cam;
  return true;
}

// Offset of the view*projection matrix inside the native Camera object. The
// dump says 0x2FC; if that moves we auto-heal by scanning the object.
//
// The scan is a LAST RESORT and is deliberately hard to trigger: it costs
// hundreds of locked driver round-trips (each one shared with the entity worker
// and the aim worker), so running it on every failed frame is what starved the
// ESP over a long session. It is (a) only run when the current offset fails,
// (b) rate-limited, (c) bounded to a small window, and (d) NOT allowed to latch
// an offset on its own - the caller commits a found offset only after the full
// strong oracle accepts the resulting matrix. Latching a weak hit here used to
// let the offset oscillate between unrelated slots, and every re-latch re-ran
// the whole sweep ("ESP stucks then continues").
static uint64_t g_matrixOff = 0x2FC;
static std::chrono::steady_clock::time_point g_lastMatrixScan{};
static constexpr auto kMatrixScanGap = std::chrono::milliseconds(1500);

// Read the view*projection out of the native Camera object at nat, verifying
// it with the camera-position oracle (camPos = Camera.position @0x444). If the
// cached offset no longer holds a usable matrix, scan the object for one and
// report the candidate offset via foundOff (0 = none) WITHOUT committing it.
static bool ReadViewProj(uint64_t nat, const Vector3 &camPos, bool havePos,
                         Matrix4x4 &out, bool allowScan, uint64_t &foundOff) {
  foundOff = 0;
  out = Driver->Read<Matrix4x4>(nat + g_matrixOff);
  if (havePos ? IsUsableViewProj(out, camPos) : IsAnyViewProj(out))
    return true;
  if (!allowScan)
    return false;

  // Rate-limit: a scan that just failed must not immediately re-run.
  const auto now = std::chrono::steady_clock::now();
  if (now - g_lastMatrixScan < kMatrixScanGap)
    return false;
  g_lastMatrixScan = now;

  // Bounded window. Every offset ever observed live (0x84, 0x9C, 0xA4, 0xB4,
  // 0xE4, 0xF0, 0x204, 0x3E8, dump 0x2FC) is < 0x800, so scanning to 0x2000
  // (an extra ~1300 round-trips per probe) bought nothing.
  for (uint64_t o = 0x80; o <= 0x800; o += 4) {
    if (o == g_matrixOff)
      continue;
    Matrix4x4 c = Driver->Read<Matrix4x4>(nat + o);
    if (havePos ? IsUsableViewProj(c, camPos) : IsAnyViewProj(c)) {
      foundOff = o;
      out = c;
      return true;
    }
  }
  return false;
}

// The [camdbg] narration is a ~60-line burst per probe, and while a chain heals
// the probe can run several times a second - which floods the console, and the
// console's synchronous redraw is itself a steady CPU/heat cost over a long
// session. Cap the detailed burst to one per few seconds; the one-line summary
// still prints on every successful probe.
static bool CamDbgDue() {
  using namespace std::chrono;
  static steady_clock::time_point last{};
  const auto now = steady_clock::now();
  if (now - last < seconds(5))
    return false;
  last = now;
  return true;
}

// Finds the Camera pointer slot in MainCamera's static fields. The current
// dump says camera=0x48, but while in the menu the slot can read 0 (or the
// dump may go stale after a game update), so probe sf[0..0x400] for a slot
// whose managed object's native ptr (+0x10) holds a REAL view*projection
// matrix. The local player's eye (Camera.position @0x444) is only used to
// pick between candidates, never to reject the dump slot.
static bool ResolveCameraField(bool force) {
  using namespace std::chrono;
  static steady_clock::time_point last{};
  const auto now = steady_clock::now();
  // A forced probe (the cached chain just went bad) runs at most every 400 ms;
  // the routine probe waits 2 s so it never hammers the driver.
  const auto gap = force ? milliseconds(400) : seconds(2);
  if (now - last < gap)
    return false;  // don't hammer the probe (it runs per failed frame)
  last = now;

  // If the driver channel is draining a timed-out request, every read below
  // would fail anyway - and trying turns a one-off slow read into a sustained
  // re-resolution storm (the classic "ESP stucks then continues"). Wait for the
  // channel to recover and keep the last good matrix in the meantime.
  if (Driver->IsChannelStalled())
    return false;

  if (!g_camKlass)
    return false;
  const uint64_t sf =
      Driver->ReadPointer(g_camKlass + MainCamera_Offsets::static_fields);
  if (sf < 0x10000) {
    printf("[cam] probe: sf=0x%llX BAD\n", (unsigned long long)sf);
    return false;
  }

  // local player eye position oracle (the camera sits at the same viewpoint)
  Vector3 lpPos{};
  bool haveLp = false;
  if (g_entities) {
    const uintptr_t lp = g_entities->GetLocalPlayer();
    if (lp) {
      auto snap = g_entities->Snapshot();
      if (snap) {
        for (const auto &e : *snap) {
          if (e.address == lp && !e.position.Empty()) {
            lpPos = e.position;
            haveLp = true;
            break;
          }
        }
      }
    }
  }

  struct CamCand {
    uint64_t off;
    float dist;  // distance camera<->local player (-1 = unknown)
  };
  CamCand cands[8];
  int nCands = 0;

  auto check = [&](uint64_t off, bool verbose, bool allowScan) -> bool {
    const uint64_t v = Driver->ReadPointer(sf + off);
    uint64_t natManaged = 0, natDirect = 0, nat = 0;
    Vector3 camPos{};
    bool havePos = false;
    Matrix4x4 m{};
    // A native Camera is only accepted if its own Camera.position (0x444) can
    // verify the view*projection we read out of it (the position must map to
    // the centre of the screen). This rejects the bare projection matrix.
    auto tryNat = [&](uint64_t n) -> bool {
      if (n < 0x10000 || InModuleRange(n))
        return false;
      const Vector3 cp = Driver->Read<Vector3>(n + 0x444);
      const bool hp = !cp.Empty() && cp.IsValid();
      uint64_t foundOff = 0;
      if (!ReadViewProj(n, cp, hp, m, allowScan, foundOff))
        return false;
      // Reject a slot whose matrix does not recover a plausible camera. The
      // near-player check is only used to RANK candidates below, never to
      // reject one: while scoped/ADS the camera offset can legitimately differ
      // from the player's feet.
      Vector3 derived;
      if (!DerivedCameraPlausible(m, derived))
        return false;
      if (foundOff && foundOff != g_matrixOff) {
        g_matrixOff = foundOff;
        printf("[+] view matrix offset at native+0x%llX (dump says 0x2FC)\n",
               (unsigned long long)foundOff);
        fflush(stdout);
      }
      camPos = cp;
      havePos = hp;
      nat = n;
      return true;
    };
    bool ok = false;
    // (a) the slot holds a managed object -> +0x10 is its native peer
    if (v >= 0x10000 && !InModuleRange(v)) {
      natManaged = Driver->ReadPointer(v + Object_Offsets::m_CachedPtr);
      if (tryNat(natManaged))
        ok = true;
    }
    // (b) some builds put the native object straight in the static field
    if (!ok && v >= 0x10000 && !InModuleRange(v)) {
      if (tryNat(v)) {
        ok = true;
        natDirect = v;
      }
    }
    float dist = -1.0f;
    if (ok && haveLp && havePos)
      dist = camPos.Distance(lpPos);
    if (verbose && CamDbgDue()) {
      float xn = 0.0f, yn = 0.0f;
      if (ok && havePos)
        ProjNumerators(m, camPos, xn, yn);
      printf("[camdbg] sf+0x%llX v=0x%llX nat=0x%llX%s m11=%.4f m22=%.4f "
             "m33=%.4f m34=%.4f m44=%.4f camDist=%.1f lp=%s oracle=(%.4f,%.4f) "
             "-> %s\n",
             (unsigned long long)off, (unsigned long long)v,
             (unsigned long long)natManaged,
             natDirect ? "(direct)" : "", m._11, m._22, m._33, m._34, m._44,
             dist, haveLp ? "known" : "n/a", xn, yn, ok ? "OK" : "reject");
      printf("[camdbg] matrix @0x%llX: [%.3f %.3f %.3f %.3f | %.3f %.3f %.3f "
             "%.3f | %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f]\n",
             (unsigned long long)(nat + g_matrixOff), m._11, m._12, m._13,
             m._14, m._21, m._22, m._23, m._24, m._31, m._32, m._33, m._34,
             m._41, m._42, m._43, m._44);
    }
    if (ok && nCands < 8)
      cands[nCands++] = {off, dist};
    return ok;
  };

  if (!check(g_camField, true, true)) {  // dump slot: verbose + full diag
    // Probe 8-byte-aligned slots only: static object-pointer slots are pointer
    // aligned, so a 4-byte sweep adds nothing. The old fallback here (a SECOND
    // 0..0x400 step-4 sweep with allowScan=true, i.e. ~256 probes x ~500+ driver
    // reads = a ~half-million-read burst, re-run every 400 ms while the chain
    // was unhealthy) was the single biggest cause of the ESP stalling then
    // resuming on a long session. The auto-heal of the matrix offset still
    // happens - but only inside the dump slot's own bounded, rate-limited scan.
    for (uint64_t off = 0; off < 0x400; off += 8) {
      if (off == g_camField)
        continue;
      check(off, off < 0x100, false);  // only narrate the first few slots
    }
  }

  if (nCands == 0) {
    if (CamDbgDue())
      printf("[!] camera field not found in sf[0..0x400] (see [camdbg]) - in "
             "menu?\n");
    return false;
  }

  // Prefer the candidate whose Camera.position is closest to the local
  // player's eye; otherwise keep the first (dump slot is checked first).
  int pick = 0;
  if (haveLp) {
    float best = -1.0f;
    for (int i = 0; i < nCands; i++) {
      if (cands[i].dist >= 0.0f && (best < 0.0f || cands[i].dist < best)) {
        best = cands[i].dist;
        pick = i;
      }
    }
  }
  g_camField = cands[pick].off;
  g_camFieldLocked = true;
  printf("[+] MainCamera camera at sf+0x%llX (dump says 0x%llX, %d candidate%s"
         ", camDist=%.1f)\n",
         (unsigned long long)g_camField,
         (unsigned long long)MainCamera_Offsets::mainCamera, nCands,
         nCands == 1 ? "" : "s", cands[pick].dist);
  return true;
}

bool W2S::TryRefreshMatrixAddress() {
  if (!Driver || !Driver->IsAttached() || !g_gameBase || !g_camKlass ||
      !g_camFieldLocked)
    return false;
  if (Driver->IsChannelStalled())
    return false;

  const uint64_t sf =
      Driver->ReadPointer(g_camKlass + MainCamera_Offsets::static_fields);
  const uint64_t cam = sf ? Driver->ReadPointer(sf + g_camField) : 0;
  const uint64_t native =
      cam ? Driver->ReadPointer(cam + Object_Offsets::m_CachedPtr) : 0;
  if (native < 0x10000 || InModuleRange(native))
    return false;

  // Structural only: ADS/scope offsets must never make a valid matrix look bad.
  const Matrix4x4 m = Driver->Read<Matrix4x4>(native + g_matrixOff);
  if (!IsAnyViewProj(m))
    return false;

  const uintptr_t address = native + g_matrixOff;
  if (address != g_CachedMatrixAddr) {
    g_CachedMatrixAddr = address;
    printf("[w2s] camera matrix moved -> 0x%llX\n",
           (unsigned long long)address);
    fflush(stdout);
  }
  return true;
}

bool W2S::UpdateMatrixAddress() {
  if (!Driver || !Driver->IsAttached() || !g_gameBase) {
    return false;
  }
  if (!g_camKlass) {
    return false;  // ResolveClasses() hasn't found MainCamera yet
  }

  // Do not escalate while the driver channel is draining a timed-out request:
  // every read would fail and we would tear down the cached address and churn
  // through re-resolution. Keep the last good published matrix instead.
  if (Driver->IsChannelStalled())
    return false;

  if (!g_camFieldLocked && !ResolveCameraField(false)) {
    return false;  // slot not confirmed yet (probe throttled / in menu)
  }

  // MainCamera found in ResolveClasses: klass -> static_fields -> +camera
  // slot (dump 0xD0, re-probed if the chain dies) -> +m_CachedPtr -> +0x2FC.
  // The chain is only accepted if the matrix it yields passes the strong
  // oracle, so a reused/stale camera pointer can never be latched.
  auto readChain = [&]() -> bool {
    const uint64_t sf =
        Driver->ReadPointer(g_camKlass + MainCamera_Offsets::static_fields);
    const uint64_t cam = sf ? Driver->ReadPointer(sf + g_camField) : 0;
    const uint64_t native =
        cam ? Driver->ReadPointer(cam + Object_Offsets::m_CachedPtr) : 0;
    if (native < 0x10000 || InModuleRange(native))
      return false;

    const Matrix4x4 m = Driver->Read<Matrix4x4>(native + g_matrixOff);
    Vector3 derived;
    if (!DerivedCameraPlausible(m, derived))
      return false;

    // Independent oracle: read the camera's own position (0x444) at the same
    // instant - a real view*projection maps it to the centre of the screen.
    const Vector3 camPos = Driver->Read<Vector3>(native + 0x444);
    const bool havePos = !camPos.Empty() && camPos.IsValid();
    if (havePos && !IsUsableViewProj(m, camPos))
      return false;

    g_CachedMatrixAddr = native + g_matrixOff;
    s_camPos = camPos;
    s_havePos = havePos;
    return true;
  };

  if (readChain())
    return true;

  // The chain died (the camera object was recreated by a scene change or a
  // respawn). Drop the cached address so the ESP keeps using the last good
  // matrix instead of drawing with garbage, then force an immediate re-probe so
  // we recover quickly instead of waiting for the throttle.
  g_CachedMatrixAddr = 0;
  g_camFieldLocked = false;
  if (!ResolveCameraField(true))
    return false;
  return readChain();
}

bool W2S::ReadMatrixFromMemory() {
  if (!g_CachedMatrixAddr)
    return false;

  const Matrix4x4 tmp = Driver->Read<Matrix4x4>(g_CachedMatrixAddr);
  // Structural validation only. ADS/scope FOV changes can make the derived
  // camera-position oracle disagree with the local player for a moment; using
  // it here made every ADS transition look like a dead camera and triggered a
  // full re-probe (the ~1s ESP freeze). A matrix with a sane w row and a real
  // translation row is still a usable view*projection.
  if (!IsAnyViewProj(tmp)) {
    // Throttled so a stuck chain does not flood the console.
    static std::chrono::steady_clock::time_point lastLog{};
    const auto now = std::chrono::steady_clock::now();
    if (now - lastLog > std::chrono::seconds(2)) {
      lastLog = now;
      const float wmag = sqrtf(tmp._14 * tmp._14 + tmp._24 * tmp._24 +
                               tmp._34 * tmp._34);
      printf("[w2s] matrix @0x%llX lost (wmag=%.2f) - re-resolving camera\n",
             (unsigned long long)g_CachedMatrixAddr, wmag);
    }
    return false;  // caller re-resolves the address
  }

  PublishMatrix(tmp);
  return true;
}

// Read the published matrix into the render-visible copy. Called once per
// frame from the render thread - never touches the driver.
bool W2S::PrepareMatrix() {
  if (!g_w2sReady.load(std::memory_order_acquire))
    return false;

  Matrix4x4 m;
  Vector3 cp{};
  bool hp = false;
  if (ReadPublished(m, cp, hp)) {
    g_ViewMatrix = m;
    g_CameraPos = cp;
    g_HaveCameraPos = hp;
  }
  // If a read raced the writer we keep the last good frame (at most one frame
  // stale - the matrix is republished every couple of ms).
  return true;
}

// Runs on the updater thread: refresh the address occasionally, read the
// matrix every tick, publish.
void W2S::UpdaterTick() {
  prof::ScopeTimer _matrixTimer(&prof::matrixTick);
  using namespace std::chrono;
  if (!Driver || !Driver->IsAttached() || !g_gameBase || !g_camKlass)
    return;

  const auto now = steady_clock::now();

  if (g_CachedMatrixAddr == 0) {
    if (!UpdateMatrixAddress())
      return; // keep the last published matrix
    g_LastUpdateTime = now;
  }

  // Cheap camera-address refresh: follows a recreated/scope camera without
  // ever running the expensive probe. A failure is ignored.
  static auto lastAddressRefresh = now;
  if (now - lastAddressRefresh >= milliseconds(250)) {
    lastAddressRefresh = now;
    if (TryRefreshMatrixAddress())
      g_LastUpdateTime = now;
  }

  if (ReadMatrixFromMemory()) {
    g_LastUpdateTime = now;
    return;
  }

  // A single bad read during an ADS/scope camera swap must not trigger the
  // expensive camera probe. Ride out the transition on the last good matrix.
  if (now - g_LastUpdateTime < milliseconds(300))
    return;

  // Cached address stopped yielding a usable matrix (scene change, respawn).
  // Re-resolve once and recover.
  if (!UpdateMatrixAddress())
    return;
  g_LastUpdateTime = now;
  ReadMatrixFromMemory();
}

void W2S::StartUpdater() {
  // Dedicated matrix lane: slot 0 is the entity/image worker, so this read is
  // never queued behind a busy ESP tick. 1 ms = a fresh matrix for every
  // overlay frame, which is what keeps boxes glued to the world while the
  // mouse turns.
  g_w2sThread.Start(
      [] {
        if (Driver)
          Driver->SelectSlot(1);
        W2S::UpdaterTick();
      },
      std::chrono::milliseconds(1));
  printf("[+] W2S updater thread started (1ms tick, driver lane 1)\n");
}

void W2S::StopUpdater() { g_w2sThread.Stop(); }

namespace {
bool W2S_Project(const Matrix4x4 &m, const Vector3 &worldPos,
                 Vector2 &screenPos) {
  float w =
      m._14 * worldPos.x + m._24 * worldPos.y + m._34 * worldPos.z + m._44;

  if (w < 0.001f)
    return false;

  float x =
      m._11 * worldPos.x + m._21 * worldPos.y + m._31 * worldPos.z + m._41;
  float y =
      m._12 * worldPos.x + m._22 * worldPos.y + m._32 * worldPos.z + m._42;

  screenPos.x = (W2S::g_ScreenW * 0.5f) * (1.0f + x / w);
  screenPos.y = (W2S::g_ScreenH * 0.5f) * (1.0f - y / w);

  if (screenPos.x < -1000.0f || screenPos.x > W2S::g_ScreenW + 1000.0f ||
      screenPos.y < -1000.0f || screenPos.y > W2S::g_ScreenH + 1000.0f)
    return false;

  return true;
}
} // namespace

bool W2S::WorldToScreen(const Vector3 &worldPos, Vector2 &screenPos) {
  // Render thread: use the local copy the frame already prepared.
  return W2S_Project(g_ViewMatrix, worldPos, screenPos);
}

// Worker threads (the aim worker) must not touch g_ViewMatrix / g_CameraPos:
// that would race the render thread's WorldToScreen(). Read straight from the
// seqlock-published copy instead.
bool W2S::ProjectPublished(const Vector3 &worldPos, Vector2 &screenPos) {
  Matrix4x4 m;
  Vector3 cp{};
  bool hp = false;
  if (!ReadPublished(m, cp, hp))
    return false;
  return W2S_Project(m, worldPos, screenPos);
}

bool W2S::GetPublishedCameraPos(Vector3 &out) {
  Matrix4x4 m;
  Vector3 cp{};
  bool hp = false;
  if (!ReadPublished(m, cp, hp) || !hp)
    return false;
  out = cp;
  return true;
}

uint64_t W2S::PublishedUs() {
  return g_matrixPublishUs.load(std::memory_order_acquire);
}
