#pragma once
// ---------------------------------------------------------------------------
// Map colliders linked into the executable (src/game/maps_embedded.S, built
// from assets/maps/*.pkm with .incbin). Lets the trigger work with the binary
// alone: nothing has to be pushed next to it.
//
// The rise build (RISE_BUILD) does not link the blob object and never uses
// baked geometry, so its table stays empty.
// ---------------------------------------------------------------------------

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mapworld {

struct EmbeddedMap {
    const char* name;
    const uint8_t* data;
    uint32_t size;
};

#ifdef RISE_BUILD

inline constexpr int kEmbeddedMapCount = 0;
inline const EmbeddedMap* const kEmbeddedMaps = nullptr;

#else

extern "C" const uint8_t map_blob_Arena_start[];
extern "C" const uint8_t map_blob_Arena_end[];
extern "C" const uint8_t map_blob_Block_start[];
extern "C" const uint8_t map_blob_Block_end[];
extern "C" const uint8_t map_blob_Breeze_start[];
extern "C" const uint8_t map_blob_Breeze_end[];
extern "C" const uint8_t map_blob_Bridge_start[];
extern "C" const uint8_t map_blob_Bridge_end[];
extern "C" const uint8_t map_blob_Cableway_start[];
extern "C" const uint8_t map_blob_Cableway_end[];
extern "C" const uint8_t map_blob_Dune_start[];
extern "C" const uint8_t map_blob_Dune_end[];
extern "C" const uint8_t map_blob_Favelas_start[];
extern "C" const uint8_t map_blob_Favelas_end[];
extern "C" const uint8_t map_blob_Hanami_start[];
extern "C" const uint8_t map_blob_Hanami_end[];
extern "C" const uint8_t map_blob_Hanari_start[];
extern "C" const uint8_t map_blob_Hanari_end[];
extern "C" const uint8_t map_blob_Perimeter_start[];
extern "C" const uint8_t map_blob_Perimeter_end[];
extern "C" const uint8_t map_blob_Pipeline_start[];
extern "C" const uint8_t map_blob_Pipeline_end[];
extern "C" const uint8_t map_blob_Polygon_start[];
extern "C" const uint8_t map_blob_Polygon_end[];
extern "C" const uint8_t map_blob_Pool_start[];
extern "C" const uint8_t map_blob_Pool_end[];
extern "C" const uint8_t map_blob_Prison_start[];
extern "C" const uint8_t map_blob_Prison_end[];
extern "C" const uint8_t map_blob_Province_start[];
extern "C" const uint8_t map_blob_Province_end[];
extern "C" const uint8_t map_blob_Pyramid_start[];
extern "C" const uint8_t map_blob_Pyramid_end[];
extern "C" const uint8_t map_blob_Rust_start[];
extern "C" const uint8_t map_blob_Rust_end[];
extern "C" const uint8_t map_blob_Sandstone_start[];
extern "C" const uint8_t map_blob_Sandstone_end[];
extern "C" const uint8_t map_blob_Sandyards_start[];
extern "C" const uint8_t map_blob_Sandyards_end[];
extern "C" const uint8_t map_blob_Temple_start[];
extern "C" const uint8_t map_blob_Temple_end[];
extern "C" const uint8_t map_blob_TrainingOutside_start[];
extern "C" const uint8_t map_blob_TrainingOutside_end[];
extern "C" const uint8_t map_blob_Village_start[];
extern "C" const uint8_t map_blob_Village_end[];
extern "C" const uint8_t map_blob_Yard_start[];
extern "C" const uint8_t map_blob_Yard_end[];

// size = end - start, evaluated at startup: the blobs cannot change at runtime.
inline const EmbeddedMap kEmbeddedMaps[] = {
    {"Arena", map_blob_Arena_start, uint32_t(map_blob_Arena_end - map_blob_Arena_start)},
    {"Block", map_blob_Block_start, uint32_t(map_blob_Block_end - map_blob_Block_start)},
    {"Breeze", map_blob_Breeze_start, uint32_t(map_blob_Breeze_end - map_blob_Breeze_start)},
    {"Bridge", map_blob_Bridge_start, uint32_t(map_blob_Bridge_end - map_blob_Bridge_start)},
    {"Cableway", map_blob_Cableway_start, uint32_t(map_blob_Cableway_end - map_blob_Cableway_start)},
    {"Dune", map_blob_Dune_start, uint32_t(map_blob_Dune_end - map_blob_Dune_start)},
    {"Favelas", map_blob_Favelas_start, uint32_t(map_blob_Favelas_end - map_blob_Favelas_start)},
    {"Hanami", map_blob_Hanami_start, uint32_t(map_blob_Hanami_end - map_blob_Hanami_start)},
    {"Hanari", map_blob_Hanari_start, uint32_t(map_blob_Hanari_end - map_blob_Hanari_start)},
    {"Perimeter", map_blob_Perimeter_start, uint32_t(map_blob_Perimeter_end - map_blob_Perimeter_start)},
    {"Pipeline", map_blob_Pipeline_start, uint32_t(map_blob_Pipeline_end - map_blob_Pipeline_start)},
    {"Polygon", map_blob_Polygon_start, uint32_t(map_blob_Polygon_end - map_blob_Polygon_start)},
    {"Pool", map_blob_Pool_start, uint32_t(map_blob_Pool_end - map_blob_Pool_start)},
    {"Prison", map_blob_Prison_start, uint32_t(map_blob_Prison_end - map_blob_Prison_start)},
    {"Province", map_blob_Province_start, uint32_t(map_blob_Province_end - map_blob_Province_start)},
    {"Pyramid", map_blob_Pyramid_start, uint32_t(map_blob_Pyramid_end - map_blob_Pyramid_start)},
    {"Rust", map_blob_Rust_start, uint32_t(map_blob_Rust_end - map_blob_Rust_start)},
    {"Sandstone", map_blob_Sandstone_start, uint32_t(map_blob_Sandstone_end - map_blob_Sandstone_start)},
    {"Sandyards", map_blob_Sandyards_start, uint32_t(map_blob_Sandyards_end - map_blob_Sandyards_start)},
    {"Temple", map_blob_Temple_start, uint32_t(map_blob_Temple_end - map_blob_Temple_start)},
    {"TrainingOutside", map_blob_TrainingOutside_start, uint32_t(map_blob_TrainingOutside_end - map_blob_TrainingOutside_start)},
    {"Village", map_blob_Village_start, uint32_t(map_blob_Village_end - map_blob_Village_start)},
    {"Yard", map_blob_Yard_start, uint32_t(map_blob_Yard_end - map_blob_Yard_start)},
};
inline constexpr int kEmbeddedMapCount = int(sizeof(kEmbeddedMaps) / sizeof(kEmbeddedMaps[0]));

#endif

inline const EmbeddedMap* find_embedded(const char* name) {
#ifndef RISE_BUILD
    for (int i = 0; i < kEmbeddedMapCount; ++i)
        if (kEmbeddedMaps[i].name && std::strcmp(kEmbeddedMaps[i].name, name) == 0) return &kEmbeddedMaps[i];
#endif
    (void)name;
    return nullptr;
}

} // namespace mapworld
