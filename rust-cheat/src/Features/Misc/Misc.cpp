
// ================================================================================
// Эксклюзив от @imguisource_bot
// Канал: @ImGui_Source_FIXNEY (ImGui Source)
// Резерв: @il2cpp_src (shit src 🧨)
// ================================================================================

#include "Misc.h"

#include "GetEntities/Entities.h"
#include "Il2Cpp/Decryptions/decryptor.hpp"
#include "Menu/Config.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

extern BaseNetworkable *g_entities;

namespace misc {
namespace {

// ── field offsets inside PlayerWalkMovement / BaseMovement ───────────────────
constexpr uint64_t kOwner            = 0x30;   // BaseMovement::Owner
constexpr uint64_t kMaxVelocity      = 0x108;  // PlayerWalkMovement::body (diag only)
constexpr uint64_t kGroundAngle      = 0x110;  // PlayerWalkMovement::groundAngle
constexpr uint64_t kGroundAngleNew   = 0x118;  // PlayerWalkMovement::groundAngleNew
constexpr uint64_t kGroundTime       = 0x120;  // PlayerWalkMovement::groundTime
constexpr uint64_t kPreviousPosition = 0x138;  // PlayerWalkMovement::previousPosition (diag only)

// BasePlayer::movement from the 2026-11-06 dump (new build). The live build's
// BasePlayer layout has drifted before, so this value is re-verified at
// runtime and re-found by scanning if it moved.
constexpr uint64_t kMovementDump = 0x590;

// Movement-pointer sweep bounds and budget. The old code swept all 256 offsets
// in a single tick - ~1000 driver round-trips in one frame, which starved the
// entity worker and froze the overlay for about a second. Now the sweep is
// chunked: a few probes per tick, with a quiet period after a full sweep that
// finds nothing.
constexpr uint64_t kScanLo = 0x100;
constexpr uint64_t kScanHi = 0x900;   // exclusive
constexpr int kScanProbesPerTick = 16;
constexpr auto kScanBackoff = std::chrono::milliseconds(2000);

// Worker cadence. The write only has to beat the game's movement update, so a
// few ms is plenty; it is deadline-based so it does not drift.
constexpr auto kTickInterval = std::chrono::milliseconds(4);

std::thread g_thread;
std::atomic<bool> g_run{false};

bool SanePtr(uint64_t p) {
  return p > 0x10000u && p < 0x00007fffffffffffu && (p & 7u) == 0;
}

uint64_t ReadPtr(uint64_t a) { return Driver->Read<uint64_t>(a); }

float AsFloat(uint32_t u) {
  float f;
  std::memcpy(&f, &u, sizeof(f));
  return f;
}

// A PlayerWalkMovement object points back at its owning BasePlayer through
// BaseMovement::Owner (0x30). That is a solid fingerprint - random memory will
// not point back at the exact BasePlayer - and it is one driver round-trip.
bool IsMovementOwner(uint64_t bp, uint64_t mv) {
  return SanePtr(mv) && ReadPtr(mv + kOwner) == bp;
}

// Fingerprint used when resolving a candidate. The 26480361 build moved the
// movement-state bools and encrypts them, so the owner back-pointer is the
// stable check: random memory will not point back at the exact BasePlayer.
bool IsMovement(uint64_t bp, uint64_t mv) {
  return IsMovementOwner(bp, mv);
}

// Cached resolution so the steady state costs a few reads (movement + Owner).
struct Resolution {
  uint64_t off = 0;
  uint64_t ptr = 0;
};
Resolution g_res;

// Chunked-sweep state (worker-thread only).
uint64_t s_scanCursor = 0;
bool s_scanning = false;
std::chrono::steady_clock::time_point s_scanNext{};

// Rate-limited console report so the live field layout can be verified: the
// decrypted values should be a sensible angle (0..~90), a game-time clock
// (groundTime) and a world position near the player (previousPosition). If the
// dump layout were stale these would read as NaN / astronomically large.
void Diag(uint64_t lp, uint64_t off, uint64_t mv) {
  static uint64_t lastMs = 0;
  const uint64_t now =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count();
  if (now - lastMs < 2000)
    return;
  lastMs = now;

  const uint32_t rawAngle = Driver->Read<uint32_t>(mv + kGroundAngle);
  const uint32_t rawNew = Driver->Read<uint32_t>(mv + kGroundAngleNew);
  const uint32_t rawTime = Driver->Read<uint32_t>(mv + kGroundTime);
  const uint32_t rawMax = Driver->Read<uint32_t>(mv + kMaxVelocity);
  const auto pvRaw =
      Driver->Read<std::array<uint32_t, 3>>(mv + kPreviousPosition);
  const auto pv = decryptor::decrypt_previous_position(pvRaw);

  printf("[spider] lp=0x%llX off=0x%llX mv=0x%llX | ang=%.2f new=%.2f "
         "gTime=%.1f maxVel=%.2f prev=(%.1f,%.1f,%.1f)\n",
         static_cast<unsigned long long>(lp),
         static_cast<unsigned long long>(off),
         static_cast<unsigned long long>(mv),
         decryptor::decrypt_ground_angle(rawAngle),
         decryptor::decrypt_ground_angle_new(rawNew),
         decryptor::decrypt_ground_time(rawTime),
         decryptor::decrypt_max_velocity(rawMax), AsFloat(pv[0]),
         AsFloat(pv[1]), AsFloat(pv[2]));
}

void Tick() {
  if (!g_cfg || !g_entities || !Driver)
    return;

  if (!g_cfg->spiderman) {
    g_res = Resolution{};
    s_scanCursor = 0;
    s_scanning = false;
    return;
  }

  const uint64_t lp = g_entities->GetLocalPlayer();
  if (!SanePtr(lp))
    return;

  // Steady state: the cached offset must still point at our movement object.
  // Cheap owner-only check (one read) so the per-tick cost stays a read + two
  // writes; the full bool fingerprint is only needed when resolving.
  if (g_res.off != 0) {
    const uint64_t mv = ReadPtr(lp + g_res.off);
    if (IsMovementOwner(lp, mv)) {
      g_res.ptr = mv;
    } else {
      // The cached offset went bad (respawn / layout drift): restart the sweep
      // on the next tick.
      g_res = Resolution{};
      s_scanning = false;
      s_scanCursor = kScanLo;
    }
  }

  if (g_res.off == 0) {
    const auto now = std::chrono::steady_clock::now();

    // Fast path, once per sweep: the dump offset.
    if (!s_scanning && now >= s_scanNext) {
      const uint64_t dumpMv = ReadPtr(lp + kMovementDump);
      if (IsMovement(lp, dumpMv)) {
        g_res.off = kMovementDump;
        g_res.ptr = dumpMv;
        printf("[spider] local=0x%llX movement resolved off=0x%llX ptr=0x%llX "
               "(dump=0x%llX match)\n",
               static_cast<unsigned long long>(lp),
               static_cast<unsigned long long>(g_res.off),
               static_cast<unsigned long long>(g_res.ptr),
               static_cast<unsigned long long>(kMovementDump));
      } else {
        s_scanning = true;
        s_scanCursor = kScanLo;
      }
    }

    // Chunked sweep: at most kScanProbesPerTick offsets this tick, so one tick
    // can never hog the shared driver channel.
    if (g_res.off == 0 && s_scanning) {
      for (int i = 0; i < kScanProbesPerTick && s_scanCursor < kScanHi; ++i) {
        const uint64_t off = s_scanCursor;
        s_scanCursor += 8;
        const uint64_t p = ReadPtr(lp + off);
        if (IsMovement(lp, p)) {
          g_res.off = off;
          g_res.ptr = p;
          s_scanning = false;
          printf("[spider] local=0x%llX movement resolved off=0x%llX ptr=0x%llX "
                 "(dump=0x%llX MOVED)\n",
                 static_cast<unsigned long long>(lp),
                 static_cast<unsigned long long>(g_res.off),
                 static_cast<unsigned long long>(g_res.ptr),
                 static_cast<unsigned long long>(kMovementDump));
          break;
        }
      }
      if (g_res.off == 0 && s_scanCursor >= kScanHi) {
        // Full sweep found nothing - quiet for a while instead of re-sweeping
        // every tick.
        s_scanning = false;
        s_scanNext = now + kScanBackoff;
        printf("[spider] local=0x%llX movement NOT resolved (swept 0x%llX..0x%llX)\n",
               static_cast<unsigned long long>(lp),
               static_cast<unsigned long long>(kScanLo),
               static_cast<unsigned long long>(kScanHi));
      }
    }
  }

  const uint64_t mv = g_res.ptr;
  if (!SanePtr(mv))
    return;

  Diag(lp, g_res.off, mv);

  // The game's ground check reads both the current (groundAngle) and the
  // smoothed/next (groundAngleNew) angle to decide a surface is too steep and
  // slide the player off. Feed it an encrypted 0.0 for both so it always
  // believes the ground under our feet is flat - walk up any wall.
  Driver->Write<uint32_t>(mv + kGroundAngle,
                          decryptor::encrypt_ground_angle(0.0f));
  Driver->Write<uint32_t>(mv + kGroundAngleNew,
                          decryptor::encrypt_ground_angle_new(0.0f));
}

} // namespace

void Start() {
  if (g_run.exchange(true))
    return;
  g_thread = std::thread([] {
    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    while (g_run.load(std::memory_order_acquire)) {
      Tick();
      next += kTickInterval;
      const auto now = clock::now();
      if (next > now)
        std::this_thread::sleep_for(next - now);
      else
        next = now;  // behind: resync, never burst
    }
  });
  printf("[spider] worker started (%lldms)\n",
         (long long)kTickInterval.count());
  fflush(stdout);
}

void Stop() {
  if (!g_run.exchange(false))
    return;
  if (g_thread.joinable())
    g_thread.join();
}

} // namespace misc
