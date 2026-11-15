---
name: reverser-triage
description: Fast-path binary triage — format/arch/mitigations, high-signal strings and imports in under a minute.
---

# Binary Triage (fast path)

1. Identify: format, arch, bitness, NX, PIE, entry.
2. Mitigations: checksec (canary, RELRO, fortify) — missing = flag.
3. Strings, categorized: secret / url / crypto / version; secrets → flag, urls → entrypoint/C2 candidates.
4. Imports: `nm -D` or `objdump -T` dynamic undefined symbols; risky API combos → deeper.
5. Version → CVE lookup for known libraries.
6. Handoff: NX/PIE off + dangerous imports = exploit-dev target.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
