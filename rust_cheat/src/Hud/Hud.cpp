
// ================================================================================
// Origin: @imguisource_bot
// Community:
//  - ImGui Source: @ImGui_Source_FIXNEY
//  - shit src 🧨: @il2cpp_src
// ================================================================================

#include "Hud.h"

#include "Features/Visuals/Esp/ImageEsp.h"
#include "Menu/Config.h"
#include "libs/overlay/Overlay.h"
#include "libs/render/Renderer.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

namespace hud {
namespace {

struct Toast {
  std::string text;
  std::chrono::steady_clock::time_point start;
  std::chrono::steady_clock::time_point end;
};

std::vector<Toast> g_toasts;

constexpr float kPanelX = 18.f;
constexpr float kPanelY = 18.f;
constexpr float kToastH = 44.f;
constexpr float kGap = 8.f;

render::Color accent() {
  if (g_cfg)
    return render::from_float(g_cfg->accent);
  return render::rgba(255, 122, 26);
}

void DrawToasts() {
  if (g_toasts.empty())
    return;

  const auto now = std::chrono::steady_clock::now();
  int index = 0;
  for (auto it = g_toasts.begin(); it != g_toasts.end();) {
    if (now >= it->end) {
      it = g_toasts.erase(it);
      continue;
    }

    const float total =
        std::chrono::duration<float>(it->end - it->start).count();
    const float elapsed =
        std::chrono::duration<float>(now - it->start).count();
    const float remain = total > 0.f ? std::clamp(1.f - elapsed / total, 0.f, 1.f)
                                     : 0.f;

    const render::Vec2 size = render::measure_text(it->text, 12.5f);
    const float width = std::max(248.f, size.x + 46.f);
    const float y = kPanelY + index * (kToastH + kGap);

    render::fill_rect({kPanelX, y}, {kPanelX + width, y + kToastH},
                      render::rgba(12, 13, 17, 238), 8.f);
    render::rect({kPanelX, y}, {kPanelX + width, y + kToastH},
                 render::rgba(255, 255, 255, 20), 1.f, 8.f);
    // Accent rail.
    render::fill_rect({kPanelX + 1.f, y + 1.f}, {kPanelX + 4.f, y + kToastH - 1.f},
                      accent(), 2.f);
    render::text({kPanelX + 18.f, y + 13.f}, it->text,
                 render::rgba(236, 239, 245));
    // Expiry bar.
    render::fill_rect({kPanelX + 10.f, y + kToastH - 4.f},
                      {kPanelX + 10.f + (width - 20.f) * remain,
                       y + kToastH - 3.f},
                      render::with_alpha(accent(), 0.65f));

    ++index;
    ++it;
  }
}

void DrawWatermark() {
  if (!g_cfg || !g_cfg->watermark || !overlay)
    return;

  char buffer[160];
  std::snprintf(buffer, sizeof(buffer), "%d FPS", (int)(overlay->get_fps() + 0.5f));

  const std::string left = "LOSTKERNEL";
  const std::string mid = "  //  ";
  const std::string right = buffer;
  const std::string tail = g_cfg->esp_enabled ? "  ESP ON" : "  ESP OFF";

  const render::Vec2 l = render::measure_text(left, 11.f);
  const render::Vec2 m = render::measure_text(mid, 11.f);
  const render::Vec2 r = render::measure_text(right, 11.f);
  const render::Vec2 t = render::measure_text(tail, 11.f);

  const float width = l.x + m.x + r.x + t.x + 28.f;
  const float x = overlay->window_width - width - 18.f;
  const float y = 16.f;
  const float h = 28.f;

  render::fill_rect({x, y}, {x + width, y + h}, render::rgba(12, 13, 17, 190),
                    6.f);
  render::rect({x, y}, {x + width, y + h}, render::rgba(255, 255, 255, 16),
               1.f, 6.f);
  render::fill_circle({x + 12.f, y + h * 0.5f}, 2.5f, accent());

  float cx = x + 22.f;
  render::text({cx, y + 5.f}, left, render::rgba(240, 242, 247), 11.f,
               render::TextStyle::Plain);
  cx += l.x;
  render::text({cx, y + 5.f}, mid, render::rgba(105, 110, 122), 11.f,
               render::TextStyle::Plain);
  cx += m.x;
  render::text({cx, y + 5.f}, right, accent(), 11.f, render::TextStyle::Plain);
  cx += r.x;
  render::text({cx, y + 5.f}, tail,
               g_cfg->esp_enabled ? render::rgba(120, 200, 140)
                                  : render::rgba(160, 110, 110),
               11.f, render::TextStyle::Plain);
}

} // namespace

void Notify(const std::string &message, int duration_ms) {
  const auto now = std::chrono::steady_clock::now();
  Toast toast;
  toast.text = message;
  toast.start = now;
  toast.end = now + std::chrono::milliseconds(std::max(300, duration_ms));
  g_toasts.push_back(std::move(toast));
  if (g_toasts.size() > 5)
    g_toasts.erase(g_toasts.begin());
}

void Render() {
  DrawToasts();
  DrawWatermark();

  // Hidden development probe: validates the WIC -> D2D icon path when the game
  // is not attached.
  static const bool image_test = [] {
    wchar_t value[8]{};
    return GetEnvironmentVariableW(L"VEXUS_IMAGE_TEST", value, 8) > 0;
  }();
  if (image_test)
    image_esp::RenderTestIcons(80.f, 330.f);
}

} // namespace hud
