
// ================================================================================
// Community links:
// - @imguisource_bot
// - @ImGui_Source_FIXNEY (ImGui Source)
// - @il2cpp_src (shit src 🧨)
// ================================================================================

#pragma once

#include <cstdint>
#include <vector>

#include "GetEntities/EntityCaching.h"
#include "World2Screen/Math/Math.h"

namespace image_esp {

// Draws the 16×16 PNG icon layer over the already-prepared view matrix.
// Called from esp::Render() after the normal box/label pass.
void Render(const std::vector<CachedEntity> &entities, uintptr_t local,
            const Vector3 &local_pos, bool have_local, float screen_w,
            float screen_h);

// Decodes every icon once at startup. Doing this up front keeps the large
// players/sleepers art (1920x1080 and 1400x1400) from causing a decode hitch
// the first time a player appears mid-fight.
void Preload();

// Development aid: draws the icon set at a fixed overlay position. Used only
// when VEXUS_IMAGE_TEST is set (validates WIC decoding without a live game).
void RenderTestIcons(float x, float y);

} // namespace image_esp
