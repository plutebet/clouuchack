---
name: game-supply-chain-security
description: Build/launcher/update/mod/dependency trust, signing, provenance. Use when release or content ingestion is the security boundary.
---

# Game supply chain security

Refs in `references/`:
- review-guide.md — release authority, recovery, mods, plugins
- repository-resources.md — CI, patching, assets, parsers

Route: `game-engine-resources` (runtime models), `windows-kernel-security` (driver artifacts).

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
