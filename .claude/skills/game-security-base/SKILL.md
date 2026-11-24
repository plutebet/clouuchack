---
name: game-security-base
description: Accumulated knowledge base for game-security and reverse engineering — verified search sources (blast.hk, yougame, back.engineering), field-proven methods (trace-lifting, KUSD parity, pipe interception), per-vendor empirical facts (BattlEye siege map). Use when asked "where to search", "what do we know about X anticheat", or before starting any game-security operation.
---

# Game security base — accumulated field knowledge

Everything here was earned in real operations, not assembled from lore. When a
new operation starts, read this before searching the internet: the answer may
already be banked.

## Refs in `references/`
- sources.md — verified search sources and what each is good for
- methods.md — field-proven techniques with the tools that worked
- battleye.md — BattlEye empirical map (client, service, master channel)
- uefi-smm.md — boot phases, SMM/SMRAM exploitation classes, Inverted Call telemetry, where the tower starts
- open-protectors.md — open-source VM protectors as Rosetta stones (VMPacker: rolling XOR by position, reversed bytecode, random opcodes, interpreter injection)
- byovd.md — BYOVD pipeline (source → SDDL gate → IOCTL surface), case studies ardrv/NdkPing/hmpalert with IOCTL codes, 2026 corrections (HVCI blocklist, version pinning, load-trace hygiene)

## Routing
- VMP x64 runtime patterns → `reverse-engineering-tools` skill, `references/vmprotect-x64-patterns.md`
- Per-vendor strategy (EAC/BattlEye/Vanguard/VAC/Ricochet/FACEIT) → `anticheat-bypass` skill
- Anticheat architecture/detection layers → `anti-cheat-systems` skill
- Kernel contracts, KUSER_SHARED_DATA, callbacks → `windows-kernel-security` skill
- Project-specific operation logs → the project's `research/` folder (per-project, not here)

## Standing lessons (never relearn these the hard way)
1. Ground truth over theory: probe the LIVE kernel/process with a minimal exe
   before encoding any answer into an emulator. Theories cost days; probes cost
   minutes (probe_nt/probe_ksecdd/probe_wnf pattern).
2. A negative result is a result: record every sweep that found nothing — it
   closes a branch forever.
3. Observability first: before attacking a blocker, make the tracer see the
   whole window (tracer lifetime, fault pc vs entry pc, live-base dumps). A
   broken instrument manufactures false theories.
4. Patch nothing generic: verify a site is operation-specific before patching
   (a "verdict patch" once corrupted the VM's generic address adder).
5. Never patch encrypted literals: the rolling key absorbs every decoded value;
   a literal edit desyncs all downstream decodes.
