
// ================================================================================
// Бот с сурсами: @imguisource_bot
// Основной канал (ImGui Source): @ImGui_Source_FIXNEY
// Дополнительный канал (shit src 🧨): @il2cpp_src
// ================================================================================

#include "../Pch/Pch.h"
#include "Config.h"

#include <fstream>
#include <string>

Config* g_cfg = nullptr;

namespace {
void SetColor(float dst[4], float r, float g, float b, float a) {
  dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = a;
}
} // namespace

void Config_SetDefaults(Config& c) {
  using C = Prefabs::Category;

  c.esp_enabled       = true;
  c.esp_names         = true;
  c.esp_meters        = true;
  c.box_style         = BOX_2D;
  c.outline           = true;
  c.line_width        = 1.0f;
  c.snapline          = false;
  c.snapline_pos      = 1;
  c.player_max_dist   = 1000;
  c.resource_max_dist = 500;
  c.hide_sleepers     = false;
  c.chams             = false;
  c.chams_material    = 0;
  c.steven_invert     = false;
  c.vsync             = false;
  c.overlay_fps       = 1000;
  c.ghost_time_ms     = 3000;
  c.ghost_predict     = true;

  c.aim_enabled    = false;
  c.aim_key        = 0;
  c.aim_hold       = true;
  c.aim_mode       = 0;
  c.aim_smoothness = 0.45f;
  c.aim_fov        = 150;
  c.aim_max_dist   = 350;
  c.aim_bone       = 1;
  c.aim_vert       = 0.0f;
  c.aim_write_input = false;
  c.aim_drop_speed = 0.0f;
  c.aim_draw_fov   = true;

  c.spiderman      = false;

  c.accent[0] = 1.f;
  c.accent[1] = 122.f / 255.f;
  c.accent[2] = 26.f / 255.f;
  c.accent[3] = 1.f;
  c.menu_accent[0] = 167.f / 255.f;
  c.menu_accent[1] = 85.f / 255.f;
  c.menu_accent[2] = 247.f / 255.f;
  c.menu_accent[3] = 1.f;
  SetColor(c.wounded_color, 1.00f, 0.55f, 0.10f, 1.f);
  SetColor(c.sleeper_color, 0.55f, 0.55f, 0.70f, 1.f);
  SetColor(c.ghost_color,   0.80f, 0.80f, 0.90f, 1.f);
  c.watermark        = true;
  c.custom_cursor    = true;
  c.esp_preview      = false;
  c.notify_style     = 0;
  c.image_esp        = false;
  c.image_animals    = true;
  c.image_hemp       = true;
  c.image_metal      = true;
  c.image_stone      = true;
  c.image_sulfur     = true;
  c.image_plants     = true;
  c.image_wood       = true;
  c.image_hqm        = true;
  c.image_crates     = true;
  c.image_barrels    = true;
  c.image_vehicles   = true;
  c.image_supplies   = true;
  c.image_distance   = true;

  // Every category draws its box + label by default. The resources page no
  // longer exposes those toggles (only the enable checkbox), so a category the
  // user turns on always shows a box and a label.
  for (int i = 0; i < Prefabs::kCategoryCount; ++i) {
    CategoryConfig& cc = c.cat[i];
    cc.enabled       = false;
    cc.draw_box      = true;
    cc.draw_label    = true;
    cc.draw_distance = true;
    cc.draw_skeleton = true;
    cc.max_distance  = 0;
    cc.box_style     = 0;
    SetColor(cc.color, 1.f, 1.f, 1.f, 1.f);
  }

  auto set = [&](C cat, bool en, float r, float g, float b) {
    CategoryConfig& cc = c.Cat(cat);
    cc.enabled  = en;
    cc.draw_box = true;
    SetColor(cc.color, r, g, b, 1.f);
  };

  set(C::Player,     true,  1.00f, 0.20f, 0.20f);
  set(C::Npc,        true,  1.00f, 0.60f, 0.15f);
  set(C::Ore,        true,  0.98f, 0.85f, 0.20f);
  set(C::Plant,      false, 0.35f, 0.90f, 0.35f);
  set(C::Crate,      true,  1.00f, 0.80f, 0.20f);
  set(C::Barrel,     false, 0.80f, 0.50f, 0.20f);
  set(C::Loot,       false, 0.60f, 0.60f, 1.00f);
  set(C::SupplyDrop, true,  1.00f, 0.40f, 0.80f);
  set(C::Vehicle,    false, 0.40f, 0.80f, 1.00f);
  set(C::Deployable, false, 0.75f, 0.75f, 0.75f);
  set(C::Trap,       false, 1.00f, 0.25f, 0.25f);
  set(C::Door,       false, 0.60f, 0.40f, 0.20f);
  set(C::Battery,    false, 0.90f, 0.90f, 0.40f);
  set(C::Corpse,     true,  0.85f, 0.45f, 0.95f);
}

bool Config_Save(const Config& c, const char* path) {
  std::ofstream f(path, std::ios::trunc);
  if (!f) return false;

  f << "version 1\n";
  f << "esp_enabled "     << c.esp_enabled << '\n';
  f << "esp_names "       << c.esp_names << '\n';
  f << "esp_meters "      << c.esp_meters << '\n';
  f << "box_style "       << c.box_style << '\n';
  f << "outline "         << c.outline << '\n';
  f << "line_width "      << c.line_width << '\n';
  f << "snapline "        << c.snapline << '\n';
  f << "snapline_pos "    << c.snapline_pos << '\n';
  f << "player_max_dist "   << c.player_max_dist << '\n';
  f << "resource_max_dist " << c.resource_max_dist << '\n';
  f << "hide_sleepers "     << c.hide_sleepers << '\n';
  f << "chams "             << c.chams << '\n';
  f << "chams_material "    << c.chams_material << '\n';
  f << "steven_invert "     << c.steven_invert << '\n';
  f << "vsync "             << c.vsync << '\n';
  f << "overlay_fps "       << c.overlay_fps << '\n';
  f << "ghost_time_ms "     << c.ghost_time_ms << '\n';
  f << "ghost_predict "     << c.ghost_predict << '\n';
  f << "aim_enabled "       << c.aim_enabled << '\n';
  f << "aim_key "           << c.aim_key << '\n';
  f << "aim_hold "          << c.aim_hold << '\n';
  f << "aim_mode "          << c.aim_mode << '\n';
  f << "aim_smoothness "    << c.aim_smoothness << '\n';
  f << "aim_fov "           << c.aim_fov << '\n';
  f << "aim_max_dist "      << c.aim_max_dist << '\n';
  f << "aim_bone "          << c.aim_bone << '\n';
  f << "aim_vert "          << c.aim_vert << '\n';
  f << "aim_write_input "   << c.aim_write_input << '\n';
  f << "aim_drop_speed "    << c.aim_drop_speed << '\n';
  f << "aim_draw_fov "      << c.aim_draw_fov << '\n';
  f << "spiderman "         << c.spiderman << '\n';

  for (int i = 0; i < Prefabs::kCategoryCount; ++i) {
    const CategoryConfig& cc = c.cat[i];
    f << "cat " << i << ' ' << cc.enabled << ' ' << cc.draw_box << ' '
      << cc.draw_label << ' ' << cc.draw_distance << ' ' << cc.max_distance
      << ' ' << cc.color[0] << ' ' << cc.color[1] << ' ' << cc.color[2] << ' '
      << cc.color[3] << ' ' << cc.draw_skeleton << '\n';
    f << "catstyle " << i << ' ' << cc.box_style << '\n';
  }

  f << "accent " << c.accent[0] << ' ' << c.accent[1] << ' ' << c.accent[2] << ' '
    << c.accent[3] << '\n';
  f << "menu_accent " << c.menu_accent[0] << ' ' << c.menu_accent[1] << ' '
    << c.menu_accent[2] << ' ' << c.menu_accent[3] << '\n';
  f << "wounded_color " << c.wounded_color[0] << ' ' << c.wounded_color[1] << ' '
    << c.wounded_color[2] << ' ' << c.wounded_color[3] << '\n';
  f << "sleeper_color " << c.sleeper_color[0] << ' ' << c.sleeper_color[1] << ' '
    << c.sleeper_color[2] << ' ' << c.sleeper_color[3] << '\n';
  f << "ghost_color " << c.ghost_color[0] << ' ' << c.ghost_color[1] << ' '
    << c.ghost_color[2] << ' ' << c.ghost_color[3] << '\n';
  f << "watermark "     << c.watermark << '\n';
  f << "custom_cursor " << c.custom_cursor << '\n';
  f << "esp_preview "   << c.esp_preview << '\n';
  f << "notify_style "  << c.notify_style << '\n';
  f << "image_esp "      << c.image_esp << '\n';
  f << "image_animals "  << c.image_animals << '\n';
  f << "image_hemp "     << c.image_hemp << '\n';
  f << "image_metal "    << c.image_metal << '\n';
  f << "image_stone "    << c.image_stone << '\n';
  f << "image_sulfur "   << c.image_sulfur << '\n';
  f << "image_plants "   << c.image_plants << '\n';
  f << "image_wood "     << c.image_wood << '\n';
  f << "image_hqm "      << c.image_hqm << '\n';
  f << "image_crates "   << c.image_crates << '\n';
  f << "image_barrels "  << c.image_barrels << '\n';
  f << "image_vehicles " << c.image_vehicles << '\n';
  f << "image_supplies " << c.image_supplies << '\n';
  f << "image_distance " << c.image_distance << '\n';
  return true;
}

bool Config_Load(Config& c, const char* path) {
  std::ifstream f(path);
  if (!f) return false;

  Config_SetDefaults(c);

  std::string key;
  while (f >> key) {
    if (key == "version") {
      int v; f >> v;
    } else if (key == "esp_enabled") {
      int v; f >> v; c.esp_enabled = v != 0;
    } else if (key == "esp_names") {
      int v; f >> v; c.esp_names = v != 0;
    } else if (key == "esp_meters") {
      int v; f >> v; c.esp_meters = v != 0;
    } else if (key == "box_style") {
      f >> c.box_style;
    } else if (key == "outline") {
      int v; f >> v; c.outline = v != 0;
    } else if (key == "line_width") {
      f >> c.line_width;
    } else if (key == "snapline") {
      int v; f >> v; c.snapline = v != 0;
    } else if (key == "snapline_pos") {
      f >> c.snapline_pos;
    } else if (key == "player_max_dist") {
      f >> c.player_max_dist;
    } else if (key == "resource_max_dist") {
      f >> c.resource_max_dist;
    } else if (key == "hide_sleepers") {
      int v; f >> v; c.hide_sleepers = v != 0;
    } else if (key == "chams") {
      int v; f >> v; c.chams = v != 0;
    } else if (key == "chams_material") {
      f >> c.chams_material;
    } else if (key == "steven_invert") {
      int v; f >> v; c.steven_invert = v != 0;
    } else if (key == "vsync") {
      int v; f >> v; c.vsync = v != 0;
    } else if (key == "overlay_fps") {
      f >> c.overlay_fps;
    } else if (key == "ghost_time_ms") {
      f >> c.ghost_time_ms;
    } else if (key == "ghost_predict") {
      int v; f >> v; c.ghost_predict = v != 0;
    } else if (key == "aim_enabled") {
      int v; f >> v; c.aim_enabled = v != 0;
    } else if (key == "aim_key") {
      f >> c.aim_key;
    } else if (key == "aim_hold") {
      int v; f >> v; c.aim_hold = v != 0;
    } else if (key == "aim_mode") {
      f >> c.aim_mode;
    } else if (key == "aim_smoothness") {
      f >> c.aim_smoothness;
    } else if (key == "aim_fov") {
      f >> c.aim_fov;
    } else if (key == "aim_max_dist") {
      f >> c.aim_max_dist;
    } else if (key == "aim_bone") {
      f >> c.aim_bone;
    } else if (key == "aim_vert") {
      f >> c.aim_vert;
    } else if (key == "aim_write_input") {
      int v; f >> v; c.aim_write_input = v != 0;
    } else if (key == "aim_drop_speed") {
      f >> c.aim_drop_speed;
    } else if (key == "aim_draw_fov") {
      int v; f >> v; c.aim_draw_fov = v != 0;
    } else if (key == "spiderman") {
      int v; f >> v; c.spiderman = v != 0;
    } else if (key == "global_max_dist") {
      // legacy config: one distance for everything
      int v = 3000; f >> v;
      c.player_max_dist = v;
      c.resource_max_dist = v;
    } else if (key == "cat") {
      int i = -1; f >> i;
      CategoryConfig cc;
      int en = 0, box = 0, lab = 1, dist = 1;
      f >> en >> box >> lab >> dist >> cc.max_distance >> cc.color[0] >>
          cc.color[1] >> cc.color[2] >> cc.color[3];
      int skel = 1;
      if (!(f >> skel)) {
        // Older config lines have no skeleton field; keep it enabled.
        f.clear();
        skel = 1;
      }
      cc.enabled       = en != 0;
      cc.draw_box      = box != 0;
      cc.draw_label    = lab != 0;
      cc.draw_distance = dist != 0;
      cc.draw_skeleton = skel != 0;
      if (i >= 0 && i < Prefabs::kCategoryCount) c.cat[i] = cc;
    } else if (key == "catstyle") {
      int i = -1, s = 0;
      f >> i >> s;
      if (i >= 0 && i < Prefabs::kCategoryCount) c.cat[i].box_style = s;
    } else if (key == "accent") {
      f >> c.accent[0] >> c.accent[1] >> c.accent[2] >> c.accent[3];
    } else if (key == "menu_accent") {
      f >> c.menu_accent[0] >> c.menu_accent[1] >> c.menu_accent[2] >>
          c.menu_accent[3];
    } else if (key == "wounded_color") {
      f >> c.wounded_color[0] >> c.wounded_color[1] >> c.wounded_color[2] >>
          c.wounded_color[3];
    } else if (key == "sleeper_color") {
      f >> c.sleeper_color[0] >> c.sleeper_color[1] >> c.sleeper_color[2] >>
          c.sleeper_color[3];
    } else if (key == "ghost_color") {
      f >> c.ghost_color[0] >> c.ghost_color[1] >> c.ghost_color[2] >>
          c.ghost_color[3];
    } else if (key == "watermark") {
      int v; f >> v; c.watermark = v != 0;
    } else if (key == "custom_cursor") {
      int v; f >> v; c.custom_cursor = v != 0;
    } else if (key == "esp_preview") {
      int v; f >> v; c.esp_preview = v != 0;
    } else if (key == "notify_style") {
      f >> c.notify_style;
    } else if (key == "image_esp") {
      int v; f >> v; c.image_esp = v != 0;
    } else if (key == "image_animals") {
      int v; f >> v; c.image_animals = v != 0;
    } else if (key == "image_hemp") {
      int v; f >> v; c.image_hemp = v != 0;
    } else if (key == "image_metal") {
      int v; f >> v; c.image_metal = v != 0;
    } else if (key == "image_stone") {
      int v; f >> v; c.image_stone = v != 0;
    } else if (key == "image_sulfur") {
      int v; f >> v; c.image_sulfur = v != 0;
    } else if (key == "image_plants") {
      int v; f >> v; c.image_plants = v != 0;
    } else if (key == "image_wood") {
      int v; f >> v; c.image_wood = v != 0;
    } else if (key == "image_hqm") {
      int v; f >> v; c.image_hqm = v != 0;
    } else if (key == "image_crates") {
      int v; f >> v; c.image_crates = v != 0;
    } else if (key == "image_barrels") {
      int v; f >> v; c.image_barrels = v != 0;
    } else if (key == "image_vehicles") {
      int v; f >> v; c.image_vehicles = v != 0;
    } else if (key == "image_supplies") {
      int v; f >> v; c.image_supplies = v != 0;
    } else if (key == "image_distance") {
      int v; f >> v; c.image_distance = v != 0;
    } else {
      // Unknown token: skip the rest of the line.
      std::string rest;
      std::getline(f, rest);
    }
  }

  // sanitise
  if (c.box_style < 0 || c.box_style >= BOX_STYLE_COUNT) c.box_style = BOX_2D;
  if (c.notify_style < 0 || c.notify_style > 1) c.notify_style = 0;
  if (c.line_width < 0.1f || c.line_width > 4.f) c.line_width = 0.1f;
  if (c.aim_smoothness < 0.f) c.aim_smoothness = 0.f;
  if (c.aim_smoothness > 1.f) c.aim_smoothness = 1.f;
  if (c.aim_fov < 10) c.aim_fov = 10;
  if (c.aim_fov > 800) c.aim_fov = 800;
  if (c.aim_max_dist < 5) c.aim_max_dist = 5;
  if (c.aim_bone < 0 || c.aim_bone > 1) c.aim_bone = 1;
  if (c.aim_vert < -1.5f) c.aim_vert = -1.5f;
  if (c.aim_vert >  1.5f) c.aim_vert =  1.5f;
  if (c.aim_drop_speed < 0.f) c.aim_drop_speed = 0.f;
  if (c.aim_drop_speed > 2000.f) c.aim_drop_speed = 2000.f;
  if (c.aim_mode < 0 || c.aim_mode > 1) c.aim_mode = 0;
  if (c.aim_key < 0 || c.aim_key > 0xFF) c.aim_key = 0;
  if (c.chams_material < 0 || c.chams_material > 6) c.chams_material = 0;
  for (int i = 0; i < Prefabs::kCategoryCount; ++i)
    if (c.cat[i].box_style < 0 || c.cat[i].box_style > BOX_STYLE_COUNT)
      c.cat[i].box_style = 0;
  return true;
}
