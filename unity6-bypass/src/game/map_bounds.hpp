#pragma once
// ---------------------------------------------------------------------------
// Map volumes of the PKM2 collider set shipped in assets/maps.
//
// Generated from the asset headers, not guessed: every row is the exact AABB
// of the map's triangles as they are stored in assets/maps/<Name>.pkm.
// Source of the assets: Standoff 2 1.1.0 / versionCode 103982 (Android APK +
// OBB), i.e. the same build the mobile profile in game.hpp targets.
//
// The XZ ranges overlap heavily between maps (Prison alone covers most of the
// set), so an AABB only pre-selects candidates. The decisive test lives in
// map_world.hpp: the ground under a player's feet has to be at the expected
// height for that map.
// ---------------------------------------------------------------------------

#include "../other/vector3.h"

#include <cstdint>

namespace mapworld {

struct MapEntry {
    const char* name;
    float lo[3];
    float hi[3];
    uint32_t triangles;
};

inline constexpr MapEntry kMaps[] = {
    {"Arena", {-24.104f, -3.154f, -33.407f}, {33.481f, 57.918f, 33.622f}, 17392u},
    {"Block", {-41.162f, -2.840f, -34.152f}, {28.800f, 19.943f, 36.931f}, 20035u},
    {"Breeze", {-106.295f, -23.077f, -53.770f}, {157.718f, 70.978f, 146.378f}, 85111u},
    {"Bridge", {-31.277f, -3.794f, -40.522f}, {39.473f, 23.066f, 17.962f}, 24383u},
    {"Cableway", {-31.313f, -14.177f, -36.508f}, {22.145f, 19.326f, 33.786f}, 33237u},
    {"Dune", {-94.211f, -3.701f, -89.139f}, {43.102f, 91.728f, 86.946f}, 83711u},
    {"Favelas", {-57.406f, -4.534f, -87.092f}, {55.945f, 61.426f, 88.190f}, 42155u},
    {"Hanami", {-61.184f, -5.291f, -55.511f}, {67.306f, 21.441f, 52.080f}, 79830u},
    {"Hanari", {-61.184f, -4.080f, -55.511f}, {67.306f, 60.335f, 32.045f}, 52774u},
    {"Perimeter", {-105.126f, -3.494f, -204.784f}, {130.693f, 17.240f, 114.578f}, 23902u},
    {"Pipeline", {-33.452f, -0.537f, -50.503f}, {34.292f, 27.707f, 43.000f}, 15327u},
    {"Polygon", {-31.200f, -0.976f, -17.498f}, {27.288f, 9.745f, 46.286f}, 6125u},
    {"Pool", {-241.063f, -16.049f, -268.651f}, {170.054f, 19.995f, 84.495f}, 17512u},
    {"Prison", {-141.536f, -9.000f, -214.293f}, {130.693f, 128.058f, 117.842f}, 67326u},
    {"Province", {-79.272f, -9.326f, -117.718f}, {58.057f, 44.714f, 87.388f}, 83371u},
    {"Pyramid", {-250.822f, -4.297f, -117.422f}, {195.647f, 26.285f, 213.707f}, 43665u},
    {"Rust", {-85.220f, -2.500f, -69.572f}, {48.195f, 78.656f, 101.967f}, 62600u},
    {"Sandstone", {-84.657f, -3.115f, -77.594f}, {75.177f, 29.027f, 52.346f}, 59244u},
    {"Sandyards", {-84.657f, -2.633f, -71.201f}, {63.353f, 28.481f, 31.358f}, 42137u},
    {"Temple", {-49.705f, -5.239f, -77.302f}, {57.798f, 20.088f, 57.077f}, 27110u},
    {"TrainingOutside", {-75.930f, -19.007f, -61.053f}, {70.575f, 31.465f, 82.416f}, 41137u},
    {"Village", {-65.281f, -2.169f, -75.169f}, {53.960f, 58.221f, 75.807f}, 35297u},
    {"Yard", {-35.532f, -4.534f, -26.230f}, {18.101f, 36.697f, 21.717f}, 22991u},
};

inline constexpr int kMapCount = int(sizeof(kMaps) / sizeof(kMaps[0]));

inline bool in_bounds(const MapEntry& map, const Vector3& point, float margin) {
    return point.x >= map.lo[0] - margin && point.x <= map.hi[0] + margin &&
           point.y >= map.lo[1] - margin && point.y <= map.hi[1] + margin &&
           point.z >= map.lo[2] - margin && point.z <= map.hi[2] + margin;
}

inline float volume(const MapEntry& map) {
    const float sx = map.hi[0] - map.lo[0];
    const float sy = map.hi[1] - map.lo[1];
    const float sz = map.hi[2] - map.lo[2];
    return (sx > 0.f ? sx : 0.f) * (sy > 0.f ? sy : 0.f) * (sz > 0.f ? sz : 0.f);
}

} // namespace mapworld
