
// ================================================================================
// Источник: @imguisource_bot | ImGui Source: @ImGui_Source_FIXNEY | shit src 🧨: @il2cpp_src
// ================================================================================

#include "EntityCaching.h"
#include "../Driver/Rpm.h"
#include "../Features/Visuals/Esp/EspStats.h"
#include "../GetEntities/Prefabs/Prefabs.h"
#include "../Menu/Config.h"
#include "../Offsets/Offsets.h"
#include "../Diag/Profiler.h"
#include "../World2Screen/World2Screen.h"
#include "Entities.h"
#include "GhostClock.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <algorithm>

// Rust (BasePlayer) state fields used for the player features.
//   LifeState: Alive = 0, Dead = 1  (BaseCombatEntity.lifestate)
//   PlayerFlags: Wounded = 64, Sleeping = 16  (BasePlayer.playerFlags)
static constexpr uint32_t kPlayerFlagWounded  = 64u;
static constexpr uint32_t kPlayerFlagSleeping = 16u;

// How often (in ticks) an entity's identity + category are re-validated even
// while its category is switched off. This is what lets a switched-off entity
// cost zero driver reads on almost every tick.
static constexpr uint32_t kCategoryRecheck = 128;

// Static resources (ore, trees, crates, structures ...) are re-read only every
// kResourceRefresh ticks and reused in between: their world position barely
// changes and the render thread still projects it with the live view matrix, so
// the boxes stay glued to the object while the driver load drops by this factor
// on resource-heavy servers. Moving actors (players / NPCs) always refresh.
static constexpr uint32_t kResourceRefresh = 24;

// If a refresh fails (bad read / the object is briefly unreadable / the entity
// dropped out of the client list for a tick) keep showing the last good entity
// for this many ticks instead of flickering it off screen. At the 8 ms worker
// tick this is ~160 ms of grace: long enough to ride out any transient hiccup,
// short enough that a stale box can never linger.
static constexpr uint32_t kGraceTicks = 20;

// A transform hierarchy that failed to resolve is RETRIED with this cadence
// instead of being given up on. This is the important one: a freshly streamed-in
// entity appears in the client list BEFORE Unity has instantiated its prefab, so
// its native transform chain is not readable yet. Retrying makes it pop in a few
// tens of ms later - without it the entity stayed invisible until its object was
// destroyed and re-created (which is the "players disappear for seconds/minutes"
// report).
static constexpr uint32_t kHierRetryTicks = 8;

// How many consecutive ticks an identity resolve may fail before the entity is
// accepted as an unknown object. Bounded so a destroyed object that still sits
// in the list cannot cost reads forever.
static constexpr uint32_t kIdMaxTries = 8;

// Worker-side spatial cull. Every entity read costs a locked kernel round-trip,
// and the matrix updater shares that one channel - so the fewer entities the
// worker reads, the fresher the view matrix (which is what makes the ESP track
// the mouse instead of swimming). An entity whose last known position is well
// outside its category's draw distance cannot be drawn, so it is not read at
// all except for a rare re-check (so it pops back in when you move towards it).
// The +margin means an object is read a little before it actually reaches the
// cap, hiding any pop-in. 0 caps (unlimited) disable the cull entirely.
static constexpr float kCullMargin = 40.f;          // metres
static constexpr uint32_t kFarRecheckActor = 8;      // re-check far players ~every 8 ticks
static constexpr uint32_t kFarRecheck = 32;          // re-check far resources ~every 32 ticks

// The local-player wrapper read (encrypted handle -> decrypt) fails a few
// percent of ticks. Hold the last known local player / position for this many
// ticks across such a run, so the ESP's distance origin - and therefore its
// range filter and the worker's cull centre - never blinks out.
static constexpr uint32_t kLpMissHold = 40;   // ~320 ms at the 8 ms tick
static constexpr uint32_t kLpPosHold  = 32;   // ~256 ms at the 8 ms tick

// ── last-known-position ghosts (server-side player occlusion) ────────────────
// Facepunch's occlusion update stops networking a player to us while we cannot
// see them, so the entity is destroyed client-side and there is no live position
// left to read. We keep the last known spot instead and let the ESP fade it out.
//   0  -> the ESP draws nothing (feature off)
static constexpr float kGhostMergeDist = 8.f;   // m; merge radius (moved player)
static constexpr float kGhostMaxSpeed  = 14.f;  // m/s; clamp a bad velocity spike
static constexpr uint32_t kGhostMax    = 64;    // hard cap on stored ghosts

// How long a vanished player stays on screen as a normal, fully opaque ESP
// before it becomes the fading "Last seen" marker. This must stay well below
// ghost_time_ms: the old 10 s player hold meant the ghost was already older
// than its TTL when it was finally created, so it was pruned before it could
// ever fade and the player just popped off. A short hold rides out a one-tick
// entity-list hiccup and lets the fade start right when the player vanishes.
static constexpr double kPlayerVanishMs = 250.0;

// Motion guard. A player who is physically moving cannot be dead or asleep, so
// movement overrides those flags: a wrong or stale `lifestate` / `playerFlags`
// read must never hide a live player who is standing in plain sight (reported
// as "players vanish even when fully visible"). Sum of |components| in metres
// across one ~40 ms velocity sample - 0.03 m works out to ~0.75 m/s, far below
// any walk, and a stationary transform read is bit-identical tick to tick.
static constexpr float kMoveEps    = 0.03f;
static constexpr float kMoveHoldMs = 600.f;  // keep "moving" this long after it

// A position is usable when it is finite, non-zero and inside the map. The
// non-zero test matters: a failed read yields exactly (0,0,0), which used to be
// accepted as a valid position and teleported the box to the middle of the map
// (where it is usually culled -> the entity "disappears" for that tick).
static bool PlausiblePos(const Vector3 &p) {
  if (p.Empty())
    return false;
  return p.IsValid() && p.x > -10000.f && p.x < 30000.f &&
         p.y > -2000.f && p.y < 10000.f && p.z > -10000.f && p.z < 30000.f;
}

// BasePlayer._displayName is an IL2CPP string (length at +0x10, UTF-16 chars
// at +0x14). Read it on the worker and cache the UTF-8 result; the render thread
// only ever sees the shared string.
static std::string ReadPlayerName(uintptr_t player) {
  const uintptr_t str =
      Driver->Read<uintptr_t>(player + BasePlayer_Offsets::_displayName);
  if (!str || str < 0x10000)
    return {};

  const int len = Driver->ReadInt32(str + 0x10);
  if (len <= 0 || len > 32)
    return {};

  const std::wstring wide = Driver->ReadWString(str + 0x14, (size_t)len);
  if (wide.empty() || wide.size() > 32)
    return {};

  const int need = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                       (int)wide.size(), nullptr, 0, nullptr,
                                       nullptr);
  if (need <= 0 || need > 128)
    return {};
  std::string out((size_t)need, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), out.data(),
                      need, nullptr, nullptr);

  // Reject control characters / blank labels.
  bool visible = false;
  for (char &c : out) {
    if ((unsigned char)c < 0x20)
      c = '?';
    else if (c != ' ')
      visible = true;
  }
  if (!visible)
    return {};
  return out;
}

// Steam64 sanity check. BasePlayer.userID is a Steam64; if the field read lands
// on something else (offset drift / a bad read) the value is rejected and the
// identity match falls back to the real player name.
static bool PlausibleUserId(uint64_t id) {
  return id >= 76561197960265728ULL && id <= 76561202255233023ULL;
}

// A player successfully read in the current worker tick. Used after the entity
// loop to recognise stale grace copies of the SAME player after the client has
// destroyed and re-created the BasePlayer at a new address (server-side
// occlusion), which is what produced the "same player two/three times" ESP.
struct LivePlayerIdentity {
  uintptr_t address = 0;
  uint64_t userId = 0;
  std::shared_ptr<const std::string> name;
};

static bool SamePlayer(const EntityRuntime &stale, uintptr_t staleAddress,
                       const LivePlayerIdentity &live) {
  if (!live.address || live.address == staleAddress)
    return false;  // never match a runtime against itself
  if (stale.userId != 0 && live.userId != 0)
    return stale.userId == live.userId;
  // Fallback for the rare case where the userID field could not be read.
  if (stale.playerName && !stale.playerName->empty() && live.name &&
      !live.name->empty())
    return *stale.playerName == *live.name;
  return false;
}

void BaseNetworkable::Tick() {
  prof::ScopeTimer _tickTimer(&prof::entityTick);
  if (!Driver || !Driver->IsAttached())
    return;

  const uint32_t seq = ++m_tickSeq;
  const double tickNowMs = GhostNowMs();

  RetryCameraByName();  // keep hunting the MainCamera class name in background

  uintptr_t entities = GetClientEntities();
  if (!entities)
    return;

  if (entities != m_lastEntitiesPtr) {
    m_lastEntitiesPtr = entities;
    m_entitiesPtr.store(entities, std::memory_order_release);
  }

  // Local player entity. The encrypted-handle read + decrypt fails a few
  // percent of ticks; hold the last known address across such a short run
  // instead of publishing 0, which used to tear down the local transform chain
  // and take the ESP's distance origin away for a frame. Give up after
  // kLpMissHold ticks so a genuinely gone local player (death / menu) is not
  // held for ever.
  uintptr_t localPlayer = ReadLocalPlayer();
  if (localPlayer) {
    m_lpMiss = 0;
    m_localPlayer.store(localPlayer, std::memory_order_release);
  } else {
    const uintptr_t held = (m_lpMiss < kLpMissHold)
                               ? m_localPlayer.load(std::memory_order_acquire)
                               : 0;
    ++m_lpMiss;
    if (held) {
      localPlayer = held;
    } else {
      m_localPlayer.store(0, std::memory_order_release);
    }
  }

  // Respawn/death: the local-player address changes. The cached transform
  // hierarchy belongs to the OLD body (often the death/helicopter spot), which
  // used to leave every distance at ~800 m and stopped skeleton updates because
  // every player looked out of range. Drop the whole local position cache.
  if (localPlayer != m_lastLocalPlayer) {
    m_lastLocalPlayer = localPlayer;
    m_localHierarchy = 0;
    m_lpHaveSeq = 0;
    m_lpMiss = 0;
    m_lpPos = Vector3{};
  }

  // Resolve + read the local player's world position (used as the ESP distance
  // origin AND as the worker-side spatial cull centre). Cached hierarchy:
  // resolved once, then one read per tick.
  Vector3 originPos{};
  bool haveOrigin = false;
  {
    Vector3 lpPos{};
    bool lpHave = false;
    if (localPlayer) {
      if (m_localHierarchy == 0)
        ResolveHierarchy(localPlayer, m_localHierarchy);
      if (m_localHierarchy) {
        lpPos = ReadPositionAt(m_localHierarchy);
        lpHave = PlausiblePos(lpPos);
      }
      if (!lpHave) {
        // chain died (scene change?) -> rebuild it once
        m_localHierarchy = 0;
        if (ResolveHierarchy(localPlayer, m_localHierarchy)) {
          lpPos = ReadPositionAt(m_localHierarchy);
          lpHave = PlausiblePos(lpPos);
        }
      }
      if (!lpHave)
        m_localHierarchy = 0;
    } else {
      m_localHierarchy = 0;
    }

    // Hold the last known-good position through a short run of failed reads
    // (~kLpPosHold ticks). Without this the ESP's range filter and the worker's
    // cull centre flipped on/off several times a second, which showed up as
    // boxes blinking at range.
    if (!lpHave && m_lpHaveSeq != 0 && (seq - m_lpHaveSeq) < kLpPosHold) {
      lpPos = m_lpLastPos;
      lpHave = true;
    }
    if (lpHave) {
      m_lpLastPos = lpPos;
      m_lpHaveSeq = seq;
    }

    originPos = lpPos;
    haveOrigin = lpHave;

    // On vehicles/trains the local player transform is parented to the mount
    // and can report a seat/local position. The camera eye is always the true
    // world viewpoint; use it whenever the local position disagrees with it.
    Vector3 camPos{};
    if (W2S::GetPublishedCameraPos(camPos)) {
      if (!haveOrigin || originPos.Distance(camPos) > 15.f) {
        originPos = camPos;
        haveOrigin = true;
      }
    }

    m_lpSeq.fetch_add(1, std::memory_order_acq_rel);  // odd -> writing
    m_lpPos = lpPos;
    m_lpHp = lpHave;
    m_lpSeq.fetch_add(1, std::memory_order_acq_rel);  // even -> stable
  }

  const std::vector<uintptr_t> &addresses = FetchEntityList();

  // Safety valve: on extreme maps (thousands of entities) slow the worker a
  // touch so it can never saturate a core and starve the overlay / matrix
  // thread. Normal maps stay at the responsive 8 ms tick.
  {
    static std::chrono::milliseconds curInterval{0};
    const size_t n = addresses.size();
    const std::chrono::milliseconds want =
        (n > 6000) ? std::chrono::milliseconds(12)
        : (n > 3000) ? std::chrono::milliseconds(10)
                     : std::chrono::milliseconds(8);
    if (want != curInterval) {
      curInterval = want;
      m_worker.SetInterval(want);
    }
  }

  std::vector<CachedEntity> list;
  list.reserve(addresses.size() + 64);

  // Cheap counters that explain where entities are lost (published in the rare
  // console line at the bottom). Cost is a few increments per entity.
  //   offCat = dropped because the user has that category switched off
  //   offFar = dropped by the worker-side spatial cull (beyond draw distance)
  size_t stOffCat = 0, stOffFar = 0, stIdFail = 0, stNoHier = 0, stNoPos = 0,
         stActor = 0, stDead = 0, stGrace = 0, stGhost = 0, stBackoff = 0,
         stReplaced = 0;
  // Nearest / furthest player the worker actually emitted (metres from the
  // distance origin). This is the number that answers "why can I not see
  // players further than X m": if pFar caps out around the client's network
  // streaming radius, the entity simply is not in the client's list.
  size_t pCount = 0;
  float pNear = 1e9f, pFar = 0.f;

  // Players read this tick, with their identity. After the loop, stale player
  // grace entries are matched against this list so an occlusion-recreated
  // player is not published twice.
  static thread_local std::vector<LivePlayerIdentity> livePlayers;
  livePlayers.clear();

  for (uintptr_t address : addresses) {
    EntityRuntime &rt = m_runtime[address];
    rt.lastSeen = seq;

    // Invalid/freed/stale list entries used to be retried every tick. Keep the
    // last good copy (if any) and retry with exponential backoff instead; this
    // was the 150 ms entity tick on a 2,900-entry list.
    if (rt.backoffAt > seq) {
      ++stBackoff;
      if (rt.haveLast)
        list.push_back(rt.last);
      continue;
    }

    auto deferRetry = [&]() {
      ++rt.failStreak;
      const uint32_t shift = std::min<uint32_t>(rt.failStreak, 4u);
      rt.backoffAt = seq + std::min<uint32_t>(8u << shift, 64u);
    };

    const uint32_t phase =
        static_cast<uint32_t>(address >> 4) & (kCategoryRecheck - 1);
    const bool recheckDue = ((seq + phase) & (kCategoryRecheck - 1)) == 0;

    // ── zero-read fast path (disabled) + refresh cadence (enabled) ───────
    // A known entity whose category the user switched off is skipped
    // outright. On a tree/rock-heavy map this removes almost the entire
    // per-entity cost, which is what keeps big servers smooth. Enabled static
    // resources are only re-read every kResourceRefresh ticks and reused in
    // between (actors every tick).
    if (rt.haveStatics && rt.klass != 0 && !recheckDue && g_cfg) {
      const Prefabs::Category cat =
          (rt.category >= 0 && rt.category < Prefabs::kCategoryCount)
              ? (Prefabs::Category)rt.category
              : Prefabs::GetCategory(rt.prefabID);
      if (!g_cfg->Cat(cat).enabled &&
          !g_cfg->ImageWants(cat, rt.prefabID,
                             rt.className ? rt.className.get() : nullptr)) {
        ++stOffCat;
        continue;
      }

      if (rt.haveLast) {
        // Moving world objects must be refreshed every tick. Corpses belong
        // here too: a player corpse is a physics ragdoll that keeps sliding /
        // settling after death, and the old resource cadence (24 ticks) made
        // the corpse ESP label visibly step. Static resources keep the cheap
        // refresh path below.
        const bool isActor = (cat == Prefabs::Category::Player ||
                              cat == Prefabs::Category::Npc ||
                              cat == Prefabs::Category::Corpse);

        // ── spatial cull (biggest read reduction on large maps) ───────────
        // If the cached position is far beyond this category's draw distance it
        // cannot appear on screen, so skip every driver read except an
        // occasional re-check. This keeps only the near set flowing through the
        // shared kernel channel, so the matrix thread stays fresh.
        if (haveOrigin) {
          const int cap = (cat == Prefabs::Category::Player)
                              ? g_cfg->player_max_dist
                              : g_cfg->resource_max_dist;
          if (cap > 0) {
            const float maxd = static_cast<float>(cap) + kCullMargin;
            const Vector3 d = rt.last.position - originPos;
            const float d2 = d.x * d.x + d.y * d.y + d.z * d.z;
            if (d2 > maxd * maxd) {
              const uint32_t rc = isActor ? kFarRecheckActor : kFarRecheck;
              if (rc && ((seq + phase) % rc) != 0) {
                ++stOffFar;
                continue;  // far: skip reads AND the emit (ESP would cull it)
              }
            }
          }
        }

        // Reuse the cached entity between refreshes - but only while the
        // transform chain is still known-good, otherwise fall through and heal
        // it instead of serving a stale copy for ever.
        if (rt.hierarchy != 0 && !isActor &&
            ((seq + phase) % kResourceRefresh) != 0) {
          list.push_back(rt.last);
          continue;
        }
      }
    }

    // ── identity check (one read) ────────────────────────────────────────
    // A changed object header means this address now holds a different
    // object, so the cached name / prefab / hierarchy must be rebuilt.
    const uint64_t klass = Driver->Read<uint64_t>(address);
    if (klass < 0x10000) {
      // Transient bad read: keep the last good entity for a few ticks so it
      // does not flicker out of the ESP.
      ++stIdFail;
      if (rt.haveLast && rt.misses < kGraceTicks) {
        ++rt.misses;
        ++stGrace;
        list.push_back(rt.last);
      }
      deferRetry();
      continue;
    }

    if (rt.klass != klass) {
      rt.klass = klass;
      rt.haveStatics = false;  // full re-derive below
      rt.idTries = 0;
      rt.hierRetryAt = 0;
      rt.hierRevalidateAt = 0;
      rt.backoffAt = 0;
      rt.failStreak = 0;
      rt.prefabID = 0;
      rt.category = -1;
      rt.className.reset();
      rt.displayName.reset();
      rt.playerName.reset();
      rt.playerNameAt = 0;
      rt.userId = 0;
      rt.userIdAt = 0;
      rt.hierarchy = 0;
      // Different object now: the old velocity must not leak into the new one.
      rt.vel = Vector3{};
      rt.velSampleMs = 0.f;
      // NOTE: keep rt.haveLast so a spurious class-pointer read does not make
      // the entity blink off for a tick; the grace paths reuse it.
    }
    rt.refreshedAt = seq;

    // ── deep resolve: class name + prefab id + category + label ───────────
    // Retried until it succeeds (a freshly streamed-in entity's prefab is not
    // fully readable on the very first tick). The class NAME is read as well as
    // the prefab id and is used as an oracle for the one category a wrong/zero
    // prefab id would silently drop: players.
    if (!rt.haveStatics) {
      std::string cls = GetObjectClassName(address);
      const uint32_t pid = GetPrefabID(address);
      if (cls.empty() && pid == 0) {
        ++stIdFail;
        if (rt.idTries < kIdMaxTries) {
          ++rt.idTries;
          if (rt.haveLast && rt.misses < kGraceTicks) {
            ++rt.misses;
            ++stGrace;
            list.push_back(rt.last);
          }
        } else {
          rt.idTries = 0;
          deferRetry();
        }
        continue;
      }
      rt.idTries = 0;

      rt.prefabID = pid;
      rt.className = cls.empty()
                         ? std::shared_ptr<const std::string>()
                         : std::make_shared<const std::string>(std::move(cls));

      Prefabs::Category cat = Prefabs::GetCategory(pid);
      if (cat == Prefabs::Category::Deployable && rt.className &&
          *rt.className == "BasePlayer")
        cat = Prefabs::Category::Player;  // safety net: never lose a player
      // Unknown animal prefab (new Rust variant, changed id): the IL2CPP class
      // name is a reliable fallback so lamb/sheep/rabbit-style entities do not
      // fall into the Deployable bucket and disappear behind the filter.
      if (cat == Prefabs::Category::Deployable && rt.className &&
          Prefabs::IsNpcClass(*rt.className))
        cat = Prefabs::Category::Npc;
      rt.category = (int)cat;

      // Pre-compute the display label ONCE (pretty name, else class name, else
      // a generic fallback) so the render thread never runs the lookup - that
      // used to allocate a std::string per entity, per frame.
      std::string pretty = Prefabs::GetPrettyName(rt.prefabID);
      if (pretty.empty() && rt.className && !rt.className->empty())
        pretty = *rt.className;
      if (pretty.empty())
        pretty = "object";
      rt.displayName = std::make_shared<const std::string>(std::move(pretty));

      rt.haveStatics = true;
      // The transform hierarchy is resolved (and re-resolved) below, with its
      // own retry cadence - never latched here, so a failure cannot stick.
    }

    // ── periodic deep re-validation (one cheap prefab-id read) ────────────
    // Every kCategoryRecheck ticks the cached classification is re-checked
    // against the live prefab id, so an entity that was mis-read once (wrong
    // category -> dropped by a disabled filter) heals itself instead of
    // staying wrong for the whole session.
    if (rt.haveStatics && recheckDue) {
      const uint32_t pid2 = GetPrefabID(address);
      if (pid2 != 0 && pid2 != rt.prefabID) {
        rt.prefabID = pid2;
        Prefabs::Category c2 = Prefabs::GetCategory(pid2);
        if (c2 == Prefabs::Category::Deployable && rt.className &&
            *rt.className == "BasePlayer")
          c2 = Prefabs::Category::Player;
        if (c2 == Prefabs::Category::Deployable && rt.className &&
            Prefabs::IsNpcClass(*rt.className))
          c2 = Prefabs::Category::Npc;
        rt.category = (int)c2;
        std::string pretty2 = Prefabs::GetPrettyName(pid2);
        if (!pretty2.empty())
          rt.displayName =
              std::make_shared<const std::string>(std::move(pretty2));
      }
    }

    // ── category gate (only with a classified entity) ────────────────────
    const Prefabs::Category cat =
        (rt.category >= 0 && rt.category < Prefabs::kCategoryCount)
            ? (Prefabs::Category)rt.category
            : Prefabs::GetCategory(rt.prefabID);
    if (g_cfg && !g_cfg->Cat(cat).enabled &&
        !g_cfg->ImageWants(cat, rt.prefabID,
                           rt.className ? rt.className.get() : nullptr)) {
      ++stOffCat;
      continue;
    }

    // ── transform hierarchy: resolve, and RETRY while it is missing ──────
    if (rt.hierarchy == 0) {
      if (seq >= rt.hierRetryAt) {
        if (!ResolveHierarchy(address, rt.hierarchy)) {
          rt.hierarchy = 0;
          rt.hierRetryAt = seq + kHierRetryTicks;
        }
      }
      if (rt.hierarchy == 0) {
        ++stNoHier;
        if (rt.haveLast && rt.misses < kGraceTicks) {
          ++rt.misses;
          ++stGrace;
          list.push_back(rt.last);
        }
        deferRetry();
        continue;  // not there yet - retried with backoff
      }
    }

    // ── position: a single read from the cached hierarchy pointer ────────
    Vector3 pos = ReadPositionAt(rt.hierarchy);
    if (!PlausiblePos(pos)) {
      // The cached chain went stale (object destroyed / memory reused). Rebuild
      // it - throttled so a genuinely dead object is not re-walked every tick.
      if (seq >= rt.hierRetryAt) {
        rt.hierRetryAt = seq + kHierRetryTicks;
        uintptr_t rebuilt = 0;
        if (ResolveHierarchy(address, rebuilt)) {
          rt.hierarchy = rebuilt;
          pos = ReadPositionAt(rebuilt);
        } else {
          rt.hierarchy = 0;
        }
      }
      if (!PlausiblePos(pos)) {
        ++stNoPos;
        if (rt.haveLast && rt.misses < kGraceTicks) {
          ++rt.misses;
          ++stGrace;
          list.push_back(rt.last);
        }
        deferRetry();
        continue;
      }
    }
    rt.hierRetryAt = 0;  // chain healthy
    rt.failStreak = 0;
    rt.backoffAt = 0;

    // Players can be reparented when they mount a vehicle; the cached hierarchy
    // then points at the old body/seat and can keep returning a plausible but
    // wrong position (the player appears to vanish). Re-resolve actor
    // transforms on a slow cadence so they heal without costing reads per tick.
    if (rt.category == static_cast<int>(Prefabs::Category::Player) &&
        seq >= rt.hierRevalidateAt) {
      rt.hierRevalidateAt = seq + 512;
      uintptr_t fresh = 0;
      if (ResolveHierarchy(address, fresh) && fresh != rt.hierarchy) {
        const Vector3 freshPos = ReadPositionAt(fresh);
        if (PlausiblePos(freshPos)) {
          rt.hierarchy = fresh;
          pos = freshPos;
        }
      }
    }

    CachedEntity entity;
    entity.address = address;
    entity.className = rt.className;
    entity.displayName = rt.displayName;
    entity.prefabID = rt.prefabID;
    entity.category = rt.category;
    entity.position = pos;
    entity.sampleMs = tickNowMs;

    // ── velocity for moving entities (players + corpses) ───────────────────
    // The render thread extrapolates positions by this velocity every frame,
    // which is why player names/boxes are buttery. Corpses are moving physics
    // objects too (ragdolls slide/settle); without velocity + per-tick reads
    // their label stepped at the old 24-tick resource refresh cadence.
    if (cat == Prefabs::Category::Player ||
        cat == Prefabs::Category::Corpse) {
      // Velocity in m/s, sampled every ~40 ms (one tick of position noise would
      // give a uselessly jumpy vector) and smoothed, so a ghost can keep
      // drifting the way the player was heading while they are hidden.
      const double nowMs = GhostNowMs();
      if (rt.velSampleMs > 0.0) {
        const double dtd = nowMs - rt.velSampleMs;
        if (dtd >= 40.0) {
          const float dt = (float)dtd;
          const float inv = 1000.f / dt;
          Vector3 v{(pos.x - rt.velSamplePos.x) * inv,
                    (pos.y - rt.velSamplePos.y) * inv,
                    (pos.z - rt.velSamplePos.z) * inv};
          const float sp = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
          if (sp > kGhostMaxSpeed) {
            const float s = kGhostMaxSpeed / sp;
            v.x *= s; v.y *= s; v.z *= s;
          }
          const float a = 0.6f;  // weight of the fresh sample
          rt.vel.x += (v.x - rt.vel.x) * a;
          rt.vel.y += (v.y - rt.vel.y) * a;
          rt.vel.z += (v.z - rt.vel.z) * a;

          // Any real movement across the sample window proves the player is up
          // and about.
          const Vector3 mv = pos - rt.velSamplePos;
          if (fabsf(mv.x) + fabsf(mv.y) + fabsf(mv.z) > kMoveEps)
            rt.lastMoveMs = nowMs;

          rt.velSamplePos = pos;
          rt.velSampleMs = nowMs;
        }
      } else {
        rt.velSamplePos = pos;
        rt.velSampleMs = nowMs;
      }
      entity.velocity = rt.vel;
      entity.sampleMs = nowMs;
    }

    // Player-only: read the life state + player flags so the ESP can drop dead
    // players, tag wounded ones and hide / recolour sleepers. Two extra reads,
    // only for players.
    //
    // Every state bit is DEBOUNCED: it only flips after two consecutive reads
    // agree. A single mis-read/mis-aligned read used to make a live player blink
    // off the ESP (or flash as "Sleeper"/"Wounded") for a frame - which is
    // exactly the "players disappear randomly" symptom.
    if (cat == Prefabs::Category::Player) {
      const uint32_t life = Driver->Read<uint32_t>(
          address + BaseCombatEntity_Offsets::lifestate);
      rt.deadHits = (life != 0) ? (rt.deadHits + 1) : 0;

      const uint32_t flags = Driver->Read<uint32_t>(
          address + BasePlayer_Offsets::playerFlags);
      rt.sleepHits = ((flags & kPlayerFlagSleeping) != 0) ? (rt.sleepHits + 1) : 0;
      rt.woundHits = ((flags & kPlayerFlagWounded) != 0) ? (rt.woundHits + 1) : 0;

      const double nowMs = GhostNowMs();
      rt.lastSeenMs = nowMs;

      // Motion guard (see kMoveEps): movement wins over the state flags, so a
      // bad read can never drop a live player out of the ESP.
      const bool moving = (nowMs - rt.lastMoveMs) < (double)kMoveHoldMs;
      entity.alive = (rt.deadHits < 2) || moving;  // LifeState::Alive == 0, Dead == 1
      entity.sleeping = (rt.sleepHits >= 2) && !moving;
      entity.wounded  = (rt.woundHits >= 2);

      // Real player name (not the generic "Player" pretty name). Re-read
      // rarely; the shared_ptr makes the render-thread copy free.
      if (!rt.playerName || (seq - rt.playerNameAt) > 256) {
        rt.playerNameAt = seq;
        std::string name = ReadPlayerName(address);
        if (!name.empty())
          rt.playerName = std::make_shared<const std::string>(std::move(name));
      }
      if (rt.playerName)
        entity.displayName = rt.playerName;

      // Cached Steam64 identity, used to recognise this player when the client
      // destroys/re-creates the BasePlayer after server-side occlusion. Read
      // once (it never changes) and retried slowly if the first read failed.
      if (rt.userId == 0 &&
          (rt.userIdAt == 0 || (seq - rt.userIdAt) > 512)) {
        rt.userIdAt = seq;
        const uint64_t uid =
            Driver->Read<uint64_t>(address + BasePlayer_Offsets::userID);
        if (PlausibleUserId(uid))
          rt.userId = uid;
      }

      {
        LivePlayerIdentity live;
        live.address = address;
        live.userId = rt.userId;
        live.name = rt.playerName;
        livePlayers.push_back(std::move(live));
      }

      // Skeleton data is produced by the dedicated skeleton thread and
      // published separately; the entity snapshot stays bone-free.

      // Visible again: the live box takes over, so drop any marker that was
      // standing in for them - including one left at an old address/position
      // after an occlusion re-create (never show a player twice).
      DropGhostNear(pos, kGhostMergeDist);
      DropGhostForPlayer(address, rt.userId, rt.playerName);

      ++stActor;
      if (!entity.alive)
        ++stDead;
      if (haveOrigin) {
        const float pd = pos.Distance(originPos);
        if (pd < pNear)
          pNear = pd;
        if (pd > pFar)
          pFar = pd;
        ++pCount;
      }
    }

    rt.last = entity;
    rt.haveLast = true;
    rt.misses = 0;
    list.push_back(std::move(entity));
  }

  // Smooth the list: entities that briefly vanished (or whose read failed) are
  // kept for kGraceTicks using their last good data, so the ESP does not blink.
  // Anything unseen longer than that is forgotten.
  for (auto it = m_runtime.begin(); it != m_runtime.end();) {
    EntityRuntime &rt = it->second;
    if (rt.lastSeen != seq) {
      const Prefabs::Category cat =
          (rt.category >= 0 && rt.category < Prefabs::kCategoryCount)
              ? (Prefabs::Category)rt.category
              : Prefabs::GetCategory(rt.prefabID);
      const bool enabled =
          !g_cfg || g_cfg->Cat(cat).enabled ||
          g_cfg->ImageWants(cat, rt.prefabID,
                            rt.className ? rt.className.get() : nullptr);
      if (rt.haveLast && enabled) {
        // Server-side occlusion destroys the client entity; when the player
        // becomes visible again the server re-creates the BasePlayer at a NEW
        // object address. The old address then sits in the 10 s player grace
        // while the live copy draws at the new address - which is exactly the
        // "same player shown two or three times" bug. If this player is already
        // live in this snapshot, drop the stale copy and any marker left at the
        // old address instead of emitting it.
        bool replaced = false;
        if (cat == Prefabs::Category::Player) {
          for (const LivePlayerIdentity &live : livePlayers) {
            if (SamePlayer(rt, it->first, live)) {
              replaced = true;
              break;
            }
          }
        }
        if (replaced) {
          ++stReplaced;
          DropGhostForPlayer(it->first, rt.userId, rt.playerName);
          it = m_runtime.erase(it);
          continue;
        }

        bool keep = rt.misses < kGraceTicks;
        if (cat == Prefabs::Category::Player) {
          // Hold the player only briefly after the client removes the entity,
          // then hand over to the fading last-seen marker. Using the real
          // last-seen timestamp means the marker starts at (almost) full
          // opacity and fades out over ghost_time_ms.
          const double now = GhostNowMs();
          keep = rt.lastSeenMs > 0.0 &&
                 (now - rt.lastSeenMs) < kPlayerVanishMs;
          if (rt.lastSeenMs <= 0.0)
            keep = rt.misses < kGraceTicks;
        }
        if (keep) {
          ++rt.misses;
          ++stGrace;
          list.push_back(rt.last);
          ++it;
          continue;
        }
      }

      // Really gone now - not a transient miss. If it was a live player this is
      // almost certainly Facepunch's server-side player occlusion dropping them
      // from our client (or they slipped out of the network range), so leave a
      // last-known-position marker instead of letting the box blink out.
      if (rt.haveLast && enabled && cat == Prefabs::Category::Player &&
          rt.last.alive && g_cfg && g_cfg->ghost_time_ms > 0 &&
          it->first != m_localPlayer.load(std::memory_order_acquire)) {
        DropGhostNear(rt.last.position, kGhostMergeDist);
        // Update an existing marker for this player instead of appending
        // another one. Repeated occlusion cycles used to leave several markers
        // for the same player at different spots, which also looked like the
        // ESP duplicating itself.
        GhostEntity *slot = nullptr;
        for (GhostEntity &g : m_ghosts) {
          const bool sameAddress = g.address == it->first;
          const bool sameUser = rt.userId != 0 && g.userId == rt.userId;
          const bool bothIds = rt.userId != 0 && g.userId != 0;
          const bool sameName =
              !bothIds && rt.playerName && !rt.playerName->empty() &&
              g.name && !g.name->empty() && *rt.playerName == *g.name;
          if (sameAddress || sameUser || sameName) {
            slot = &g;
            break;
          }
        }
        if (!slot && m_ghosts.size() < kGhostMax) {
          m_ghosts.emplace_back();
          slot = &m_ghosts.back();
        }
        if (slot) {
          slot->address    = it->first;
          slot->userId     = rt.userId;
          slot->name       = rt.playerName;
          slot->position   = rt.last.position;
          slot->velocity   = rt.vel;
          slot->lastSeenMs = rt.lastSeenMs > 0.0 ? rt.lastSeenMs : GhostNowMs();
          slot->wounded    = rt.last.wounded;
          slot->sleeping   = rt.last.sleeping;
          ++stGhost;
        }
      }

      it = m_runtime.erase(it);
      continue;
    }
    ++it;
  }

  // Drop markers past their lifetime before publishing, so the render thread
  // only ever sees live ones.
  PruneGhosts(GhostNowMs(), g_cfg ? (double)g_cfg->ghost_time_ms : 0.0);

  const size_t cachedCount = list.size();
  m_cache.Set(std::move(list), m_ghosts);
  prof::snapshots.fetch_add(1, std::memory_order_relaxed);

  // Report the cache size and where entities were lost. This used to print only
  // when the emit count changed, which hid exactly the case we need to see: a
  // player the render side dropped (dead/sleeper/W2S) never changes the emit
  // count. So it is now a steady heartbeat - one line every 3 s - which makes
  // "the box vanished" answerable from the counters.
  {
    using namespace std::chrono;
    static size_t lastSize = (size_t)-1;
    static steady_clock::time_point lastPrint{};
    const auto now = steady_clock::now();
    // Keep the rate low: every console write briefly stalls the worker.
    if (lastSize == (size_t)-1 || now - lastPrint >= seconds(3)) {
      lastSize = cachedCount;
      lastPrint = now;
      // One line, everything needed to explain a missing box:
      //   offCat   = the user has that category switched off (cheap skip)
      //   offFar   = worker spatial cull past the draw distance (cheap skip)
      //   emit     = entities handed to the ESP this tick
      //   pNear/pFar = nearest/furthest player in that set (m from local player)
      //   ghost    = players that just dropped out (server occlusion) -> markers
      //   esp ...  = what the render side did with it since the last line
      //              (pDraw drawn, pDead/pSlp/pDist/pW2S/pOff = dropped why,
      //               gHost = fading last-known-position markers drawn)
      printf("[cache] list=%zu offCat=%zu offFar=%zu | emit=%zu actors=%zu "
             "dead=%zu | pNear=%.0f pFar=%.0f | nos=%zu grace=%zu ghost=%zu "
             "backoff=%zu repl=%zu | esp tot=%u pDraw=%u pDead=%u pSlp=%u pDist=%u pW2S=%u "
             "pOff=%u gHost=%u frm=%.2fms | local=0x%llX\n",
             addresses.size(), stOffCat, stOffFar, cachedCount, stActor, stDead,
             pCount ? pNear : -1.f, pCount ? pFar : -1.f,
             stIdFail + stNoHier + stNoPos, stGrace, stGhost, stBackoff,
             stReplaced,
             espstats::take(espstats::total), espstats::take(espstats::pDraw),
             espstats::take(espstats::pDead), espstats::take(espstats::pSleep),
             espstats::take(espstats::pDist), espstats::take(espstats::pW2S),
             espstats::take(espstats::pOff), espstats::take(espstats::gDraw),
             espstats::renderUs.load(std::memory_order_relaxed) / 1000.0,
             (unsigned long long)m_localPlayer.load());
    }
  }
}

// ── last-known-position marker bookkeeping ───────────────────────────────────
// Drop the marker a now-visible player has taken over, so they are never drawn
// twice (live box + stale marker in the same spot).
void BaseNetworkable::DropGhostNear(const Vector3 &p, float radius) {
  if (m_ghosts.empty())
    return;
  const float r2 = radius * radius;
  for (auto it = m_ghosts.begin(); it != m_ghosts.end();) {
    const Vector3 d = it->position - p;
    if (d.x * d.x + d.y * d.y + d.z * d.z <= r2)
      it = m_ghosts.erase(it);
    else
      ++it;
  }
}

// Identity-aware version of DropGhostNear. Server-side occlusion re-creates the
// BasePlayer at a new object address, so a marker can belong to the same player
// but sit far from where they reappeared. Match the cached Steam64 first, then
// the real player name, and drop the marker regardless of distance.
void BaseNetworkable::DropGhostForPlayer(
    uintptr_t address, uint64_t userId,
    const std::shared_ptr<const std::string> &name) {
  if (m_ghosts.empty())
    return;
  for (auto it = m_ghosts.begin(); it != m_ghosts.end();) {
    const bool sameAddress = address != 0 && it->address == address;
    const bool sameUser = userId != 0 && it->userId == userId;
    const bool bothIds = userId != 0 && it->userId != 0;
    const bool sameName = !bothIds && name && !name->empty() && it->name &&
                          !it->name->empty() && *name == *it->name;
    if (sameAddress || sameUser || sameName)
      it = m_ghosts.erase(it);
    else
      ++it;
  }
}

// Forget markers older than the configured lifetime (ttl <= 0 = feature off).
void BaseNetworkable::PruneGhosts(double nowMs, double ttlMs) {
  if (m_ghosts.empty())
    return;
  if (ttlMs <= 0.0) {
    m_ghosts.clear();
    return;
  }
  for (auto it = m_ghosts.begin(); it != m_ghosts.end();) {
    if (nowMs - it->lastSeenMs > ttlMs)
      it = m_ghosts.erase(it);
    else
      ++it;
  }
}
