
// ================================================================================
// Bot: @imguisource_bot | TG: @ImGui_Source_FIXNEY & @il2cpp_src
// ================================================================================

#pragma once
#include <cstdint>
#include "../GetEntities/Prefabs/Prefabs.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ESP configuration.
//
//  Everything the menu edits lives in a single Config instance (pointed to by
//  the global g_cfg). ESP and the menu both read/write that one object, so a
//  checkbox immediately changes the draw. Colours are plain float[4] so the
//  renderer and the JSON bridge both consume them directly.
// ─────────────────────────────────────────────────────────────────────────────

// Per-category ESP options + colour.
struct CategoryConfig {
  bool  enabled       = false;                // draw this category at all
  bool  draw_box      = false;                // box around the entity
  bool  draw_label    = true;                 // name / label above it
  bool  draw_distance = true;                 // distance text
  bool  draw_skeleton = true;                 // player bone skeleton
  int   max_distance  = 0;                     // metres; 0 -> use the global cap
  int   box_style     = 0;                     // 0 -> follow global, else (value-1)=EspBoxStyle
  float color[4]      = {1.f, 1.f, 1.f, 1.f};  // RGBA
};

// Box styles (used for players / NPCs, and for object categories that opt in).
enum EspBoxStyle {
  BOX_NONE = 0,
  BOX_2D,
  BOX_CORNER,
  BOX_FILLED,
  BOX_STYLE_COUNT
};

struct Config {
  // ── master ──────────────────────────────────────────────────────────────
  bool  esp_enabled = true;
  bool  esp_names   = true;   // global name/status gate (per-category draw_label too)
  bool  esp_meters  = true;   // global distance gate (per-category draw_distance too)

  // ── global drawing style ────────────────────────────────────────────────
  int   box_style         = BOX_2D;   // EspBoxStyle
  bool  outline           = true;     // black outline around text/boxes
  float line_width        = 1.0f;
  bool  snapline          = false;
  int   snapline_pos      = 1;        // 0 top, 1 middle, 2 bottom
  int   player_max_dist   = 1000;     // metres; draw distance for players
  int   resource_max_dist = 500;      // metres; draw distance for everything else
  bool  hide_sleepers     = false;    // don't draw players who are sleeping
  // Opt-in external chams. Writes a curated glow material ID into the player
  // model's renderer material arrays. Off by default because a wrong material
  // ID can crash the game.
  bool  chams             = false;
  int   chams_material    = 0;        // chams::MaterialPreset (0 = GreenGlow)
  bool  steven_invert     = false;    // swap Steven's green/red if the game flag is inverted
  bool  vsync             = false;    // cap the overlay to the monitor refresh
  // Overlay present cap. 1000 restores the high-FPS build; lowering it toward
  // the monitor refresh (144) uses far less GPU/CPU while playing.
  int   overlay_fps       = 1000;

  // ── last-known-position ghosts ──────────────────────────────────────────
  // Facepunch's server-side player occlusion stops sending a player to us the
  // moment we are occluded from them, so their box used to pop out and back in.
  // Instead we keep drawing a fading marker where they were last seen.
  int   ghost_time_ms     = 3000;     // how long the marker lives (0 = off)
  bool  ghost_predict     = true;     // let it drift along their last velocity

  // ── aim assist ──────────────────────────────────────────────────────────
  // Two interchangeable methods (see aim_mode):
  //   0 = write the view angles straight into the local player's PlayerInput
  //       through the kernel driver (the "silent"/internal-style method the
  //       reference project uses - no OS input involved at all);
  //   1 = nudge the real mouse with relative SendInput moves (pure input
  //       injection; needs the game to own the foreground).
  // The key is captured from the Aim page ("Bind Key" -> press any key).
  bool  aim_enabled    = false;  // master switch, OFF by default
  int   aim_key        = 0;      // virtual-key code; 0 = unbound
  bool  aim_hold       = true;   // true: aim while held; false: toggle on press
  int   aim_mode       = 0;      // 0 = write view angles, 1 = mouse (SendInput)
  float aim_smoothness = 0.45f;  // 0 = instant snap, 1 = very gentle pull
  int   aim_fov        = 150;    // crosshair radius (pixels) a target must be in
  int   aim_max_dist   = 350;    // metres
  int   aim_bone       = 1;      // 0 = chest, 1 = head
  float aim_vert       = 0.0f;   // metres added to the aim point on the target.
                                 // Negative aims LOWER: use it to pull the shot
                                 // down when impacts land a bit high (the real
                                 // camera sits slightly above feet+1.62).
  bool  aim_write_input = false; // ALSO write PlayerInput.bodyAngles. The shot is
                                 // steered by the eye rotation alone (as in the
                                 // reference); writing bodyAngles changes the
                                 // player's movement basis (movement goes weird)
                                 // and races the game's own update, so it is OFF
                                 // by default. Turn on only if the eye write
                                 // alone ever fails to register.
  float aim_drop_speed  = 0.f;   // projectile speed (m/s) for ballistic drop
                                 // compensation. 0 = off (direct aim). Set to
                                 // the weapon's muzzle velocity (e.g. ~100 for a
                                 // bow) to raise the aim so the arc lands on the
                                 // target at range.
  bool  aim_draw_fov   = true;   // draw the target circle while aiming is on

  // ── movement ────────────────────────────────────────────────────────────
  bool  spiderman      = false;  // Spider-Man climb: keep groundAngle at 0

  // ── per category ────────────────────────────────────────────────────────
  CategoryConfig cat[(int)Prefabs::Category::Count];

  // ── interface ───────────────────────────────────────────────────────────
  float   accent[4]      = {1.f, 122.f / 255.f, 26.f / 255.f, 1.f};
  // Menu chrome only (the native HUD / aim FOV use accent above).
  float   menu_accent[4] = {167.f / 255.f, 85.f / 255.f, 247.f / 255.f, 1.f};
  float   wounded_color[4] = {1.f, 0.55f, 0.10f, 1.f};   // downed players
  float   sleeper_color[4] = {0.55f, 0.55f, 0.70f, 1.f}; // sleeping players
  float   ghost_color[4]   = {0.80f, 0.80f, 0.90f, 1.f}; // last known position
  bool    watermark      = true;
  bool    custom_cursor  = true;
  bool    esp_preview    = false;
  int     notify_style   = 0;         // 0 circle, 1 line

  // ── image ESP (16×16 art + distance) ──────────────────────────────────
  // Independent of the box/label toggles: the entity worker keeps these
  // categories alive while image ESP needs them, so boxes can be switched off
  // without killing the icons.
  bool    image_esp      = false;  // master for the icon layer
  bool    image_animals  = true;
  bool    image_hemp     = true;
  bool    image_metal    = true;
  bool    image_stone    = true;
  bool    image_sulfur   = true;
  bool    image_plants   = true;
  bool    image_wood     = true;
  bool    image_hqm      = true;
  bool    image_crates   = true;
  bool    image_barrels  = true;
  bool    image_vehicles = true;
  bool    image_supplies = true;
  bool    image_distance = true;   // draw the "Nm" text under each icon

  bool ImageEnabled(Prefabs::ImageKind kind) const {
    switch (kind) {
    case Prefabs::ImageKind::Hemp: return image_hemp;
    case Prefabs::ImageKind::MetalOre: return image_metal;
    case Prefabs::ImageKind::StoneOre: return image_stone;
    case Prefabs::ImageKind::SulfurOre: return image_sulfur;
    case Prefabs::ImageKind::Mushroom:
    case Prefabs::ImageKind::Potato:
    case Prefabs::ImageKind::Pumpkin: return image_plants;
    case Prefabs::ImageKind::WoodPile: return image_wood;
    case Prefabs::ImageKind::HqmOre: return image_hqm;
    case Prefabs::ImageKind::SupplyDrop: return image_supplies;
    case Prefabs::ImageKind::CrateTools:
    case Prefabs::ImageKind::CrateFood:
    case Prefabs::ImageKind::CrateNormal: return image_crates;
    case Prefabs::ImageKind::LootBarrel:
    case Prefabs::ImageKind::OilBarrel: return image_barrels;
    case Prefabs::ImageKind::Minicopter: return image_vehicles;
    case Prefabs::ImageKind::None: return false;
    default: return image_animals;
    }
  }

  // Used by the entity worker so a category that is switched off for normal
  // ESP is still streamed while its icon layer is on.
  bool ImageWants(Prefabs::Category category, uint32_t prefabID,
                  const std::string *className = nullptr) const {
    if (!image_esp)
      return false;
    if (category == Prefabs::Category::Player)
      return false; // player art is handled by names/meters/skeleton
    Prefabs::ImageKind kind = Prefabs::GetImageKind(prefabID);
    if (kind == Prefabs::ImageKind::None && className)
      kind = Prefabs::GetImageKindByClass(*className);
    return ImageEnabled(kind);
  }

  CategoryConfig& Cat(Prefabs::Category c) { return cat[(int)c]; }
  const CategoryConfig& Cat(Prefabs::Category c) const {
    return cat[(int)c];
  }
};

extern Config* g_cfg;

// Fills the config with the shipped defaults (also sets the per-category
// colours). Safe to call on a fresh or reused Config.
void Config_SetDefaults(Config& c);

// Plain-text save / load. Returns false when the file could not be used.
bool Config_Save(const Config& c, const char* path);
bool Config_Load(Config& c, const char* path);
