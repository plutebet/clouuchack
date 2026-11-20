
// ================================================================================
// t.me/imguisource_bot | t.me/ImGui_Source_FIXNEY (ImGui Source) | t.me/il2cpp_src (shit src 🧨)
// ================================================================================

#include "Prefabs.h"

#include <algorithm>
#include <cctype>

namespace Prefabs {

ImageKind GetImageKind(uint32_t id) {
  switch (id) {
    case HEMP:
    case HEMP2: return ImageKind::Hemp;
    case METAL_ORE:
    case METAL_ORE2:
    case METAL_ORE3:
    case METAL_ORE4:
    case MINI_METAL_ORE: return ImageKind::MetalOre;
    case STONE_ORE:
    case STONE_ORE2:
    case STONE_ORE3:
    case STONE_ORE4:
    case MINI_STONE_ORE: return ImageKind::StoneOre;
    case SULFUR_ORE:
    case SULFUR_ORE2:
    case SULFUR_ORE3:
    case SULFUR_ORE4:
    case MINI_SULFUR_ORE: return ImageKind::SulfurOre;

    // Animals with dedicated art in rust/.
    case BEAR: return ImageKind::Bear;
    case BOAR: return ImageKind::Boar;
    case BULL: return ImageKind::Bull;
    case CALF:
    case CALF_MALE: return ImageKind::Calf;
    case CHICKEN: return ImageKind::Chicken;
    case COW: return ImageKind::Cow;
    case CRAB:
    case CRAB_SINGLE: return ImageKind::Crab;
    case CROCODILE: return ImageKind::Crocodile;
    case FROG: return ImageKind::Frog;
    case HORSE:
    case HORSE2:
    case HORSE3: return ImageKind::Horse;
    case LAMB: return ImageKind::Lamb;
    case PANTHER: return ImageKind::Panther;
    case POLAR_BEAR: return ImageKind::PolarBear;
    case RABBIT: return ImageKind::Rabbit;
    case SHEEP: return ImageKind::Sheep;
    case SNAKE: return ImageKind::Snake;
    case SQUIRREL: return ImageKind::Squirrel;
    case STAG: return ImageKind::Stag;
    case TIGER: return ImageKind::Tiger;
    case WOLF:
    case WOLF2: return ImageKind::Wolf;

    case MINI_COPTER: return ImageKind::Minicopter;

    // Crates: three art files cover the loot family.
    case TOOL_CRATE:
    case TOOL_CRATE2: return ImageKind::CrateTools;
    case FOOD_CRATE:
    case FOOD_CRATE2:
    case FOOD_CRATE3:
    case FOOD_CRATE4:
    case FOOD_BOX: return ImageKind::CrateFood;
    case ELITE_CRATE:
    case ELITE_CRATE2:
    case BASIC_CRATE:
    case MILITARY_CRATE:
    case NORMAL_CRATE2:
    case NORMAL_CRATE3:
    case HACKABLE_CRATE:
    case HACKABLE_CRATE2:
    case HACKABLE_CRATE3:
    case AMMO_CRATE:
    case FUEL_CRATE:
    case TECH_PARTS:
    case TECH_PARTS2:
    case VEHICLE_PARTS:
    case VEHICLE_PARTS2:
    case UW_CRATE_BASIC:
    case UW_CRATE_ADV:
    case UW_CRATE_NORM:
    case MEDICAL_CRATE:
    case MEDICAL_CRATE2:
    case MINE_CRATE:
    case BRADLEY_CRATE:
    case HELI_CRATE:
    case STASH: return ImageKind::CrateNormal;

    // Barrel art: normal red/blue loot barrels vs the oil barrels.
    case RED_BARREL:
    case RED_BARREL2:
    case BLUE_BARREL:
    case BLUE_BARREL2: return ImageKind::LootBarrel;
    case OIL_BARREL:
    case DIESEL: return ImageKind::OilBarrel;

    // Remaining resource / NPC art supplied with the client.
    case HQM_ORE: return ImageKind::HqmOre;
    case WoodPiles: return ImageKind::WoodPile;
    case MUSHROOM:
    case MUSHROOM2: return ImageKind::Mushroom;
    case POTATO:
    case POTATO2: return ImageKind::Potato;
    case PUMPKIN:
    case PUMPKIN2: return ImageKind::Pumpkin;
    case SUPPLY_DROP:
    case XMAS_PRESENT: return ImageKind::SupplyDrop;
    case SCIENTIST2:
    case SCIENTIST2_HEAVY:
    case SCIENTIST2_SHOTGUN: return ImageKind::Scientist;
    case SCARECROW:
    case SCARECROW_DUNGEON: return ImageKind::Scarecrow;
    case AIRWOLF_SPAWNER: return ImageKind::AirwolfVendor;

    default: return ImageKind::None;
  }
}

ImageKind GetImageKindByClass(const std::string &className) {
  if (className.empty())
    return ImageKind::None;
  std::string lower = className;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  const auto has = [&](const char *needle) {
    return lower.find(needle) != std::string::npos;
  };

  if (has("scientist")) return ImageKind::Scientist;
  if (has("scarecrow")) return ImageKind::Scarecrow;
  if (has("polar") && has("bear")) return ImageKind::PolarBear;
  if (has("bear")) return ImageKind::Bear;
  if (has("boar")) return ImageKind::Boar;
  if (has("bull")) return ImageKind::Bull;
  if (has("calf")) return ImageKind::Calf;
  if (has("chicken")) return ImageKind::Chicken;
  if (has("cow")) return ImageKind::Cow;
  if (has("crab")) return ImageKind::Crab;
  if (has("crocodile")) return ImageKind::Crocodile;
  if (has("frog")) return ImageKind::Frog;
  if (has("horse")) return ImageKind::Horse;
  if (has("lamb")) return ImageKind::Lamb;
  if (has("panther")) return ImageKind::Panther;
  if (has("rabbit")) return ImageKind::Rabbit;
  if (has("sheep")) return ImageKind::Sheep;
  if (has("snake")) return ImageKind::Snake;
  if (has("squirrel")) return ImageKind::Squirrel;
  if (has("stag")) return ImageKind::Stag;
  if (has("tiger")) return ImageKind::Tiger;
  if (has("wolf")) return ImageKind::Wolf;
  if (has("mushroom")) return ImageKind::Mushroom;
  if (has("potato")) return ImageKind::Potato;
  if (has("pumpkin")) return ImageKind::Pumpkin;
  if (has("supply")) return ImageKind::SupplyDrop;
  if (has("hqm")) return ImageKind::HqmOre;
  if (has("wood")) return ImageKind::WoodPile;
  if (has("crate")) return ImageKind::CrateNormal;
  if (has("barrel")) return ImageKind::OilBarrel;
  return ImageKind::None;
}

bool IsAnimalClass(const std::string &className) {
  if (className.empty())
    return false;

  std::string lower = className;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (lower.find("animal") != std::string::npos)
    return true;

  static const char *kNames[] = {
      "bear",   "boar",   "bull",   "calf",   "chicken", "cow",
      "crab",   "crocodile", "frog", "horse", "lamb",   "panther",
      "rabbit", "sheep",  "snake",  "squirrel", "stag", "tiger",
      "wolf",   "shark",  "jellyfish", "seaturtle", "bee",
  };
  for (const char *name : kNames) {
    if (lower == name)
      return true;
  }
  return false;
}

bool IsNpcClass(const std::string &className) {
  if (IsAnimalClass(className))
    return true;
  std::string lower = className;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower.find("scientist") != std::string::npos ||
         lower.find("scarecrow") != std::string::npos ||
         lower.find("bandit") != std::string::npos ||
         lower.find("tunneldweller") != std::string::npos ||
         lower.find("underwaterdweller") != std::string::npos ||
         lower.find("npcplayer") != std::string::npos;
}

std::string GetPrettyName(uint32_t id) {
  switch (id) {
    // Players
    case BASE_PLAYER: return "Player";

    // Sulfur nodes
    case SULFUR_ORE:
    case SULFUR_ORE2:
    case SULFUR_ORE3:
    case SULFUR_ORE4:
    case MINI_SULFUR_ORE: return "Sulfur";

    // Metal nodes
    case METAL_ORE:
    case METAL_ORE2:
    case METAL_ORE3:
    case METAL_ORE4:
    case MINI_METAL_ORE: return "Metal";

    // Stone nodes
    case STONE_ORE:
    case STONE_ORE2:
    case STONE_ORE3:
    case STONE_ORE4:
    case MINI_STONE_ORE: return "Stone";

    // Wood
    case WoodPiles: return "Wood Pile";

    // Plants
    case HEMP:
    case HEMP2: return "Hemp";
    case CORN:
    case CORN2: return "Corn";
    case POTATO:
    case POTATO2: return "Potato";
    case PUMPKIN:
    case PUMPKIN2: return "Pumpkin";
    case MUSHROOM:
    case MUSHROOM2: return "Mushroom";

    // Berries
    case YELLOW_BERRY: return "Yellow Berry";
    case BLUE_BERRY: return "Blue Berry";
    case RED_BERRY: return "Red Berry";
    case GREEN_BERRY: return "Green Berry";
    case BLACK_BERRY: return "Black Berry";
    case WHITE_BERRY: return "White Berry";

    // Vehicles
    case MINI_COPTER: return "Minicopter";
    case SCRAP_TRANSPORT_HELI: return "Scrap Heli";
    case ROWBOAT: return "Rowboat";
    case RHIB: return "RHIB";
    case TUGBOAT: return "Tugboat";
    case SUBMARINE_SOLO: return "Solo Sub";
    case SUBMARINE_DUO: return "Duo Sub";
    case HOT_AIR_BALLOON: return "HAB";
    case DPV: return "DPV";

    // Air / Bradley
    case PATROL_HELI_COPTER: return "Patrol Heli";
    case ATTACK_HELI: return "Attack Heli";
    case BRADLEY: return "Bradley APC";

    // Animals
    case WOLF:
    case WOLF2: return "Wolf";
    case BOAR: return "Boar";
    case BEAR: return "Bear";
    case POLAR_BEAR: return "Polar Bear";
    case BULL: return "Bull";
    case CALF:
    case CALF_MALE: return "Calf";
    case COW: return "Cow";
    case CRAB:
    case CRAB_SINGLE: return "Crab";
    case CROCODILE: return "Crocodile";
    case FROG: return "Frog";
    case LAMB: return "Lamb";
    case RABBIT: return "Rabbit";
    case SHEEP: return "Sheep";
    case SQUIRREL: return "Squirrel";
    case SEA_TURTLE: return "Sea Turtle";
    case JELLYFISH: return "Jellyfish";
    case SCIENTIST2:
    case SCIENTIST2_HEAVY:
    case SCIENTIST2_SHOTGUN: return "Scientist";
    case SCARECROW:
    case SCARECROW_DUNGEON: return "Scarecrow";
    case AIRWOLF_SPAWNER: return "Airwolf Vendor";
    case HQM_ORE: return "HQM Ore";
    case HORSE:
    case HORSE2:
    case HORSE3: return "Horse";
    case CHICKEN: return "Chicken";
    case STAG: return "Stag";
    case SHARK: return "Shark";
    case SNAKE: return "Snake";
    case BEE_SWARM:
    case BEE_SWARM2: return "Bee Swarm";
    case TIGER: return "Tiger";
    case PANTHER: return "Panther";

    // Traps
    case FLAME_TURRET: return "Flame Turret";
    case LAND_MINE: return "Land Mine";
    case SAM_SITE: return "SAM Site";
    case GUN_TRAP: return "Shotgun Trap";
    case BEAR_TRAP: return "Bear Trap";
    case TURRETS: return "Auto Turret";

    // Crates
    case ELITE_CRATE:
    case ELITE_CRATE2: return "Elite Crate";
    case BASIC_CRATE: return "Wooden Crate";
    case MILITARY_CRATE: return "Military Crate";
    case NORMAL_CRATE2:
    case NORMAL_CRATE3:
    case UW_CRATE_NORM: return "Normal Crate";
    case TOOL_CRATE:
    case TOOL_CRATE2: return "Tool Crate";
    case HACKABLE_CRATE:
    case HACKABLE_CRATE2:
    case HACKABLE_CRATE3: return "Hackable Crate";
    case AMMO_CRATE: return "Ammo Crate";
    case FOOD_CRATE:
    case FOOD_CRATE2:
    case FOOD_CRATE3:
    case FOOD_CRATE4:
    case FOOD_BOX: return "Food Crate";
    case FUEL_CRATE: return "Fuel Crate";
    case TECH_PARTS:
    case TECH_PARTS2: return "Tech Parts";
    case VEHICLE_PARTS:
    case VEHICLE_PARTS2: return "Vehicle Parts";
    case UW_CRATE_BASIC:
    case UW_CRATE_ADV: return "UW Crate";
    case MEDICAL_CRATE:
    case MEDICAL_CRATE2: return "Medical Crate";
    case MINECART: return "Minecart";
    case MINE_CRATE: return "Mine Crate";
    case BRADLEY_CRATE: return "Bradley Crate";
    case HELI_CRATE: return "Heli Crate";
    case STASH: return "Stash";

    // Barrels
    case RED_BARREL:
    case RED_BARREL2:
    case BLUE_BARREL:
    case BLUE_BARREL2: return "Barrel";
    case OIL_BARREL: return "Oil Barrel";
    case DIESEL: return "Diesel Barrel";

    // Supply drops
    case SUPPLY_DROP:
    case XMAS_PRESENT: return "Supply Drop";

    // Corpses
    case CORPSE:
    case CORPSE2:
    case CORPSE3:
    case CORPSE4:
    case CORPSE5: return "Corpse";

    // Dropped items / backpack
    case BACKPACK: return "Backpack";
    case DROPPED_ITEMS: return "Dropped Item";

    // Deployables
    case TOOL_CUPBOARD:
    case TOOL_CUPBOARD_RETRO:
    case TOOL_CUPBOARD_SHOCK: return "Tool Cupboard";
    case WORKBENCH1:
    case WORKBENCH1_2: return "Workbench T1";
    case WORKBENCH2:
    case WORKBENCH2_2: return "Workbench T2";
    case WORKBENCH3: return "Workbench T3";
    case VENDING_MACHINE: return "Vending Machine";
    case BED: return "Bed";
    case SLEEPING_BAG:
    case BEACH_TOWEL: return "Sleeping Bag";
    case FURNACE:
    case FURNACE2: return "Furnace";
    case FURNACE_LARGE: return "Large Furnace";
    case CAMPFIRE:
    case CAMPFIRE2: return "Campfire";
    case RESEARCH_TABLE:
    case RESEARCH_TABLE2: return "Research Table";
    case LARGE_WOODBOX: return "Large Box";

    // Doors
    case A_single_Door_deployed: return "Door";
    case A_double_Door_deployed: return "Double Door";

    // Batteries
    case BATTERY_SMALL: return "Small Battery";
    case BATTERY_MEDIUM: return "Medium Battery";
    case BATTERY_LARGE: return "Large Battery";

    default: return {};
  }
}

bool IsPlayer(uint32_t id) { return id == BASE_PLAYER; }

bool IsResource(uint32_t id) {
  switch (id) {
    case SULFUR_ORE:
    case SULFUR_ORE2:
    case SULFUR_ORE3:
    case SULFUR_ORE4:
    case MINI_SULFUR_ORE:
    case METAL_ORE:
    case METAL_ORE2:
    case METAL_ORE3:
    case METAL_ORE4:
    case MINI_METAL_ORE:
    case STONE_ORE:
    case STONE_ORE2:
    case STONE_ORE3:
    case STONE_ORE4:
    case MINI_STONE_ORE:
    case WoodPiles:
    case HEMP:
    case HEMP2:
    case YELLOW_BERRY:
    case BLUE_BERRY:
    case RED_BERRY:
    case GREEN_BERRY:
    case BLACK_BERRY:
    case WHITE_BERRY: return true;
    default: return false;
  }
}

Category GetCategory(uint32_t id) {
  switch (id) {
    // ── Players ──
    case BASE_PLAYER: return Category::Player;

    // ── Animals / NPCs ──
    case WOLF: case WOLF2: case BOAR: case BEAR: case POLAR_BEAR:
    case HORSE: case HORSE2: case HORSE3: case CHICKEN:
    case STAG: case SHARK: case SNAKE:
    case BEE_SWARM: case BEE_SWARM2: case TIGER: case PANTHER:
    case BULL: case CALF: case CALF_MALE: case COW:
    case CRAB: case CRAB_SINGLE: case CROCODILE: case FROG:
    case LAMB: case RABBIT: case SHEEP: case SQUIRREL:
    case SEA_TURTLE: case JELLYFISH:
    case SCIENTIST2: case SCIENTIST2_HEAVY: case SCIENTIST2_SHOTGUN:
    case SCARECROW: case SCARECROW_DUNGEON: case AIRWOLF_SPAWNER:
      return Category::Npc;

    // ── Ores / resources ──
    case SULFUR_ORE: case SULFUR_ORE2: case SULFUR_ORE3:
    case SULFUR_ORE4: case MINI_SULFUR_ORE:
    case METAL_ORE: case METAL_ORE2: case METAL_ORE3:
    case METAL_ORE4: case MINI_METAL_ORE:
    case STONE_ORE: case STONE_ORE2: case STONE_ORE3:
    case STONE_ORE4: case MINI_STONE_ORE:
    case HQM_ORE:
    case WoodPiles:
      return Category::Ore;

    // ── Plants / food ──
    case HEMP: case HEMP2:
    case CORN: case CORN2:
    case POTATO: case POTATO2:
    case PUMPKIN: case PUMPKIN2:
    case MUSHROOM: case MUSHROOM2:
    case YELLOW_BERRY: case BLUE_BERRY: case RED_BERRY:
    case GREEN_BERRY: case BLACK_BERRY: case WHITE_BERRY:
      return Category::Plant;

    // ── Crates ──
    case ELITE_CRATE: case ELITE_CRATE2: case BASIC_CRATE:
    case MILITARY_CRATE: case NORMAL_CRATE2: case NORMAL_CRATE3:
    case TOOL_CRATE: case TOOL_CRATE2:
    case HACKABLE_CRATE: case HACKABLE_CRATE2: case HACKABLE_CRATE3:
    case AMMO_CRATE:
    case FOOD_CRATE: case FOOD_CRATE2: case FOOD_CRATE3:
    case FOOD_CRATE4: case FOOD_BOX:
    case FUEL_CRATE: case TECH_PARTS: case TECH_PARTS2:
    case VEHICLE_PARTS: case VEHICLE_PARTS2:
    case UW_CRATE_BASIC: case UW_CRATE_ADV: case UW_CRATE_NORM:
    case MEDICAL_CRATE: case MEDICAL_CRATE2:
    case MINE_CRATE: case BRADLEY_CRATE: case HELI_CRATE:
    case STASH:
      return Category::Crate;

    // ── Barrels ──
    case RED_BARREL: case RED_BARREL2:
    case BLUE_BARREL: case BLUE_BARREL2:
    case OIL_BARREL: case DIESEL:
      return Category::Barrel;

    // ── Loot (dropped items) ──
    case BACKPACK: case DROPPED_ITEMS: case TREASURES:
      return Category::Loot;

    // ── Corpses (their own category so they can be toggled separately) ──
    case CORPSE: case CORPSE2: case CORPSE3: case CORPSE4: case CORPSE5:
      return Category::Corpse;

    // ── Supply drops ──
    case SUPPLY_DROP: case XMAS_PRESENT:
      return Category::SupplyDrop;

    // ── Vehicles ──
    case MINI_COPTER: case BRADLEY: case ROWBOAT: case RHIB:
    case SCRAP_TRANSPORT_HELI: case PATROL_HELI_COPTER: case ATTACK_HELI:
    case TUGBOAT: case SUBMARINE_SOLO: case SUBMARINE_DUO:
    case HOT_AIR_BALLOON: case DPV: case MINECART:
      return Category::Vehicle;

    // ── Traps ──
    case FLAME_TURRET: case LAND_MINE: case SAM_SITE:
    case GUN_TRAP: case BEAR_TRAP: case TURRETS:
      return Category::Trap;

    // ── Doors ──
    case A_single_Door_deployed: case A_double_Door_deployed:
      return Category::Door;

    // ── Batteries ──
    case BATTERY_SMALL: case BATTERY_MEDIUM: case BATTERY_LARGE:
      return Category::Battery;

    // ── Deployables (default bucket for known structures) ──
    case TOOL_CUPBOARD: case TOOL_CUPBOARD_RETRO: case TOOL_CUPBOARD_SHOCK:
    case FURNACE: case FURNACE2: case FURNACE_LARGE:
    case CAMPFIRE: case CAMPFIRE2:
    case RESEARCH_TABLE: case RESEARCH_TABLE2:
    case WORKBENCH1: case WORKBENCH1_2:
    case WORKBENCH2: case WORKBENCH2_2: case WORKBENCH3:
    case VENDING_MACHINE:
    case BED: case SLEEPING_BAG: case BEACH_TOWEL: case LARGE_WOODBOX:
      return Category::Deployable;

    default: return Category::Deployable;
  }
}

const char* CategoryName(Category category) {
  switch (category) {
    case Category::Player:     return "Players";
    case Category::Npc:        return "NPCs";
    case Category::Ore:        return "Ores";
    case Category::Plant:      return "Plants";
    case Category::Crate:      return "Crates";
    case Category::Barrel:     return "Barrels";
    case Category::Loot:       return "Loot";
    case Category::SupplyDrop: return "Supply Drops";
    case Category::Vehicle:    return "Vehicles";
    case Category::Deployable: return "Deployables";
    case Category::Trap:       return "Traps";
    case Category::Door:       return "Doors";
    case Category::Battery:    return "Batteries";
    case Category::Corpse:     return "Corpses";
    default:                   return "Unknown";
  }
}

} // namespace Prefabs
