
// ================================================================================
// Watermark:
// t.me/imguisource_bot
// t.me/ImGui_Source_FIXNEY (ImGui Source)
// t.me/il2cpp_src (shit src 🧨)
// ================================================================================

#pragma comment(lib, "winmm.lib")
#include <timeapi.h>
#include <intrin.h>
#include <objbase.h>
#include <cstdlib>
#include <cstring>

#include "../GetEntities/Entities.h"
#include "../Il2Cpp/Decryptions/decryptor.hpp"
#include "../Il2Cpp/GcHandle/il2cpp.h"
#include "../libs/overlay/Overlay.h"
#include "../src/Hud/Hud.h"
#include "../src/libs/web/MenuWeb.h"
#include "../src/Menu/Config.h"
#include "../src/Features/Aim/Aim.h"
#include "../src/Features/Misc/Misc.h"
#include "../src/Features/Visuals/Chams/Chams.h"
#include "../src/Features/Visuals/Esp/Esp.h"
#include "../src/Features/Visuals/Esp/ImageEsp.h"
#include "../Diag/Profiler.h"
#include "../World2Screen/World2Screen.h"

uint64_t         g_gameBase  = 0;
uint64_t         g_unityBase = 0;
BaseNetworkable* g_entities  = nullptr;

static bool Init(bool attach_to_game) {
    timeBeginPeriod(1);

    if (!attach_to_game) {
        printf("[*] menu-only mode: game attach skipped\n");
        return true;
    }

    Driver = new Memory();
    printf("[+] transport: Vexus kernel driver\n");
    // get rust client
    printf("[*] Waiting for RustClient.exe...\n");
    while (!Driver->Attach("RustClient.exe"))
        std::this_thread::sleep_for(std::chrono::seconds(10));
    printf("[+] RustClient.exe PID: %u\n", Driver->GetProcessId());

    // get gameassembly.dll
    printf("[*] Waiting for GameAssembly.dll...\n");
    while (!(g_gameBase = Driver->GetModuleBase("GameAssembly.dll")))
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    printf("[+] GameAssembly.dll: 0x%llX\n", g_gameBase);

    // get unityplayer.dll
    printf("[*] Waiting for UnityPlayer.dll...\n");
    while (!(g_unityBase = Driver->GetModuleBase("UnityPlayer.dll")))
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    printf("[+] UnityPlayer.dll: 0x%llX\n", g_unityBase);

    // install the dump's decryptor hooks (decryptions.txt -> decryptor.hpp):
    //   decryptor::read       = RPM of a uint64 field
    //   decryptor::get_handle = slab GCHandle resolver
    decryptor::read = [](std::uint64_t address) -> std::uint64_t {
        return Driver->Read<std::uint64_t>(address);
    };
    decryptor::get_handle = [](std::uint64_t handle) -> std::uint64_t {
        return static_cast<uint64_t>(
            Il2CppHandle::Il2cppGetHandle(handle, g_gameBase));
    };
    printf("[+] Decryptor hooks installed\n");

    // show the dump values in use so the console documents the config
    printf("[*] offsets: BN typeinfo=0x%llX static=0x%llX cePtr=0x%llX "
           "entityList=0x%llX dictVals=0x%llX buf=0x%llX count=0x%llX\n",
           (unsigned long long)BaseNetworkable_Static_Offsets::typeinfo,
           (unsigned long long)BaseNetworkable_Static_Offsets::static_fields,
           (unsigned long long)BaseNetworkable_Static_Offsets::clientEntities,
           (unsigned long long)BaseNetworkable_EntityRealm_Offsets::entityList,
           (unsigned long long)System_ListDictionary_Offsets::vals,
           (unsigned long long)System_BufferList_Offsets::buffer,
           (unsigned long long)System_BufferList_Offsets::count);
    printf("[*] offsets: LP typeinfo=0x%llX Entity=0x%llX | MainCamera "
           "typeinfo=0x%llX mainCamera=0x%llX m_CachedPtr=0x%llX +0x2FC\n",
           (unsigned long long)LocalPlayer_Static_Offsets::typeinfo,
           (unsigned long long)LocalPlayer_Static_Offsets::Entity,
           (unsigned long long)MainCamera_Offsets::typeinfo,
           (unsigned long long)MainCamera_Offsets::mainCamera,
           (unsigned long long)Object_Offsets::m_CachedPtr);

    // direct TypeInfo fast path (current dump matches the live build),
    // with the structural name/chain passes as fallback
    ResolveClasses();

    // start entity system
    g_entities = new BaseNetworkable(g_gameBase);
    printf("[+] Entity system enabled\n");

    // start the view-matrix updater thread (keeps the render thread off the
    // driver so the overlay stays smooth)
    W2S::StartUpdater();

    return true;
}

// Insert toggles the menu. The WebView window owns input while it is open, so
// the overlay itself stays click-through.
static void HandleMenuToggle() {
    static bool key_down = false;
    static bool welcome_shown = false;

    const bool down = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (!down) {
        key_down = false;
        return;
    }
    if (key_down)
        return;
    key_down = true;

    // While the aim page is waiting for a key press, swallow the toggle so
    // binding Insert does not also close the menu.
    if (webmenu::BindingKey())
        return;

    const bool open = !overlay->menu_open;
    if (!webmenu::SetOpen(open))
        return;

    overlay->menu_open = open;
    if (open && !welcome_shown) {
        welcome_shown = true;
        webmenu::Notify("Welcome to LostKernel.Wtf", 3500);
    }
}

// ── frame pacing ────────────────────────────────────────────────────────────
// The overlay is a full-screen transparent swapchain, so every Present costs a
// surface copy plus a DWM composition. Presenting it thousands of times per
// second competes with Rust for GPU/CPU and adds input latency. Cap it, and
// sleep for most of the budget: a busy spin pins a core and makes the game feel
// delayed even when the overlay itself is doing almost nothing.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

static long long FrameBudgetUs() {
    static const long long envBudget = [] {
        wchar_t value[32]{};
        if (GetEnvironmentVariableW(L"VEXUS_FPS_CAP", value, 32) > 0) {
            const int fps = _wtoi(value);
            if (fps > 0)
                return 1000000LL / fps;
        }
        return 0LL;
    }();
    if (envBudget > 0)
        return envBudget;

    const int fps = (g_cfg && g_cfg->overlay_fps > 0) ? g_cfg->overlay_fps : 1000;
    return 1000000LL / fps;
}

static HANDLE FrameTimer() {
    // High-resolution timer: lets the frame wait sleep off-CPU instead of
    // spinning, without the 1 ms granularity of Sleep().
    static HANDLE timer =
        CreateWaitableTimerExW(nullptr, nullptr,
                               CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                               TIMER_ALL_ACCESS);
    return timer;
}

static void PaceFrame(std::chrono::steady_clock::time_point start,
                      long long budgetUs) {
    using clock = std::chrono::steady_clock;
    const auto elapsed = [&] {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   clock::now() - start)
            .count();
    };

    const long long remain = budgetUs - elapsed();
    if (remain <= 0)
        return;

    // 1000+ FPS mode: the early build used a precise spin and that is what kept
    // frame times flat. The driver now has a single shared lane, so the spin is
    // the only worker that keeps the CPU hot. Lower caps sleep on the timer.
    if (budgetUs <= 1200) {
        while (elapsed() < budgetUs)
            _mm_pause();
        return;
    }

    // Lower caps (e.g. 144): sleep off-CPU for the bulk and spin only the last
    // ~100 us so timer resolution does not show up as frame jitter.
    constexpr long long kSpinTailUs = 100;
    const long long waitUs = remain - kSpinTailUs;
    if (waitUs >= 100) {
        HANDLE timer = FrameTimer();
        if (timer) {
            LARGE_INTEGER due{};
            due.QuadPart = -(waitUs * 10); // relative, 100 ns units
            if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
                WaitForSingleObject(timer, INFINITE);
        } else {
            Sleep(static_cast<DWORD>((waitUs + 999) / 1000));
        }
    }

    while (elapsed() < budgetUs)
        _mm_pause();
}

// Main loop for logic and rendering.
static void Loop() {
    static auto s_lastPidCheck = std::chrono::steady_clock::now();
    static const bool perfLog = [] {
        wchar_t value[8]{};
        return GetEnvironmentVariableW(L"VEXUS_PERF_LOG", value, 8) > 0;
    }();
    static auto perfWindow = std::chrono::steady_clock::now();
    static long long perfWorkUs = 0;
    static int perfFrames = 0;

    while (overlay && !overlay->exit) {
        const auto frameStart = std::chrono::steady_clock::now();
        prof::renderFrames.fetch_add(1, std::memory_order_relaxed);
        prof::Report();
        auto nowSteady = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(nowSteady - s_lastPidCheck).count() >= 1.0) {
            s_lastPidCheck = nowSteady;
            // stop if we dont have game
            if (Driver && !Driver->IsProcessRunning()) {
                printf("[!] Game closed\n");
                break;
            }
        }

        // handle the menu toggles off and on
        HandleMenuToggle();

        // begin rendering: pumps messages and (when on-screen) opens a frame
        if (!overlay->begin_frame())
            continue;

        // ESP/aim projection must use the GAME CLIENT rect, not the monitor:
        // Rust's borderless window is 1920x1009 on a 1080p desktop, and using
        // 1080 for W2S shifts distant boxes/icons toward the centre-bottom.
        if (overlay->window_width > 1.f && overlay->window_height > 1.f) {
            W2S::g_ScreenW = overlay->window_width;
            W2S::g_ScreenH = overlay->window_height;
        }

        // keep the web menu's live stats flowing while it is visible
        webmenu::Tick();

        if (overlay->visible) {
            // render esp inside rendering loop
            esp::Render();

            // aim assist (also draws the FOV circle / lock marker). Does
            // nothing unless it is enabled and the bound key is held.
            aim::Tick();

            // native watermark + toast notifications
            hud::Render();

            // mirror the menu's VSync toggle (off by default)
            overlay->vsync = g_cfg->vsync;

            overlay->end_frame();

            // Frame pacing: sleep off-CPU for the bulk of the budget and spin
            // only the last ~100 us. The old busy spin pinned a core at
            // ABOVE_NORMAL priority and starved Rust's render thread.
            const auto usedUs =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - frameStart)
                    .count();
            if (perfLog) {
                perfWorkUs += usedUs;
                ++perfFrames;
                const auto now = std::chrono::steady_clock::now();
                const double span =
                    std::chrono::duration<double>(now - perfWindow).count();
                if (span >= 1.0) {
                    std::printf(
                        "[perf] overlay %.0f fps | work %.0f us/frame | cap "
                        "%.0f fps\n",
                        perfFrames / span,
                        perfWorkUs / static_cast<double>(perfFrames),
                        1000000.0 / static_cast<double>(FrameBudgetUs()));
                    std::fflush(stdout);
                    perfWindow = now;
                    perfFrames = 0;
                    perfWorkUs = 0;
                }
            }
            if (!overlay->vsync)
                PaceFrame(frameStart, FrameBudgetUs());
        } else {
            Sleep(1); // avoid a busy spin while hidden
        }
    }
}

int main(int argc, char **argv) {
    // The client is normally launched from a console; keep the log usable even
    // when stdout is redirected to a pipe by a launcher.
    setvbuf(stdout, nullptr, _IONBF, 0);

    bool menu_only = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--menu-only") == 0)
            menu_only = true;
    }

    SetProcessDPIAware();
    // NORMAL, not ABOVE_NORMAL: the overlay must never outrank the game's
    // render thread. The foreground game already gets the scheduler boost, and
    // sleep-based frame pacing keeps the overlay smooth at its cap.
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

    // WebView2 needs a single-threaded apartment on the UI thread.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com))
        printf("[!] CoInitializeEx failed (0x%08lX) - menu bridge may not work\n",
               static_cast<unsigned long>(com));

    // gets the modules and attaches
    if (!Init(!menu_only)) {
        printf("Failed to init\n");
        return 1;
    }
    // create the shared ESP config (menu + ESP read/write this one object)
    g_cfg = new Config();
    Config_SetDefaults(*g_cfg);

    // create overlay
    overlay = new c_overlay();
    if (!overlay->is_ready()) {
        printf("Failed to setup overlay\n");
        delete overlay;
        return 1;
    }

    // boot the web menu host (the React UI lives in its own topmost window)
    webmenu::Init(overlay->get_game_hwnd());

    // Decode the icon set once, on the render thread, before the loop starts.
    // Without this, the first player/sleeper seen triggers a 1920x1080 WIC
    // decode + transparent-pixel scan in the middle of a frame.
    image_esp::Preload();

    // start the aim worker now that g_cfg + overlay exist (all aim driver I/O
    // happens on this thread, never on the render thread)
    if (!menu_only)
        aim::Start();

    // Opt-in external chams worker: validates the renderer chain and applies a
    // material ID only while the Player-page checkbox is on.
    if (!menu_only)
        chams::Start();

    // start the movement-feature worker (Spider-Man climb) - also off the
    // render thread so the overlay never blocks on a driver round-trip
    if (!menu_only)
        misc::Start();

    if (menu_only) {
        hud::Notify("Menu-only preview - game attach disabled", 6000);
        overlay->menu_open = true;
        webmenu::SetOpen(true);
    }

    // start our loop with our "feature logic"
    Loop();

    // delete heap allocated objects
    misc::Stop();
    chams::Stop();
    aim::Stop();
    if (!menu_only)
        W2S::StopUpdater();
    webmenu::Shutdown();
    delete g_entities;
    delete overlay;
    delete g_cfg;
    delete Driver;

    timeEndPeriod(1);
    if (SUCCEEDED(com))
        CoUninitialize();
    return 0;
}
