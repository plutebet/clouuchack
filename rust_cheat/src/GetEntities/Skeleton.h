
// ================================================================================
// Все обновления тут:
// Бот: https://t.me/imguisource_bot
// Канал: https://t.me/ImGui_Source_FIXNEY (ImGui Source)
// Второй: https://t.me/il2cpp_src (shit src 🧨)
// ================================================================================

#pragma once

#include <cstdint>
#include <vector>

#include "../World2Screen/Math/Math.h"

// Player skeleton ESP.
//
// Bone transforms are read on the entity worker (never the render thread) and
// cached per entity. Everything expensive is amortised:
//   - model/bone-array/transform pointers are resolved once, a few bones per
//     worker tick, so a newly seen player never causes a 40-read burst;
//   - the transform matrix/index buffer pointers are cached;
//   - bone data is stored as world positions plus the feet position it was read
//     at, so the render pass can slide the whole skeleton to the player's live
//     position every frame (no 20 Hz trailing behind a moving player).
namespace skeleton {

enum BoneIndex : int {
  Head = 0,
  Neck,
  Spine4,
  Spine3,
  Spine2,
  Spine1,
  Pelvis,
  LClavicle,
  LUpperArm,
  LForearm,
  LHand,
  RClavicle,
  RUpperArm,
  RForearm,
  RHand,
  LHip,
  LKnee,
  LFoot,
  RHip,
  RKnee,
  RFoot,
  Count
};

struct Data {
  Vector3 bones[Count]{};
  Vector3 origin{}; // feet position when the bones were read
  bool hasOrigin = false;
  uint32_t mask = 0; // bit i = bones[i] is valid

  bool Valid() const { return mask != 0; }
  bool Has(int index) const {
    return index >= 0 && index < Count && ((mask >> index) & 1u) != 0;
  }
  bool HasOrigin() const { return mask != 0 && hasOrigin; }
};

struct BoneRef {
  uintptr_t trans = 0;
  int index = -1;
};

// Per-entity resolution/update cache (worker thread only).
struct Cache {
  uintptr_t player = 0;
  uintptr_t model = 0;

  // Resolution progress. `scanCount` bone pointers are read in one bulk call,
  // then `resolveDone` bones are resolved a few at a time on later updates.
  uintptr_t bonePointers[66]{};
  int scanCount = 0;
  int resolveDone = 0;
  bool resolving = false;

  BoneRef refs[Count];
  bool resolved = false;
  int readFails = 0;

  // Cached index topology. The matrix buffer pointer is re-read every update
  // (Unity moves/reallocates it); caching that pointer made most bone reads
  // target a stale buffer, which is exactly what dropped the skeleton to 1 FPS.
  uintptr_t indexBuffer = 0;
  uint32_t indexAge = 0;
  // Parent indices are static topology; cache them instead of re-reading the
  // index buffer on every update.
  std::vector<int> parents;

  double lastUpdateMs = 0.0;
  Data data;
};

// Skeleton updates are only useful up close; beyond this the bones collapse to
// a few pixels. Names/meters still draw at the normal player distance.
constexpr float kMaxDistance = 500.f;

// Upper bound on players whose bones may be refreshed in one 8 ms worker tick.
// The worker picks the most overdue players, so a lobby larger than this is
// still covered fairly (each player just updates less often).
constexpr int kMaxUpdatesPerTick = 6;

// Called once at the start of every entity worker tick.
void BeginTick();

// True when this player's skeleton is due for a refresh. Used by the skeleton
// worker to pick players without consuming budget.
bool Due(const Cache &cache, float distance, float speed, double nowMs);

// Updates `cache` when due. `origin` is the player's current feet position and
// `speed` is the worker's smoothed velocity (m/s); moving players refresh more
// often than idle ones. Returns true when fresh bone data was read.
bool Update(uintptr_t player, const Vector3 &origin, float distance, float speed,
            double nowMs, Cache &cache);

// Worker -> render published skeleton data, keyed by player entity address.
// The skeleton worker runs on its own thread so bone reads never delay the
// entity/name snapshot.
void Publish(uintptr_t address, const Data &data);
bool Get(uintptr_t address, Data &out);
void ClearPublished();

} // namespace skeleton
