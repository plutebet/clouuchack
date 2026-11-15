# Internal D3D11 ImGui Hook — complete reference build

// language: C++20, file: dllmain.cpp, target: x86/x64 injected DLL, Windows 10/11, MSVC
// deps: Dear ImGui v1.91+ (backends: imgui_impl_dx11, imgui_impl_win32), MinHook, nlohmann/json (header-only)
// link: d3d11.lib, dxgi.lib; vtable indices 0-based — verify Present(8) by dumping if a game fails to init

The one file that carries: vtable dump via dummy swap chain → Present hook → lazy ImGui init on the game's real device → WndProc with input gating → tabbed menu bound to a JSON-persisted config → RTV recreation on resize.

```cpp
// language: C++20, file: dllmain.cpp, target: x86/x64 injected DLL, Windows 10/11, MSVC
#include <windows.h>
#include <d3d11.h>
#include <string>
#include <filesystem>
#include <fstream>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <MinHook.h>
#include <nlohmann/json.hpp>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---------------------------------------------------------------- config ---
namespace cfg {
    struct Settings {
        // aim
        bool  aimEnabled   = false;
        float aimFov       = 4.5f;     // degrees
        float aimSmoothing = 12.0f;    // higher = slower
        int   aimKey       = VK_LBUTTON;
        bool  aimSilent    = false;
        // visuals
        bool  espBox       = true;
        bool  espName      = true;
        bool  espHealth    = true;
        float espColor[3]  = { 1.0f, 0.25f, 0.25f };
        // misc
        bool  bhop         = false;
        float viewFov      = 110.0f;
    };
    inline Settings g;
    inline std::filesystem::path Dir() {
        std::filesystem::path d = std::filesystem::path(getenv("APPDATA")) / L"mycheat" / L"configs";
        std::filesystem::create_directories(d);
        return d;
    }
    nlohmann::json ToJson(const Settings& s) {
        return {
            {"aim",       {{"enabled", s.aimEnabled}, {"fov", s.aimFov}, {"smoothing", s.aimSmoothing},
                           {"key", s.aimKey}, {"silent", s.aimSilent}}},
            {"visuals",   {{"box", s.espBox}, {"name", s.espName}, {"health", s.espHealth},
                           {"color", s.espColor}}},
            {"misc",      {{"bhop", s.bhop}, {"viewfov", s.viewFov}}},
        };
    }
    void FromJson(const nlohmann::json& j, Settings& s) {
        if (auto a = j.find("aim"); a != j.end()) {
            s.aimEnabled   = a->at("enabled"); s.aimFov = a->at("fov");
            s.aimSmoothing = a->at("smoothing"); s.aimKey = a->at("key"); s.aimSilent = a->at("silent");
        }
        if (auto v = j.find("visuals"); v != j.end()) {
            s.espBox = v->at("box"); s.espName = v->at("name"); s.espHealth = v->at("health");
            for (int i = 0; i < 3; ++i) s.espColor[i] = v->at("color")[i];
        }
        if (auto m = j.find("misc"); m != j.end()) { s.bhop = m->at("bhop"); s.viewFov = m->at("viewfov"); }
    }
    bool Save(std::string_view name) {
        std::ofstream f(Dir() / (std::string(name) + ".json"));
        if (!f) return false;
        f << ToJson(g).dump(2);
        return true;
    }
    bool Load(std::string_view name) {
        std::ifstream f(Dir() / (std::string(name) + ".json"));
        if (!f) return false;
        try { FromJson(nlohmann::json::parse(f), g); } catch (...) { return false; }
        return true;
    }
}

// ------------------------------------------------------------------ menu ---
namespace menu {
    inline float g_alpha = 0.0f;   // animated fade
    inline bool  g_open  = false;  // toggled from WndProc (VK_INSERT)
    inline char  g_cfgName[64] = "default";

    void Style() {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding  = 6.0f;  s.FrameRounding = 4.0f;  s.GrabRounding = 4.0f;
        s.WindowPadding   = {12, 12}; s.FramePadding = {8, 4}; s.ItemSpacing = {8, 6};
        s.WindowBorderSize = 1.0f;
        ImVec4* c = s.Colors;
        c[ImGuiCol_WindowBg]         = { 0.06f, 0.06f, 0.09f, 0.97f };
        c[ImGuiCol_Border]           = { 0.20f, 0.22f, 0.35f, 0.60f };
        c[ImGuiCol_Tab]              = { 0.10f, 0.10f, 0.15f, 1.00f };
        c[ImGuiCol_TabSelected]      = { 0.24f, 0.36f, 0.90f, 1.00f };
        c[ImGuiCol_FrameBg]          = { 0.10f, 0.10f, 0.15f, 1.00f };
        c[ImGuiCol_FrameBgHovered]   = { 0.14f, 0.14f, 0.20f, 1.00f };
        c[ImGuiCol_CheckMark]        = { 0.45f, 0.58f, 1.00f, 1.00f };
        c[ImGuiCol_SliderGrab]       = { 0.45f, 0.58f, 1.00f, 1.00f };
        c[ImGuiCol_SliderGrabActive] = { 0.60f, 0.72f, 1.00f, 1.00f };
        c[ImGuiCol_Header]           = { 0.20f, 0.28f, 0.55f, 0.60f };
    }

    void Render() {
        // fade toward target with frame-time-independent lerp
        const float target = g_open ? 1.0f : 0.0f;
        g_alpha += (target - g_alpha) * (1.0f - std::exp(-12.0f * ImGui::GetIO().DeltaTime));
        if (g_alpha < 0.01f) return;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_alpha);
        ImGui::SetNextWindowSize({ 680, 420 }, ImGuiCond_Once);
        if (ImGui::Begin("##main", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (ImGui::BeginTabBar("#tabs")) {
                if (ImGui::BeginTabItem("Aim")) {
                    ImGui::Checkbox("enable", &cfg::g.aimEnabled);
                    ImGui::SliderFloat("fov", &cfg::g.aimFov, 0.5f, 30.0f, "%.1f deg");
                    ImGui::SliderFloat("smoothing", &cfg::g.aimSmoothing, 1.0f, 50.0f, "%.0f");
                    ImGui::Checkbox("silent", &cfg::g.aimSilent);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Visuals")) {
                    ImGui::Checkbox("box", &cfg::g.espBox);
                    ImGui::Checkbox("name", &cfg::g.espName);
                    ImGui::Checkbox("health", &cfg::g.espHealth);
                    ImGui::ColorEdit3("color", cfg::g.espColor);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Misc")) {
                    ImGui::Checkbox("bhop", &cfg::g.bhop);
                    ImGui::SliderFloat("view fov", &cfg::g.viewFov, 90.0f, 150.0f, "%.0f");
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Config")) {
                    ImGui::InputText("name", g_cfgName, sizeof(g_cfgName));
                    if (ImGui::Button("Save"))   cfg::Save(g_cfgName);
                    ImGui::SameLine();
                    if (ImGui::Button("Load"))   cfg::Load(g_cfgName);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
}

// ------------------------------------------------------------------- hook ---
namespace hook {
    using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);

    inline PresentFn            o_present   = nullptr;
    inline HWND                 g_window    = nullptr;
    inline ID3D11Device*        g_device    = nullptr;
    inline ID3D11DeviceContext* g_context   = nullptr;
    inline ID3D11RenderTargetView* g_rtv    = nullptr;
    inline UINT                 g_w = 0, g_h = 0;
    inline bool                 g_ready     = false;
    inline WNDPROC              g_oWndProc  = nullptr;

    void CreateTargets(IDXGISwapChain* sc) {
        if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
        ID3D11Texture2D* back = nullptr;
        if (SUCCEEDED(sc->GetBuffer(0, IID_PPV_ARGS(&back)))) {
            g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
            back->Release();
        }
        DXGI_SWAP_CHAIN_DESC d{};
        sc->GetDesc(&d);
        g_w = d.BufferDesc.Width; g_h = d.BufferDesc.Height;
    }

    void InitImGui(HWND hwnd) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;                       // don't litter the game dir with imgui.ini
        io.MouseDrawCursor = true;
        // readable latin+cyrillic; merge FontAwesome icons over it if wanted
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, nullptr,
                                     io.Fonts->GetGlyphRangesCyrillic());
        menu::Style();
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(g_device, g_context);
    }

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_KEYDOWN && wp == VK_INSERT)
            menu::g_open = !menu::g_open;

        if (g_ready && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp) && menu::g_open)
            return 1;                                    // ImGui consumed the message

        if (menu::g_open && g_ready) {
            const ImGuiIO& io = ImGui::GetIO();
            const bool mouse = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
            const bool key   = (msg >= WM_KEYFIRST   && msg <= WM_KEYLAST);
            if ((io.WantCaptureMouse && mouse) || (io.WantCaptureKeyboard && key))
                return 1;                                // game must not see it
        }
        return CallWindowProcW(g_oWndProc, hwnd, msg, wp, lp);
    }

    HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
        if (!g_ready) {
            // first real present: this is the GAME's swap chain, not our dummy
            if (FAILED(sc->GetDevice(IID_PPV_ARGS(&g_device))))
                return o_present(sc, sync, flags);
            g_device->GetImmediateContext(&g_context);
            DXGI_SWAP_CHAIN_DESC d{};
            sc->GetDesc(&d);
            g_window = d.OutputWindow;
            CreateTargets(sc);
            InitImGui(g_window);
            g_oWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
            g_ready = true;
        }
        // recreate targets on resolution change (covers most of what ResizeBuffers hooking guards)
        DXGI_SWAP_CHAIN_DESC d{};
        sc->GetDesc(&d);
        if (d.BufferDesc.Width != g_w || d.BufferDesc.Height != g_h)
            CreateTargets(sc);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        menu::Render();
        // always-on widgets (ESP etc.) draw here too:
        // ImGui::GetBackgroundDrawList()->AddRect(...) — under the menu, over the world

        ImGui::Render();
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        return o_present(sc, sync, flags);
    }

    void Install() {
        // dummy device+swapchain only to read the vtable — addresses live in dxgi.dll,
        // identical for the game's swap chains. release immediately after.
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        sd.OutputWindow = GetForegroundWindow();
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;

        IDXGISwapChain* dummy_sc = nullptr;
        ID3D11Device* dummy_dev = nullptr;
        ID3D11DeviceContext* dummy_ctx = nullptr;
        const D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                fls, 2, D3D11_SDK_VERSION, &sd, &dummy_sc, &dummy_dev, nullptr, &dummy_ctx)))
            return;

        void** vt = *reinterpret_cast<void***>(dummy_sc);
        constexpr int kPresent = 8;                  // IDXGISwapChain::Present, 0-based
        dummy_sc->Release(); dummy_dev->Release(); dummy_ctx->Release();

        if (MH_Initialize() != MH_OK) return;
        MH_CreateHook(reinterpret_cast<void*>(vt[kPresent]),
                      reinterpret_cast<void*>(&hkPresent),
                      reinterpret_cast<void**>(&o_present));
        MH_EnableHook(reinterpret_cast<void*>(vt[kPresent]));
    }

    void Uninstall() {
        if (!g_ready) return;
        SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_oWndProc));
        MH_DisableHook(MH_ALL_HOOKS);
        Sleep(50);                                   // drain any in-flight Present call
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        if (g_rtv)    { g_rtv->Release(); }
        if (g_context){ g_context->Release(); }
        if (g_device) { g_device->Release(); }
        MH_Uninitialize();
    }
}

// ------------------------------------------------------------------ entry ---
DWORD WINAPI MainThread(LPVOID) {
    hook::Install();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        // loader lock held here — all real work moves to the worker thread
        CloseHandle(CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr));
    } else if (reason == DLL_PROCESS_DETACH) {
        hook::Uninstall();
    }
    return TRUE;
}
```

## Notes

- `kPresent = 8` is the 0-based `IDXGISwapChain::Present` slot; if a game inits but nothing draws, dump `vt` and disassemble slot 8 before trusting any hardcoded index.
- The `MessageBox`-free init means a silent failure is invisible; wire `Install()`'s failure paths to a `MessageBoxA` while developing, strip for release.
- For **D3D9**: same skeleton, vtable of `IDirect3DDevice9`, `EndScene` (42) instead of Present, `ImGui_ImplDX9_Init(device)`, and hook `Reset` (16) to call `ImGui_ImplDX9_InvalidateDeviceObjects/CreateDeviceObjects`.
- For **D3D12**: additionally hook `ExecuteCommandLists` (slot 10 of `ID3D12CommandQueue`) to capture the queue; UE 5.3+ uses distinct render/present queues — use the queue whose command lists are actually being presented, or crash.
- The per-frame `GetDesc` comparison costs ~nothing and removes a whole class of alt-tab crashes; a dedicated `ResizeBuffers` hook (slot 12) is the belt-and-braces version.
