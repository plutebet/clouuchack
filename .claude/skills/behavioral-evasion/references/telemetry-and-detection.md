# Telemetry and detection — what servers compute

## Feature extraction (server-side, per match)

All of this ships without any client agent — it is reconstructed from authoritative server state (positions, view angles per tick, fire events, hit events):

- **View-angle deltas per shot:** angle moved in the ticks before/after firing. Histograms feed classifiers; snaps of specific magnitude clusters (e.g., every flick ~exactly 8.0°) are model fingerprints.
- **Pre-fire alignment:** angular distance between crosshair and target hitbox in a window ending at the click. Humans: wide distribution, settles late with noise. Lock-on: near-zero variance across hundreds of shots.
- **Tracking angular velocity:** continuous aim at moving targets. Human profile is discontinuous — saccade, overshoot, correction. Constant-velocity tracking is the textbook bot signature.
- **Time-to-target:** Fitts-law relationship between distance-to-target and acquisition time. Violations (far targets acquired in identical time as near) score high.
- **Reaction time:** target-visible → first aim movement. Human distribution is log-normal, floor ≈ 120–150ms under ideal conditions, mostly 200–400ms.
- **Post-kill target switch:** latency and re-acquisition error after a frag. Instant switch with zero error is inhuman.
- **Wallhack inference:** aim direction correlating with unseen enemy positions (behind cover, outside FOV history). Server knows what was *knowable* when — Ricochet Shield and Valve's patents operate here.
- **Hit-through-wall / cover rates**, pre-aim at future positions (predictive aim beyond human lead).
- **Click analytics:** inter-click interval distribution (mean, variance, refractory floor), double-click bpms, jitter spectrum. Autoclickers: too constant, or pseudo-random with detectable PRNG structure (fixed seeds!).
- **Movement entropy:** strafe timing regularity (tick-perfect = scripted), path repetition (nav-mesh orbits), 180° turn angular profiles.

## Classifiers and scoring

- **Threshold heuristics** (older/server-cheap): "pre-fire alignment < 1° for N shots", "reaction < 100ms" — batch-flag.
- **ML classifiers** (VACnet-class): trained on labeled human + cheat populations, consume the feature vectors above, output per-account suspicion. Key property: they score *distance from the human manifold*, so hard caps and uniformity hurt more than magnitude.
- **Accumulation:** confidence accumulates across matches and sessions; one clean session does not reset suspicion, one bad session does not insta-ban (usually).
- **Ban waves:** detections queue for days-to-weeks (VAC waves, FACEIT batches). Corollary: a bypass that "works" for two weeks proved nothing; corollary 2: testing on mains poisons HWID + account graphs.

## Per-vendor scoring notes

- **VAC/VACnet (Valve):** the dominant source of modern VAC bans; client scan mostly legacy signatures. Demo data is public — population distributions are free.
- **Ricochet Shield (CoD):** server-side aimbot/wallhack inference explicitly; client bypass irrelevant to this layer.
- **FACEIT AI + human review:** suspicion score surfaces in matchmaking, triggers manual demo review by moderators — human reviewers watch *how* the aim moves; statistical evasion must also survive eyeballing (smooth aim with no corrective phase still looks wrong at 0.5× playback).
- **ESEA:** longest-running client + server stats; hardware bans.
- **Riot (Vanguard):** client-heavy, but server telemetry still scores; HWID infrastructure aggressive.

## What this means for the build order

1. Client bypass (`anticheat-bypass`) handles memory/process detection — necessary, insufficient.
2. Humanizer (`humanization-craft.md`) shapes what the server sees — necessary on any server-scored title.
3. Account/infra hygiene: no cross-account feature correlation (same "human profile" on 5 accounts with identical parameters = clustering flag; profiles must differ per account AND drift independently).
4. Validation loop is mandatory, not optional — see humanization ref §validation.
