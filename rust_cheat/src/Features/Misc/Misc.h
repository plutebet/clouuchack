
// ================================================================================
// Protected source: @imguisource_bot
// Official channels: t.me/ImGui_Source_FIXNEY & t.me/il2cpp_src
// ================================================================================

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  Spider-Man climb (ported from the pawjob-rust external, re-keyed for this
//  build's dump).
//
//  Rust's PlayerWalkMovement keeps the angle of the ground under the player in
//  two *encrypted* float fields (groundAngle @ +0x108 and groundAngleNew @
//  +0x110). The walk code uses them to decide the surface is too steep and
//  slides you off walls and cliffs. While this feature is on we overwrite both
//  fields with an encrypted 0.0, so the game always believes it is standing on
//  flat ground and lets you walk straight up any surface.
//
//  The dump's BasePlayer::movement offset (0x2f0) is verified at runtime
//  against the movement object's back-pointer (BaseMovement::Owner) and the
//  offset is re-found by a CHUNKED scan if the live layout has drifted.
//
//  Runs on its own worker thread (Start/Stop). It never touches the render
//  thread: every driver read/write is a round-trip on the transport's global
//  mutex, and doing that from the render loop is what made enabling this
//  feature freeze the overlay for a second at a time.
//
//  Off by default. Nothing is written while the toggle is off.
// ─────────────────────────────────────────────────────────────────────────────

namespace misc {

// Start/stop the movement-feature worker. Start once the overlay exists; the
// worker does nothing until the Spider-Man toggle is on.
void Start();
void Stop();

} // namespace misc
