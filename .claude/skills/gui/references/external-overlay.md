# External transparent overlay — complete reference build

// language: C++20, file: overlay.exe, target: Windows 10 2004+ (x64), MSVC
// deps: Dear ImGui v1.91+ (imgui_impl_dx11, imgui_impl_win32), link d3d11.lib dxgi.lib
// own-process overlay: invisible to OBS game/window capture by construction; WDA_EXCLUDEFROMCAPTURE additionally blinds window-capture and screenshot tools.

Architecture: a ghost window (`WS_EX_TRANSPARENT` → clicks pass through) with its own swap chain tracks the game window's client rect every frame. Menu open → strip `WS_EX_TRANSPARENT` so the overlay takes the mouse. ESP geometry arrives from a memory thread through a snapshot struct.

```cpp
// language: C++20, file: overlay.cpp, target: Windows 10 2004+ x64 EXE, MSVC
#include <windows.h>
#include <d3d11.h>
#include <thread>
#include <atomic>
#include <vector>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ----------------------------------------------------------- shared state ---
struct Snapshot {                       // written by the memory thread, read by render
    bool   valid      = false;
    float  view[16];                  // view-projection matrix for w2s
    int    screenW, screenH;
    struct Ent { float head[3]; float feet[3]; float hp; char name[32]; };
    Ent    ents[64];
    int    entCount = 0;
};
inline std::atomic<bool> g_run{ true };
inline Snapshot g_snap;                 // fine at 60–120 Hz swaps; add a mutex if torn reads appear

// ------------------------------------------------------------ overlay win ---
inline constexpr wchar_t kClass[] = L"ovl_ghost";

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(h, msg, wp, lp)) return 1;
    switch (msg) {
    case WM_SIZE: return 0;                       // swap chain resize handled in the loop
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

HWND MakeGhostWindow(HINSTANCE inst, int w, int h) {
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = inst;
    wc.lpszClassName = kClass;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // TOOLWINDOW: no taskbar button; NOACTIVATE: never steal focus at creation
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClass, L"", WS_POPUP, 100, 100, w, h,
        nullptr, nullptr, inst, nullptr);
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);   // fully opaque, layered for per-pixel alpha
    ShowWindow(hwnd, SW_SHOW);

    // streamproof: excluded from window capture / screenshots (Win10 2004+)
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    return hwnd;
}

void SetClickThrough(HWND hwnd, bool through) {
    // input path: through=true → clicks go to the game; false → overlay receives mouse
    const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, through ? (ex | WS_EX_TRANSPARENT) : (ex & ~WS_EX_TRANSPARENT));
}

// ------------------------------------------------------------------ menu ---
namespace menu {
    inline bool g_open = false;
    struct Settings {
        bool espBox = true, espName = true, espHp = true;
        float espColor[3] = { 0.35f, 1.0f, 0.45f };
        bool  snaplines = false;
    };
    inline Settings s;

    void Render() {
        ImGui::SetNextWindowSize({ 420, 300 }, ImGuiCond_Once);
        if (ImGui::Begin("overlay", nullptr,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::BeginTabBar("#t")) {
                if (ImGui::BeginTabItem("ESP")) {
                    ImGui::Checkbox("box", &s.espBox);
                    ImGui::Checkbox("name", &s.espName);
                    ImGui::Checkbox("health", &s.espHp);
                    ImGui::Checkbox("snaplines", &s.snaplines);
                    ImGui::ColorEdit3("color", s.espColor);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void DrawEsp(int w, int h) {
        if (!g_snap.valid) return;
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const ImU32 col = IM_COL32(255, 255, 255, 255);
        for (int i = 0; i < g_snap.entCount; ++i) {
            const auto& e = g_snap.ents[i];
            // world-to-screen against the game's own matrix (memory thread's job to keep it fresh);
            // boxes projected from head/feet, clamped to screen — w2s math lives in game-math skill
            ImVec2 top{ e.head[0], e.head[1] }, bot{ e.feet[0], e.feet[1] };
            if (top.y >= bot.y || top.y < 0 || bot.y > h) continue;
            const float boxH = bot.y - top.y;
            const float boxW = boxH * 0.35f;
            const ImVec2 tl{ (top.x + bot.x) * 0.5f - boxW * 0.5f, top.y };
            const ImVec2 br{ (top.x + bot.x) * 0.5f + boxW * 0.5f, bot.y };
            if (s.espBox)
                dl->AddRect(tl, br, ImGui::ColorConvertFloat4ToU32({ s.espColor[0], s.espColor[1], s.espColor[2], 1.f }));
            if (s.espName)
                dl->AddText({ tl.x, tl.y - 16.0f }, col, e.name);
            if (s.espHp) {
                dl->AddRectFilled({ tl.x - 5.f, tl.y }, { tl.x - 2.f, br.y }, IM_COL32(0,0,0,180));
                dl->AddRectFilled({ tl.x - 5.f, br.y - (br.y - tl.y) * (e.hp / 100.f) },
                                  { tl.x - 2.f, br.y }, IM_COL32(60, 220, 60, 255));
            }
            if (s.snaplines)
                dl->AddLine({ w * 0.5f, (float)h }, { (tl.x + br.x) * 0.5f, br.y }, col);
        }
    }
}

// ---------------------------------------------------------------- main ---
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    // 1) find game window (subclass of: enumerate by window title / process name)
    const HWND game = FindWindowW(nullptr, L"Counter-Strike 2");   // replace per target
    if (!game) return 1;

    RECT rc{}; GetClientRect(game, &rc);
    const int W = rc.right, H = rc.bottom;

    // 2) ghost window + D3D11 + ImGui
    const HWND hwnd = MakeGhostWindow(inst, W, H);
    IDXGISwapChain* sc = nullptr; ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr;
    {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.Width = W; sd.BufferDesc.Height = H;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;   // flip model: no full-alpha layered penalty
        ID3D11Device* d = nullptr;
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &d, nullptr, &ctx);
        dev = d;
    }
    ID3D11RenderTargetView* rtv = nullptr;
    {
        ID3D11Texture2D* back = nullptr;
        sc->GetBuffer(0, IID_PPV_ARGS(&back));
        dev->CreateRenderTargetView(back, nullptr, &rtv);
        back->Release();
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, nullptr,
                                 io.Fonts->GetGlyphRangesCyrillic());
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(dev, ctx);

    // 3) memory thread reads game state into g_snap (hollow out for the target: handles, modules, offsets)
    std::thread mem([&] {
        while (g_run) {
            // read view matrix + entity list into g_snap ...
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    });

    // 4) render loop
    MSG msg{};
    while (g_run && msg.message != WM_QUIT) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        // track the game window (moves, resizes, monitor changes)
        if (GetClientRect(game, &rc)) {
            POINT pt{ 0, 0 }; ClientToScreen(game, &pt);
            SetWindowPos(hwnd, HWND_TOPMOST, pt.x, pt.y, rc.right, rc.bottom, SWP_NOACTIVATE);
        }
        const bool gameAlive = IsWindow(game) && !IsIconic(game);

        SetClickThrough(hwnd, !menu::g_open);           // menu takes the mouse only when open

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (menu::g_open) menu::Render();
        if (gameAlive)    menu::DrawEsp(rc.right, rc.bottom);

        ImGui::Render();
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->ClearRenderTargetView(rtv, { 0, 0, 0, 0 });   // fully transparent frame base
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        sc->Present(1, 0);                               // vsync; pass DXGI_PRESENT_ALLOW_TEARING for latency
    }

    g_run = false;
    if (mem.joinable()) mem.join();
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    rtv->Release(); ctx->Release(); dev->Release(); sc->Release();
    DestroyWindow(hwnd);
    return 0;
}
```

## Notes

- **Click-through toggle order matters**: strip `WS_EX_TRANSPARENT` *before* drawing an interactive menu, restore it when closed — otherwise the first click lands in the game. For per-region click-through (ESP clicks pass, menu clicks don't), keep the window click-through and route input by `WindowFromPoint` on a second topmost window holding only the menu.
- **Fullscreen exclusive games hide the overlay** — force the game to borderless/windowed or the ghost window is behind the swapchain. Most modern titles are borderless by default.
- **`WDA_EXCLUDEFROMCAPTURE`** blinds OBS window capture, Discord, Game Bar and PrintScreen; it does NOT hide the window from the desktop compositor — it is visible on the physical monitor. DWM-level cloaking (`DwmSetWindowAttribute`, `DWMWA_CLOAK`) is the legacy trick and less reliable across capture tools.
- **AC signature**: an always-on-top transparent toolwindow named after nothing is enumerable (`EnumWindows` sees everything). Randomize class/title, consider hiding from enumeration only where the AC's usermode scan is understood — kernel ACs see the window regardless.
- **DPI**: call `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` before window creation or the overlay drifts on scaled monitors.
- Anti-cheat note: for ACs that flag the overlay process at all, the same ImGui menu can be rendered through the internal hook (see `internal-d3d11-hook.md`) — same menu code, different plumbing.
