
// ================================================================================
// t.me/imguisource_bot | t.me/ImGui_Source_FIXNEY (ImGui Source) | t.me/il2cpp_src (shit src 🧨)
// ================================================================================

#pragma once

// Opt-in external chams.
//
// The game stores a small array of material IDs inside every native renderer.
// Writing a known glow/wallhack material ID into those slots makes the player
// model render with that material. Everything is done through the kernel driver,
// on a dedicated worker thread, and only while the player enables the feature.
//
// Chams.cpp validates every pointer in the chain (playerModel -> multiMesh ->
// Renderers -> native renderer -> material array) before writing. Invalid or
// stale chains are skipped instead of being written blindly.
namespace chams {

// Curated material presets for the 2026-11-06 Rust dump (build 26480361).
// Material IDs are build-specific; the values in Chams.cpp are re-read from
// the dump's Materials section when the game updates.
enum MaterialPreset : int {
  kGreenGlow = 0,   // GreenGlow (core glow material - safe default)
  kRedGlow = 1,     // RedGlow
  kNeon = 2,        // NeonTR_On
  kBody = 3,        // generic Body
  kManpadCrt = 4,   // manpad_crt_hdr - through-walls / X-Ray
  kDefaultMat = 5,  // Default_Material - guaranteed loaded (safe test)
  kSteven = 6,      // green while visible, red while behind cover
  kPresetCount = 7
};

constexpr int kDefaultPreset = kGreenGlow;

// Maps a preset index to the in-game material ID, falling back to the default
// preset when the value is out of range.
int MaterialIdForPreset(int preset);

// Worker lifecycle. Safe to call once at startup / shutdown.
void Start();
void Stop();

} // namespace chams
