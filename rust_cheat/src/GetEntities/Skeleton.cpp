
// ================================================================================
// Бот с сурсами: @imguisource_bot
// Основной канал (ImGui Source): @ImGui_Source_FIXNEY
// Дополнительный канал (shit src 🧨): @il2cpp_src
// ================================================================================

#include "Skeleton.h"

#include <intrin.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "../Driver/Rpm.h"
#include "../Offsets/Offsets.h"

namespace skeleton {
namespace {

// Rust bone indices (see the dump's model namespace).
constexpr int kGameBoneId[Count] = {
    53, // Head
    52, // Neck
    23, // Spine4
    22, // Spine3
    21, // Spine2
    20, // Spine1
    0,  // Pelvis
    24, // LClavicle
    25, // LUpperArm
    26, // LForearm
    29, // LHand
    60, // RClavicle
    61, // RUpperArm
    62, // RForearm
    65, // RHand
    1,  // LHip
    3,  // LKnee
    4,  // LFoot
    14, // RHip
    16, // RKnee
    17, // RFoot
};

constexpr int kMaxBoneId = 65;
constexpr int kScanCount = kMaxBoneId + 1;
constexpr int kMaxUpdatePerTick = kMaxUpdatesPerTick;
constexpr int kResolvePerUpdate = 4;
constexpr int kMaxTransformIndex = 2048;
constexpr size_t kMaxMatrixBytes = 512 * 1024;
constexpr size_t kMaxIndexBytes = 64 * 1024;

int g_budget = 0;

// Auto-diagnostics: one line every 5 s, only when skeleton reads are slow or
// failing. This keeps the console quiet on a healthy setup but tells us exactly
// what is wrong if the skeleton still stutters.
uint64_t g_tryCount = 0;
uint64_t g_okCount = 0;
uint64_t g_totalUs = 0;
uint64_t g_maxUs = 0;
std::chrono::steady_clock::time_point g_lastStats{};

std::mutex g_publishMutex;
std::unordered_map<uintptr_t, Data> g_published;

int UpdateIntervalMs(float distance, float speed) {
  // Pose latency is what makes a skeleton look "laggy": the bones are world
  // positions, so sliding them to the live player position keeps them glued to
  // the body, but the LIMB rotation stays frozen until the next bone read. The
  // reference (Cloak-Rust) re-reads bones every ESP frame, so anything above
  // ~16 ms is visible while a player turns/aims. Near players therefore update
  // at the worker tick rate (8 ms); the budget below still caps total work.
  const bool moving = speed > 0.3f;
  if (distance <= 0.f)
    return moving ? 16 : 33;
  if (distance <= 25.f)
    return moving ? 8 : 16;
  if (distance <= 50.f)
    return moving ? 12 : 24;
  if (distance <= 100.f)
    return moving ? 24 : 33;
  return moving ? 33 : 66;
}

void ResetResolution(Cache &cache) {
  cache.resolving = false;
  cache.resolved = false;
  cache.resolveDone = 0;
  cache.scanCount = 0;
  cache.indexBuffer = 0;
  cache.indexAge = 0;
  cache.parents.clear();
  for (BoneRef &ref : cache.refs)
    ref = BoneRef{};
}

// Reads the model/bone pointers once, then resolves a few bones per call so a
// newly seen player never causes a 40-read burst on the shared driver lane.
bool ResolveStep(uintptr_t player, Cache &cache) {
  uintptr_t model =
      Driver->Read<uintptr_t>(player + BaseEntity_Offsets::model);
  if (!model || model < 0x10000)
    model = Driver->Read<uintptr_t>(player + 0x1b8); // older dump layout
  if (!model || model < 0x10000)
    return false;

  if (model != cache.model) {
    cache.model = model;
    cache.data = Data{};
    ResetResolution(cache);
  }
  if (cache.resolved)
    return true;

  if (!cache.resolving) {
    const uintptr_t boneTransforms =
        Driver->Read<uintptr_t>(model + Model_Offsets::boneTransforms);
    if (!boneTransforms || boneTransforms < 0x10000)
      return false;

    // boneTransforms is an IL2CPP Transform[]: klass/monitor/bounds/length in
    // the object header, element 0 at +0x20. (The old +0x28 skipped the first
    // element, so every bone read the transform of the NEXT bone - that is what
    // made the skeleton look shifted/funny.) Read the whole array in one call;
    // the model always has at least 66 bones (max index used here is 65).
    if (!Driver->ReadBytesInto(boneTransforms + 0x20, cache.bonePointers,
                               static_cast<size_t>(kScanCount) *
                                   sizeof(uintptr_t)))
      return false;

    cache.scanCount = kScanCount;
    cache.resolveDone = 0;
    cache.resolving = true;
    return false; // start resolving on the next scheduled update
  }

  int processed = 0;
  while (cache.resolveDone < Count && processed < kResolvePerUpdate) {
    const int i = cache.resolveDone++;
    ++processed;

    const int id = kGameBoneId[i];
    if (id < 0 || id >= cache.scanCount)
      continue;
    const uintptr_t bone = cache.bonePointers[id];
    if (!bone || bone < 0x10000)
      continue;

    const uintptr_t trans = Driver->Read<uintptr_t>(bone + 0x10);
    if (!trans || trans < 0x10000)
      continue;
    const int index = Driver->ReadInt32(trans + 0x30);
    if (index < 0 || index > kMaxTransformIndex)
      continue;

    cache.refs[i].trans = trans;
    cache.refs[i].index = index;
  }

  if (cache.resolveDone >= Count) {
    cache.resolving = false;
    cache.resolveDone = 0;
    int valid = 0;
    for (const BoneRef &ref : cache.refs)
      if (ref.trans && ref.index >= 0)
        ++valid;
    cache.resolved = valid > 0;
  }
  return cache.resolved;
}

bool ReadBones(Cache &cache, const Vector3 &origin) {
  uintptr_t anyTrans = 0;
  int maxIndex = -1;
  for (const BoneRef &ref : cache.refs) {
    if (!ref.trans || ref.index < 0)
      continue;
    anyTrans = ref.trans;
    maxIndex = std::max(maxIndex, ref.index);
  }
  if (!anyTrans || maxIndex < 0)
    return false;

  // Re-read the live hierarchy/matrix pointers every update. Unity relocates
  // these transform arrays while the model animates; caching the matrix pointer
  // made most bone reads target a stale buffer, which is what dropped the
  // skeleton to ~1 FPS.
  const uintptr_t hierarchy = Driver->Read<uintptr_t>(anyTrans + 0x28);
  if (!hierarchy || hierarchy < 0x10000)
    return false;

  uintptr_t transformData[2]{};
  if (!Driver->ReadBytesInto(hierarchy + 0x18, transformData,
                             sizeof(transformData)) ||
      !transformData[0] || !transformData[1])
    return false;

  const uintptr_t matrixBuffer = transformData[0];
  const uintptr_t indexBuffer = transformData[1];

  const size_t matrixBytes = 0x30ull * static_cast<size_t>(maxIndex) + 0x30ull;
  const size_t indexBytes = 4ull * static_cast<size_t>(maxIndex) + 4ull;
  if (matrixBytes > kMaxMatrixBytes || indexBytes > kMaxIndexBytes)
    return false;

  // Parent topology is static; read it once (or when the index buffer moves)
  // and reuse it for every animation update.
  if (indexBuffer != cache.indexBuffer ||
      cache.parents.size() < static_cast<size_t>(maxIndex) + 1 ||
      cache.indexAge++ > 32) {
    static thread_local std::vector<uint8_t> indexRaw;
    indexRaw.resize(indexBytes);
    if (!Driver->ReadBytesInto(indexBuffer, indexRaw.data(), indexBytes)) {
      cache.indexBuffer = 0;
      cache.parents.clear();
      return false;
    }
    cache.indexBuffer = indexBuffer;
    cache.indexAge = 0;
    cache.parents.resize(static_cast<size_t>(maxIndex) + 1);
    std::memcpy(cache.parents.data(), indexRaw.data(),
                (static_cast<size_t>(maxIndex) + 1) * sizeof(int));
  }

  // Reused scratch buffer: continuous skeleton updates must not churn the heap
  // that the render thread also uses.
  static thread_local std::vector<uint8_t> matrices;
  matrices.resize(matrixBytes);

  if (!Driver->ReadBytesInto(matrixBuffer, matrices.data(), matrixBytes))
    return false;

  const __m128 mulVec0 = {-2.000f, 2.000f, -2.000f, 0.000f};
  const __m128 mulVec1 = {2.000f, -2.000f, -2.000f, 0.000f};
  const __m128 mulVec2 = {-2.000f, -2.000f, 2.000f, 0.000f};

  struct Matrix34 {
    uint8_t vec0[16];
    uint8_t vec1[16];
    uint8_t vec2[16];
  };

  Data data;
  for (int bone = 0; bone < Count; ++bone) {
    const BoneRef &ref = cache.refs[bone];
    if (!ref.trans || ref.index < 0)
      continue;
    if (static_cast<size_t>(ref.index) * 0x30ull + 0x30ull > matrixBytes ||
        static_cast<size_t>(ref.index) * 4ull + 4ull > indexBytes)
      continue;

    __m128 result =
        _mm_loadu_ps(reinterpret_cast<const float *>(matrices.data() +
                                                     static_cast<size_t>(
                                                         ref.index) *
                                                         0x30ull));
    int transformIndex = 0;
    if (static_cast<size_t>(ref.index) < cache.parents.size())
      transformIndex = cache.parents[ref.index];

    for (int guard = 0; transformIndex >= 0 && guard < 128; ++guard) {
      if (static_cast<size_t>(transformIndex) * 0x30ull + 0x30ull >
          matrixBytes)
        break;

      Matrix34 matrix34;
      std::memcpy(&matrix34,
                  matrices.data() + static_cast<size_t>(transformIndex) * 0x30ull,
                  0x30);

      const __m128 v1 =
          _mm_loadu_ps(reinterpret_cast<const float *>(matrix34.vec1));
      const __m128i v1i = _mm_castps_si128(v1);
      const __m128 xxxx = _mm_castsi128_ps(_mm_shuffle_epi32(v1i, 0x00));
      const __m128 yyyy = _mm_castsi128_ps(_mm_shuffle_epi32(v1i, 0x55));
      const __m128 zwxy = _mm_castsi128_ps(_mm_shuffle_epi32(v1i, 0x8E));
      const __m128 wzyw = _mm_castsi128_ps(_mm_shuffle_epi32(v1i, 0xDB));
      const __m128 zzzz = _mm_castsi128_ps(_mm_shuffle_epi32(v1i, 0xAA));
      const __m128 yxwy = _mm_castsi128_ps(_mm_shuffle_epi32(v1i, 0x71));

      const __m128 tmp7 =
          _mm_mul_ps(_mm_loadu_ps(reinterpret_cast<const float *>(matrix34.vec2)),
                     result);

      result = _mm_add_ps(
          _mm_add_ps(
              _mm_add_ps(
                  _mm_mul_ps(
                      _mm_sub_ps(_mm_mul_ps(_mm_mul_ps(xxxx, mulVec1), zwxy),
                                 _mm_mul_ps(_mm_mul_ps(yyyy, mulVec2), wzyw)),
                      _mm_castsi128_ps(
                          _mm_shuffle_epi32(_mm_castps_si128(tmp7), 0xAA))),
                  _mm_mul_ps(
                      _mm_sub_ps(_mm_mul_ps(_mm_mul_ps(zzzz, mulVec2), wzyw),
                                 _mm_mul_ps(_mm_mul_ps(xxxx, mulVec0), yxwy)),
                      _mm_castsi128_ps(
                          _mm_shuffle_epi32(_mm_castps_si128(tmp7), 0x55)))),
              _mm_add_ps(
                  _mm_mul_ps(
                      _mm_sub_ps(_mm_mul_ps(_mm_mul_ps(yyyy, mulVec0), yxwy),
                                 _mm_mul_ps(_mm_mul_ps(zzzz, mulVec1), zwxy)),
                      _mm_castsi128_ps(
                          _mm_shuffle_epi32(_mm_castps_si128(tmp7), 0x00))),
                  tmp7)),
          _mm_loadu_ps(reinterpret_cast<const float *>(matrix34.vec0)));

      if (static_cast<size_t>(transformIndex) >= cache.parents.size())
        break;
      transformIndex = cache.parents[static_cast<size_t>(transformIndex)];
    }

    Vector3 position(result.m128_f32[0], result.m128_f32[1],
                     result.m128_f32[2]);
    if (!position.IsValid() || position.Empty())
      continue;
    data.bones[bone] = position;
    data.mask |= (1u << bone);
  }

  if (!data.mask)
    return false;
  data.origin = origin;
  data.hasOrigin = true;
  cache.data = data;

  static bool loggedOnce = false;
  if (!loggedOnce) {
    loggedOnce = true;
    std::printf("[skeleton] bone transforms online\n");
    std::fflush(stdout);
  }
  return true;
}

} // namespace

void BeginTick() { g_budget = kMaxUpdatePerTick; }

bool Due(const Cache &cache, float distance, float speed, double nowMs) {
  if (distance > kMaxDistance)
    return false;
  const int interval = UpdateIntervalMs(distance, speed);
  return cache.lastUpdateMs <= 0.0 ||
         nowMs - cache.lastUpdateMs >= static_cast<double>(interval);
}

bool Update(uintptr_t player, const Vector3 &origin, float distance, float speed,
            double nowMs, Cache &cache) {
  if (!player || !Driver || !Driver->IsAttached())
    return false;
  if (g_budget <= 0 || !Due(cache, distance, speed, nowMs))
    return false;

  if (cache.player != player) {
    cache = Cache{};
    cache.player = player;
  }

  cache.lastUpdateMs = nowMs;
  --g_budget;

  using clock = std::chrono::steady_clock;
  const auto started = clock::now();
  bool ok = false;

  if (ResolveStep(player, cache)) {
    if (ReadBones(cache, origin)) {
      cache.readFails = 0;
      ok = true;
    } else if (++cache.readFails >= 8) {
      // A rebuilt player model can invalidate the cached transform pointers.
      // Keep the resolved bone refs for a while (transient driver failures are
      // common) and only do the expensive full re-resolve if misses persist.
      cache.readFails = 0;
      ResetResolution(cache);
    }
  }

  const auto elapsedUs =
      std::chrono::duration_cast<std::chrono::microseconds>(clock::now() -
                                                            started)
          .count();
  ++g_tryCount;
  if (ok)
    ++g_okCount;
  g_totalUs += static_cast<uint64_t>(elapsedUs);
  g_maxUs = std::max(g_maxUs, static_cast<uint64_t>(elapsedUs));

  const auto statsNow = clock::now();
  if (g_lastStats.time_since_epoch().count() == 0)
    g_lastStats = statsNow;
  if (statsNow - g_lastStats >= std::chrono::seconds(5)) {
    const double avg =
        g_tryCount ? static_cast<double>(g_totalUs) / g_tryCount : 0.0;
    if (avg > 30.0 || g_okCount * 4 < g_tryCount) {
      std::printf("[skeleton] 5s try=%llu ok=%llu avg=%.2fms max=%.1fms\n",
                  (unsigned long long)g_tryCount,
                  (unsigned long long)g_okCount, avg / 1000.0,
                  static_cast<double>(g_maxUs) / 1000.0);
      std::fflush(stdout);
    }
    g_tryCount = 0;
    g_okCount = 0;
    g_totalUs = 0;
    g_maxUs = 0;
    g_lastStats = statsNow;
  }

  return ok;
}

void Publish(uintptr_t address, const Data &data) {
  if (!address)
    return;
  std::lock_guard<std::mutex> lock(g_publishMutex);
  if (g_published.size() > 512)
    g_published.clear(); // dropped players can leave stale keys; bounded cache
  g_published[address] = data;
}

bool Get(uintptr_t address, Data &out) {
  std::lock_guard<std::mutex> lock(g_publishMutex);
  auto found = g_published.find(address);
  if (found == g_published.end())
    return false;
  out = found->second;
  return true;
}

void ClearPublished() {
  std::lock_guard<std::mutex> lock(g_publishMutex);
  g_published.clear();
}

} // namespace skeleton
