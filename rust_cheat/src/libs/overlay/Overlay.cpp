
// ================================================================================
// Телеграм экосистема:
// - Бот: t.me/imguisource_bot
// - Канал 1: t.me/ImGui_Source_FIXNEY (ImGui Source)
// - Канал 2: t.me/il2cpp_src (shit src 🧨)
// ================================================================================

#include "Overlay.h"

#include "../render/Renderer.h"

c_overlay *overlay = nullptr;

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <d3dcompiler.h>
#include <dwmapi.h>
#include <dxgi1_4.h>
#include <dxgi1_5.h>
#include <tlhelp32.h>
#include <vector>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowsapp.lib")

/*
 Asgard-style DWM-glass overlay.
  - topmost layered window synced to the RustClient.exe client rect
  - DwmExtendFrameIntoClientArea(-1) + clear-to-(0,0,0,0) => only our pixels show
  - Present is VSync'd only when overlay->vsync is set
*/

namespace {

DWORD PidOfProcess(const wchar_t *exeName) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return 0;
  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  DWORD pid = 0;
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, exeName) == 0) {
        pid = pe.th32ProcessID;
        break;
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return pid;
}

struct EnumCtx {
  DWORD pid;
  HWND hwnd;
  LONG area;
};

BOOL CALLBACK EnumProc(HWND h, LPARAM lp) {
  EnumCtx *c = reinterpret_cast<EnumCtx *>(lp);
  DWORD wpid = 0;
  GetWindowThreadProcessId(h, &wpid);
  if (wpid != c->pid)
    return TRUE;
  if (!IsWindowVisible(h))
    return TRUE;
  if (GetWindow(h, GW_OWNER) != nullptr)
    return TRUE;
  RECT r{};
  GetClientRect(h, &r);
  const LONG w = r.right - r.left;
  const LONG hgt = r.bottom - r.top;
  if (w <= 0 || hgt <= 0)
    return TRUE;
  const LONG area = w * hgt;
  if (area <= c->area)
    return TRUE;
  c->hwnd = h;
  c->area = area;
  return TRUE;
}

HWND FindMainWindow(DWORD pid) {
  EnumCtx c{pid, nullptr, 0};
  EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&c));
  return c.hwnd;
}

} // namespace

LRESULT CALLBACK c_overlay::WndProc(HWND hwnd, UINT msg, WPARAM wp,
                                    LPARAM lp) {
  switch (msg) {
  case WM_NCHITTEST:
    // The overlay is a pure HUD: every click belongs to the game / menu.
    return HTTRANSPARENT;
  case WM_ERASEBKGND:
    return 1;
  case WM_SIZE:
    if (overlay && overlay->d3d_device && wp != SIZE_MINIMIZED) {
      const UINT nw = (UINT)LOWORD(lp);
      const UINT nh = (UINT)HIWORD(lp);
      // ResizeBuffers drains the GPU pipeline and can block for a long time
      // while the game is rendering, so only do it when the back buffer really
      // changed size (ShowWindow / focus transitions fire WM_SIZE with the same
      // dimensions, which used to trigger a pointless blocking resize).
      if (nw && nh &&
          (nw != overlay->backbuffer_w || nh != overlay->backbuffer_h)) {
        render::release_back_buffer();
        overlay->dest_render_target();
        overlay->swap_chain->ResizeBuffers(
            0, 0, 0, DXGI_FORMAT_UNKNOWN,
            overlay->allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
        overlay->init_render_target();
        overlay->backbuffer_w = nw;
        overlay->backbuffer_h = nh;
        render::resize(overlay->swap_chain, overlay->scene_texture);
      }
    }
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

bool c_overlay::locate_game_window() {
#if RPM_DISABLE_GAME_WINDOW
  return false;
#else
  const DWORD pid = PidOfProcess(L"RustClient.exe");
  if (!pid)
    return false;
  game_pid = pid;
  game_hwnd = FindMainWindow(pid);
  return game_hwnd != nullptr;
#endif
}

bool c_overlay::create_window() {
  locate_game_window();

  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"VexusOverlay";
  wnd_class_atom = RegisterClassExW(&wc);

  // Start on the game window if we found it, otherwise fall back to the
  // primary monitor for the menu-only case.
  int x = 0, y = 0;
  int w = GetSystemMetrics(SM_CXSCREEN);
  int h = GetSystemMetrics(SM_CYSCREEN);
  if (game_hwnd) {
    RECT rc{};
    GetClientRect(game_hwnd, &rc);
    POINT tl{0, 0};
    ClientToScreen(game_hwnd, &tl);
    x = tl.x;
    y = tl.y;
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
      w = GetSystemMetrics(SM_CXSCREEN);
      h = GetSystemMetrics(SM_CYSCREEN);
    }
  }

  window_handle = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT |
          WS_EX_NOACTIVATE | WS_EX_LAYERED,
      wc.lpszClassName, L"", WS_POPUP, x, y, w, h, nullptr, nullptr,
      wc.hInstance, nullptr);
  if (!window_handle)
    return false;

  ShowWindow(window_handle, SW_SHOW);
  SetLayeredWindowAttributes(window_handle, RGB(0, 0, 0), 255, LWA_ALPHA);
  MARGINS margins{-1, -1, -1, -1};
  DwmExtendFrameIntoClientArea(window_handle, &margins);
  SetWindowPos(window_handle, HWND_TOPMOST, x, y, w, h,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);

  window_width = static_cast<FLOAT>(w);
  window_height = static_cast<FLOAT>(h);
  return true;
}

bool c_overlay::init_device() {
  D3D_FEATURE_LEVEL got{};
  // BGRA support is required for the Direct2D render target.
  HRESULT hr = D3D11CreateDevice(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      nullptr, 0, D3D11_SDK_VERSION, &d3d_device, &got, &device_context);
  if (FAILED(hr) || !d3d_device || !device_context)
    return false;

  IDXGIDevice *dxgi_device = nullptr;
  IDXGIAdapter *adapter = nullptr;
  IDXGIFactory2 *factory = nullptr;
  if (FAILED(d3d_device->QueryInterface(IID_PPV_ARGS(&dxgi_device))) ||
      FAILED(dxgi_device->GetAdapter(&adapter)) ||
      FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) {
    safe_release(dxgi_device);
    safe_release(adapter);
    safe_release(factory);
    return false;
  }

  RECT rc{};
  GetClientRect(window_handle, &rc);
  UINT sw = (rc.right - rc.left) ? (UINT)(rc.right - rc.left) : 1920u;
  UINT sh = (rc.bottom - rc.top) ? (UINT)(rc.bottom - rc.top) : 1080u;

  allow_tearing = false;

  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferDesc.Width = sw;
  sd.BufferDesc.Height = sh;
  sd.BufferDesc.RefreshRate = {60, 1};
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.SampleDesc.Count = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = 1;
  sd.OutputWindow = window_handle;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  hr = factory->CreateSwapChain(d3d_device, &sd, &swap_chain);
  if (SUCCEEDED(hr))
    factory->MakeWindowAssociation(window_handle, DXGI_MWA_NO_ALT_ENTER);

  safe_release(dxgi_device);
  safe_release(adapter);
  safe_release(factory);
  if (FAILED(hr) || !swap_chain)
    return false;

  swap_buffer_count = 1;
  backbuffer_w = sw;
  backbuffer_h = sh;

  init_render_target();
  if (!render::init(swap_chain, scene_texture))
    printf("[overlay] Direct2D target creation failed\n");
  return true;
}

void c_overlay::dest_device() {
  release_capture();
  release_background();
  render::shutdown();
  dest_render_target();
  safe_release(swap_chain);
  safe_release(device_context);
  safe_release(d3d_device);
}

void c_overlay::init_render_target() {
  dest_render_target();
  for (UINT i = 0; i < swap_buffer_count; ++i) {
    ID3D11Texture2D *back_buffer = nullptr;
    if (SUCCEEDED(swap_chain->GetBuffer(i, IID_PPV_ARGS(&back_buffer))) &&
        back_buffer)
      backbuffer_textures.push_back(back_buffer);
  }
  if (!backbuffer_textures.empty() && backbuffer_textures[0])
    d3d_device->CreateRenderTargetView(backbuffer_textures[0], nullptr,
                                       &backbuffer_rtv);

  D3D11_TEXTURE2D_DESC td{};
  td.Width = backbuffer_w;
  td.Height = backbuffer_h;
  td.MipLevels = 1;
  td.ArraySize = 1;
  td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_DEFAULT;
  td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  if (FAILED(d3d_device->CreateTexture2D(&td, nullptr, &scene_texture)) ||
      !scene_texture)
    return;
  if (FAILED(d3d_device->CreateRenderTargetView(scene_texture, nullptr,
                                                &scene_rtv)) ||
      !scene_rtv)
    return;
}

void c_overlay::dest_render_target() {
  if (backbuffer_rtv) {
    backbuffer_rtv->Release();
    backbuffer_rtv = nullptr;
  }
  for (ID3D11Texture2D *tex : backbuffer_textures) {
    if (tex)
      tex->Release();
  }
  backbuffer_textures.clear();
  if (scene_rtv) {
    scene_rtv->Release();
    scene_rtv = nullptr;
  }
  if (scene_texture) {
    scene_texture->Release();
    scene_texture = nullptr;
  }
}

bool c_overlay::create_capture_texture(UINT width, UINT height) {
  if (!d3d_device || !width || !height)
    return false;
  safe_release(capture_srv);
  safe_release(capture_texture);

  D3D11_TEXTURE2D_DESC td{};
  td.Width = width;
  td.Height = height;
  td.MipLevels = 1;
  td.ArraySize = 1;
  td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_DEFAULT;
  td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  if (FAILED(d3d_device->CreateTexture2D(&td, nullptr, &capture_texture)) ||
      !capture_texture)
    return false;
  if (FAILED(d3d_device->CreateShaderResourceView(capture_texture, nullptr,
                                                   &capture_srv)) ||
      !capture_srv)
    return false;
  return true;
}

bool c_overlay::init_capture() {
  release_capture();
  const HWND target = game_hwnd;
  if (target && init_capture_wgc()) {
    capture_hwnd = target;
    return true;
  }
  release_capture();
  if (init_capture_duplication()) {
    capture_hwnd = target;
    return true;
  }
  capture_hwnd = target;
  return false;
}

bool c_overlay::init_capture_wgc() {
  if (!game_hwnd || !d3d_device)
    return false;

  try {
    auto interop = winrt::get_activation_factory<
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
        ::IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{nullptr};
    winrt::check_hresult(interop->CreateForWindow(
        game_hwnd,
        winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
        winrt::put_abi(item)));
    if (!item)
      return false;

    winrt::com_ptr<IDXGIDevice> dxgi_device;
    winrt::check_hresult(
        d3d_device->QueryInterface(IID_PPV_ARGS(dxgi_device.put())));
    winrt::com_ptr<::IInspectable> inspectable_device;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(
        dxgi_device.get(), inspectable_device.put()));
    auto direct3d_device =
        inspectable_device.as<
            winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();

    const auto size = item.Size();
    if (size.Width <= 0 || size.Height <= 0)
      return false;

    wgc_item = item;
    wgc_device = direct3d_device;
    wgc_pool =
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::
            CreateFreeThreaded(
                wgc_device,
                winrt::Windows::Graphics::DirectX::DirectXPixelFormat::
                    B8G8R8A8UIntNormalized,
                2, size);
    wgc_session = wgc_pool.CreateCaptureSession(wgc_item);
    wgc_session.IsCursorCaptureEnabled(false);
    // Windows 11 22H2+: remove the yellow capture border. Older systems throw
    // or do not have the property; the capture still works without it.
    try {
      wgc_session.IsBorderRequired(false);
    } catch (...) {
    }
    wgc_token = wgc_pool.FrameArrived(
        [this](const winrt::Windows::Graphics::Capture::
                   Direct3D11CaptureFramePool &sender,
               const winrt::Windows::Foundation::IInspectable &) {
          auto frame = sender.TryGetNextFrame();
          if (!frame)
            return;
          std::lock_guard<std::mutex> lock(wgc_mutex);
          if (wgc_pending)
            wgc_pending.Close();
          wgc_pending = frame;
        });
    wgc_session.StartCapture();

    if (!create_capture_texture((UINT)size.Width, (UINT)size.Height))
      return false;

    capture_rect = {0, 0, size.Width, size.Height};
    capture_is_window = true;
    capture_ready = false;
    printf("[overlay] WGC window capture: %dx%d\n", size.Width, size.Height);
    return true;
  } catch (const winrt::hresult_error &e) {
    printf("[overlay] WGC window capture failed (0x%08X)\n",
           (unsigned)e.code());
    return false;
  } catch (...) {
    printf("[overlay] WGC window capture failed\n");
    return false;
  }
}

bool c_overlay::init_capture_duplication() {
  if (!d3d_device || !window_handle)
    return false;

  IDXGIDevice *dxgi_device = nullptr;
  IDXGIAdapter *adapter = nullptr;
  if (FAILED(d3d_device->QueryInterface(IID_PPV_ARGS(&dxgi_device))) ||
      !dxgi_device || FAILED(dxgi_device->GetAdapter(&adapter)) || !adapter) {
    safe_release(dxgi_device);
    safe_release(adapter);
    return false;
  }

  RECT wr{};
  GetWindowRect(window_handle, &wr);
  POINT center{wr.left + (wr.right - wr.left) / 2,
               wr.top + (wr.bottom - wr.top) / 2};

  IDXGIOutput *chosen = nullptr;
  for (UINT i = 0;; ++i) {
    IDXGIOutput *out = nullptr;
    if (adapter->EnumOutputs(i, &out) == DXGI_ERROR_NOT_FOUND)
      break;
    if (!out)
      break;
    DXGI_OUTPUT_DESC desc{};
    if (SUCCEEDED(out->GetDesc(&desc)) &&
        PtInRect(&desc.DesktopCoordinates, center)) {
      chosen = out;
      output_desc = desc;
      break;
    }
    out->Release();
  }
  if (!chosen) {
    if (FAILED(adapter->EnumOutputs(0, &chosen)) || !chosen) {
      safe_release(dxgi_device);
      safe_release(adapter);
      return false;
    }
    chosen->GetDesc(&output_desc);
  }

  IDXGIOutput1 *output1 = nullptr;
  HRESULT hr = chosen->QueryInterface(IID_PPV_ARGS(&output1));
  if (SUCCEEDED(hr) && output1)
    hr = output1->DuplicateOutput(d3d_device, &duplication);
  safe_release(output1);
  safe_release(chosen);
  safe_release(dxgi_device);
  safe_release(adapter);
  if (FAILED(hr) || !duplication) {
    printf("[overlay] DuplicateOutput failed (0x%08X)\n", (unsigned)hr);
    return false;
  }

  const UINT out_w = (UINT)(output_desc.DesktopCoordinates.right -
                            output_desc.DesktopCoordinates.left);
  const UINT out_h = (UINT)(output_desc.DesktopCoordinates.bottom -
                            output_desc.DesktopCoordinates.top);
  if (!out_w || !out_h || !create_capture_texture(out_w, out_h))
    return false;

  capture_rect = output_desc.DesktopCoordinates;
  capture_is_window = false;
  capture_ready = false;
  printf("[overlay] desktop duplication: output %ld,%ld %ux%u\n",
         output_desc.DesktopCoordinates.left, output_desc.DesktopCoordinates.top,
         out_w, out_h);
  return true;
}

void c_overlay::release_capture() {
  if (wgc_pool)
    wgc_pool.FrameArrived(wgc_token);
  {
    std::lock_guard<std::mutex> lock(wgc_mutex);
    if (wgc_pending) {
      wgc_pending.Close();
      wgc_pending = nullptr;
    }
  }
  if (wgc_session) {
    wgc_session.Close();
    wgc_session = nullptr;
  }
  if (wgc_pool) {
    wgc_pool.Close();
    wgc_pool = nullptr;
  }
  wgc_item = nullptr;
  wgc_device = nullptr;
  safe_release(capture_srv);
  safe_release(capture_texture);
  safe_release(duplication);
  capture_ready = false;
  capture_is_window = false;
  capture_rect = {};
  capture_hwnd = nullptr;
}

bool c_overlay::capture_frame() {
  if (capture_is_window) {
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame{nullptr};
    {
      std::lock_guard<std::mutex> lock(wgc_mutex);
      frame = wgc_pending;
      wgc_pending = nullptr;
    }
    if (!frame)
      return capture_ready;

    auto surface = frame.Surface();
    auto access =
        surface.as<::Windows::Graphics::DirectX::Direct3D11::
                       IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<ID3D11Texture2D> texture;
    if (access && SUCCEEDED(access->GetInterface(
                      IID_PPV_ARGS(texture.put()))) &&
        texture) {
      D3D11_TEXTURE2D_DESC desc{};
      texture->GetDesc(&desc);
      if (desc.Width != (UINT)capture_rect.right ||
          desc.Height != (UINT)capture_rect.bottom) {
        if (!create_capture_texture(desc.Width, desc.Height)) {
          frame.Close();
          return false;
        }
        capture_rect = {0, 0, (LONG)desc.Width, (LONG)desc.Height};
      }
      device_context->CopyResource(capture_texture, texture.get());
      capture_ready = true;
    }
    frame.Close();
    return capture_ready;
  }

  if (!duplication || !capture_texture)
    return false;

  DXGI_OUTDUPL_FRAME_INFO info{};
  IDXGIResource *resource = nullptr;
  HRESULT hr = duplication->AcquireNextFrame(0, &info, &resource);
  if (hr == DXGI_ERROR_WAIT_TIMEOUT)
    return capture_ready;
  if (hr == DXGI_ERROR_ACCESS_LOST) {
    release_capture();
    init_capture();
    return false;
  }
  if (FAILED(hr) || !resource)
    return false;

  ID3D11Texture2D *frame = nullptr;
  if (SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&frame))) && frame) {
    device_context->CopyResource(capture_texture, frame);
    capture_ready = true;
    frame->Release();
  }
  resource->Release();
  duplication->ReleaseFrame();
  return capture_ready;
}

bool c_overlay::init_background() {
  release_background();
  if (!d3d_device)
    return false;

  static const char *kShader = R"(
    cbuffer UV : register(b0) { float4 uvScaleBias; };
    struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
    VSOut VS(float4 pos : POSITION, float2 uv : TEXCOORD0) {
      VSOut o;
      o.pos = pos;
      o.uv = uv * uvScaleBias.xy + uvScaleBias.zw;
      return o;
    }
    Texture2D tex : register(t0);
    SamplerState samp : register(s0);
    float4 PS(VSOut i) : SV_TARGET {
      return tex.Sample(samp, i.uv);
    }
  )";

  ID3DBlob *vs_blob = nullptr;
  ID3DBlob *ps_blob = nullptr;
  ID3DBlob *err = nullptr;
  HRESULT hr = D3DCompile(kShader, std::strlen(kShader), "overlay_bg",
                          nullptr, nullptr, "VS", "vs_5_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs_blob, &err);
  if (FAILED(hr) || !vs_blob) {
    if (err)
      printf("[overlay] VS compile: %s\n", (const char *)err->GetBufferPointer());
    safe_release(err);
    safe_release(vs_blob);
    return false;
  }
  if (err) { err->Release(); err = nullptr; }

  hr = D3DCompile(kShader, std::strlen(kShader), "overlay_bg", nullptr, nullptr,
                  "PS", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps_blob,
                  &err);
  if (FAILED(hr) || !ps_blob) {
    if (err)
      printf("[overlay] PS compile: %s\n", (const char *)err->GetBufferPointer());
    safe_release(err);
    safe_release(vs_blob);
    safe_release(ps_blob);
    return false;
  }
  if (err) { err->Release(); err = nullptr; }

  hr = d3d_device->CreateVertexShader(vs_blob->GetBufferPointer(),
                                      vs_blob->GetBufferSize(), nullptr, &bg_vs);
  if (SUCCEEDED(hr))
    hr = d3d_device->CreatePixelShader(ps_blob->GetBufferPointer(),
                                       ps_blob->GetBufferSize(), nullptr, &bg_ps);

  D3D11_INPUT_ELEMENT_DESC layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
  };
  if (SUCCEEDED(hr))
    hr = d3d_device->CreateInputLayout(layout, 2, vs_blob->GetBufferPointer(),
                                       vs_blob->GetBufferSize(), &bg_layout);
  safe_release(vs_blob);
  safe_release(ps_blob);
  if (FAILED(hr) || !bg_vs || !bg_ps || !bg_layout)
    return false;

  struct Vertex {
    float x, y, z, w;
    float u, v;
  };
  const Vertex verts[4] = {
      {-1.f, 1.f, 0.f, 1.f, 0.f, 0.f},
      {1.f, 1.f, 0.f, 1.f, 1.f, 0.f},
      {-1.f, -1.f, 0.f, 1.f, 0.f, 1.f},
      {1.f, -1.f, 0.f, 1.f, 1.f, 1.f},
  };
  D3D11_BUFFER_DESC vbd{};
  vbd.ByteWidth = sizeof(verts);
  vbd.Usage = D3D11_USAGE_IMMUTABLE;
  vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA vinit{verts, 0, 0};
  if (FAILED(d3d_device->CreateBuffer(&vbd, &vinit, &bg_vb)) || !bg_vb)
    return false;

  D3D11_BUFFER_DESC cbd{};
  cbd.ByteWidth = sizeof(float) * 4;
  cbd.Usage = D3D11_USAGE_DYNAMIC;
  cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (FAILED(d3d_device->CreateBuffer(&cbd, nullptr, &bg_cb)) || !bg_cb)
    return false;

  D3D11_SAMPLER_DESC sd{};
  sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sd.MinLOD = 0.f;
  sd.MaxLOD = D3D11_FLOAT32_MAX;
  if (FAILED(d3d_device->CreateSamplerState(&sd, &bg_sampler)) || !bg_sampler)
    return false;

  return true;
}

void c_overlay::release_background() {
  safe_release(bg_sampler);
  safe_release(bg_cb);
  safe_release(bg_vb);
  safe_release(bg_layout);
  safe_release(bg_ps);
  safe_release(bg_vs);
}

void c_overlay::draw_background(unsigned int buffer_index) {
  (void)buffer_index;
  if (!capture_ready || !capture_srv || !bg_vs || !bg_ps || !bg_layout ||
      !bg_vb || !bg_cb || !bg_sampler || !device_context)
    return;

  RECT rc{};
  GetClientRect(window_handle, &rc);
  const float w = (float)(rc.right - rc.left);
  const float h = (float)(rc.bottom - rc.top);
  const LONG cap_w = capture_rect.right - capture_rect.left;
  const LONG cap_h = capture_rect.bottom - capture_rect.top;
  if (w <= 0.f || h <= 0.f || cap_w <= 0 || cap_h <= 0)
    return;

  POINT tl{0, 0};
  ClientToScreen(window_handle, &tl);
  const float uv_scale_x = w / (float)cap_w;
  const float uv_scale_y = h / (float)cap_h;
  // WGC captures the game window's client area, which is exactly what the
  // overlay covers. Desktop Duplication captures a whole monitor, so the
  // overlay's screen position becomes a sub-rect of that texture.
  const float uv_bias_x =
      capture_is_window ? 0.f : (float)(tl.x - capture_rect.left) / (float)cap_w;
  const float uv_bias_y =
      capture_is_window ? 0.f : (float)(tl.y - capture_rect.top) / (float)cap_h;

  D3D11_MAPPED_SUBRESOURCE mapped{};
  if (FAILED(device_context->Map(bg_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    return;
  float *uv = reinterpret_cast<float *>(mapped.pData);
  uv[0] = uv_scale_x;
  uv[1] = uv_scale_y;
  uv[2] = uv_bias_x;
  uv[3] = uv_bias_y;
  device_context->Unmap(bg_cb, 0);

  const UINT stride = sizeof(float) * 6;
  const UINT offset = 0;
  device_context->IASetInputLayout(bg_layout);
  device_context->IASetVertexBuffers(0, 1, &bg_vb, &stride, &offset);
  device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  device_context->VSSetShader(bg_vs, nullptr, 0);
  device_context->VSSetConstantBuffers(0, 1, &bg_cb);
  device_context->PSSetShader(bg_ps, nullptr, 0);
  device_context->PSSetShaderResources(0, 1, &capture_srv);
  device_context->PSSetSamplers(0, 1, &bg_sampler);
  D3D11_VIEWPORT vp{0.f, 0.f, w, h, 0.f, 1.f};
  device_context->RSSetViewports(1, &vp);
  device_context->Draw(4, 0);
}

void c_overlay::sync_overlay() {
  // Re-locate the game window if it changed (game restart).
  if (!game_hwnd || !IsWindow(game_hwnd)) {
    game_hwnd = nullptr;
    game_pid = 0;
    locate_game_window(); // may stay null -> fall back to the primary monitor
  }
  if (game_hwnd && game_pid == 0)
    GetWindowThreadProcessId(game_hwnd, &game_pid);

  HWND fg = GetForegroundWindow();
  DWORD fg_pid = 0;
  if (fg)
    GetWindowThreadProcessId(fg, &fg_pid);

  // Match the whole game process, not only its top-level HWND: Rust can put a
  // child/aux window in the foreground (and the old exact-HWND check made the
  // overlay stay hidden until the user alt-tabbed and back).
  //
  // The client is normally started from its console, so at startup the
  // foreground window belongs to our own process, not RustClient. Treat that
  // as "show" too; otherwise the ESP stays hidden until the user clicks the
  // game once. Once another app is focused, the normal hide path resumes.
  const bool own_process_fg = fg_pid == GetCurrentProcessId();
  const bool game_focused =
      game_hwnd != nullptr &&
      (fg == game_hwnd || (game_pid != 0 && fg_pid == game_pid) ||
       own_process_fg);
  const bool should = game_focused || menu_open;
  const bool was_visible = visible;

  if (should != visible) {
    ShowWindow(window_handle, should ? SW_SHOW : SW_HIDE);
    visible = should;
  }
  if (!should)
    return;

  // Target rect: the game client area when available, otherwise the primary
  // monitor (menu-only / no-game fallback).
  int x = 0, y = 0, w = 0, h = 0;
  RECT rc{};
  POINT tl{0, 0};
  if (game_hwnd && GetClientRect(game_hwnd, &rc) &&
      (rc.right - rc.left) > 0 && (rc.bottom - rc.top) > 0) {
    ClientToScreen(game_hwnd, &tl);
    x = tl.x;
    y = tl.y;
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
  } else {
    x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  }
  if (w <= 0 || h <= 0)
    return;

  window_width = static_cast<FLOAT>(w);
  window_height = static_cast<FLOAT>(h);

  static int lastX = INT_MIN, lastY = INT_MIN, lastW = 0, lastH = 0;
  const bool moved = x != lastX || y != lastY || w != lastW || h != lastH;
  if (moved || !was_visible) {
    lastX = x;
    lastY = y;
    lastW = w;
    lastH = h;
    // Re-assert topmost + force a compositor update on every hide->show edge.
    SetWindowPos(window_handle, HWND_TOPMOST, x, y, w, h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
  }

  // Rust's borderless window can itself be WS_EX_TOPMOST. A single SetWindowPos
  // at creation is not enough: when the game re-raises itself the ESP ends up
  // behind it until the user alt-tabs. Keep our transparent HUD above it with a
  // cheap non-activating raise; the menu owns the top slot while it is open.
  if (!menu_open) {
    static std::chrono::steady_clock::time_point last_raise{};
    const auto now = std::chrono::steady_clock::now();
    if (last_raise.time_since_epoch().count() == 0 ||
        now - last_raise >= std::chrono::milliseconds(400)) {
      SetWindowPos(window_handle, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
      last_raise = now;
    }
  }
}

c_overlay::c_overlay() {
  if (!create_window())
    return;
  if (!init_device())
    return;
}

c_overlay::~c_overlay() {
  if (d3d_device)
    dest_device();
  if (window_handle) {
    DestroyWindow(window_handle);
    window_handle = nullptr;
  }
  if (wnd_class_atom) {
    UnregisterClassW(L"VexusOverlay", GetModuleHandleW(nullptr));
    wnd_class_atom = 0;
  }
}

bool c_overlay::begin_frame() {
  MSG msg = {};
  while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
    if (msg.message == WM_QUIT) {
      exit = true;
      return false;
    }
  }

  sync_overlay();

  if (!visible)
    return true;
  if (!backbuffer_rtv)
    return true;

  const float clear[4] = {0.f, 0.f, 0.f, 0.f};
  device_context->OMSetRenderTargets(1, &backbuffer_rtv, nullptr);
  device_context->ClearRenderTargetView(backbuffer_rtv, clear);

  // Rebuild the D2D target if EndDraw lost it (GPU reset / driver update).
  if (!render::ready())
    render::init(swap_chain, scene_texture);

  render::begin_frame();
  return true;
}

void c_overlay::end_frame() {
  if (!visible || !d3d_device || !backbuffer_rtv || !swap_chain) {
    last_frame_end = std::chrono::steady_clock::time_point{};
    return;
  }

  render::end_frame();
  device_context->Flush();

  HRESULT present_hr = swap_chain->Present(vsync ? 1 : 0, 0);
  static bool logged_present = false;
  if (!logged_present) {
    logged_present = true;
    printf("[overlay] first present hr=0x%08X\n", (unsigned)present_hr);
  }

  const auto now = std::chrono::steady_clock::now();
  if (last_frame_end.time_since_epoch().count() != 0) {
    const float dt = std::chrono::duration<float>(now - last_frame_end).count();
    const float inst = dt > 0.000001f ? 1.f / dt : 0.f;
    fps_value = fps_value <= 0.f ? inst : fps_value * 0.9f + inst * 0.1f;
  }
  last_frame_end = now;
}
