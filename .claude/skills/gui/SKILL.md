---
name: gui
description: Draw the in-game cheat menu/GUI — ImGui internal Present-hook menus (D3D9/D3D11/D3D12/OpenGL/Vulkan), external transparent overlay windows, WndProc input gating, tabs/control layout bound to config, Cyrillic + icon fonts, styling and animations, streamproof (invisible to OBS/screen capture). Use for any menu/GUI drawing task in cheat development. For the raw graphics-hooking pipeline use graphics-api-hooking; for menu stealth vs auto-screenshot AC checks read the capture section here first.
---

# In-game GUI / Menu

How people actually build game menus. Two families: **internal** (code lives in the game process, renders inside the game's own Present call) and **external** (own process, own transparent topmost window drawn over the game). ImGui is the de-facto standard for both; the menu is 90% plumbing (hook + input + fonts) and 10% widgets.

## Choose the lane

| | Internal hook | External overlay |
|---|---|---|
| Lives in | game process (DLL) | own EXE |
| Renders | inside game's Present — one frame, zero window | own D3D/GL device, own window |
| Input | WndProc hook inside game | own window messages, click-through toggle |
| Captured by OBS game/window capture | **yes** (it IS the game's frame) | no (separate window) → naturally streamproof |
| AC visibility | high (inside the process) | medium (topmost transparent window is a known signature) |
| Use when | full internal cheat, ESP+menu in one draw pass | memory-only external, quick ESP, streamproof needed |

## Internal — hook targets per graphics API

Present/EndScene are found by dumping the vtable of a **temporarily created** dummy object, then releasing the dummy — the addresses live in the driver/DLL and are identical for the game's objects. Verify indices at build time by dumping the vtable; tutorials disagree by ±1 because of 0- vs 1-based counting.

| API | Hook | vtable index (0-based) | Notes |
|---|---|---|---|
| D3D9 | `IDirect3DDevice9::EndScene` | 42 | classic; also `Reset` (16) must be hooked or ImGui dies on alt-tab; `Present` (17) if EndScene is skipped |
| D3D11 | `IDXGISwapChain::Present` | 8 | the standard; address lives in dxgi.dll, same for every swap chain |
| D3D11 | `IDXGISwapChain1::Present1` | 22 | MS-recommended since D3D 11.1 — some games call **only** Present1, so a Present-only hook never fires; extra `DXGI_PRESENT_PARAMETERS*` arg. Hook both or verify which the target calls |
| D3D11 | `IDXGISwapChain::ResizeBuffers` | 12 | hook or recreate RTV on resize (some listings say 13 — 1-based) |
| D3D12 | `IDXGISwapChain::Present` | 8 | plus `ID3D12CommandQueue::ExecuteCommandLists` (10) — swap chain does NOT expose its queue; hook ECL, capture the queue pointer, render with it |
| D3D12 | ⚠ UE 5.3+ | — | separate render vs present queues; naive queue caching crashes — verify queue type |
| OpenGL | `wglSwapBuffers` | — | `GetProcAddress(GetModuleHandleA("opengl32.dll"), "wglSwapBuffers")`; GLEW must link **statically** (dynamic glew.dll → LoadLibrary deadlock/denied inside injected DLL) |
| Vulkan | `vkQueuePresentKHR` | — | device function pointer; render in its own command buffer |
| DXVK games | DXGI path | 8 | DXVK translates D3D→Vulkan but still presents through dxgi.dll — the D3D11 hook works; raw-GL via DXVK does not |

**Hooking libraries:** MinHook (the workhorse, x86+x64), or **kiero** (universal graphics hook, D3D9–D3D12/OpenGL/Vulkan — unmaintained but the ecosystem standard; successor: `rdbo/DX11-BaseHook`). Cheat-engine-style template repos: `zetsr/Minimal-D3D9-Hook-ImGui`, `rdbo/ImGui-DirectX-11-Kiero-Hook` (→ DX11-BaseHook), `adamhlt/D3D11-Hook-ImGui`, `DrNseven/D3D12-Hook-ImGui`, `Sh0ckFR/Universal-Dear-ImGui-Hook` (DX9–12 + Vulkan), `Sera-Nya/Windows-Dx11-ImGui-CheatMenu` (full menu framework).

**Internal pipeline (D3D11, the one to memorize):** `D3D11CreateDeviceAndSwapChain` dummy → vtable[8] → MinHook on Present → first real Present: `swapchain->GetDevice()` + `GetImmediateContext()` + `GetDesc().OutputWindow` → create RTV from back buffer → `ImGui_ImplWin32_Init(hwnd)` + `ImGui_ImplDX11_Init(device, ctx)` → every frame: NewFrame ×3 → draw → `Render()` → `OMSetRenderTargets` → `RenderDrawData` → call original. Full runnable code: `references/internal-d3d11-hook.md`.

## External overlay pipeline

1. Own window: `CreateWindowExW` with `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE`, `WS_POPUP`, `SetLayeredWindowAttributes(alpha)` — a click-through ghost window.
2. Own D3D11 device + swap chain against that hwnd (or same trick for GL/Vulkan). Full code: `references/external-overlay.md`.
3. Per-frame: `GetClientRect(gameHwnd)` → move/resize overlay to match (`SetWindowPos` HWND_TOPMOST), handle resolution changes and multi-monitor DPI.
4. Input: when menu closed → window stays `WS_EX_TRANSPARENT` (clicks pass to game). When menu opens → strip that flag so the overlay receives mouse, `SetForegroundWindow(overlay)`, game input untouched by design.
5. ESP geometry from a separate memory-reading thread; share via lock-free snapshot struct, never touch game memory from the render thread.

## Input handling (the part everyone gets wrong)

- Hook the game's WndProc with `SetWindowLongPtrW(hwnd, GWLP_WNDPROC, ...)` (use **Ptr** — 32-bit `SetWindowLong` silently fails to receive mouse input). Restore the original on unload.
- First line of the hook: `ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)` (declare it `extern`). It feeds ImGui's input queue.
- Gate with `ImGui::GetIO().WantCaptureMouse / WantCaptureKeyboard`: if menu open and ImGui wants it → `return 1` (swallow); else `CallWindowProc(original, ...)`. Forward everything even when menu is closed.
- **Window messages are not the only input path.** Games polling Raw Input (`WM_INPUT`, `RegisterRawInputDevices`) or DirectInput (dinput8) bypass WndProc — block those too: hook `dinput8!GetDeviceState/GetDeviceData`, or `RegisterRawInputDevices` to strip `RIDEV_INPUTSINK`, or ClipCursor/ShowCursor juggling. Symptom: menu works, camera still spins while you click it → the game reads input elsewhere.
- Toggle key read in the WndProc hook (`WM_KEYDOWN`, typically `VK_INSERT`/`VK_DELETE`) — not polled in the render loop, or you get double-toggles from key-repeat.
- Newer ImGui (1.87+): key handling went through `ImGuiKey` enum rewrite; pre-1.87 tutorials using legacy `ImGuiKey_` indices and `io.KeysDown` do not compile as-is. ImGui **1.92.x** (current line, 1.92.9b) changed the backend texture protocol and enables dynamic font scaling — backends and ImGui must come from the same release.

## Menu construction

Structure that scales from single-window to full cheat:

```cpp
void Menu::Render() {
    // fade-in on toggle: animate alpha toward target with io.DeltaTime
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_menuAlpha);
    ImGui::SetNextWindowSize({680, 420}, ImGuiCond_Once);
    if (g_menuOpen && ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (ImGui::BeginTabBar("#tabs")) {
            if (ImGui::BeginTabItem("Aim"))  { /* checkbox/slider bound to Config::g */ ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Visuals")) { /* ... */ ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Misc")) { /* ... */ ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }
    ImGui::PopStyleVar();
}
```

- **Config binding**: one plain struct of settings (`Config::g`), serialized with nlohmann::json to `%APPDATA%\<product>\configs\<name>.json`; load/save buttons + config list = standard cheat UX. Widgets write straight into the struct; the cheat modules read it — no cross-thread toys needed for plain values.
- **Styling**: don't fight `ImGuiStyle` defaults — set `WindowRounding/FrameRounding/WindowPadding`, a dark `ImGuiCol_*` palette with one accent color, then stop. Reference collections for the "cheat menu" aesthetic: `Ch1nChun/ImGuiMenu-s` (curated dumps), UC thread *"Post your Menu/GUI Framework"* (hundreds of iterations).
- **Custom widgets**: the modern look (animated tabs, sliders with fill gradients, keybind lists) comes from **ImDrawList** primitives + per-widget animation state (lerp a float with `io.DeltaTime`), not from style tweaks. `ImGui::GetForegroundDrawList()` draws on top of everything. Hit-testing custom widgets: `ImGui::InvisibleButton` + `IsItemHovered/IsItemActive` so ImGui's input system still owns them.
- **Fonts**: default ProggyClean is unreadable — load a real TTF (Inter/Segoe/Roboto) at 15–16px. **Cyrillic**: `io.Fonts->AddFontFromFileTTF(path, px, nullptr, io.Fonts->GetGlyphRangesCyrillic())`. **Icons**: merge an icon font (FontAwesome `fa-*.ttf`) over the text font with `ImFontConfig` + `GlyphOffset`, map icons to `ImWchar` ranges. Rebuild fonts once at init — atlas rebuild at runtime costs a frame hitch and (in 1.92+) is dynamic anyway.
- **One draw context for menu + ESP**: same ImGui frame renders the menu window and the world-space ESP (drawn via `GetBackgroundDrawList()` so the game world stays between ESP and menu). No second render pass.

## Streamproof / capture exclusion

Capture software (OBS game capture, Discord, ShadowPlay, Game Bar) hooks the **game's** Present — an internal menu is therefore always on stream. Defenses, strongest first:

1. **External overlay** — nothing to exclude: the menu is a separate window; game capture never sees it. This is how most "streamproof" products work.
2. **`SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)`** (Win10 2004+) on the overlay window — excludes it from window capture and even PrintScreen-style shots; OBS *window capture* of the overlay goes black. Windows-only, no driver needed.
3. Internal only: detect capture/recording and blank the menu for those frames (hook detection of `CopyResource`/`CopySubresourceRegion` on the back buffer, or check `GetForegroundWindow` against known capture windows) — brittle, per-tool, last resort.

Nvidia ShadowPlay additionally captures at the driver level — the UC thread *"nvidia streamproof bypass"* covers thepresent-hook vs driver-capture layering. A topmost transparent window with `WDA_EXCLUDEFROMCAPTURE` set is itself an AC-observable signature (window enumeration) — weigh it against the target's AC.

## Pitfalls checklist

- **Alt-tab / resize crash** → recreate RTV when swap chain desc changes (or hook ResizeBuffers); on D3D9 also hook Reset.
- **Unload crash** → never `MH_RemoveHook` while the game is inside your hook: disable hook, `Sleep(50)` to let the in-flight call drain, then uninit ImGui (from a non-Present thread).
- **DllMain loader lock** → spawn a `CreateThread` worker in `DLL_PROCESS_ATTACH`; do all init there.
- **Menu crashes on some PCs** → stale device/context pointers after device loss; re-`GetDevice` if `Present` returns `DXGI_ERROR_DEVICE_REMOVED`.
- **Rendered under game's own menu** → the game draws after Present-time; hook a later point (after game UI) or accept under-menu rendering; never draw before the game's scene clears.
- **Multiple swap chains** (split-screen, picture-in-picture) → global Present hook fires for all; init/guard per-swapchain (map swapchain ptr → state).
- **Mouse cursor invisible with menu open** → `io.MouseDrawCursor = true` (software cursor) or force `ShowCursor`/`SetCursor` on the game window.
- **Fullscreen exclusive** → some games bypass Present composition; force borderless via `SetFullscreenState(FALSE)` or hook `Present1`.
- **32/64-bit** → vtable indices identical, but hooking lib and DLL arch must match the game.

## Beyond D3D11 — alternatives

D3D11 Present is the most-hooked, most-scanned point in the ecosystem. The escape routes, by what you're escaping:

**Same game, different hook point / API** — first check what the game actually uses (PresentMon, RenderDoc, import table) rather than defaulting to D3D11:
- D3D9 / D3D12 / OpenGL / Vulkan — see the hook table above; older AC builds cover D3D9 Present (42) far less thoroughly than D3D11.
- `Present1` instead of `Present` — different vtable slot (22), different signature; ACs that string-scan the `Present` prologue often miss it.
- **Chain onto a whitelisted hook** — Discord's/GeForce's overlays already hook the game's Present legitimately; hook *their* trampoline instead of creating a second hook on dxgi (UC thread *"Present hook in Discord"*). Nothing extra sits on the DXGI prologue to scan.
- Kernel-level presentation hooks (DXGK/DriverDisplay) leave usermode Present untouched entirely — see windows-kernel / anticheat-bypass lanes for that ceiling.

**External rendering, no game pipeline at all:**
- **Direct2D overlay** — own window, `ID2D1DeviceContext` + DirectWrite; the cleanest pure-ESP renderer (crisp text, no D3D12-style ceremony). UC tutorial *"Transparent overlay with Direct2D"* and `coltonon/D2DOverlay` (header-only, internal support too). No graphics hook exists to detect — the window is the only artifact.
- **GDI** — old school: own layered window + `TextOut`/`Rectangle`, or direct desktop-DC drawing (`GetWindowDC(NULL)`) which dies on every repaint — that wipe is why layered windows won. Zero D3D surface, minimal footprint, flickery, slow for 60 FPS menus; fine for cheap ESP.
- Overlay-window detection is the real cost, not the renderer: ACs enumerate for overlay-trait windows (UC *"Window HiJacking"* thread); `PierreCiholas/NotAnOverlay` documents designs that avoid presenting as an overlay at all.

**GUI frameworks instead of Dear ImGui** (Guided Hacking *"ImGui Alternatives"*):
- **RmlUi** — retained mode, HTML/CSS-like markup, the "pretty product menu" look; heavier and more setup than ImGui; rendering backend must be wired to your hook yourself.
- **Nuklear** — single-header immediate mode, smaller than ImGui, thinner ecosystem, fewer ready backends.
- **Raw ImDrawList / custom renderer** — skip widget machinery entirely, draw every control by hand; what the polished "product" menus actually do.
- Verdict of the scene: ImGui stays the default because its DX9/11/12/GL/Vulkan backends exist out of the box — an alternative framework must re-earn all of that plumbing.

## Sources

- [UC: Hook a D3D11 Overlay to a Game](https://www.unknowncheats.me/forum/d3d-tutorials-and-source/433114-hook-d3d11-overly-game.html) · [UC: finding IDXGISwapChain::Present](https://www.unknowncheats.me/forum/d3d-tutorials-and-source/411481-d3d11-idxgiswapchain.html)
- [Guided Hacking: Guide to ImGui Hooks for Game Hack Menus](https://guidedhacking.com/threads/guide-to-imgui-hooks-for-game-hack-menus.13993) · [GH: DX12 + ImGui tutorial](https://guidedhacking.com/threads/directx-12-hook-tutorial-with-imgui.20835) · [GH: ImGui WndProc Hook](https://guidedhacking.com/threads/imgui-wndproc-hook.9942)
- [werwolv: ImGui Game Overlays using DLL injection](https://werwolv.net/blog/dll_injection) (OpenGL/wglSwapBuffers walkthrough)
- [rdbo/ImGui-DirectX-11-Kiero-Hook](https://github.com/rdbo/ImGui-DirectX-11-Kiero-Hook) · [Rebzzel/kiero](https://github.com/Rebzzel/kiero) · [Sh0ckFR/Universal-Dear-ImGui-Hook](https://github.com/Sh0ckFR/Universal-Dear-ImGui-Hook) · [DrNseven/D3D12-Hook-ImGui](https://github.com/DrNseven/D3D12-Hook-ImGui) · [zetsr/Minimal-D3D9-Hook-ImGui](https://github.com/zetsr/Minimal-D3D9-Hook-ImGui) · [adamhlt/D3D11-Hook-ImGui](https://github.com/adamhlt/D3D11-Hook-ImGui) / [D3D11-Overlay-ImGui](https://github.com/adamhlt/D3D11-Overlay-ImGui) · [3Vathl/ImGui-External-Overlay](https://github.com/3Vathl/ImGui-External-Overlay) · [Ch1nChun/ImGuiMenu-s](https://github.com/Ch1nChun/ImGuiMenu-s)
- [ocornut/imgui releases](https://github.com/ocornut/imgui/releases) (1.92.x — texture protocol, dynamic fonts; changelog is required reading before pinning a version)
- [UC: nvidia streamproof bypass](https://www.unknowncheats.me/forum/alternative-online-mods/762875-nvidia-streamproof-bypass.html) · [UC: ImGui menu styling](https://www.unknowncheats.me/forum/off-topic/730797-imgui-menu-styling.html) · [UC: Post your Menu/GUI Framework](https://www.unknowncheats.me/forum/c-and-c-/68024-post-menu-gui-framework-93.html)
- [SO: getting DX12 CommandQueue from a swap chain](https://stackoverflow.com/questions/36286425/how-do-i-get-the-directx-12-command-queue-from-a-swap-chain) (ECL-hook technique; UE5.3+ multi-queue caveat)
- [MS Learn: IDXGISwapChain1::Present1](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgiswapchain1-present1) · [UC: Present hook detection](https://www.unknowncheats.me/forum/anti-cheat-bypass/486318-hook-detection.html) · [UC: Present hook in Discord (chaining whitelisted hook)](https://www.unknowncheats.me/forum/printthread.php?t=743872)
- [UC: Transparent overlay with Direct2D](https://www.unknowncheats.me/forum/programming-for-beginners/126003-transparent-overlay-direct2d.html) · [coltonon/D2DOverlay](https://github.com/coltonon/D2DOverlay) · [GH: C++ Overlay window (GDI)](https://guidedhacking.com/threads/c-overlay-window.10021) · [UC: external overlay window detection](https://www.unknowncheats.me/forum/anti-cheat-bypass/408764-external-overlay-window-detection.html) · [UC: Window HiJacking](https://www.unknowncheats.me/forum/2025225-post1.html) · [PierreCiholas/NotAnOverlay](https://github.com/PierreCiholas/NotAnOverlay)
- [GH: ImGui Alternatives](https://guidedhacking.com/threads/imgui-alternatives.13095) · [mikke89/RmlUi](https://github.com/mikke89/RmlUi)
