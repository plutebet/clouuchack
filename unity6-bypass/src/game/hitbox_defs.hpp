#pragma once
#include "../other/vector3.h"
#include "player.hpp"
#include <array>
#include <cmath>
#include <optional>

namespace hitbox {

struct HitboxDef {
    bool capsule;
    float cx, cy, cz;    // bone-local center offset
    float sx, sy, sz;     // OBB half-extents (OBB only)
    float radius;         // capsule radius
    float height;         // capsule height
    int direction;        // capsule axis in bone-local: 0=X, 1=Y
};

// SO2 1.1.0 collider prefab dimensions, indexed by HumanBodyBones ID.
inline constexpr std::array<HitboxDef, player::kBoneCount> kDefs = {{
    {false, 5.96e-08f,0.f,0.00018f, 0.339f,0.137f,0.254f, 0,0,0},         // 0  Hips (OBB)
    {true,  -0.216f,0.007f,0.010f,  0,0,0, 0.098f,0.574f,0},               // 1  LeftUpperLeg
    {true,  0.216f,0.007f,-0.010f,  0,0,0, 0.098f,0.574f,0},               // 2  RightUpperLeg
    {true,  -0.206f,-0.008f,0.f,    0,0,0, 0.083f,0.576f,0},               // 3  LeftLowerLeg
    {true,  0.206f,-0.008f,0.f,     0,0,0, 0.083f,0.576f,0},               // 4  RightLowerLeg
    {false, 0.042f,0.023f,-0.002f,  0.289f,0.088f,0.122f, 0,0,0},          // 5  LeftFoot (OBB)
    {false, -0.037f,-0.030f,0.002f, 0.289f,0.088f,0.122f, 0,0,0},         // 6  RightFoot (OBB)
    {false, 0.f,-0.010f,0.001f,     0.325f,0.188f,0.250f, 0,0,0},          // 7  Spine (OBB)
    {false, 0.f,0.117f,0.001f,      0.365f,0.257f,0.232f, 0,0,0},          // 8  Chest (OBB)
    {true,  0.f,0.056f,-0.023f,     0,0,0, 0.078f,0.193f,1},               // 9  Neck
    {true,  0.f,0.100f,-0.015f,     0,0,0, 0.109f,0.301f,1},               // 10 Head
    {true,  -0.093f,-0.013f,0.f,    0,0,0, 0.093f,0.249f,0},               // 11 LeftShoulder
    {true,  0.093f,0.016f,0.f,      0,0,0, 0.093f,0.249f,0},               // 12 RightShoulder
    {true,  -0.142f,0.f,0.f,        0,0,0, 0.070f,0.312f,0},               // 13 LeftUpperArm
    {true,  0.142f,0.f,0.f,         0,0,0, 0.070f,0.312f,0},               // 14 RightUpperArm
    {true,  -0.114f,0.f,0.f,        0,0,0, 0.060f,0.346f,0},               // 15 LeftLowerArm
    {true,  0.114f,0.f,0.f,         0,0,0, 0.060f,0.346f,0},               // 16 RightLowerArm
    {false, -0.047f,-0.014f,-0.009f,0.151f,0.080f,0.099f, 0,0,0},          // 17 LeftHand (OBB)
    {false, 0.047f,0.019f,0.009f,   0.151f,0.080f,0.099f, 0,0,0},          // 18 RightHand (OBB)
}};

struct HitVolume {
    int bone;
    bool capsule;
    Vector3 center;
    Vector3 a, b;     // capsule endpoints (world space)
    float radius;
};

// Skeleton connectivity: for capsule bones, estimate axis from parent→child.
inline Vector3 bone_axis(int bone, const player::BoneData& bones) {
    using namespace player;
    static constexpr int kAxisConn[][2] = {
        {0, 0},   // 0  Hips (OBB)
        {0, 3},   // 1  LeftUpperLeg → LeftLowerLeg
        {0, 4},   // 2  RightUpperLeg → RightLowerLeg
        {1, 5},   // 3  LeftLowerLeg → LeftFoot
        {2, 6},   // 4  RightLowerLeg → RightFoot
        {0, 0},   // 5  LeftFoot (OBB)
        {0, 0},   // 6  RightFoot (OBB)
        {0, 0},   // 7  Spine (OBB)
        {0, 0},   // 8  Chest (OBB)
        {8,10},   // 9  Neck → Head
        {9,10},   // 10 Head (upward from neck)
        {8,13},   // 11 LeftShoulder → LeftUpperArm
        {8,14},   // 12 RightShoulder → RightUpperArm
        {11,15},  // 13 LeftUpperArm → LeftLowerArm
        {12,16},  // 14 RightUpperArm → RightLowerArm
        {13,17},  // 15 LeftLowerArm → LeftHand
        {14,18},  // 16 RightLowerArm → RightHand
        {0, 0},   // 17 LeftHand (OBB)
        {0, 0},   // 18 RightHand (OBB)
    };
    if (bone < 0 || bone >= kBoneCount || !kDefs[bone].capsule) return {0.f, 1.f, 0.f};
    int from = kAxisConn[bone][0], to = kAxisConn[bone][1];
    if (!bones.has(from) || !bones.has(to)) return {0.f, 1.f, 0.f};
    Vector3 dir = bones.positions[to] - bones.positions[from];
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 0.001f) return {0.f, 1.f, 0.f};
    return {dir.x / len, dir.y / len, dir.z / len};
}

inline int make_hit_volumes(const player::BoneData& bones, HitVolume* out, float scale) {
    int count = 0;
    for (int i = 0; i < player::kBoneCount; ++i) {
        if (!bones.has(i)) continue;
        const HitboxDef& def = kDefs[i];
        HitVolume& vol = out[count];
        vol.bone = i;
        vol.center = bones.positions[i];
        if (def.capsule) {
            vol.capsule = true;
            vol.radius = def.radius * scale;
            float half_h = std::max(0.f, def.height * 0.5f - def.radius) * scale;
            Vector3 axis = bone_axis(i, bones);
            vol.a = {vol.center.x - axis.x * half_h, vol.center.y - axis.y * half_h, vol.center.z - axis.z * half_h};
            vol.b = {vol.center.x + axis.x * half_h, vol.center.y + axis.y * half_h, vol.center.z + axis.z * half_h};
        } else {
            vol.capsule = false;
            float r = std::max({def.sx, def.sy, def.sz}) * 0.5f * scale;
            vol.radius = std::max(r, 0.04f);
            vol.a = vol.b = vol.center;
        }
        ++count;
    }
    return count;
}

// ---- Ray intersection (ported from PC trigger_logic.cpp) ----

inline std::optional<float> ray_sphere(Vector3 o, Vector3 d, Vector3 c, float r) {
    Vector3 p = {o.x - c.x, o.y - c.y, o.z - c.z};
    float b = p.x * d.x + p.y * d.y + p.z * d.z;
    float cc = p.x * p.x + p.y * p.y + p.z * p.z - r * r;
    if (cc <= 0.f) return 0.f;
    float h = b * b - cc;
    if (h < 0.f) return std::nullopt;
    float t = -b - std::sqrt(h);
    return t >= 0.f ? std::optional<float>(t) : std::nullopt;
}

inline std::optional<float> ray_capsule(Vector3 o, Vector3 d, Vector3 ca, Vector3 cb, float r) {
    Vector3 ba = {cb.x - ca.x, cb.y - ca.y, cb.z - ca.z};
    Vector3 oa = {o.x - ca.x, o.y - ca.y, o.z - ca.z};
    float baba = ba.x * ba.x + ba.y * ba.y + ba.z * ba.z;

    // Inside-capsule check
    float u = std::clamp((oa.x * ba.x + oa.y * ba.y + oa.z * ba.z) / std::max(baba, 1e-8f), 0.f, 1.f);
    Vector3 nearest = {ca.x + ba.x * u, ca.y + ba.y * u, ca.z + ba.z * u};
    Vector3 diff = {o.x - nearest.x, o.y - nearest.y, o.z - nearest.z};
    if (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z <= r * r) return 0.f;

    std::optional<float> best;
    auto accept = [&](std::optional<float> t) { if (t && (!best || *t < *best)) best = t; };

    if (baba > 1e-8f) {
        float bard = ba.x * d.x + ba.y * d.y + ba.z * d.z;
        float baoa = ba.x * oa.x + ba.y * oa.y + ba.z * oa.z;
        float rdoa = d.x * oa.x + d.y * oa.y + d.z * oa.z;
        float a = baba - bard * bard;
        float b = baba * rdoa - baoa * bard;
        float c = baba * (oa.x * oa.x + oa.y * oa.y + oa.z * oa.z) - baoa * baoa - r * r * baba;
        float h = b * b - a * c;
        if (a > 1e-8f && h >= 0.f) {
            float t = (-b - std::sqrt(h)) / a;
            float y = baoa + t * bard;
            if (t >= 0.f && y >= 0.f && y <= baba) accept(t);
        }
    }
    accept(ray_sphere(o, d, ca, r));
    accept(ray_sphere(o, d, cb, r));
    return best;
}

inline std::optional<float> ray_hit_volume(Vector3 o, Vector3 d, const HitVolume& v) {
    if (v.capsule) return ray_capsule(o, d, v.a, v.b, v.radius);
    return ray_sphere(o, d, v.center, v.radius);
}

// ---- Camera extraction from view matrix (column-major OpenGL) ----

inline Vector3 camera_eye(const float* v) {
    return {
        -(v[0] * v[12] + v[1] * v[13] + v[2] * v[14]),
        -(v[4] * v[12] + v[5] * v[13] + v[6] * v[14]),
        -(v[8] * v[12] + v[9] * v[13] + v[10] * v[14])
    };
}

inline Vector3 camera_forward(const float* v) {
    float fx = -v[2], fy = -v[6], fz = -v[10];
    float len = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (len < 0.001f) return {0.f, 0.f, 1.f};
    return {fx / len, fy / len, fz / len};
}

inline int bone_damage_group(int bone) {
    if (bone == player::BoneHead) return 0;
    if (bone >= player::BoneLeftUpperLeg && bone <= player::BoneRightFoot) return 3;
    if (bone == player::BoneHips || bone == player::BoneSpine) return 2;
    return 1;
}

} // namespace hitbox
