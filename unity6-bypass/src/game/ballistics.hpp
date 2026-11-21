#pragma once
#include "hitbox_defs.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ballistics {

struct WeaponEntry {
    uint8_t id;
    float damage[4];           // {head, body, pelvis+arms, legs}
    float armor_penetration;   // 0-100
    float range;               // max effective range (m)
    float falloff_start;       // distance where damage starts dropping
    float falloff_end;         // distance where damage hits minimum
    float falloff_min;         // minimum damage multiplier at max range
};

// SO2 1.1.0 weapon damage tables (extracted from game data).
// Groups: 0=head, 1=body(chest/neck/shoulders/arms), 2=pelvis+spine, 3=legs
inline constexpr WeaponEntry kWeapons[] = {
    // Rifles
    { 1, {120.f, 30.f, 27.f, 22.f}, 47.5f, 60.f, 15.f, 50.f, 0.70f},  // AKR
    { 2, {132.f, 33.f, 29.f, 24.f}, 70.0f, 55.f, 15.f, 45.f, 0.75f},  // M4
    { 3, {128.f, 32.f, 28.f, 23.f}, 55.0f, 55.f, 15.f, 50.f, 0.72f},  // AUG
    { 4, {136.f, 34.f, 30.f, 25.f}, 65.0f, 55.f, 15.f, 50.f, 0.74f},  // G36
    { 5, {120.f, 30.f, 27.f, 22.f}, 50.0f, 55.f, 15.f, 50.f, 0.70f},  // FAMAS
    // Snipers
    {10, {400.f,100.f, 90.f, 75.f}, 95.0f,120.f, 40.f,100.f, 0.90f},  // AWM
    {11, {340.f, 85.f, 76.f, 63.f}, 90.0f,100.f, 35.f, 90.f, 0.85f},  // Scout
    // SMGs
    {20, { 92.f, 23.f, 20.f, 17.f}, 30.0f, 35.f, 10.f, 30.f, 0.55f},  // MP5
    {21, {100.f, 25.f, 22.f, 19.f}, 25.0f, 30.f, 10.f, 25.f, 0.50f},  // UMP
    {22, { 96.f, 24.f, 21.f, 18.f}, 28.0f, 30.f, 10.f, 28.f, 0.52f},  // P90
    // Shotguns
    {30, {180.f, 45.f, 40.f, 36.f}, 20.0f, 15.f,  5.f, 12.f, 0.30f},  // Nova
    {31, {160.f, 40.f, 36.f, 32.f}, 25.0f, 18.f,  5.f, 15.f, 0.35f},  // Sawed-off
    // Pistols
    {40, {212.f, 53.f, 47.f, 40.f}, 75.5f, 45.f, 12.f, 40.f, 0.70f},  // Deagle
    {41, {100.f, 25.f, 22.f, 19.f}, 20.0f, 35.f, 10.f, 30.f, 0.60f},  // USP
    {42, {120.f, 30.f, 27.f, 22.f}, 30.0f, 30.f, 10.f, 25.f, 0.55f},  // Glock
    {43, {128.f, 32.f, 28.f, 24.f}, 50.0f, 35.f, 12.f, 30.f, 0.65f},  // Five-Seven
    // MGs
    {50, {120.f, 30.f, 27.f, 22.f}, 60.0f, 65.f, 20.f, 55.f, 0.72f},  // M249
    {51, {128.f, 32.f, 28.f, 24.f}, 55.0f, 60.f, 18.f, 50.f, 0.70f},  // Negev
};
inline constexpr int kWeaponCount = int(sizeof(kWeapons) / sizeof(kWeapons[0]));

// Default weapon for when active weapon is unknown (rifle-class average).
inline constexpr WeaponEntry kDefault = {0, {120.f, 30.f, 27.f, 22.f}, 50.0f, 55.f, 15.f, 50.f, 0.70f};

inline float distance_falloff(const WeaponEntry& w, float dist) {
    if (dist <= w.falloff_start) return 1.f;
    if (dist >= w.falloff_end) return w.falloff_min;
    float t = (dist - w.falloff_start) / (w.falloff_end - w.falloff_start);
    return 1.f - t * (1.f - w.falloff_min);
}

// Estimate damage for a bone hit at a given distance.
// bone_group: 0=head, 1=body, 2=pelvis+arms, 3=legs
// armor: 0 = no armor, 1+ = has armor
inline float estimate_damage(const WeaponEntry& w, int bone_group, int armor, float distance) {
    bone_group = std::clamp(bone_group, 0, 3);
    float base = w.damage[bone_group];
    float falloff = distance_falloff(w, distance);
    bool is_leg = (bone_group == 3);
    float armor_scale = (armor > 0 && !is_leg)
        ? std::clamp(w.armor_penetration / 100.f, 0.35f, 1.f) : 1.f;
    return base * falloff * armor_scale;
}

// Quick check: would this shot likely kill (damage >= hp)?
inline bool would_kill(int hp, int bone_group, int armor, float distance) {
    return estimate_damage(kDefault, bone_group, armor, distance) >= float(hp);
}

} // namespace ballistics
