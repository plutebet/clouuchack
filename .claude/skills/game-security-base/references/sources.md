# Verified search sources

Where RE/game-security writeups actually live. Search these BEFORE inventing
from scratch; the answer is usually already banked in the open.

## Russian-language (deep technical, often leaked-build analyses)
- **blast.hk** (BLASTHACK forum) — RE/game-hacking threads with full technical
  content. VMP 3.5/3.6 x64 patterns: thread 167637. UEFI/SMM boot-tier map:
  thread 257954. Search: `site:blast.hk <topic>`.
- **yougame.biz** — originals behind many blast.hk reposts (threads 277698,
  264690 anti-debug by Ahora57). Often richer than the repost.

## English-language (devirt theory, tools)
- **back.engineering** — _xeroxz / can1357 circle: NoVmp (VMP 3.x x64 static
  devirtualizer), VTIL, devirt theory.
- **github.com/can1357/NoVmp** — static devirtualizer, VMP 3.0-3.5 x64.
- **github.com/LeoChen-CoreMind/VMPacker** — open-source VM protector (ARM64 ELF,
  Go+C): full pipeline in source — rolling XOR keyed by position (golden-ratio
  const), reversed bytecode, random opcode mapping, PT_NOTE interpreter injection.
  A Rosetta stone for how VM protectors are built; see references/open-protectors.md.
- **github.com/eversinc33/MogVMP** — Remill/LLVM-based devirt (32-bit only).
- **katyscode.wordpress.com** — VMP control-flow case studies (Honkai Impact 3).
- **github.com/mrexodia/ida-pro-mcp** and similar MCP bridges — debugger/RE tool
  automation (x64dbg-automate, Cheat Engine bridges — see project tooling).

## Advisory / vulnerability databases (kernel attack surface)
- **projectzero.chromium.org / issues.chromium.org** — kernel bug reports
  (e.g. KsecDD 0x390400 pool disclosure, CVE-2017-8529).
- **zerodayinitiative.com** — ZDI advisories (CVE-2018-8207 KsecDD OOB).
- **exploit-db.com** — PoC archives with struct definitions.

## Telegram (verify before use — many are lifestyle, not RE)
- Only vetted channels stay listed; unverified ones get removed on sight.

## Search queries that work
- `site:blast.hk <anticheat name> <topic>` — Russian RE depth.
- `<NTSTATUS> site:issues.chromium.org` — kernel bug context.
- `<vendor> <ioctl hex>` — device surface writeups.
- `<game> <anticheat> "UnmappedViewOfSection" OR "NtCreateSection"` — integrity
  chain writeups.
