
// ================================================================================
// Bot: @imguisource_bot | TG: @ImGui_Source_FIXNEY & @il2cpp_src
// ================================================================================

#include "Chams.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Driver/Rpm.h"
#include "GetEntities/Entities.h"
#include "GetEntities/Prefabs/Prefabs.h"
#include "Menu/Config.h"
#include "Offsets/Offsets.h"

extern BaseNetworkable *g_entities;

namespace chams {
namespace {

// Apply cadence. The material IDs persist in the renderer, so this only needs
// to re-check that the chain is still alive and re-apply after a model rebuild.
// Once a player is cached, a cycle is one 4-byte read per material slot.
constexpr auto kInterval = std::chrono::milliseconds(500);
constexpr auto kStevenInterval = std::chrono::milliseconds(100);
constexpr uint32_t kMaxRenderers = 64;
constexpr uint32_t kMaxMaterials = 8;
constexpr uint32_t kMaxCapacity = 64;

// Curated material IDs for the 2026-11-06 Rust dump (new build, game update
// 2026-11-07). Material IDs are build-specific: every value below comes from
// the dump's Materials section, so replacing the dump requires re-checking
// these names.
constexpr int kMaterialIds[kPresetCount] = {
    0x1D4D8, // kGreenGlow (120024)  - greenglow (tested working)
    0x7F4C2, // kRedGlow (521410)    - plainlight_red_pure (safe red)
    0x885DA, // kNeon (558554)       - plainlight_blue (safe neon-blue)
    0x38394, // kBody (230292)       - body
    0x79826, // kManpadCrt (497702)  - manpad_crt_hdr (through walls)
    0xBD4C,  // kDefaultMat (48460)  - default_material (guaranteed loaded)
    0x00000, // kSteven (computed per player: green visible / red hidden)
};

// Steven uses the X-Ray plainlight pair (both are in the public X-Ray chams
// material list); GreenGlow/RedGlow are still available as their own presets.
constexpr uint32_t kStevenVisibleMaterial = 0x386F8; // plainlight_green_hid
constexpr uint32_t kStevenHiddenMaterial = 0x7F4C2;  // plainlight_red_pure

struct Slot {
  uintptr_t address = 0; // address of the 4-byte material ID
  uint32_t original = 0; // last non-chams value (used to restore)
  uint32_t applied = 0;  // value we wrote (0 = never written)
};

struct PlayerCache {
  uintptr_t model = 0;
  uintptr_t modelOffset = 0;
  uintptr_t multiMesh = 0;
  uintptr_t multiMeshOffset = 0;
  uintptr_t rendererArray = 0;
  uint32_t rendererCount = 0;
  double builtMs = 0.0;
  uint64_t seenCycle = 0;
  int visSource = -1; // cached visibility-field candidate (see ReadVisibility)
  int preset = -1;
  std::vector<Slot> slots;
};

struct DynamicArray {
  uint64_t base = 0;
  uint64_t memId = 0;
  uint64_t size = 0;
  uint64_t capacity = 0;
};

std::atomic<bool> g_started{false};
std::atomic<bool> g_run{false};
std::thread g_thread;
uint64_t g_cycle = 0;

// Worker-thread only.
std::unordered_map<uintptr_t, PlayerCache> g_cache;
bool g_restored = true;
int g_activePreset = -1;

bool ValidPtr(uintptr_t p) { return p >= 0x10000; }

uintptr_t ReadPtr(uintptr_t address) {
  return Driver->Read<uintptr_t>(address);
}

// ── Steven visibility ────────────────────────────────────────────────────────
// Rust keeps a per-player-model "visible" bool which the client visibility /
// occlusion update maintains. The 26480361 dump puts PlayerModel::visible at
// 0xCC. BaseEntity+0x1A1 (just before bounds 0x1A4) is the fallback. Only clean
// 0/1 bytes are used.
constexpr uintptr_t kModelVisOffset = 0xCC;
constexpr uintptr_t kEntityVisOffset = 0x1A1;

const char *VisSourceName(int source) {
  switch (source) {
  case 0: return "PM+0xCC";
  case 1: return "BE+0x1A1";
  default: return "none";
  }
}

bool ReadVisibilityByte(uintptr_t address, bool &visible) {
  uint8_t value = 0;
  if (!Driver->ReadBytesInto(address, &value, sizeof(value)))
    return false;
  if (value > 1)
    return false;
  visible = value != 0;
  return true;
}

int ReadRawVisibilityByte(uintptr_t address) {
  uint8_t value = 0;
  if (!Driver->ReadBytesInto(address, &value, sizeof(value)))
    return -1;
  return static_cast<int>(value);
}

bool ReadVisibility(uintptr_t player, uintptr_t model, int &source,
                    bool &visible) {
  // Fast path: keep using the field that worked for this player last cycle.
  if (source == 0 && ValidPtr(model) &&
      ReadVisibilityByte(model + kModelVisOffset, visible))
    return true;
  if (source == 1 && ReadVisibilityByte(player + kEntityVisOffset, visible))
    return true;

  source = -1;
  if (ValidPtr(model) &&
      ReadVisibilityByte(model + kModelVisOffset, visible)) {
    source = 0;
    return true;
  }
  if (ReadVisibilityByte(player + kEntityVisOffset, visible)) {
    source = 1;
    return true;
  }
  return false;
}

// The native renderer stores its material-ID array as:
//   { uint64_t base; uint64_t mem_id; uint64_t size; uint64_t capacity; }
// Current dump has the array at +0x140; older builds used +0x148. Try both and
// accept only an array whose IDs look like real materials.
bool ReadMaterialArray(uintptr_t address, uintptr_t &base, uint32_t &count) {
  DynamicArray arr{};
  if (!Driver->ReadBytesInto(address, &arr, sizeof(arr)))
    return false;
  if (!ValidPtr(arr.base) || (arr.base & 3u) != 0)
    return false;
  if (arr.size == 0 || arr.size > kMaxMaterials)
    return false;
  if (arr.capacity != 0 &&
      (arr.capacity < arr.size || arr.capacity > kMaxCapacity))
    return false;
  if (arr.base + arr.size * 4ull < arr.base)
    return false;
  base = static_cast<uintptr_t>(arr.base);
  count = static_cast<uint32_t>(arr.size);
  return true;
}

bool ReadMaterialIds(uintptr_t native, uintptr_t &base, uint32_t &count,
                     uint32_t *ids) {
  const uintptr_t arrayOffsets[2] = {0x140, 0x148};
  for (uintptr_t offset : arrayOffsets) {
    uintptr_t candidateBase = 0;
    uint32_t candidateCount = 0;
    if (!ReadMaterialArray(native + offset, candidateBase, candidateCount))
      continue;
    uint32_t candidateIds[kMaxMaterials]{};
    if (!Driver->ReadBytesInto(candidateBase, candidateIds,
                               static_cast<size_t>(candidateCount) *
                                   sizeof(uint32_t)))
      continue;
    bool plausible = false;
    for (uint32_t i = 0; i < candidateCount; ++i) {
      const uint32_t id = candidateIds[i];
      if (id != 0 && (id <= 10000000u || id >= 0xFFF00000u)) {
        plausible = true;
        break;
      }
    }
    if (!plausible)
      continue;
    base = candidateBase;
    count = candidateCount;
    for (uint32_t i = 0; i < candidateCount; ++i)
      ids[i] = candidateIds[i];
    return true;
  }
  return false;
}

// SkinnedMultiMesh.Renderers is a List<Renderer>:
//   +0x10 -> il2cpp array object
//   +0x18 -> count
// The array's elements start at +0x20 (klass+monitor+bounds+length header).
bool ReadRenderers(uintptr_t multiMesh, uintptr_t &arrayOut, uint32_t &countOut) {
  const uintptr_t listOffsets[3] = {resources::SkinnedMultiMesh::Renderers,
                                    0x78,
                                    SkinnedMultiMesh_Offsets::Renderers};
  for (uintptr_t offset : listOffsets) {
    const uintptr_t list = ReadPtr(multiMesh + offset);
    if (!ValidPtr(list))
      continue;
    const uintptr_t array = ReadPtr(list + 0x10);
    const int32_t count = Driver->ReadInt32(list + 0x18);
    if (!ValidPtr(array) || count <= 0 ||
        count > static_cast<int32_t>(kMaxRenderers))
      continue;
    const int32_t arrayLength = Driver->ReadInt32(array + 0x18);
    if (arrayLength < count || arrayLength > 512)
      continue;
    arrayOut = array;
    countOut = static_cast<uint32_t>(count);
    return true;
  }
  return false;
}

bool Build(uintptr_t player, PlayerCache &cache, double nowMs,
           const PlayerCache *previous = nullptr) {
  cache = PlayerCache{};

  // Rust changed the PlayerModel component offset across builds. Try the dump
  // value first, then the public fallbacks; each candidate must resolve all the
  // way to a renderer material array before it is accepted.
  const uintptr_t modelOffsets[3] = {BasePlayer_Offsets::playerModel,
                                     0x6F0, 0x420};
  const uintptr_t multiMeshOffsets[4] = {resources::PlayerModel::_multiMesh,
                                         0x448,
                                         PlayerModel_Offsets::_multiMesh,
                                         0x3D8};
  uintptr_t model = 0;
  uintptr_t modelOffset = 0;
  uintptr_t multiMesh = 0;
  uintptr_t multiMeshOffset = 0;
  uintptr_t rendererArray = 0;
  uint32_t rendererCount = 0;
  for (uintptr_t candidateModelOffset : modelOffsets) {
    const uintptr_t candidateModel = ReadPtr(player + candidateModelOffset);
    if (!ValidPtr(candidateModel))
      continue;
    for (uintptr_t multiOffset : multiMeshOffsets) {
      const uintptr_t candidateMesh = ReadPtr(candidateModel + multiOffset);
      if (!ValidPtr(candidateMesh))
        continue;
      if (ReadRenderers(candidateMesh, rendererArray, rendererCount)) {
        model = candidateModel;
        modelOffset = candidateModelOffset;
        multiMesh = candidateMesh;
        multiMeshOffset = multiOffset;
        break;
      }
    }
    if (ValidPtr(multiMesh))
      break;
  }
  if (!ValidPtr(model) || !ValidPtr(multiMesh))
    return false;

  std::vector<Slot> slots;
  for (uint32_t i = 0; i < rendererCount; ++i) {
    const uintptr_t renderer =
        ReadPtr(rendererArray + 0x28 + static_cast<uintptr_t>(i) * 8);
    if (!ValidPtr(renderer))
      continue;

    // Managed Renderer -> native UnityEngine renderer.
    const uintptr_t native = ReadPtr(renderer + 0x10);
    if (!ValidPtr(native))
      continue;

    uintptr_t materialBase = 0;
    uint32_t materialCount = 0;
    uint32_t ids[kMaxMaterials]{};
    if (!ReadMaterialIds(native, materialBase, materialCount, ids))
      continue;

    for (uint32_t j = 0; j < materialCount; ++j) {
      Slot slot;
      slot.address = materialBase + static_cast<uintptr_t>(j) * 4;
      slot.original = ids[j];
      if (previous) {
        // Preserve the pre-chams value when the material array address is
        // unchanged. Without this a periodic re-resolve would forget what the
        // original material was, and disabling chams could not restore it.
        for (const Slot &old : previous->slots) {
          if (old.address == slot.address) {
            slot.original = old.original;
            slot.applied = old.applied;
            break;
          }
        }
      }
      slots.push_back(slot);
    }
  }

  if (slots.empty())
    return false;

  cache.model = model;
  cache.modelOffset = modelOffset;
  cache.multiMesh = multiMesh;
  cache.multiMeshOffset = multiMeshOffset;
  cache.rendererArray = rendererArray;
  cache.rendererCount = rendererCount;
  cache.builtMs = nowMs;
  cache.slots = std::move(slots);
  return true;
}

// Returns the number of material IDs written this cycle. For Steven,
// `visibleOut`/`visSourceOut` describe the per-player visibility decision.
int ApplyToPlayer(uintptr_t player, int preset, double nowMs, bool &visibleOut,
                  int &visSourceOut) {
  visibleOut = true;
  visSourceOut = -1;

  auto found = g_cache.find(player);
  bool rebuild = found == g_cache.end();
  if (!rebuild) {
    PlayerCache &cache = found->second;
    const uintptr_t model = ReadPtr(player + cache.modelOffset);
    const uintptr_t multiMesh =
        ValidPtr(model) ? ReadPtr(model + cache.multiMeshOffset) : 0;
    uintptr_t rendererArray = 0;
    uint32_t rendererCount = 0;
    rebuild = !ValidPtr(model) || model != cache.model ||
              !ValidPtr(multiMesh) || multiMesh != cache.multiMesh ||
              cache.builtMs <= 0.0 || cache.preset != preset;
    if (!rebuild) {
      // A clothing/model rebuild replaces the renderer list. Re-read the cheap
      // list header every cycle so we notice and rebuild instead of writing to
      // an array the renderer no longer uses.
      rebuild = !ReadRenderers(cache.multiMesh, rendererArray, rendererCount) ||
                rendererArray != cache.rendererArray ||
                rendererCount != cache.rendererCount;
    }
  }

  if (rebuild) {
    PlayerCache fresh;
    const PlayerCache *previous =
        found == g_cache.end() ? nullptr : &found->second;
    if (!Build(player, fresh, nowMs, previous)) {
      // A failed rebuild is usually transient (model/renderer being rebuilt).
      // Keep the previous cache when it still belongs to the same player model;
      // drop it only when the model pointer really changed, because its material
      // arrays no longer belong to this player.
      if (found != g_cache.end()) {
        const uintptr_t currentModel =
            ReadPtr(player + found->second.modelOffset);
        if (found->second.model == currentModel)
          found->second.builtMs = nowMs; // retry the rebuild next cycle
        else
          g_cache.erase(found);
      }
      return 0;
    }
    fresh.preset = preset;
    g_cache[player] = std::move(fresh);
    found = g_cache.find(player);
  }

  PlayerCache &cache = found->second;
  cache.seenCycle = g_cycle;

  uint32_t materialId = static_cast<uint32_t>(kMaterialIds[preset]);
  if (preset == kSteven) {
    bool visible = true;
    int source = cache.visSource;
    if (ReadVisibility(player, cache.model, source, visible)) {
      cache.visSource = source;
      visibleOut = visible;
      visSourceOut = source;
    } else {
      // If the game's visibility field cannot be read, fail open (green) so
      // the player is never hidden by a failed probe.
      visibleOut = true;
      visSourceOut = -1;
    }
    const bool useVisible =
        (g_cfg && g_cfg->steven_invert) ? !visibleOut : visibleOut;
    materialId = useVisible ? kStevenVisibleMaterial : kStevenHiddenMaterial;
  }

  int writes = 0;
  for (Slot &slot : cache.slots) {
    uint32_t current = 0;
    if (!Driver->ReadBytesInto(slot.address, &current, sizeof(current))) {
      // The model was rebuilt and this material array is gone. Rebuild the
      // whole chain on the next cycle; never write through a dead pointer.
      cache.builtMs = 0.0;
      return writes;
    }
    if (current == materialId) {
      slot.applied = materialId;
      continue;
    }
    // A material ID is a small positive asset id (or a negative editor id).
    // Anything in the 10M..4.29B range is almost certainly a pointer fragment,
    // meaning the array moved or the object was freed. Do not write through it.
    if (current == 0 ||
        (current > 10000000u && current < 0xFFF00000u)) {
      cache.builtMs = 0.0;
      return writes;
    }
    // Steven flips green<->red on the same slot. If the slot still contains the
    // colour we wrote last time, keep the true pre-chams material for restore;
    // only a game-side material change replaces `original`.
    const bool ourColour = slot.applied != 0 && current == slot.applied;
    if (!ourColour)
      slot.original = current;
    if (Driver->WriteUInt32(slot.address, materialId)) {
      slot.applied = materialId;
      ++writes;
    }
  }
  return writes;
}

size_t RestorePlayer(PlayerCache &cache) {
  size_t restored = 0;
  for (Slot &slot : cache.slots) {
    if (!slot.applied)
      continue;
    uint32_t current = 0;
    if (!Driver->ReadBytesInto(slot.address, &current, sizeof(current)))
      continue;
    // Only undo our own write. If the game rebuilt this material array, the
    // old "original" belongs to a different object and must not be written.
    if (current != slot.applied || slot.original == slot.applied)
      continue;
    if (Driver->WriteUInt32(slot.address, slot.original))
      ++restored;
    slot.applied = 0;
  }
  return restored;
}

void RestoreAll() {
  size_t restored = 0;
  for (auto &entry : g_cache)
    restored += RestorePlayer(entry.second);
  g_cache.clear();
  g_activePreset = -1;
  g_restored = true;
  if (restored)
    std::printf("[chams] restored %zu material slots\n", restored);
  std::fflush(stdout);
}

void Worker() {
  Driver->SelectSlot(3);
  double lastLogMs = 0.0;
  size_t lastWrites = 0;

  while (g_run.load(std::memory_order_acquire)) {
    const auto started = std::chrono::steady_clock::now();
    const double nowMs =
        std::chrono::duration<double, std::milli>(started.time_since_epoch())
            .count();

    const bool enabled = g_cfg && g_cfg->chams;
    if (!enabled) {
      if (!g_restored)
        RestoreAll();
      lastLogMs = 0.0;
      lastWrites = 0;
    } else {
      int preset = g_cfg->chams_material;
      if (preset < 0 || preset >= kPresetCount)
        preset = kDefaultPreset;
      if (preset != g_activePreset) {
        if (!g_restored)
          RestoreAll();
        g_activePreset = preset;
        g_restored = false;
        if (preset == kSteven) {
          std::printf("[chams] enabled preset=6 steven "
                      "(visible=green hidden=red) dump=2026-11-06\n");
        } else {
          std::printf("[chams] enabled preset=%d material=0x%X (%d) "
                      "dump=2026-11-06\n",
                      preset, kMaterialIds[preset], kMaterialIds[preset]);
        }
        std::fflush(stdout);
      }

      size_t playerCount = 0;
      size_t writes = 0;
      size_t visibleCount = 0;
      size_t hiddenCount = 0;
      int firstVisSource = -1;
      uintptr_t firstPlayer = 0;
      ++g_cycle;
      auto snapshot = g_entities ? g_entities->Snapshot() : nullptr;
      if (snapshot) {
        const uintptr_t local = g_entities->GetLocalPlayer();
        for (const auto &entity : *snapshot) {
          if (!entity.address || entity.address == local)
            continue;
          if (entity.category !=
              static_cast<int>(Prefabs::Category::Player))
            continue;
          if (g_cfg->hide_sleepers && entity.sleeping)
            continue;
          if (!firstPlayer)
            firstPlayer = entity.address;
          bool visible = true;
          int visSource = -1;
          writes += ApplyToPlayer(entity.address, preset, nowMs, visible,
                                  visSource);
          if (preset == kSteven) {
            if (visible)
              ++visibleCount;
            else
              ++hiddenCount;
            if (visSource >= 0 && firstVisSource < 0)
              firstVisSource = visSource;
          }
          ++playerCount;
        }

        // Players that dropped out of the snapshot (or are now hidden as
        // sleepers) must not keep their chams. Restore and forget them.
        for (auto it = g_cache.begin(); it != g_cache.end();) {
          if (it->second.seenCycle == g_cycle) {
            ++it;
            continue;
          }
          RestorePlayer(it->second);
          it = g_cache.erase(it);
        }
      }

      if (lastLogMs == 0.0 || nowMs - lastLogMs >= 5000.0 ||
          (writes > 0 && lastWrites == 0)) {
        lastLogMs = nowMs;
        lastWrites = writes;
        size_t slotCount = 0;
        for (const auto &entry : g_cache)
          slotCount += entry.second.slots.size();
        if (preset == kSteven) {
          std::printf("[chams] steven players=%zu visible=%zu hidden=%zu "
                      "src=%s invert=%d cached=%zu slots=%zu writes=%zu\n",
                      playerCount, visibleCount, hiddenCount,
                      VisSourceName(firstVisSource),
                      (g_cfg && g_cfg->steven_invert) ? 1 : 0,
                      g_cache.size(), slotCount, writes);
          if (firstPlayer) {
            const uintptr_t model =
                ReadPtr(firstPlayer + BasePlayer_Offsets::playerModel);
            const uintptr_t base = ValidPtr(model) ? model : firstPlayer;
            const bool modelValid = ValidPtr(model);
            const uintptr_t offsets[3] = {
                modelValid ? model + kModelVisOffset : 0,
                modelValid ? model + 0x6D8 : 0,   // PlayerModel::isNpc
                firstPlayer + kEntityVisOffset};
            std::printf("[chams] steven probe pm_vis=%d pm_npc=%d "
                        "be_vis=%d (base=0x%llX)\n",
                        offsets[0] ? ReadRawVisibilityByte(offsets[0]) : -1,
                        offsets[1] ? ReadRawVisibilityByte(offsets[1]) : -1,
                        ReadRawVisibilityByte(offsets[2]),
                        (unsigned long long)base);
          }
        } else {
          std::printf("[chams] players=%zu cached=%zu slots=%zu writes=%zu "
                      "material=%d\n",
                      playerCount, g_cache.size(), slotCount, writes,
                      kMaterialIds[preset]);
        }
        if (playerCount > 0 && g_cache.empty())
          std::printf("[chams] no renderer chain resolved - offsets may not "
                      "match this game build\n");
        std::fflush(stdout);
      }
    }

    const auto elapsed = std::chrono::steady_clock::now() - started;
    const auto interval =
        (g_activePreset == kSteven) ? kStevenInterval : kInterval;
    const auto sleepFor =
        elapsed < interval ? interval - elapsed : std::chrono::milliseconds(1);
    std::this_thread::sleep_for(sleepFor);
  }

  // Never leave a player's materials modified when the cheat exits.
  if (!g_restored)
    RestoreAll();
}

} // namespace

int MaterialIdForPreset(int preset) {
  if (preset < 0 || preset >= kPresetCount)
    return kMaterialIds[kDefaultPreset];
  return kMaterialIds[preset];
}

void Start() {
  if (g_started.exchange(true))
    return;
  g_run.store(true, std::memory_order_release);
  g_restored = true;
  g_activePreset = -1;
  g_thread = std::thread(Worker);
}

void Stop() {
  if (!g_started.exchange(false))
    return;
  g_run.store(false, std::memory_order_release);
  if (g_thread.joinable())
    g_thread.join();
  // Worker restores on its way out; this covers a start/stop with no tick.
  if (!g_restored)
    RestoreAll();
}

} // namespace chams
