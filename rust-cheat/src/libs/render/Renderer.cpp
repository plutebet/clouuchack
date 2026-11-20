#include "Renderer.h"

#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace render {
namespace {

ID2D1Factory1 *g_factory = nullptr;
ID2D1RenderTarget *g_target = nullptr;
IDWriteFactory *g_dwrite = nullptr;
ID2D1SolidColorBrush *g_brush = nullptr;
bool g_in_frame = false;

D2D1_COLOR_F to_d2d(Color c) {
  return D2D1::ColorF(
      ((c >> 0) & 0xFF) / 255.f,
      ((c >> 8) & 0xFF) / 255.f,
      ((c >> 16) & 0xFF) / 255.f,
      ((c >> 24) & 0xFF) / 255.f);
}

void set_brush(Color c) {
  if (g_brush)
    g_brush->SetColor(to_d2d(c));
}

IDWriteTextFormat *get_format(float size) {
  IDWriteTextFormat *fmt = nullptr;
  if (g_dwrite)
    g_dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
                               DWRITE_FONT_STYLE_NORMAL,
                               DWRITE_FONT_STRETCH_NORMAL, size, L"en-us",
                               &fmt);
  return fmt;
}

bool create_target(IDXGISwapChain *sc) {
  if (!sc || !g_factory)
    return false;

  IDXGISurface *surface = nullptr;
  if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&surface))) || !surface)
    return false;

  D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));

  HRESULT hr = g_factory->CreateDxgiSurfaceRenderTarget(surface, &props, &g_target);
  surface->Release();
  if (FAILED(hr) || !g_target)
    return false;

  g_target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  g_target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
  hr = g_target->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &g_brush);
  if (FAILED(hr))
    g_brush = nullptr;
  return true;
}

} // namespace

bool init(IDXGISwapChain *sc, ID3D11Texture2D *) {
  if (!g_factory) {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                   __uuidof(ID2D1Factory1),
                                   reinterpret_cast<void **>(&g_factory));
    if (FAILED(hr) || !g_factory)
      return false;
  }
  if (!g_dwrite) {
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown **>(&g_dwrite));
  }
  return create_target(sc);
}

void shutdown() {
  if (g_brush) { g_brush->Release(); g_brush = nullptr; }
  if (g_target) { g_target->Release(); g_target = nullptr; }
  if (g_dwrite) { g_dwrite->Release(); g_dwrite = nullptr; }
  if (g_factory) { g_factory->Release(); g_factory = nullptr; }
  g_in_frame = false;
}

bool ready() { return g_target != nullptr; }

void resize(IDXGISwapChain *sc, ID3D11Texture2D *target) {
  if (g_brush) { g_brush->Release(); g_brush = nullptr; }
  if (g_target) { g_target->Release(); g_target = nullptr; }
  create_target(sc);
}

void release_back_buffer() {
  if (g_in_frame && g_target) {
    g_target->EndDraw();
    g_in_frame = false;
  }
  if (g_brush) { g_brush->Release(); g_brush = nullptr; }
  if (g_target) { g_target->Release(); g_target = nullptr; }
}

void begin_frame() {
  if (!g_target)
    return;
  g_target->BeginDraw();
  g_in_frame = true;
}

void end_frame() {
  if (!g_target || !g_in_frame)
    return;
  HRESULT hr = g_target->EndDraw();
  g_in_frame = false;
  if (hr == D2DERR_RECREATE_TARGET) {
    if (g_brush) { g_brush->Release(); g_brush = nullptr; }
    g_target->Release();
    g_target = nullptr;
  }
}

void text(Vec2 pos, const std::string &str, Color col, float size,
          TextStyle style) {
  if (!g_target || !g_brush || !g_dwrite || !g_in_frame)
    return;

  const int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(),
                                        nullptr, 0);
  if (wlen <= 0)
    return;
  std::wstring wide(wlen, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wide[0], wlen);

  IDWriteTextFormat *fmt = get_format(size);
  if (!fmt)
    return;

  D2D1_RECT_F layout = {pos.x, pos.y, pos.x + 4096.f, pos.y + 4096.f};

  if (style == TextStyle::Shadow || style == TextStyle::Outline) {
    set_brush(rgba(0, 0, 0, (int)(((col >> 24) & 0xFF) * 0.85f)));
    D2D1_RECT_F shadow = {pos.x + 1.f, pos.y + 1.f, layout.right + 1.f,
                           layout.bottom + 1.f};
    g_target->DrawText(wide.c_str(), (UINT32)wide.size(), fmt, shadow, g_brush);
    if (style == TextStyle::Outline) {
      shadow = {pos.x - 1.f, pos.y, layout.right - 1.f, layout.bottom};
      g_target->DrawText(wide.c_str(), (UINT32)wide.size(), fmt, shadow, g_brush);
      shadow = {pos.x, pos.y - 1.f, layout.right, layout.bottom - 1.f};
      g_target->DrawText(wide.c_str(), (UINT32)wide.size(), fmt, shadow, g_brush);
    }
  }

  set_brush(col);
  g_target->DrawText(wide.c_str(), (UINT32)wide.size(), fmt, layout, g_brush);
  fmt->Release();
}

Vec2 measure_text(const std::string &str, float size) {
  if (!g_dwrite || str.empty())
    return {0.f, 0.f};

  const int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(),
                                        nullptr, 0);
  if (wlen <= 0)
    return {0.f, 0.f};
  std::wstring wide(wlen, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wide[0], wlen);

  IDWriteTextFormat *fmt = get_format(size);
  if (!fmt)
    return {0.f, 0.f};

  IDWriteTextLayout *layout = nullptr;
  g_dwrite->CreateTextLayout(wide.c_str(), (UINT32)wide.size(), fmt, 4096.f,
                             4096.f, &layout);
  fmt->Release();
  if (!layout)
    return {0.f, 0.f};

  DWRITE_TEXT_METRICS metrics{};
  layout->GetMetrics(&metrics);
  layout->Release();
  return {metrics.widthIncludingTrailingWhitespace, metrics.height};
}

void line(Vec2 a, Vec2 b, Color col, float thickness) {
  if (!g_target || !g_brush || !g_in_frame)
    return;
  set_brush(col);
  g_target->DrawLine({a.x, a.y}, {b.x, b.y}, g_brush, thickness);
}

void lines(Vec2 *segments, int count, Color col, float thickness) {
  if (!g_target || !g_brush || !g_in_frame || count < 2)
    return;
  set_brush(col);
  for (int i = 0; i < count - 1; i += 2)
    g_target->DrawLine({segments[i].x, segments[i].y},
                       {segments[i + 1].x, segments[i + 1].y}, g_brush,
                       thickness);
}

void rect(Vec2 a, Vec2 b, Color col, float thickness, float radius) {
  if (!g_target || !g_brush || !g_in_frame)
    return;
  set_brush(col);
  if (radius > 0.f)
    g_target->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(a.x, a.y, b.x, b.y), radius, radius),
        g_brush, thickness);
  else
    g_target->DrawRectangle(D2D1::RectF(a.x, a.y, b.x, b.y), g_brush,
                            thickness);
}

void fill_rect(Vec2 a, Vec2 b, Color col, float radius) {
  if (!g_target || !g_brush || !g_in_frame)
    return;
  set_brush(col);
  if (radius > 0.f)
    g_target->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(a.x, a.y, b.x, b.y), radius, radius),
        g_brush);
  else
    g_target->FillRectangle(D2D1::RectF(a.x, a.y, b.x, b.y), g_brush);
}

void fill_circle(Vec2 center, float radius, Color col) {
  if (!g_target || !g_brush || !g_in_frame)
    return;
  set_brush(col);
  g_target->FillEllipse(D2D1::Ellipse({center.x, center.y}, radius, radius),
                        g_brush);
}

Color from_float(const float c[4], float alpha_mul) {
  auto clamp = [](float v) -> uint8_t {
    return (uint8_t)std::clamp((int)(v * 255.f + 0.5f), 0, 255);
  };
  return (uint32_t)clamp(c[0]) | ((uint32_t)clamp(c[1]) << 8) |
         ((uint32_t)clamp(c[2]) << 16) |
         ((uint32_t)clamp(c[3] * alpha_mul) << 24);
}

Color with_alpha(Color c, float alpha) {
  uint8_t a = (uint8_t)std::clamp((int)(alpha * 255.f + 0.5f), 0, 255);
  return (c & 0x00FFFFFF) | ((uint32_t)a << 24);
}

Color rgba(int r, int g, int b, int a) {
  return (uint32_t)(r & 0xFF) | ((uint32_t)(g & 0xFF) << 8) |
         ((uint32_t)(b & 0xFF) << 16) | ((uint32_t)(a & 0xFF) << 24);
}

} // namespace render
