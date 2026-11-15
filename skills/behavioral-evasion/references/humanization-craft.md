# Humanization craft

## Design stance

A humanizer is a **stochastic state machine sampled from population distributions**, not a function with noise sprinkled on. Every constant you ship (delay, overshoot, smoothing k) becomes a fingerprint the moment it repeats. Structure:

1. Parameterize each behavior with distributions, not values.
2. Sample per-account profile once (persisted), drift per session.
3. Every observable must degrade with session time (fatigue model).

## Reaction model

Log-normal: `RT ~ exp(N(μ, σ))`. Calibration from real data: mean ≈ 250ms, σ ≈ 0.25; floor at 150ms (clamp, don't truncate — clamped values still cluster at the floor and that cluster is a signature; instead resample). Add:

- Stimulus-dependent delay: audio cue +30–60ms vs visual; peripheral-vision +80–150ms.
- Session drift: μ increases ~10–20% over 2 hours.
- Occasional whiffs: 5–10% of reactions land on wrong target or late by 2× (humans misread; perfect reaction every time is the tell).

## Aim model — two-phase saccade

Human acquisition of a target at angular distance D:

1. **Saccade (ballistic):** fast move covering ~90% of D, duration ≈ 20 + 2·D ms (D in degrees), with **overshoot 3–12%** (higher on large D).
2. **Corrective phase:** 100–250ms of low-velocity drift landing on target, sometimes a second micro-saccade.
3. **Tremor/noise:** ±0.05–0.2° persistent noise on the crosshair, even "at rest".

Implementation shape: piecewise angular velocity profile (accelerate → peak → decelerate past target → correct), noise as smoothed random walk (raw white noise looks synthetic — filter it: `noise = lerp(noise, randn, 0.1)`).

Tracking moving targets: humans lag with correction bursts, not continuous lock. Model: aim follows target with time-constant ~150–250ms plus correction impulses when error exceeds ~1.5°.

## Click model

- Inter-click intervals: bursty — 2–6 clicks fast (80–150ms apart), then pause 400–2000ms. Not gaussian, not constant.
- Refractory floor: no two clicks under ~60ms (humans can't; sub-60ms regularity = autoclicker).
- Fatigue: variance grows, bursts lengthen late-session; accuracy (timing on target) drops.
- Never seed PRNGs deterministically per session — seed from entropy; identical jitter sequence across two sessions is detectable by cross-correlation.

## Movement model

- Path entropy: insert micro-deviations into repeated routes; humans don't walk the same line twice.
- Strafe timing jitter ±20–50ms around optimal; never tick-perfect.
- 180° turns: saccade-overshoot model applies (same as aim).
- Cross-coupling: movement and aim correlate in humans (peek → aim settles after stop). Decoupled modules (perfect aim while strafing) read as scripted.

## Per-account profiles

Sample each parameter set (μ_reaction, σ_overshoot, burst pattern, k_aim, tremor scale) independently per account from the population priors — **and keep them**. Cross-account clustering (same profile vectors) is its own detection: detector-side clustering of feature vectors across accounts flags farms. Profiles drift independently over weeks.

## Population data — mine demos

Source distributions from reality, not intuition:

- CS/CS2: `.dem` files parse with demoinfo/demo.js/demoparser — per-tick view angles, shots, clicks. Extract the feature set from hundreds of matches → priors for every model above.
- Movement/shooter genre equivalents: kill-cams, server logs, published reaction-time literature (esports: 150–200ms pros, 250+ casual — pick the percentile band the account's rank implies).

**Rank-matching matters:** a Gold account with pro-tier Fitts curves is an anomaly in its own population. Sample from the distribution of that skill band.

## Validation loop

1. Build the same feature extractor the server plausibly runs (from the telemetry ref list) over parsed demos.
2. Baselines: (a) clean human accounts, (b) known cheat accounts (public VAC-banned demo sets).
3. Run your humanizer on throwaway accounts → extract features → per-feature distribution distance vs baseline (a) using KS test / Wasserstein; iterate until distances match human-vs-human variance, and classification accuracy of your bot-vs-human model collapses to chance.
4. Longitudinal: 20+ matches per test account, watch for gradual suspicion signals (matchmaking scoring, review flags) before scaling.
5. Eyeball test: record aim at 0.5× — if it reads robotic to you at slow playback, it reads robotic to a FACEIT moderator.
