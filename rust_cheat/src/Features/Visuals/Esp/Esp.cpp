
// ================================================================================
// Package distributed via @imguisource_bot
// Channel ImGui Source: @ImGui_Source_FIXNEY
// Channel shit src 🧨: @il2cpp_src
// ================================================================================

#include "Esp.h"
#include "EspStats.h"
#include "ImageEsp.h"
#include "libs/overlay/Overlay.h"
#include "libs/render/Renderer.h"
#include "../../../GetEntities/Entities.h"
#include "../../../GetEntities/GhostClock.h"
#include "../../../GetEntities/Prefabs/Prefabs.h"
#include "../../../GetEntities/Skeleton.h"
#include "../../../Menu/Config.h"
#include "../../../World2Screen/World2Screen.h"
#include "../../../Diag/Profiler.h"

#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>

extern BaseNetworkable* g_entities;

// ─────────────────────────────────────────────────────────────────────────────
//  ESP render (trimmed + cached).
//
//  Players keep their body box + label. EVERYTHING else - animals, ores,
//  plants, crates, barrels, loot, corpses, drops, vehicles, deployables,
//  traps, doors, batteries - draws as just a name + metres, no rectangles at
//  all (box drawing was the single biggest per-frame vertex cost on
//  resource-heavy servers).
//
//  Drawing goes through the Direct2D renderer (libs/render); no UI library.
// ─────────────────────────────────────────────────────────────────────────────

namespace esp {

namespace {

// Records how long one Render() pass took, on every exit path (published for
// the [cache] heartbeat). Trivial cost: two clock reads per frame.
struct RenderTimer {
  std::chrono::steady_clock::time_point t0;
  RenderTimer() : t0(std::chrono::steady_clock::now()) {}
  ~RenderTimer() {
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
    espstats::renderUs.store(static_cast<uint32_t>(us),
                             std::memory_order_relaxed);
  }
};

static inline render::Color Col(const float c[4]) {
  return render::from_float(c);
}

// ESP text is drawn a little smaller than the menu font.
static constexpr float kFontSize = 12.f;

static render::Vec2 MeasureTextSize(const char *text) {
  return render::measure_text(text, kFontSize);
}

// Transparent string hasher so the text-size cache can be looked up by
// string_view / const char* without ever constructing a std::string.
struct StringHash {
  using is_transparent = void;
  size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
};

// Memoised text measurement. The font and scale are constant for the whole
// frame, so a given string always measures the same; caching by content removes
// the per-label DirectWrite layout allocation that otherwise runs for every
// label, every frame.
static render::Vec2 CachedTextSize(const char *text) {
  if (!text || !*text)
    return {};
  static std::unordered_map<std::string, render::Vec2, StringHash,
                            std::equal_to<>>
      cache;
  auto it = cache.find(std::string_view(text));
  if (it != cache.end())
    return it->second;
  const render::Vec2 v = MeasureTextSize(text);
  if (cache.size() >= 4096)
    cache.clear();
  cache.emplace(std::string(text), v);
  return v;
}

// Label text style. The 4-way outline is 5 glyph runs per label, so it is
// reserved for the few, important character labels; the many resource/object
// labels use a single 1px shadow - visually almost identical and cheaper.
enum TextStyle { TEXT_PLAIN = 0, TEXT_SHADOW = 1, TEXT_OUTLINE = 2 };

static render::TextStyle ToRenderStyle(int style) {
  if (style == TEXT_OUTLINE)
    return render::TextStyle::Outline;
  if (style == TEXT_SHADOW)
    return render::TextStyle::Shadow;
  return render::TextStyle::Plain;
}

static void DrawOutlinedText(const render::Vec2 &pos, render::Color col,
                             const char *text, int style) {
  render::text(pos, text, col, kFontSize, ToRenderStyle(style));
}

static void DrawCornerBox(const render::Vec2 &a, const render::Vec2 &b,
                          render::Color col, float t) {
  const float w = b.x - a.x, h = b.y - a.y;
  const float lx = w * 0.25f, ly = h * 0.25f;
  render::line({a.x, a.y}, {a.x + lx, a.y}, col, t);
  render::line({a.x, a.y}, {a.x, a.y + ly}, col, t);
  render::line({b.x, a.y}, {b.x - lx, a.y}, col, t);
  render::line({b.x, a.y}, {b.x, a.y + ly}, col, t);
  render::line({a.x, b.y}, {a.x + lx, b.y}, col, t);
  render::line({a.x, b.y}, {a.x, b.y - ly}, col, t);
  render::line({b.x, b.y}, {b.x - lx, b.y}, col, t);
  render::line({b.x, b.y}, {b.x, b.y - ly}, col, t);
}

// Compose a label into the caller's stack buffer (no heap allocation): either
// "name", "dist", "name  dist", or nothing. Returns the buffer.
static const char *BuildLabel(char *buf, size_t n, const char *name, float dist,
                              bool wantName, bool wantDist) {
  const bool haveName = wantName && name && *name;
  const bool haveDist = wantDist && dist >= 0.f;
  if (haveName && haveDist)
    snprintf(buf, n, "%s  %.0fm", name, dist);
  else if (haveName)
    snprintf(buf, n, "%s", name);
  else if (haveDist)
    snprintf(buf, n, "%.0fm", dist);
  else
    buf[0] = '\0';
  return buf;
}

// Player skeleton connections (bone index pairs). Head/neck/spine and the
// four limbs, matching the bone data cached by the entity worker.
struct SkeletonLink {
  int a;
  int b;
};
constexpr SkeletonLink kSkeletonLinks[] = {
    {skeleton::Head, skeleton::Neck},
    {skeleton::Neck, skeleton::Spine4},
    {skeleton::Spine4, skeleton::Spine3},
    {skeleton::Spine3, skeleton::Spine2},
    {skeleton::Spine2, skeleton::Spine1},
    {skeleton::Spine1, skeleton::Pelvis},
    {skeleton::Spine4, skeleton::LClavicle},
    {skeleton::LClavicle, skeleton::LUpperArm},
    {skeleton::LUpperArm, skeleton::LForearm},
    {skeleton::LForearm, skeleton::LHand},
    {skeleton::Spine4, skeleton::RClavicle},
    {skeleton::RClavicle, skeleton::RUpperArm},
    {skeleton::RUpperArm, skeleton::RForearm},
    {skeleton::RForearm, skeleton::RHand},
    {skeleton::Pelvis, skeleton::LHip},
    {skeleton::LHip, skeleton::LKnee},
    {skeleton::LKnee, skeleton::LFoot},
    {skeleton::Pelvis, skeleton::RHip},
    {skeleton::RHip, skeleton::RKnee},
    {skeleton::RKnee, skeleton::RFoot},
};

// Player box width as a fraction of its on-screen height. The box top is the
// head and the bottom is the feet, so the box is exactly the player's height;
// this ratio sets how wide it is (a player is taller than wide, so < 1).
static constexpr float kPlayerBoxAspect = 0.45f;

// Resolve the effective box style for a category (per-category override wins).
static int ResolveBoxStyle(const CategoryConfig &cc) {
  if (cc.box_style > 0 && (cc.box_style - 1) < BOX_STYLE_COUNT)
    return cc.box_style - 1;
  return g_cfg->box_style;
}

// ── last-known-position markers (players only) ──────────────────────────────
static constexpr float kGhostPredictCap = 1.2f;

static void RenderGhosts(uintptr_t local, const Vector3 &localPos, bool haveLocal,
                         float sw, float sh, float sw2, float sh2,
                         int labelStyle, float thick) {
  if (!g_cfg || g_cfg->ghost_time_ms <= 0 || !g_entities)
    return;

  const CategoryConfig &cc = g_cfg->Cat(Prefabs::Category::Player);
  if (!cc.enabled)
    return;

  auto ghosts = g_entities->Ghosts();
  if (!ghosts || ghosts->empty())
    return;

  const double ttlMs = (double)g_cfg->ghost_time_ms;
  const double nowMs = GhostNowMs();
  const int boxStyle = ResolveBoxStyle(cc);

  for (const auto &g : *ghosts) {
    if (g.address && local && g.address == local)
      continue;
    if (g_cfg->hide_sleepers && g.sleeping)
      continue;

    const double ageMs = nowMs - g.lastSeenMs;
    if (ageMs < 0.0 || ageMs > ttlMs)
      continue;
    const float fade = (float)(1.0 - (ageMs / ttlMs));

    Vector3 wp = g.position;
    if (g_cfg->ghost_predict) {
      float t = (float)(ageMs * 0.001);
      if (t > kGhostPredictCap)
        t = kGhostPredictCap;
      wp.x += g.velocity.x * t;
      wp.y += g.velocity.y * t;
      wp.z += g.velocity.z * t;
    }

    float dist = -1.f;
    if (haveLocal) {
      dist = wp.Distance(localPos);
      const int cap = g_cfg->player_max_dist;
      if (cap > 0 && dist > (float)cap)
        continue;
    }

    const Vector3 headWorld{wp.x, wp.y + 1.9f, wp.z};
    Vector2 feet, head;
    if (!W2S::WorldToScreen(wp, feet))
      continue;
    if (!W2S::WorldToScreen(headWorld, head)) {
      head.x = feet.x;
      head.y = feet.y - 40.f;
    }
    if (feet.x < -80.f || feet.y < -80.f || feet.x > sw + 80.f ||
        feet.y > sh + 80.f)
      continue;

    const render::Color gcol =
        render::from_float(g_cfg->ghost_color, fade);

    float top = head.y, bot = feet.y;
    if (bot < top) {
      const float t = top;
      top = bot;
      bot = t;
    }
    float bh = bot - top;
    if (bh < 3.f)
      bh = 3.f;
    const float bw = bh * kPlayerBoxAspect;
    const float cx = (feet.x + head.x) * 0.5f;
    const render::Vec2 a(cx - bw * 0.5f, top), b(cx + bw * 0.5f, bot);

    if (cc.draw_box) {
      switch (boxStyle) {
      case BOX_2D:
        render::rect(a, b, gcol, thick);
        break;
      case BOX_CORNER:
        DrawCornerBox(a, b, gcol, thick);
        break;
      case BOX_FILLED: {
        render::fill_rect(a, b, render::with_alpha(gcol, 0.16f * fade));
        render::rect(a, b, gcol, thick);
        break;
      }
      default:
        break;
      }
    }

    espstats::gDraw.fetch_add(1, std::memory_order_relaxed);

    const bool wantNames = g_cfg->esp_names && cc.draw_label;
    const bool wantMeters = g_cfg->esp_meters && cc.draw_distance;
    if (wantNames || wantMeters) {
      char seen[96];
      snprintf(seen, sizeof(seen), "Last seen %.1fs", ageMs * 0.001f);
      char lbuf[192];
      const char *txt =
          BuildLabel(lbuf, sizeof(lbuf), seen, dist, wantNames, wantMeters);
      if (txt && *txt) {
        const render::Vec2 ts = CachedTextSize(txt);
        DrawOutlinedText({cx - ts.x * 0.5f, a.y - ts.y - 2.f}, gcol, txt,
                         labelStyle);
      }
    }

    if (g_cfg->snapline) {
      const float sy = (g_cfg->snapline_pos == 0) ? 0.f
                       : (g_cfg->snapline_pos == 1) ? sh2 : sh;
      render::line({sw2, sy}, {cx, b.y}, gcol, thick);
    }
  }
}

} // namespace

void Render() {
  RenderTimer _rtime;

  // Rate-limited reason reporting: when nothing draws, this names the stage
  // that stopped the pass instead of leaving a blank overlay with no clue.
  static const char *last_reason = nullptr;
  static std::chrono::steady_clock::time_point last_report{};
  const auto report = [&](const char *reason) {
    const auto now = std::chrono::steady_clock::now();
    if (reason == last_reason && now - last_report < std::chrono::seconds(5))
      return;
    last_reason = reason;
    last_report = now;
    std::printf("[esp] paused: %s\n", reason);
    std::fflush(stdout);
  };

  if (!g_cfg) {
    report("config missing");
    return;
  }
  const bool normal_esp = g_cfg->esp_enabled;
  if (!normal_esp && !g_cfg->image_esp) {
    report("master ESP and Image ESP are off");
    return;
  }
  if (!overlay) {
    report("overlay missing");
    return;
  }
  if (!g_entities) {
    report("entity system not created");
    return;
  }
  if (!W2S::PrepareMatrix()) {
    report("view matrix / camera not ready");
    return;
  }

  const uintptr_t local = g_entities->GetLocalPlayer();
  auto snapshot = g_entities->Snapshot();
  const uint64_t snapshotPublishUs = g_entities->SnapshotPublishUs();
  if (snapshotPublishUs)
    prof::snapshotAge.Add(prof::NowUs() - snapshotPublishUs);
  const uint64_t matrixPublishUs = W2S::PublishedUs();
  if (matrixPublishUs)
    prof::matrixAge.Add(prof::NowUs() - matrixPublishUs);
  if (!snapshot) {
    report("entity snapshot unavailable");
    return;
  }
  if (snapshot->empty()) {
    report("entity snapshot empty");
    return;
  }
  last_reason = nullptr;

  // Distance origin for the draw-distance filter + the distance labels.
  Vector3 localPos{};
  bool haveLocal = g_entities->GetLocalPlayerPos(localPos);
  const bool haveCam = W2S::HaveCameraPos();
  Vector3 camPos{};
  if (haveCam)
    camPos = W2S::GetCameraPos();
  if (!haveLocal && haveCam) {
    localPos = camPos;
    haveLocal = true;
  } else if (haveLocal && haveCam && localPos.Distance(camPos) > 15.f) {
    // Mounted players (helicopter/train) can expose a seat/local transform.
    localPos = camPos;
  }
  if (!haveLocal && local) {
    for (const auto &e : *snapshot) {
      if (e.address == local && !e.position.Empty()) {
        localPos = e.position;
        haveLocal = true;
        break;
      }
    }
  }
  if (!haveLocal) {
    static bool warned = false;
    if (!warned) {
      warned = true;
      printf("[esp] no distance origin (camera+local missing) - range filter "
             "off\n");
    }
  }

  const float sw =
      W2S::g_ScreenW > 0.f ? W2S::g_ScreenW : overlay->window_width;
  const float sh =
      W2S::g_ScreenH > 0.f ? W2S::g_ScreenH : overlay->window_height;
  const float sw2 = sw * 0.5f, sh2 = sh * 0.5f;
  const double drawNowMs = GhostNowMs();

  // ── everything stable for the whole frame is hoisted / precomputed once ──
  const bool outline = g_cfg->outline;
  const int charStyle = outline ? TEXT_OUTLINE : TEXT_SHADOW;
  constexpr int resStyle = TEXT_SHADOW;
  const float thick = g_cfg->line_width;
  const bool wantSnapline = g_cfg->snapline;
  const int snaplinePos = g_cfg->snapline_pos;
  const float snaplineY = (snaplinePos == 0) ? 0.f
                          : (snaplinePos == 1) ? sh2 : sh;
  const bool hideSleepers = g_cfg->hide_sleepers;
  const int playerCap = g_cfg->player_max_dist;
  const int resourceCap = g_cfg->resource_max_dist;
  render::Color catCol[Prefabs::kCategoryCount];
  for (int _ci = 0; _ci < Prefabs::kCategoryCount; ++_ci)
    catCol[_ci] = Col(g_cfg->Cat((Prefabs::Category)_ci).color);
  const render::Color woundedCol = Col(g_cfg->wounded_color);
  const render::Color sleeperCol = Col(g_cfg->sleeper_color);
  const bool namesOn = g_cfg->esp_names;
  const bool metersOn = g_cfg->esp_meters;

  if (normal_esp) {
  for (const auto &e : *snapshot) {
    espstats::total.fetch_add(1, std::memory_order_relaxed);

    // Never draw the local player.
    if (local && e.address == local)
      continue;

    // Category is classified ONCE on the worker thread and cached in the
    // snapshot (the prefab switch is only re-run as a fallback).
    const Prefabs::Category cat =
        (e.category >= 0 && e.category < Prefabs::kCategoryCount)
            ? (Prefabs::Category)e.category
            : Prefabs::GetCategory(e.prefabID);
    const CategoryConfig &cc = g_cfg->Cat(cat);
    if (!cc.enabled)
      continue;

    // Dead players are kept visible for a moment instead of vanishing the
    // instant a hit lands; the worker's player grace covers the gap when the
    // entity is removed from the client list.
    const bool dead = cat == Prefabs::Category::Player && !e.alive;
    if (dead)
      espstats::pDead.fetch_add(1, std::memory_order_relaxed);

    // Optional sleeper filter. This skips both the draw and (in the skeleton
    // worker) the bone reads, so hiding sleepers actually removes overhead.
    if (cat == Prefabs::Category::Player && hideSleepers && e.sleeping) {
      espstats::pSleep.fetch_add(1, std::memory_order_relaxed);
      continue;
    }

    // Interpolate players between worker snapshots. The worker can be busy
    // with a large ESP/image pass, so its 8 ms target can stretch; extrapolating
    // the last smoothed velocity keeps boxes moving at overlay frame rate.
    Vector3 drawPos = e.position;
    if (e.sampleMs > 0.0) {
      const double ageMs = drawNowMs - e.sampleMs;
      if (ageMs > 0.0 && ageMs < 250.0 && !e.velocity.Empty()) {
        const float dt = static_cast<float>(ageMs * 0.001);
        drawPos.x += e.velocity.x * dt;
        drawPos.y += e.velocity.y * dt;
        drawPos.z += e.velocity.z * dt;
      }
    }

    float dist = -1.f;
    if (haveLocal) {
      dist = drawPos.Distance(localPos);
      const int cap =
          (cat == Prefabs::Category::Player) ? playerCap : resourceCap;
      if (cap > 0 && dist > (float)cap) {
        if (cat == Prefabs::Category::Player)
          espstats::pDist.fetch_add(1, std::memory_order_relaxed);
        continue;
      }
    }

    const render::Color col = catCol[(int)cat];

    // Players get a distinct colour while wounded (downed) or sleeping.
    render::Color pcol = col;
    if (cat == Prefabs::Category::Player) {
      if (dead)
        pcol = render::with_alpha(pcol, 0.55f);
      else if (e.wounded)
        pcol = woundedCol;
      else if (e.sleeping)
        pcol = sleeperCol;
    }

    if (cat == Prefabs::Category::Player) {
      // ── players: skeleton + name/meters only (no box, no icon) ─────────
      Vector2 feet;
      if (!W2S::WorldToScreen(drawPos, feet)) {
        espstats::pW2S.fetch_add(1, std::memory_order_relaxed);
        continue;
      }

      Vector2 boneScreen[skeleton::Count];
      bool boneOk[skeleton::Count]{};
      skeleton::Data skel;
      const bool haveSkel = skeleton::Get(e.address, skel);
      const bool skeletonOn =
          cc.draw_skeleton && haveSkel && skel.Valid() &&
          (dist < 0.f || dist <= skeleton::kMaxDistance);

      if (skeletonOn) {
        // Bones are world positions from the last worker read. Slide the whole
        // skeleton by the player's movement since then so it tracks the live
        // entity position every frame instead of trailing a 25 Hz snapshot.
        Vector3 anchor{};
        if (skel.HasOrigin())
          anchor = drawPos - skel.origin;

        for (int b = 0; b < skeleton::Count; ++b) {
          if (!skel.Has(b))
            continue;
          const Vector3 world = skel.bones[b] + anchor;
          boneOk[b] = W2S::WorldToScreen(world, boneScreen[b]);
        }
        render::Vec2 segments[sizeof(kSkeletonLinks) /
                              sizeof(kSkeletonLinks[0]) * 2];
        int segmentCount = 0;
        for (const SkeletonLink &link : kSkeletonLinks) {
          if (!boneOk[link.a] || !boneOk[link.b])
            continue;
          segments[segmentCount++] = {boneScreen[link.a].x,
                                      boneScreen[link.a].y};
          segments[segmentCount++] = {boneScreen[link.b].x,
                                      boneScreen[link.b].y};
        }
        if (segmentCount)
          render::lines(segments, segmentCount, pcol, thick);
      }

      const Vector3 headWorld{drawPos.x, drawPos.y + 1.9f, drawPos.z};
      Vector2 head;
      bool haveHead = false;
      if (skeletonOn && boneOk[skeleton::Head]) {
        head = boneScreen[skeleton::Head];
        haveHead = true;
      }
      if (!haveHead)
        haveHead = W2S::WorldToScreen(headWorld, head);
      if (!haveHead) {
        head.x = feet.x;
        head.y = feet.y - 40.f;
      }
      espstats::pDraw.fetch_add(1, std::memory_order_relaxed);

      if (head.x < -80.f || head.y < -80.f || head.x > sw + 80.f ||
          head.y > sh + 80.f)
        continue;

      const bool wantNames = namesOn && cc.draw_label;
      const bool wantMeters = metersOn && cc.draw_distance;
      if (wantNames || wantMeters) {
        const char *name = (e.displayName && !e.displayName->empty())
                               ? e.displayName->c_str()
                               : "player";
        char lbuf[192];
        const char *txt =
            BuildLabel(lbuf, sizeof(lbuf), name, dist, wantNames, wantMeters);
        if (txt && *txt) {
          const render::Vec2 ts = CachedTextSize(txt);
          DrawOutlinedText({head.x - ts.x * 0.5f, head.y - ts.y - 3.f}, pcol,
                           txt, charStyle);
        }
      }
    } else {
      // ── everything else: name + metres only, no box ─────────────────────
      Vector2 p;
      if (!W2S::WorldToScreen(drawPos, p))
        continue;
      if (p.x < -60.f || p.y < -60.f || p.x > sw + 60.f || p.y > sh + 60.f) {
        if (cat == Prefabs::Category::Player)
          espstats::pOff.fetch_add(1, std::memory_order_relaxed);
        continue;
      }

      {
        const bool wantNames = namesOn && cc.draw_label;
        const bool wantMeters = metersOn && cc.draw_distance;
        if (wantNames || wantMeters) {
          // The name is pre-computed on the worker thread (e.displayName) so no
          // allocation happens here.
          const char *name = (e.displayName && !e.displayName->empty())
                                 ? e.displayName->c_str()
                                 : "object";
          char lbuf[192];
          const char *txt = BuildLabel(lbuf, sizeof(lbuf), name, dist, wantNames,
                                       wantMeters);
          if (txt && *txt) {
            const render::Vec2 ts = CachedTextSize(txt);
            DrawOutlinedText({p.x - ts.x * 0.5f, p.y - ts.y - 2.f}, col, txt,
                             resStyle);
          }
        }
      }

      if (wantSnapline) {
        render::line({sw2, snaplineY}, {p.x, p.y}, col, thick);
      }
    }
  }

  } // normal_esp

  // Fading last-known-position markers for players the client removed (server
  // side occlusion). This used to be dead code - the function existed but was
  // never called - so a vanished player simply popped off instead of fading.
  if (normal_esp)
    RenderGhosts(local, localPos, haveLocal, sw, sh, sw2, sh2, charStyle,
                 thick);

  // Optional icon layer (16×16 PNGs). Independent of the normal master and of
  // the player/resource category toggles; the Image ESP page has its own master.
  if (g_cfg->image_esp)
    image_esp::Render(*snapshot, local, localPos, haveLocal, sw, sh);
}

} // namespace esp
