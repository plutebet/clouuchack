---
name: behavioral-evasion
description: Server-side behavioral and statistical detection evasion — aim/telemetry signatures, click and macro analytics, movement entropy, humanization models, distribution matching. Use when bans come from server-side ML (VACnet, Ricochet Shield, FACEIT AI) or when a client-side bypass works but flags still arrive. For client-side detection use anticheat-bypass; for AC architecture use anti-cheat-systems.
---

# Behavioral evasion — server telemetry

**The model:** modern detection scores *features*, not rules — per-shot view deltas, pre-click alignment, tracking angular profiles, click-interval distributions, movement entropy — computed server-side from match data, scored by ML (VACnet-class) or thresholds, batched into ban waves days-to-weeks later. Client-side bypass is invisible to it: the aimbot that never gets memory-scanned still ships its telemetry in every shot it takes.

**Prime directive:** land inside the *human population distribution* for every feature, not under a threshold. Detectors score feature-space distance and anomaly rank — "never snaps more than 3°" is itself an outlier (humans snap 10–40°); a hard cap is a signature. Model the population, sample per-account parameters from it.

**Feature → countermeasure map:**

| Server feature | Human profile | Cheat tell |
|---|---|---|
| Per-shot view delta | log-normal, age/skill-dependent spread | uniform small deltas, or perfect 0 pre-shot |
| Pre-click alignment | aim wanders, settles late, ±2° noise at click | crosshair locked on hitbox pre-click |
| Tracking profile | saccade → overshoot 3–12% → corrective drift 100–250ms | constant angular velocity, direct path |
| Reaction time | log-normal 180–450ms | sub-120ms responses to triggers |
| Click intervals | bursty, refractory, fatigue drift over session | near-constant or naive-gaussian |
| Movement | path entropy, imperfect strafes | tick-perfect repeats, orbiting LOS |
| Target switching | latency after kill, re-acquisition error | instant snap to next target |

**Discipline:** the humanizer must persist per account (one profile, drifting with session time), sample from real distributions (mine demo files — `.dem` parsing gives free population data), and degrade over time (accuracy drops late in session, on streaks, under fatigue). Full model: `references/humanization-craft.md`. What servers actually compute: `references/telemetry-and-detection.md`.

**Testing:** extract the same features from your own clean demos (baseline), from accounts running the humanizer, and compare — KS test / histogram distance per feature until distance matches natural variance. Never validate on a main: flag confidence accumulates account-side before the wave lands.

Refs in `references/`:
- telemetry-and-detection.md — server feature extraction, ML classifiers, ban-wave mechanics, per-vendor scoring
- humanization-craft.md — reaction/aim/click/movement stochastic models, demo-derived distributions, validation loop

Route: `anticheat-bypass` (client vectors), `anti-cheat-systems` (AC architecture), `game-server-security` (what the server validates), `game-math` (the aim math being humanized).

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
