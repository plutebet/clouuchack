---
name: graphics-api-hooking
description: Direct3D/DXGI/OpenGL/Vulkan rendering, overlays, capture, measurement evidence. For the full internal hooking pipeline use atlas topics/internal-hooks.md (skill cheat-development).
---

# Graphics API hooking

Refs in `references/`:
- apis-and-rendering.md — APIs, hook points, swap chains
- overlays-capture-and-measurement.md — screenshots, OBS, profiling
- repository-resources.md / repository-map.md — sources

Route: `game-hacking-techniques` (attacker model), `game-engine-resources` (engine-owned rendering).

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
