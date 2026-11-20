
// ================================================================================
// Release channel: t.me/ImGui_Source_FIXNEY (ImGui Source)
// Second channel: t.me/il2cpp_src (shit src 🧨)
// Telegram bot: t.me/imguisource_bot
// ================================================================================

#include "Aim.h"

#include "Driver/Rpm.h"
#include "GetEntities/Entities.h"
#include "GetEntities/GhostClock.h"
#include "GetEntities/Prefabs/Prefabs.h"
#include "GetEntities/Skeleton.h"
#include "Il2Cpp/Decryptions/decryption.h"
#include "Menu/Config.h"
#include "Offsets/Offsets.h"
#include "Threading/Threading.h"
#include "World2Screen/World2Screen.h"
#include "libs/overlay/Overlay.h"
#include "libs/render/Renderer.h"

#include <atomic>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdarg>

extern BaseNetworkable *g_entities;

namespace aim {
namespace {

// ── tuning ──────────────────────────────────────────────────────────────────

constexpr float kAlignedPx = 1.5f;      // mouse dead-zone (px)
constexpr float kMaxStepCounts = 8.f;   // max mouse counts per tick (kept low so
                                        // the aim assists instead of pinning the
                                        // cursor - it must stay escapable)

// Keep the current target while it stays inside FOV * this (hysteresis), so the
// aim does not flick between two players both near the crosshair.
constexpr float kStickyFovMul = 1.4f;

// Aim point above the target's feet (the ESP box runs feet -> feet + 1.9).
constexpr float kBoneHead = 1.62f;
constexpr float kBoneChest = 1.05f;
// The local player's eye above the feet: the origin the view angles use.
constexpr float kEyeHeight = 1.62f;
// Gravity used for ballistic drop compensation (Rust ~= 9.81 m/s^2).
constexpr float kGravity = 9.81f;

// BasePlayer -> PlayerEyes field offsets. The 2026-11-06 dump (new build)
// puts eyes at 0x328; the second candidate is the same value via Offsets.h.
constexpr uintptr_t kEyesFieldDump = 0x328;  // resources.hpp BasePlayer::eyes

// Aim worker cadence. The angle method writes the exact aim every tick and owns
// ALL of the aim's driver traffic (the render thread never blocks on the driver
// - that was the FPS regression). 2.5 ms keeps a fresh write well inside every
// game frame; the loop is deadline-based so it does not drift.
constexpr auto kAimInterval = std::chrono::microseconds(2500);
// Re-validate the cached eye pointer this often (ticks), in case we respawned.
constexpr int kEyeRecheckTicks = 400;

// How long a briefly-lost target keeps its last angle written. Snapshot churn,
// smoke or a single culled/occluded frame must not interrupt a burst; a target
// that is really gone stops being aimed shortly after.
constexpr auto kHoldWindow = std::chrono::milliseconds(350);

// ── state published to the render thread (draw only) ─────────────────────────
std::atomic<bool>  g_active{false};
std::atomic<bool>  g_haveTarget{false};
std::atomic<float> g_targetX{0.f};
std::atomic<float> g_targetY{0.f};
std::atomic<bool>  g_started{false};

std::thread g_thread;
std::atomic<bool> g_run{false};

// ── worker-thread-only state ─────────────────────────────────────────────────
bool      s_keyDown = false;
bool      s_toggled = false;
uintptr_t s_target = 0;
uintptr_t s_lp = 0;
uintptr_t s_eyes = 0;
uintptr_t s_input = 0;
int       s_eyeAge = 0;
bool      s_warnedSend = false;
float     s_damp[2] = {1.f, 1.f};
float     s_lastErr[2] = {0.f, 0.f};
bool      s_haveLastErr = false;

// Last angle actually written through the driver, and until when we keep
// re-asserting it across a brief target loss (see the !bestAddr branch).
Vector2   s_holdAngle{};
bool      s_haveHold = false;
std::chrono::steady_clock::time_point s_holdUntil{};

// ── small helpers ────────────────────────────────────────────────────────────

bool Finite(float v) { return std::isfinite(v); }

// A Unity rotation is a unit quaternion: |q| ~= 1. That makes the components a
// very strong oracle - random memory almost never satisfies it - so we can use
// it to confirm a PlayerEyes pointer before reading or writing through it.
bool ValidQuat(const Vector4 &q) {
  if (!Finite(q.x) || !Finite(q.y) || !Finite(q.z) || !Finite(q.w))
    return false;
  const float m2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  return m2 > 0.64f && m2 < 1.56f;
}

// Euler (pitch about X, yaw about Y, roll = 0) -> quaternion, component order
// (x, y, z, w) exactly as Unity stores it. This must be Unity's
// Quaternion.Euler(pitch, yaw, 0) (rotation order Z-X-Y), whose z term is
// -sin(pitch/2)*sin(yaw/2) - NOT zero. Setting z = 0 leaves the quaternion
// non-unit and its forward vector's vertical component at -sin(pitch)cos^2(yaw/2),
// so the shot loses its pitch and only lands on a target at the same height.
Vector4 ToQuat(float pitchDeg, float yawDeg) {
  const float d2r = 3.14159265358979323846f / 180.f * 0.5f;
  const float c1 = std::cos(pitchDeg * d2r), s1 = std::sin(pitchDeg * d2r);
  const float c2 = std::cos(yawDeg * d2r), s2 = std::sin(yawDeg * d2r);
  return Vector4(s1 * c2, c1 * s2, -(s1 * s2), c1 * c2);
}



void MoveMouseRelative(int dx, int dy) {
  INPUT in{};
  in.type = INPUT_MOUSE;
  in.mi.dx = dx;
  in.mi.dy = dy;
  in.mi.dwFlags = MOUSEEVENTF_MOVE;
  if (SendInput(1, &in, sizeof(INPUT)) == 1)
    return;
  if (!s_warnedSend) {
    s_warnedSend = true;
    printf("[aim] SendInput rejected (err=%lu). err=5 => RustClient.exe is "
           "elevated; run the overlay as administrator, or use the angle "
           "method.\n",
           GetLastError());
    fflush(stdout);
  }
}

void AimLog(const char *how, uintptr_t addr, float a, float b, int cands) {
  static std::chrono::steady_clock::time_point last{};
  const auto now = std::chrono::steady_clock::now();
  if (now - last < std::chrono::seconds(2))
    return;
  last = now;
  printf("[aim] %s addr=0x%llX a=%.2f b=%.2f cands=%d\n", how,
         (unsigned long long)addr, a, b, cands);
  fflush(stdout);
}

// Throttled (2 s) free-form diagnostic so the console shows the whole aim path
// without flooding.
void AimDbg(const char *fmt, ...) {
  static std::chrono::steady_clock::time_point last{};
  const auto now = std::chrono::steady_clock::now();
  if (now - last < std::chrono::seconds(2))
    return;
  last = now;
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  fflush(stdout);
}

// ── pointer resolution (cached per local player) ─────────────────────────────

uintptr_t ResolveEyes(uintptr_t lp) {
  // Log the resolution only when it changes, so the 1s re-validation does not
  // spam the console.
  static uintptr_t s_logged = 0;

  // The BasePlayer->PlayerEyes field is a GC-handle-ENCRYPTED pointer, exactly
  // like clientEntities: the field points at a wrapper whose +0x18 holds an
  // encrypted handle. decrypt_eyes() runs the 2026-11-06 dump's word transform
  // and resolves it through the GCHandle slab. A plain read of the field is NOT
  // a PlayerEyes*.
  //
  // The field offset is re-checked against a unit quaternion before use, so a
  // stale candidate cannot be written through.
  const uintptr_t fields[2] = {kEyesFieldDump, BasePlayer_Offsets::eyes};
  for (int i = 0; i < 2; ++i) {
    const uintptr_t enc = Driver->Read<uintptr_t>(lp + fields[i]);
    if (enc < 0x10000)
      continue;
    const uintptr_t eyes = (uintptr_t)decryptor::decrypt_eyes(enc);
    if (eyes &&
        ValidQuat(Driver->Read<Vector4>(eyes + PlayerEyes_Offsets::bodyRotation))) {
      if (eyes != s_logged) {
        s_logged = eyes;
        printf("[aim] PlayerEyes via BasePlayer+0x%llX (decrypted) -> 0x%llX\n",
               (unsigned long long)fields[i], (unsigned long long)eyes);
        fflush(stdout);
      }
      return eyes;
    }
  }

  // 2) native component walk, exactly like the reference project:
  //    native(m_CachedPtr) -> GameObject -> component list -> [4] -> +0x28.
  const uintptr_t a = Driver->Read<uintptr_t>(lp + 0x10);
  const uintptr_t b = a ? Driver->Read<uintptr_t>(a + 0x30) : 0;
  const uintptr_t list = b ? Driver->Read<uintptr_t>(b + 0x30) : 0;
  const uintptr_t comp = list ? Driver->Read<uintptr_t>(list + 0x10 * 4 + 0x8) : 0;
  const uintptr_t eyes = comp ? Driver->Read<uintptr_t>(comp + 0x28) : 0;
  if (eyes &&
      ValidQuat(Driver->Read<Vector4>(eyes + PlayerEyes_Offsets::bodyRotation))) {
    if (eyes != s_logged) {
      s_logged = eyes;
      printf("[aim] PlayerEyes via component(4) -> 0x%llX\n",
             (unsigned long long)eyes);
      fflush(stdout);
    }
    return eyes;
  }
  return 0;
}

} // namespace

// ── the worker ───────────────────────────────────────────────────────────────

namespace {

bool AimKeyDown() {
  if (g_cfg->aim_key <= 0)
    return false;
  const bool down = (GetAsyncKeyState(g_cfg->aim_key) & 0x8000) != 0;
  if (g_cfg->aim_hold) {
    s_keyDown = down;
    return down;
  }
  if (down && !s_keyDown)
    s_toggled = !s_toggled;
  s_keyDown = down;
  return s_toggled;
}

void SetTarget(bool have, float x, float y) {
  g_targetX.store(x, std::memory_order_relaxed);
  g_targetY.store(y, std::memory_order_relaxed);
  g_haveTarget.store(have, std::memory_order_relaxed);
}

// Push one aim angle through the driver. The eye rotation is what steers the
// shot; the input record is written alongside only if explicitly enabled (see
// aim_write_input) - writing bodyAngles changes the player's movement basis and
// races the game's own update, which is what made movement go weird and caused
// the occasional shot to leave at the wrong yaw.
void WriteAngle(uintptr_t eyes, uintptr_t input, const Vector2 &a) {
  if (eyes)
    Driver->Write<Vector4>(eyes + PlayerEyes_Offsets::bodyRotation,
                           ToQuat(a.x, a.y));
  if (input && g_cfg && g_cfg->aim_write_input)
    Driver->Write<Vector2>(input + PlayerInput_Offsets::bodyAngles, a);
}

void WorkerTick() {
  g_active.store(false, std::memory_order_relaxed);
  if (!g_cfg || !g_entities || !Driver) {
    SetTarget(false, 0.f, 0.f);
    return;
  }
  if (!g_cfg->aim_enabled) {
    s_target = 0;
    SetTarget(false, 0.f, 0.f);
    return;
  }
  if (overlay && overlay->menu_open) {
    SetTarget(false, 0.f, 0.f);
    return;
  }
  if (!AimKeyDown()) {
    SetTarget(false, 0.f, 0.f);
    return;
  }

  // The mouse method injects input, so it must only run while the GAME owns the
  // foreground. The angle method writes memory, so it needs no foreground.
  if (g_cfg->aim_mode == 1) {
    const HWND game = overlay ? overlay->get_game_hwnd() : nullptr;
    if (!game || GetForegroundWindow() != game) {
      SetTarget(false, 0.f, 0.f);
      return;
    }
  }

  g_active.store(true, std::memory_order_relaxed);

  // NOTE: no PrepareMatrix() here - that writes the render thread's shared
  // g_ViewMatrix. ProjectPublished() reads the seqlock copy directly instead.
  auto snapshot = g_entities->Snapshot();
  if (!snapshot)
    return;

  const float sw = W2S::g_ScreenW, sh = W2S::g_ScreenH;
  if (sw <= 1.f || sh <= 1.f)
    return;
  const float cx = sw * 0.5f, cy = sh * 0.5f;
  const float fov = (float)g_cfg->aim_fov;

  const uintptr_t lp = g_entities->GetLocalPlayer();

  // Local eye position + range origin. On a mount (helicopter/train/car) the
  // BasePlayer transform can be the seat/local transform, or the position read
  // can briefly fail, so the camera eye is the authoritative viewpoint. Use it
  // whenever it disagrees with the player transform by more than a few metres.
  Vector3 eye{};
  Vector3 origin{};
  bool haveEye = false;
  bool haveOrigin = false;
  Vector3 localPos{};
  const bool haveLocal = g_entities->GetLocalPlayerPos(localPos);
  Vector3 camPos{};
  const bool haveCam = W2S::GetPublishedCameraPos(camPos);
  if (haveCam && (!haveLocal || localPos.Distance(camPos) > 15.f)) {
    origin = camPos;
    eye = camPos; // GetPublishedCameraPos is already the eye position
    haveEye = true;
    haveOrigin = true;
  } else if (haveLocal) {
    origin = localPos;
    eye = Vector3(localPos.x, localPos.y + kEyeHeight, localPos.z);
    haveEye = true;
    haveOrigin = true;
  }
  if (g_cfg->aim_mode == 0 && !haveEye) {
    AimDbg("[aim] no eye origin (mode=angle) - lp=0x%llX\n",
           (unsigned long long)lp);
    SetTarget(false, 0.f, 0.f);
    return;
  }

  const float maxDist = (float)g_cfg->aim_max_dist;
  // The real bone to aim at, straight from the same skeleton store the ESP
  // draws. Head is the default; chest uses Spine4. The fixed heights are only a
  // fallback for players whose skeleton has not been resolved yet.
  const bool wantHead = (g_cfg->aim_bone == 1);
  const int aimBone = wantHead ? skeleton::Head : skeleton::Spine4;
  const float fallbackBoneH = wantHead ? kBoneHead : kBoneChest;
  const float dropSpeed = g_cfg->aim_drop_speed;
  const bool compDrop = (dropSpeed > 1.f);
  const float stickyFov = fov * kStickyFovMul;
  const double aimNowMs = GhostNowMs();

  float bestDist = FLT_MAX;
  Vector2 best{};
  uintptr_t bestAddr = 0;
  Vector3 bestWorld{};
  bool haveBestWorld = false;
  float stickyDist = FLT_MAX;
  Vector2 sticky{};
  Vector3 stickyWorld{};
  bool foundSticky = false;
  int candidates = 0;

  for (const auto &e : *snapshot) {
    const Prefabs::Category cat =
        (e.category >= 0 && e.category < Prefabs::kCategoryCount)
            ? (Prefabs::Category)e.category
            : Prefabs::GetCategory(e.prefabID);
    if (cat != Prefabs::Category::Player)
      continue;
    if (!e.alive)
      continue;
    if (g_cfg->hide_sleepers && e.sleeping)
      continue;
    if (lp && e.address == lp)
      continue;
    // Predict the player's live position across the snapshot age (the same
    // extrapolation the ESP uses). Bone data is anchored to this position, so
    // the aim point stays on the head instead of one snapshot behind.
    Vector3 aimPos = e.position;
    if (e.sampleMs > 0.0 && !e.velocity.Empty()) {
      const double ageMs = aimNowMs - e.sampleMs;
      if (ageMs > 0.0 && ageMs < 250.0) {
        const float dt = static_cast<float>(ageMs * 0.001);
        aimPos += e.velocity * dt;
      }
    }
    const float range = haveOrigin ? aimPos.Distance(origin) : 0.f;
    if (haveOrigin && range > maxDist)
      continue;

    // Ballistic drop: a projectile fired straight at the target falls
    // 0.5*g*(range/speed)^2 by the time it gets there, so aim that much higher
    // (the small-angle correction that puts the arc on the target). Bows and
    // thrown weapons need this; hitscan weapons ignore it.
    Vector3 bone{aimPos.x, aimPos.y + fallbackBoneH, aimPos.z};
    skeleton::Data skel;
    if (skeleton::Get(e.address, skel) && skel.Valid() &&
        skel.Has(aimBone)) {
      // Bones are world positions read a few ms earlier. Slide them by the
      // player's movement since that read so the aim point stays on the head
      // while the target runs (same anchor the skeleton ESP uses).
      Vector3 anchor{};
      if (skel.HasOrigin())
        anchor = aimPos - skel.origin;
      bone = skel.bones[aimBone] + anchor;
    }
    bone.y += g_cfg->aim_vert;
    if (compDrop && haveOrigin) {
      const float tof = range / dropSpeed;
      bone.y += 0.5f * kGravity * tof * tof;
    }

    Vector2 sp;
    if (!W2S::ProjectPublished(bone, sp))
      continue;
    const float ddx = sp.x - cx, ddy = sp.y - cy;
    const float d = std::sqrt(ddx * ddx + ddy * ddy);
    if (d > stickyFov)
      continue;

    ++candidates;
    if (s_target && e.address == s_target) {
      foundSticky = true;
      sticky = sp;
      stickyWorld = bone;
      stickyDist = d;
    }
    if (d <= fov && d < bestDist) {
      bestDist = d;
      best = sp;
      bestWorld = bone;
      haveBestWorld = true;
      bestAddr = e.address;
    }
  }

  if (foundSticky) {
    best = sticky;
    bestDist = stickyDist;
    bestWorld = stickyWorld;
    haveBestWorld = true;
    bestAddr = s_target;
  }

  if (!bestAddr) {
    s_target = 0;
    s_haveLastErr = false;
    SetTarget(false, 0.f, 0.f);
    // A briefly-lost target (snapshot churn, smoke, one culled/occluded frame)
    // must not drop the shot mid-burst: keep re-asserting the last good angle
    // for a short, capped window. The pointer is only touched for the same local
    // player we resolved it for, so a respawn can never redirect it.
    const auto now = std::chrono::steady_clock::now();
    if (g_cfg->aim_mode == 0 && s_haveHold && now < s_holdUntil && lp &&
        lp == s_lp && (s_eyes || s_input))
      WriteAngle(s_eyes, s_input, s_holdAngle);
    return;
  }
  if (bestAddr != s_target) {
    s_target = bestAddr;
    s_damp[0] = s_damp[1] = 1.f;
    s_haveLastErr = false;
  }

  SetTarget(true, best.x, best.y);

  const float smooth = g_cfg->aim_smoothness < 0.f
                           ? 0.f
                           : g_cfg->aim_smoothness > 1.f ? 1.f
                                                         : g_cfg->aim_smoothness;

  if (g_cfg->aim_mode == 1) {
    // ── mouse method: relative SendInput toward the target ──────────────────
    const float dx = best.x - cx, dy = best.y - cy;
    if (std::fabs(dx) <= kAlignedPx && std::fabs(dy) <= kAlignedPx)
      return;
    // Assist, do not pin: a low gain means the player's own mouse movement wins,
    // so the crosshair can still be pulled off the target (a full-gain
    // proportional loop at 250 Hz would make the mouse feel locked).
    const float gain = 0.10f - smooth * 0.08f;
    const float err[2] = {dx, dy};
    float step[2] = {dx * gain, dy * gain};
    for (int a = 0; a < 2; ++a) {
      if (s_haveLastErr && s_lastErr[a] * err[a] < 0.f &&
          std::fabs(s_lastErr[a]) > 2.f && std::fabs(err[a]) > 2.f) {
        const float shrunk = s_damp[a] * 0.5f;
        s_damp[a] = shrunk < 0.12f ? 0.12f : shrunk;
      } else if (s_damp[a] < 1.f) {
        const float grown = s_damp[a] + 0.02f;
        s_damp[a] = grown > 1.f ? 1.f : grown;
      }
      s_lastErr[a] = err[a];
      step[a] *= s_damp[a];
      if (step[a] > kMaxStepCounts) step[a] = kMaxStepCounts;
      if (step[a] < -kMaxStepCounts) step[a] = -kMaxStepCounts;
    }
    s_haveLastErr = true;
    int ix = (int)std::lroundf(step[0]);
    int iy = (int)std::lroundf(step[1]);
    if (ix == 0 && std::fabs(dx) > kAlignedPx) ix = (dx > 0.f) ? 1 : -1;
    if (iy == 0 && std::fabs(dy) > kAlignedPx) iy = (dy > 0.f) ? 1 : -1;
    if (ix || iy) MoveMouseRelative(ix, iy);
    AimLog("mouse", s_target, dx, dy, candidates);
    return;
  }

  // ── angle method: write the exact aim through the driver ──────────────────
  if (!haveEye || !haveBestWorld)
    return;

  if (lp != s_lp) {
    s_lp = lp;
    s_eyes = 0;
    s_input = 0;
    s_eyeAge = 0;
    s_haveHold = false;
  }
  if (!s_input)
    s_input = Driver->Read<uintptr_t>(lp + BasePlayer_Offsets::input);
  if (!s_eyes || ++s_eyeAge >= kEyeRecheckTicks) {
    s_eyeAge = 0;
    s_eyes = ResolveEyes(lp);
  }
  if (!s_eyes && !s_input) {
    AimDbg("[aim] angle: no input/eyes pointer (lp=0x%llX) - cannot write\n",
           (unsigned long long)lp);
    return;
  }

  // Desired angle: from the eye to the target bone.
  const Vector3 dir{eye.x - bestWorld.x, eye.y - bestWorld.y, eye.z - bestWorld.z};
  const float len = dir.Length();
  if (len < 0.001f)
    return;
  const Vector2 want{RAD2DEG(std::asin(dir.y / len)),
                     RAD2DEG(-std::atan2(dir.x, -dir.z))};
  if (!Finite(want.x) || !Finite(want.y) || std::fabs(want.x) > 91.f ||
      std::fabs(want.y) > 361.f)
    return;

  // Write the EXACT target angle every tick. Deliberately do NOT blend from the
  // player's current bodyAngles: while firing, recoil and the player's own view
  // movement rewrite it every frame, so a smoothed shot lagged and missed for
  // part of the burst. An absolute write does not depend on the view at all, so
  // every shot lands on the target.
  //
  // Only the eye rotation is written (as in the reference): the shot is steered
  // by PlayerEyes.bodyRotation. PlayerInput.bodyAngles is NOT written by default
  // - it feeds the movement basis, so writing it made the player's movement turn
  // with the aim, and racing the game's own write to it was what made a shot
  // occasionally leave at the wrong yaw with no visible camera change.
  WriteAngle(s_eyes, s_input, want);

  s_holdAngle = want;
  s_haveHold = true;
  s_holdUntil = std::chrono::steady_clock::now() + kHoldWindow;

  // Show the quaternion we push and its forward vector. forward.y must be
  // -sin(pitch) (< 0 when aiming at a target below): if it is ~0 the shot will
  // leave flat and only land on a target at the same height.
  const Vector4 q = ToQuat(want.x, want.y);
  const float fwdY = -std::sin(want.x * 3.14159265358979323846f / 180.f);

  AimDbg("[aim] angle lp=0x%llX in=0x%llX eyes=0x%llX want=(%.1f,%.1f) "
         "q=(%.3f,%.3f,%.3f,%.3f) fwdY=%+.3f cands=%d wroteEyes=%d "
         "wroteIn=%d\n",
         (unsigned long long)lp, (unsigned long long)s_input,
         (unsigned long long)s_eyes, want.x, want.y, q.x, q.y, q.z, q.w, fwdY,
         candidates, s_eyes ? 1 : 0, (g_cfg && g_cfg->aim_write_input && s_input) ? 1 : 0);
}

} // namespace

void Start() {
  if (g_started.exchange(true))
    return;
  s_lp = s_eyes = s_input = 0;
  s_target = 0;
  s_haveHold = false;

  // Deadline-based loop: run a tick, then sleep until the next deadline rather
  // than for a fixed interval, so the cadence does not drift with the (driver-
  // bound) tick duration. If a tick overruns we resync instead of bursting.
  g_run.store(true, std::memory_order_release);
  g_thread = std::thread([] {
    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    while (g_run.load(std::memory_order_acquire)) {
      WorkerTick();
      next += kAimInterval;
      const auto now = clock::now();
      if (next > now)
        std::this_thread::sleep_for(next - now);
      else
        next = now;  // behind: resync, never burst
    }
  });
  printf("[aim] worker started (%lldus, exact angle, eyes via quaternion oracle)\n",
         (long long)std::chrono::duration_cast<std::chrono::microseconds>(
             kAimInterval)
             .count());
  fflush(stdout);
}

void Stop() {
  if (!g_started.exchange(false))
    return;
  g_run.store(false, std::memory_order_release);
  if (g_thread.joinable())
    g_thread.join();
}

// ── render thread: draw only, no driver I/O ──────────────────────────────────

void Tick() {
  if (!g_cfg || !overlay || !g_cfg->aim_enabled)
    return;

  const float sw =
      W2S::g_ScreenW > 0.f ? W2S::g_ScreenW : overlay->window_width;
  const float sh =
      W2S::g_ScreenH > 0.f ? W2S::g_ScreenH : overlay->window_height;
  const float cx = sw * 0.5f, cy = sh * 0.5f;
  const float fov = (float)g_cfg->aim_fov;

  if (g_cfg->aim_draw_fov && fov > 0.f) {
    const render::Color col =
        render::from_float(g_cfg->accent, 130.f / 255.f);
    render::circle({cx, cy}, fov, col, 1.f);
  }

  if (g_haveTarget.load(std::memory_order_relaxed) &&
      g_active.load(std::memory_order_relaxed)) {
    render::circle({g_targetX.load(std::memory_order_relaxed),
                    g_targetY.load(std::memory_order_relaxed)},
                   6.f, render::rgba(255, 70, 70, 220), 1.6f);
  }
}

} // namespace aim
