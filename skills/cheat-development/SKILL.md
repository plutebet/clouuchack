---
name: cheat-development
description: Write cheat code — external/internal skeletons, w2s, Present hook, overlay/input. Building only; analysis → anti-cheat-systems, engine internals → game-engine-teardown.
---

# Cheat Development

**Architecture pick:** external (RPM/WPM + own overlay; dies to ObCallbacks handle-strip) vs internal (manual map + Present hook; dies to VAD `Subsection==NULL`, image-load callbacks, stack walk). Server-ML games → input writes, never angle writes.

**External:** Toolhelp→PID→handle (SeDebugPrivilege) → module base → **pattern scan, never hardcoded RVA** → read-loop with snapshot tick.

**W2S:** clip = vec·viewProjᵀ; NDC = clip.xyz/clip.w; screen = (NDC.x·.5+.5)·W, (.5−NDC.y·.5)·H; cull `w<0.1`.

**Internal:** manual map (ref impl: `C:\Users\sss\Desktop\скиллы ИИ\manual-map-injector-src\`) → hooking classes and full DXGI pipeline in atlas `topics/internal-hooks.md` (vtable vs trampoline vs VEH choice, dummy-device resolve, ImGui/WndProc input gating, stealth variants) → min skeleton: DXGI `Present` vtable hook (idx 8; D3D9 EndScene = 42) → ImGui on hooked device → WndProc gate → clean unload.

**Features:** ESP (entity walk, box/skeleton), aim (screen-space FOV pick + humanized smoothing — ML catches snap), RCS reads punch angle.

Refs: atlas `topics/anticheat-bypass.md` (kernel view), `uc-wiki-techniques.md` §2 (pattern scan), `manual-map-injector-teardown.md` (blind window); skills `anti-cheat-systems`, `game-engine-teardown`.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
