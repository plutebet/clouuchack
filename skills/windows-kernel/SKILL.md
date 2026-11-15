---
name: windows-kernel-security
description: Windows driver trust, callbacks, IRQL, kernel memory, DSE, PatchGuard, VBS/HVCI, ETW, crash evidence — build-matched internals. For BYOVD/BYOVD-adjacent work see atlas anticheat-bypass.md.
---

# Windows kernel security

Refs in `references/`:
- foundations-and-security.md — driver surfaces, DSE, VBS/HVCI
- drivers-and-observation.md — callbacks, IRQL, APCs, ETW
- memory-and-forensics.md — pool, dumps, tools
- threats-and-virtualization.md — vulnerable drivers, hypervisors
- repository-resources.md / repository-map.md — sources

Route: `game-security-research-rigor` (version-sensitive conclusions).

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
