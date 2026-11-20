
// ================================================================================
// Original upload at: @imguisource_bot
// Channels: @ImGui_Source_FIXNEY (ImGui Source), @il2cpp_src (shit src 🧨)
// ================================================================================

#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "../World2Screen/Math/Math.h"
#include "../Diag/Profiler.h"


struct CachedEntity {
  uintptr_t address = 0;
  // Shared, immutable class name. Sharing the string (instead of copying it
  // every tick) removes hundreds of short-string copies per worker tick, which
  // was feeding the heap allocator that the render thread also uses.
  std::shared_ptr<const std::string> className;
  // Pre-computed human readable label (pretty name, else class name). Built
  // ONCE on the worker thread so the render thread never calls the pretty-name
  // lookup (which allocated a std::string per entity, per frame).
  std::shared_ptr<const std::string> displayName;
  uint32_t prefabID = 0;
  // Classified ONCE on the worker thread (and cross-checked against the class
  // name), so the render thread never re-runs the big prefab switch per entity
  // per frame - and so a wrong/unknown prefab read can never silently drop an
  // entity from the ESP (players are the ones that suffer from that).
  int category = -1;  // Prefabs::Category, -1 = not classified
  Vector3 position{};
  // Player state (read on the worker thread; only meaningful when the entity
  // classifies as Category::Player). Dead players are dropped by the ESP,
  // wounded players get a "Wounded" tag, sleepers can be hidden / recoloured.
  bool alive = true;
  bool wounded = false;
  bool sleeping = false;
  // Smoothed actor velocity (m/s) and the worker clock at which `position` was
  // sampled. The render pass extrapolates players between snapshots.
  Vector3 velocity{};
  double sampleMs = 0.0;
};

// A player we have LOST - i.e. one the server stopped networking to us.
//
// Facepunch added server-side player occlusion (Feb 2025): the server stops
// sending a player entity to our client as soon as we are occluded from them
// (terrain, rocks, buildings) or they slip out of the network range. The entity
// is then destroyed client-side, so there is NO live position left anywhere in
// the process to read - which is why such a player used to pop out of the ESP
// and back in at random.
//
// The last known spot is therefore kept here (with the velocity the player was
// moving at) and re-drawn as a fading "ghost", the same idea CS/Valorant
// cheats use for last-known-position. It is not a live position: it is where
// they were a moment ago, and it fades out.
struct GhostEntity {
  uintptr_t address = 0;     // last known object address (never draw ourselves)
  // Player identity captured with the marker. The live player may come back at
  // a different object address after server-side occlusion, so the userID /
  // name lets the worker drop this marker by identity, not just by proximity.
  uint64_t userId = 0;
  std::shared_ptr<const std::string> name;
  Vector3 position{};        // last known feet position
  Vector3 velocity{};        // smoothed m/s at the last sighting (extrapolation)
  double  lastSeenMs = 0.0;  // GhostNowMs() at the last sighting (fade start)
  bool    wounded = false;
  bool    sleeping = false;
};

// Double-buffered snapshot store. The worker publishes a whole immutable
// vector behind a shared_ptr and the render thread only copies that pointer
// (an atomic refcount bump) - no element/string copying and no long lock on
// the frame path, which is what keeps the overlay from hitching.
class EntityCache {
public:
  using Ptr = std::shared_ptr<const std::vector<CachedEntity>>;
  using GhostPtr = std::shared_ptr<const std::vector<GhostEntity>>;

  // Entities and ghosts are published together so the render thread always sees
  // a consistent pair, under one short lock.
  void Set(std::vector<CachedEntity> fresh, std::vector<GhostEntity> ghosts) {
    Ptr p = std::make_shared<const std::vector<CachedEntity>>(std::move(fresh));
    GhostPtr g =
        std::make_shared<const std::vector<GhostEntity>>(std::move(ghosts));
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entities = std::move(p);
    m_ghosts = std::move(g);
    m_publishUs.store(prof::NowUs(), std::memory_order_release);
  }

  Ptr Get() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entities;
  }

  GhostPtr GetGhosts() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ghosts;
  }

  uint64_t PublishUs() const {
    return m_publishUs.load(std::memory_order_acquire);
  }

private:
  std::mutex m_mutex;
  Ptr m_entities;
  GhostPtr m_ghosts;
  std::atomic<uint64_t> m_publishUs{0};
};
