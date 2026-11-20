
// ================================================================================
// -----------------[ TELEGRAM SOURCE ]-----------------
// * BOT:     t.me/imguisource_bot
// * CHANNEL: t.me/ImGui_Source_FIXNEY (ImGui Source)
// * CH 2:    t.me/il2cpp_src (shit src 🧨)
// -----------------------------------------------------
// ================================================================================

#include "ImageEsp.h"

#include "Menu/Config.h"
#include "GetEntities/Prefabs/Prefabs.h"
#include "World2Screen/World2Screen.h"
#include "libs/render/Renderer.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <string>
#include <unordered_map>

namespace image_esp {
namespace {

constexpr float kIconSize = 16.f;
constexpr float kTextSize = 9.f;

std::wstring image_dir() {
  static std::wstring cached;
  static bool resolved = false;
  if (resolved)
    return cached;
  resolved = true;

  wchar_t env[32768]{};
  if (GetEnvironmentVariableW(L"VEXUS_IMG_DIR", env,
                              static_cast<DWORD>(std::size(env))) > 0)
    cached = env;

  if (cached.empty()) {
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const std::filesystem::path exe_dir =
        std::filesystem::path(exe).parent_path();

    const std::filesystem::path candidates[] = {
        exe_dir / L"rust",
        exe_dir / L".." / L".." / L"rust",
        exe_dir / L".." / L".." / L".." / L"rust",
        std::filesystem::current_path() / L"rust",
    };

    for (const auto &candidate : candidates) {
      std::error_code ec;
      const auto full = std::filesystem::weakly_canonical(candidate, ec);
      if (!ec && std::filesystem::is_directory(full, ec)) {
        cached = full.wstring();
        break;
      }
    }
  }

  if (cached.empty()) {
    std::printf("[image] rust image directory not found\n");
    return {};
  }
  std::printf("[image] icons: %ls\n", cached.c_str());
  return cached;
}

std::wstring image_path(const wchar_t *file) {
  const std::wstring dir = image_dir();
  if (dir.empty())
    return {};
  return (std::filesystem::path(dir) / file).wstring();
}

// Resolve each file once. ImageEsp::Render runs every frame; rebuilding a
// wstring + path for every icon was pure allocator churn. Keys are the literal
// pointers returned by file_for()/the player-icon call sites, so a lookup never
// constructs a temporary wstring either.
const std::wstring &cached_image_path(const wchar_t *file) {
  static std::unordered_map<const wchar_t *, std::wstring> cache;
  auto found = cache.find(file);
  if (found != cache.end())
    return found->second;
  auto inserted = cache.emplace(file, image_path(file));
  return inserted.first->second;
}

render::Vec2 cached_text_size(const char *text) {
  static std::unordered_map<std::string, render::Vec2> cache;
  auto found = cache.find(text);
  if (found != cache.end())
    return found->second;
  if (cache.size() >= 1024)
    cache.clear();
  const render::Vec2 size = render::measure_text(text, kTextSize);
  cache.emplace(text, size);
  return size;
}

const wchar_t *file_for(Prefabs::ImageKind kind) {
  switch (kind) {
  case Prefabs::ImageKind::Hemp: return L"clothhemp.png";
  case Prefabs::ImageKind::MetalOre: return L"metalore.png";
  case Prefabs::ImageKind::StoneOre: return L"stoneore.png";
  case Prefabs::ImageKind::SulfurOre: return L"sulfurore.png";

  case Prefabs::ImageKind::Bear: return L"bear.png";
  case Prefabs::ImageKind::Boar: return L"boar.png";
  case Prefabs::ImageKind::Bull: return L"bull.png";
  case Prefabs::ImageKind::Calf: return L"calf.png";
  case Prefabs::ImageKind::Chicken: return L"chicken.png";
  case Prefabs::ImageKind::Cow: return L"cow.png";
  case Prefabs::ImageKind::Crab: return L"crabs.png";
  case Prefabs::ImageKind::Crocodile: return L"crocodile.png";
  case Prefabs::ImageKind::Frog: return L"frog.png";
  case Prefabs::ImageKind::Horse: return L"horse.png";
  case Prefabs::ImageKind::Lamb: return L"lamb.png";
  case Prefabs::ImageKind::Minicopter: return L"minicopter.png";
  case Prefabs::ImageKind::Panther: return L"panther.png";
  case Prefabs::ImageKind::PolarBear: return L"polar-bear.png";
  case Prefabs::ImageKind::Rabbit: return L"rabbit.png";
  case Prefabs::ImageKind::Sheep: return L"sheep.png";
  case Prefabs::ImageKind::Snake: return L"snake.png";
  case Prefabs::ImageKind::Squirrel: return L"squirrel.png";
  case Prefabs::ImageKind::Stag: return L"stag.png";
  case Prefabs::ImageKind::Tiger: return L"tiger.png";
  case Prefabs::ImageKind::Wolf: return L"wolf.png";

  case Prefabs::ImageKind::CrateTools: return L"Crate Tools.png";
  case Prefabs::ImageKind::CrateFood: return L"Food Crate.png";
  case Prefabs::ImageKind::CrateNormal: return L"Normal Crate.png";
  case Prefabs::ImageKind::LootBarrel: return L"loot normal barrel.png";
  case Prefabs::ImageKind::OilBarrel: return L"OilBarrel.png";

  case Prefabs::ImageKind::HqmOre: return L"hqm-node.png";
  case Prefabs::ImageKind::Mushroom: return L"mushroom-collectable.png";
  case Prefabs::ImageKind::Potato: return L"potato-collectable.png";
  case Prefabs::ImageKind::Pumpkin: return L"pumpkin-collectable.png";
  case Prefabs::ImageKind::WoodPile: return L"wood-pile.png";
  case Prefabs::ImageKind::SupplyDrop: return L"supplydrop-supply-drop.png";
  case Prefabs::ImageKind::Scientist: return L"scientist.png";
  case Prefabs::ImageKind::Scarecrow: return L"scarecrow-npc.png";
  case Prefabs::ImageKind::AirwolfVendor: return L"airwolf-vendor.png";

  default: return nullptr;
  }
}

void draw_icon(const std::wstring &path, float x, float y, float distance) {
  if (path.empty())
    return;
  if (!render::draw_image(path, {x, y}, kIconSize)) {
    static std::wstring last_missing;
    if (path != last_missing) {
      last_missing = path;
      std::printf("[image] missing/unreadable: %ls\n", path.c_str());
    }
    return;
  }

  if (!g_cfg || !g_cfg->image_distance || distance < 0.f)
    return;

  char label[32];
  std::snprintf(label, sizeof(label), "%.0fm", distance);
  const render::Vec2 size = cached_text_size(label);
  render::text({x - size.x * 0.5f, y + 9.f}, label,
               render::rgba(240, 243, 249), kTextSize,
               render::TextStyle::Shadow);
}

} // namespace

void Preload() {
  int loaded = 0;
  int total = 0;
  const auto load = [&](const std::wstring &path) {
    ++total;
    if (render::preload_image(path))
      ++loaded;
  };

  load(cached_image_path(L"clothhemp.png"));
  for (int i = 1; i <= static_cast<int>(Prefabs::ImageKind::AirwolfVendor); ++i) {
    if (const wchar_t *file = file_for(static_cast<Prefabs::ImageKind>(i)))
      load(cached_image_path(file));
  }

  std::printf("[image] preloaded %d/%d icons\n", loaded, total);
}

void RenderTestIcons(float x, float y) {
  const wchar_t *files[] = {L"clothhemp.png", L"metalore.png", L"stoneore.png",
                            L"sulfurore.png", L"bear.png", L"minicopter.png"};
  for (int i = 0; i < 6; ++i)
    draw_icon(image_path(files[i]), x + i * 28.f, y, 42.f);
}

void Render(const std::vector<CachedEntity> &entities, uintptr_t local,
            const Vector3 &local_pos, bool have_local, float screen_w,
            float screen_h) {
  if (!g_cfg || !g_cfg->image_esp)
    return;

  for (const CachedEntity &entity : entities) {
    if (local && entity.address == local)
      continue;

    const Prefabs::Category category =
        (entity.category >= 0 && entity.category < Prefabs::kCategoryCount)
            ? static_cast<Prefabs::Category>(entity.category)
            : Prefabs::GetCategory(entity.prefabID);

    float distance = -1.f;
    if (have_local)
      distance = entity.position.Distance(local_pos);

    if (category == Prefabs::Category::Player)
      continue; // player art was removed; names/meters/skeleton only

    Prefabs::ImageKind kind = Prefabs::GetImageKind(entity.prefabID);
    if (kind == Prefabs::ImageKind::None && entity.className)
      kind = Prefabs::GetImageKindByClass(*entity.className);
    if (kind == Prefabs::ImageKind::None || !g_cfg->ImageEnabled(kind))
      continue;
    if (g_cfg->resource_max_dist > 0 && distance > g_cfg->resource_max_dist)
      continue;

    Vector2 point;
    if (!W2S::WorldToScreen(entity.position, point))
      continue;
    if (point.x < -40.f || point.y < -40.f || point.x > screen_w + 40.f ||
        point.y > screen_h + 40.f)
      continue;

    const wchar_t *file = file_for(kind);
    if (file)
      draw_icon(cached_image_path(file), point.x, point.y, distance);
  }
}

} // namespace image_esp
