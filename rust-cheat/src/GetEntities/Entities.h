
// ================================================================================
// +------------------------------------------------+
// | TG Bot: @imguisource_bot                       |
// | ImGui Source: @ImGui_Source_FIXNEY                        |
// | shit src 🧨: @il2cpp_src                        |
// +------------------------------------------------+
// ================================================================================

#pragma once
#include "../Threading/Threading.h"
#include "EntityCaching.h"
#include "Skeleton.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <World2Screen/Math/Math.h>

// Classes found BY NAME in the GameAssembly TypeInfo table (the dump's
// TypeInfo RVAs don't resolve on the live build - see ResolveClasses).
extern uint64_t g_bnKlass;   // BaseNetworkable
extern uint64_t g_lpKlass;   // LocalPlayer
extern uint64_t g_camKlass;  // MainCamera

// Finds the classes above by scanning the TypeInfo pointer table and
// matching klass+0x10 name strings (works regardless of build).
void ResolveClasses();

// Background retry for the MainCamera class name (called from the entity
// thread) - its name string is only readable once the game has touched it.
void RetryCameraByName();

// Per-entity data that does not change while the object lives. Keeping it
// across ticks turns the worker's steady-state cost from ~10 driver
// round-trips per entity into one (identity) plus one (position).
struct EntityRuntime {
  uint64_t klass = 0;        // object header / class pointer (identity check)
  std::shared_ptr<const std::string> className;  // cached class name label
  std::shared_ptr<const std::string> displayName; // cached pretty/readable label
  std::shared_ptr<const std::string> playerName;  // real BasePlayer name (players)
  uint32_t playerNameAt = 0;     // tick the name was last read at
  // Steam64 (BasePlayer.userID) cached for player-identity matching. Facepunch's
  // server-side occlusion makes the client destroy and re-create the BasePlayer,
  // so the SAME player comes back at a NEW entity address. The userID (or the
  // real player name as a fallback) is what lets the worker recognise the old
  // grace copy as the same player and drop it instead of drawing it twice.
  uint64_t userId = 0;
  uint32_t userIdAt = 0;         // tick the userID read was last attempted
  uint32_t prefabID = 0;     // cached prefab id (category + pretty name)
  int category = -1;         // cached Prefabs::Category of prefabID
  uintptr_t hierarchy = 0;   // resolved transform hierarchy (position source)
  bool haveStatics = false;  // className/prefabID/category resolved yet?
  uint32_t lastSeen = 0;     // tick sequence this address last appeared in
  uint32_t refreshedAt = 0;  // tick sequence identity/category last validated
  // Retry bookkeeping. A freshly streamed-in entity shows up in the client
  // list BEFORE Unity has instantiated its prefab, so its native transform
  // chain is not readable yet. These two counters make the worker keep
  // retrying (instead of dropping the entity forever) until the object is
  // really there.
  uint32_t idTries = 0;      // consecutive failed identity resolves
  uint32_t hierRetryAt = 0;  // tick before which a failed hierarchy resolve is not retried
  uint32_t hierRevalidateAt = 0;  // actors: periodic re-resolve (vehicle reparenting)
  // Entities that repeatedly cannot be resolved (freed/stale list entries,
  // streamed-out objects) are retried with exponential backoff instead of
  // costing reads every tick.
  uint32_t backoffAt = 0;
  uint32_t failStreak = 0;
  // Player-state debounce: these are consecutive agreeing reads, so a single
  // bad read can never make a live player blink off (or a live one turn
  // "Sleeper"/"Wounded" for one frame).
  uint32_t deadHits = 0;
  uint32_t sleepHits = 0;
  uint32_t woundHits = 0;
  // Last-seen tracking (server-side player occlusion). A smoothed velocity so a
  // ghost can keep drifting the way the player was heading, and the wall-clock
  // time of the last sighting (the ghost fades from that).
  Vector3 vel{};
  Vector3 velSamplePos{};
  double  velSampleMs = 0.0;
  double  lastSeenMs  = 0.0;
  // Last time this player was seen actually moving. Movement overrides a bad
  // state read (see the motion guard in the worker).
  double  lastMoveMs  = -1.0e18;
  // Last successfully built entity. Reused between refreshes (static resources
  // are only re-read every few ticks) and as a short "grace" so a single failed
  // read does not make an entity flicker out of the ESP.
  CachedEntity last{};
  bool haveLast = false;
  uint32_t misses = 0;       // consecutive ticks we failed to refresh this one
};


struct EntityPos {
  uintptr_t address = 0;
  Vector3 position{};
  uintptr_t transform = 0;
  bool transformValid = false;
  bool destroyed = false;
  uintptr_t parentEntity = 0;
  std::string name;
};


class BaseNetworkable {
public:
  explicit BaseNetworkable(uintptr_t gameAssembly);
  ~BaseNetworkable() = default;

  uintptr_t GetEntities() const {
    return m_entitiesPtr.load(std::memory_order_acquire);
  }

  EntityCache::Ptr Snapshot() { return m_cache.Get(); }
  uint64_t SnapshotPublishUs() const { return m_cache.PublishUs(); }

  // Last-known-position markers for players the server has stopped sending us
  // (server-side player occlusion). Published with the entity snapshot.
  EntityCache::GhostPtr Ghosts() { return m_cache.GetGhosts(); }

  uintptr_t GetLocalPlayer() const {
    return m_localPlayer.load(std::memory_order_acquire);
  }

  // Latest local-player world position, computed on the worker thread through
  // the same transform chain the entities use. Published with a seqlock so the
  // render thread reads it without ever blocking. Returns true when the value
  // is usable.
  bool GetLocalPlayerPos(Vector3 &out) const {
    for (int i = 0; i < 8; ++i) {
      const uint32_t s1 = m_lpSeq.load(std::memory_order_acquire);
      if (s1 & 1u)
        continue;
      const Vector3 p = m_lpPos;
      const bool hp = m_lpHp;
      const uint32_t s2 = m_lpSeq.load(std::memory_order_acquire);
      if (s1 == s2) {
        out = p;
        return hp;
      }
    }
    return false;
  }

private:
  void Tick();

  uintptr_t GetInstance();
  uintptr_t GetClientEntities();
  const std::vector<uintptr_t> &FetchEntityList();
  uintptr_t ReadLocalPlayer();

  // Last-known-position marker bookkeeping (see GhostEntity). Called only from
  // the worker tick.
  void DropGhostNear(const Vector3 &p, float radius);
  // Identity-aware ghost cleanup: removes the ghost for a player who is live
  // again even if they reappeared far from their last-known spot (server-side
  // occlusion re-creates the BasePlayer at a new address, so a distance-only
  // merge leaves the stale "last seen" marker behind next to the live ESP).
  void DropGhostForPlayer(uintptr_t address, uint64_t userId,
                          const std::shared_ptr<const std::string> &name);
  void PruneGhosts(double nowMs, double ttlMs);

  // Dedicated skeleton thread: reads bones on its own driver lane and publishes
  // them for the render thread. It never blocks the entity/name snapshot.
  void SkeletonTick();
  std::unordered_map<uintptr_t, skeleton::Cache> m_skeletonCaches;

  std::string ReadIl2CppString(uintptr_t address);
  std::string GetObjectClassName(uintptr_t entity);

  uint32_t GetPrefabID(uintptr_t entity);
  Vector3 GetPosition(uintptr_t entity);

  // Resolve the native transform-hierarchy pointer (position source) in one
  // chain of reads. Done once per entity, then only redone when the cached
  // pointer stops yielding a plausible position.
  bool ResolveHierarchy(uintptr_t entity, uintptr_t &hierarchy);
  // A single driver read: entity position from a cached hierarchy pointer.
  static Vector3 ReadPositionAt(uintptr_t hierarchy);

  uintptr_t gameAssemblyBase;
  std::atomic<uintptr_t> m_entitiesPtr;
  std::atomic<uintptr_t> m_localPlayer;
  uintptr_t m_lastEntitiesPtr;
  EntityCache m_cache;
  PollingThread m_worker;

  // Local-player position (worker writes, render reads) - seqlock published.
  std::atomic<uint32_t> m_lpSeq{0};
  Vector3 m_lpPos{};
  bool m_lpHp = false;
  uintptr_t m_localHierarchy = 0;
  uintptr_t m_lastLocalPlayer = 0; // detect respawn/entity-address change

  // Hold-the-last-known bookkeeping. The local-player wrapper read fails a few
  // percent of ticks; without this the ESP's distance origin (and the worker's
  // spatial cull centre) blinked out with it, which made the range filter and
  // the cull flip on/off several times a second.
  uint32_t m_lpMiss = 0;      // consecutive failed local-player reads
  Vector3 m_lpLastPos{};      // last known-good local player position
  uint32_t m_lpHaveSeq = 0;   // tick sequence m_lpLastPos was last refreshed at

  // Persistent per-entity cache (worker-thread only, so no lock needed).
  std::unordered_map<uintptr_t, EntityRuntime> m_runtime;
  uint32_t m_tickSeq = 0;

  // Cached entity-pointer array; re-read only when the buffer pointer/count
  // changes or every kListRefreshTicks ticks.
  std::vector<uintptr_t> m_listCache;
  uintptr_t m_listArrayPtr = 0;
  int32_t m_listCount = 0;
  uint32_t m_listReadAt = 0;

  // Players the server has stopped sending us (occlusion). Kept for a short
  // while so the ESP can draw a fading last-known-position marker instead of
  // the box blinking out. Worker thread only, so no lock is needed.
  std::vector<GhostEntity> m_ghosts;

  // Declared last so it is destroyed (and joined) before the caches/entity
  // store it reads are torn down.
  PollingThread m_skeletonWorker;
};
