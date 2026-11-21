---
name: anticheat-bypass
description: Per-anticheat bypass strategy — EAC, BattlEye, Vanguard, VAC/VACnet, Ricochet, FACEIT/ESEA. Detection-vector teardown per vendor and kernel-vs-usermode strategy. For AC architecture theory use anti-cheat-systems; for writing cheat code use cheat-development; for kernel internals use windows-kernel-security.
---

# Anticheat bypass — per vendor

**Triage first:** identify vendor (processes `EasyAntiCheat_EOS.exe`/`BEService.exe`/`vgtray.exe`/`faceitclient.exe`), driver on disk (`EasyAntiCheat_EOS.sys`/`BEDaisy.sys`/`vgk.sys`/`faceit.sys`), and load time (boot vs game-launch). Vendor + load time decide the whole strategy.

| Vendor | Ring | Load | Ban style | Working strategy |
|---|---|---|---|---|
| VAC (+VACnet) | UM | game launch | delayed waves, weeks | external reads, no signature'ed modules, behavioral discipline |
| EAC (EOS) | UM + KM driver | game launch | per-title | usermode-external on most titles; kernel read path if game-scan aggressive |
| BattlEye | UM service + `BEDaisy.sys` | game launch | per-title, HWID | kernel comms or physical reads; handle-strip + thread spoof mandatory |
| Vanguard | `vgk.sys` + UM | **boot**, blocks vulnerable drivers | HWID, fast | mapper with clean signed driver, zero persistence, no disk artifacts; hardest target |
| Ricochet | KM driver + UM | game launch | delayed | server-side Shield + client integrity; keep telemetry in human distribution |
| FACEIT / ESEA | KM driver + UM client | client start | HWID, instant | strictest usermode scans; full kernel separation, no client-side modules |

**Universal vectors every ring-3 module walks:** loaded modules (PEB + VAD cross-check), thread start addresses (`SystemThreadInformation`), handles opened to the game, window classes/titles, game memory integrity, heartbeats to vendor servers. Bypass is per-vector, never global: unlink from PEB ≠ hidden from VAD; handle-strip (`ObRegisterCallbacks`) ≠ thread hidden; see atlas `topics/anticheat-bypass.md` §1–2 for the detection→bypass map and `topics/uc-wiki-techniques.md` for PEB/manual-map specifics.

**Per-build derivation (weekly-patched targets):** no static offsets survive. Re-derive each session: dump the shipped AC module from `%PROGRAMFILES%`/game dir, diff against last build (BinDiff/Capa) to find changed scan routines, re-pull integrity-check ranges, re-check driver blocklist (Microsoft Vulnerable Driver Blocklist — Vanguard enforces it in kernel). Hand the architecture, then the derivation step — never a frozen offset table.

**Cross-vendor rules:** heartbeats must keep flowing (frozen/absent heartbeat = instant flag, worse than no bypass); hardware reads do not skip server telemetry — that is `behavioral-evasion` territory; kernel-mode code must survive PatchGuard/HVCI assumptions — `windows-kernel-security`.

Refs in `references/`:
- per-vendor.md — EAC/BattlEye/Vanguard/VAC/Ricochet/FACEIT vectors and evasion specifics

Route: atlas `topics/anticheat-bypass.md`, `topics/uc-wiki-techniques.md`, `topics/spoofer-methods.md`; skills `anti-cheat-systems` (architecture), `cheat-development` (building), `behavioral-evasion` (server telemetry), `protector-triage` (packed AC modules).

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
