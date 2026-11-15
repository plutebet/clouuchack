---
name: game-server-security
description: Server-side authority — RPCs, sessions, authorization, replication, economy, retries, replay. Use when backend state is the security boundary.
---

# Game server and backend security

Refs in `references/`:
- review-guide.md — authority maps, authorization, economy
- time-ordering-and-replay.md — clocks, idempotency, prediction
- repository-resources.md — frameworks

Route: `anti-cheat-systems` (detectors), `game-engine-resources` (replication models), `game-supply-chain-security` (release artifacts).

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
