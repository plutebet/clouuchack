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

using Color = uint32_t;

enum class TextStyle { Plain, Shadow, Outline };

bool init(IDXGISwapChain *sc, ID3D11Texture2D *target);
void shutdown();
bool ready();
void resize(IDXGISwapChain *sc, ID3D11Texture2D *target);

void begin_frame();
void end_frame();
void release_back_buffer();

void text(Vec2 pos, const std::string &str, Color col, float size = 14.f,
          TextStyle style = TextStyle::Plain);
Vec2 measure_text(const std::string &str, float size);

void line(Vec2 a, Vec2 b, Color col, float thickness = 1.f);
void lines(Vec2 *segments, int count, Color col, float thickness = 1.f);
void rect(Vec2 a, Vec2 b, Color col, float thickness = 1.f, float radius = 0.f);
void fill_rect(Vec2 a, Vec2 b, Color col, float radius = 0.f);
void circle(Vec2 center, float radius, Color col, float thickness = 1.f);
void fill_circle(Vec2 center, float radius, Color col);

Color from_float(const float c[4], float alpha_mul = 1.f);
Color with_alpha(Color c, float alpha);
Color rgba(int r, int g, int b, int a = 255);

bool draw_image(const std::wstring &path, Vec2 pos, float size);
bool preload_image(const std::wstring &path);

} // namespace render
