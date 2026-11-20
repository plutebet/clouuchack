
// ================================================================================
// @project     ImGui Source
// @bot         t.me/imguisource_bot
// @channel_1   t.me/ImGui_Source_FIXNEY (ImGui Source)
// @channel_2   t.me/il2cpp_src (shit src 🧨)
// ================================================================================

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  Aim assist (externally hosted, but drives the view through the driver).
//
//  The aim runs on its OWN worker thread, never on the render thread. That is
//  deliberate: writing the view angles is a driver round-trip, and doing that
//  from the render loop put the frame path behind the driver's shared mutex -
//  which is what made the overlay (and the game) lose FPS.
//
//  The camera is turned by writing the local player's view angles:
//    localPlayer + 0x400      -> PlayerInput  (bodyAngles {pitch,yaw} degrees)
//    playerEyes  + 0x50       -> PlayerEyes.bodyRotation (Quaternion)
//  bodyAngles is what makes the view visibly move (the reference's default,
//  non-silent path). The eye quaternion is the reference's "silent" path - it
//  drives the stored/aimed rotation. Both are written from the same smoothed
//  angle so the view turns smoothly and the server sees the same direction.
//
//  playerEyes is NOT a plain pointer: BasePlayer.eyes is a GC-handle-encrypted
//  field, so it is resolved with decryptor::decrypt_eyes() (see ResolveEyes).
//
//  A second method shifts the real mouse with relative SendInput moves instead
//  (needs the game to own the foreground). Selectable from the Aim page.
//
//  Off by default. The worker only touches the game while the bound key is
//  held (or toggled, if "Hold" is off) and never while the menu is open.
// ─────────────────────────────────────────────────────────────────────────────

namespace aim {

// Starts / stops the aim worker thread. Start() after g_entities exists.
void Start();
void Stop();

// Called once per render frame (with a frame open). Draws the FOV circle and
// the lock marker from the state the worker published. Does NO driver I/O.
void Tick();

} // namespace aim
