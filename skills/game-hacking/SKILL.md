---
name: game-hacking-techniques
description: Cheat capability taxonomy across memory, injection, rendering, input, engines, kernels — threat modeling, not operational deployment. For writing cheats use cheat-development.
---

# Game-hacking techniques

Refs in `references/`:
- capabilities-and-core-techniques.md — privilege levels, injection, cheat categories
- rendering-memory-and-input.md — overlays, W2S, driver comm
- platform-and-evasion.md — EFI, HWID targets, stack spoofing
- engines-and-workflow.md — engine-specific surfaces
- attack-surface-map.md / repository-resources.md / repository-map.md — evidence

Route: narrower engine/graphics/kernel/server skill when one boundary dominates.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
