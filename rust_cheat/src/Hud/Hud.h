
// ================================================================================
// Telegram Hub: @imguisource_bot / ImGui Source @ImGui_Source_FIXNEY / shit src 🧨 @il2cpp_src
// ================================================================================

#pragma once

#include <string>

// Native in-game HUD: toast notifications + the small watermark. Drawn with
// Direct2D directly onto the overlay, so it exists with or without the menu.
namespace hud {

// Queues a toast. Thread: main / UI thread only.
void Notify(const std::string &message, int duration_ms = 2600);

// Draws active toasts and the watermark. Called once per visible frame.
void Render();

} // namespace hud
