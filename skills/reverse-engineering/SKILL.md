---
name: reverse-engineering-tools
description: Reproducible disassembly/decompilation/debugging of game binaries, protectors, drivers, dumps. For unpacking/protector routing use protector-triage; for VMP devirt see atlas vmprotect-unpacking.md.
---

# Reverse-engineering tools

Refs in `references/`:
- evidence-and-tools.md — debuggers, disassemblers, DBI
- analysis-and-protection.md — anti-analysis, obfuscation
- vmprotect-x64-patterns.md — VMP 3.x x64 runtime patterns, field-verified (frame at RSP+90, dispatch forms, rolling key ^= decoded, MBA normalization formulas, self-check chain, trace-lifting recipe)
- ecosystem-and-advanced.md — plugins, MCP tools, diffing
- repository-resources.md / repository-map.md — sources

Route: `windows-kernel-security` (kernel contracts), `game-engine-resources` (engine models).
Search sources for RE writeups: blast.hk (BLASTHACK, Russian RE forum — VMP/anticheat
threads), yougame.biz (originals behind blast.hk reposts), back.engineering (NoVmp/VTIL)
— full list in `references/vmprotect-x64-patterns.md`.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
