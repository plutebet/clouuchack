#pragma once

#include <cstdint>
#include <string>

struct ID3D11Texture2D;
struct IDXGISwapChain;

namespace render {

struct Vec2 {
  float x = 0.f;
  float y = 0.f;
  Vec2() = default;
  Vec2(float x_, float y_) : x(x_), y(y_) {}
};

struct Color {
  float r = 0.f, g = 0.f, b = 0.f, a = 1.f;
};

enum class TextStyle { Plain, Shadow, Outline };

bool init(IDXGISwapChain *sc, ID3D11Texture2D *target);
void shutdown();
bool ready();
void resize(IDXGISwapChain *sc, ID3D11Texture2D *target);

void begin_frame();
void end_frame();
void release_back_buffer();

void text(const Vec2 &pos, const std::string &str, const Color &col,
          float size = 14.f, TextStyle style = TextStyle::Plain);
Vec2 measure_text(const std::string &str, float size);

void line(const Vec2 &a, const Vec2 &b, const Color &col, float thickness = 1.f);
void lines(const Vec2 *segments, int count, const Color &col, float thickness = 1.f);
void rect(const Vec2 &a, const Vec2 &b, const Color &col, float thickness = 1.f, float radius = 0.f);
void fill_rect(const Vec2 &a, const Vec2 &b, const Color &col, float radius = 0.f);
void circle(const Vec2 &center, float radius, const Color &col, float thickness = 1.f);
void fill_circle(const Vec2 &center, float radius, const Color &col);

Color from_float(const float c[4], float alpha_override = -1.f);
Color with_alpha(const Color &c, float alpha);
Color rgba(int r, int g, int b, int a = 255);

bool draw_image(const std::wstring &path, const Vec2 &pos, float size);
bool preload_image(const std::wstring &path);

} // namespace render
