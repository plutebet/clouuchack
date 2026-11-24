#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>

#include <chrono>
#include <mutex>
#include <vector>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

template <typename T>
void safe_release(T *&p) {
  if (p) {
    p->Release();
    p = nullptr;
  }
}

class c_overlay {
public:
  c_overlay();
  ~c_overlay();

  bool is_ready() const { return window_handle && d3d_device && swap_chain; }
  bool begin_frame();
  void end_frame();
  HWND get_game_hwnd() const { return game_hwnd; }
  float get_fps() const { return fps_value; }

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  HWND window_handle = nullptr;
  ID3D11Device *d3d_device = nullptr;
  ID3D11DeviceContext *device_context = nullptr;
  IDXGISwapChain *swap_chain = nullptr;
  ID3D11Texture2D *scene_texture = nullptr;

  FLOAT window_width = 0.f;
  FLOAT window_height = 0.f;
  bool visible = false;
  bool exit = false;
  bool menu_open = false;
  bool vsync = false;
  float fps_value = 0.f;

  HWND game_hwnd = nullptr;
  DWORD game_pid = 0;

  UINT backbuffer_w = 0;
  UINT backbuffer_h = 0;
  ID3D11RenderTargetView *backbuffer_rtv = nullptr;
  ID3D11RenderTargetView *scene_rtv = nullptr;
  std::vector<ID3D11Texture2D *> backbuffer_textures;

  ID3D11Texture2D *capture_texture = nullptr;
  ID3D11ShaderResourceView *capture_srv = nullptr;
  bool capture_ready = false;
  bool capture_is_window = false;
  RECT capture_rect{};
  HWND capture_hwnd = nullptr;

  ATOM wnd_class_atom = 0;
  bool allow_tearing = false;
  UINT swap_buffer_count = 1;

  IDXGIOutputDuplication *duplication = nullptr;
  DXGI_OUTPUT_DESC output_desc{};

  ID3D11VertexShader *bg_vs = nullptr;
  ID3D11PixelShader *bg_ps = nullptr;
  ID3D11InputLayout *bg_layout = nullptr;
  ID3D11Buffer *bg_vb = nullptr;
  ID3D11Buffer *bg_cb = nullptr;
  ID3D11SamplerState *bg_sampler = nullptr;

  winrt::Windows::Graphics::Capture::GraphicsCaptureItem wgc_item{nullptr};
  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice wgc_device{nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool wgc_pool{nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureSession wgc_session{nullptr};
  winrt::event_token wgc_token{};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame wgc_pending{nullptr};
  std::mutex wgc_mutex;

  std::chrono::steady_clock::time_point last_frame_end{};

  void init_render_target();
  void dest_render_target();

private:
  bool locate_game_window();
  bool create_window();
  bool init_device();
  void dest_device();
  bool create_capture_texture(UINT width, UINT height);
  bool init_capture();
  bool init_capture_wgc();
  bool init_capture_duplication();
  void release_capture();
  bool capture_frame();
  bool init_background();
  void release_background();
  void draw_background();
  void sync_overlay();
};

extern c_overlay *overlay;
