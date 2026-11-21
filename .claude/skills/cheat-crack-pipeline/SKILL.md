---
name: cheat-crack-pipeline
description: Crack paid cheat loaders — KeyAuth emulator/patch, dnSpy IL edits, dump manual-mapped cores. Loader cracking only; unpacking → protector-triage.
---

# Cheat Crack Pipeline

**Triage:** .NET loader → de4dot→dnSpy; native → x64dbg+ScyllaHide (VMP sections = `\.vmp\d`, tiny loader + encrypted core). Identify auth: KeyAuth (~70% market) / custom / HWID-bound.

**KeyAuth:** client-side — IL-patch validation to `true` (nop body or `jz`↔`jnz`); server-side — [Fadi002/keyauth-Emulator](https://github.com/Fadi002/keyauth-Emulator) (Flask, `/api/1.0|1.1|1.2/`), **`ENCKEY` must match target** else decryption breaks; hosts/Fiddler redirect; API version picks the tool (emulator→1.2, patchers→1.3).

**Encrypted core:** auth-patch alone gives nothing → dump post-decryption: pass loader → wait for manual-map inject (absent from module list) → dump game memory → Scylla IAT fix → patch residual HWID/heartbeat/signature checks → patch loader↔dump integrity hashes. Denuvo principle: replace the checking function, never fake the server response.

**VMP targets:** kernel-in-kernel + VMP = not crackable — crack surface is the .NET loader or external cheat. Public "cracks" post-2025 = stealers; trust only with dump.

Refs: atlas `topics/re-tools-cracking.md` §3 (primary), `manual-map-injector-teardown.md`; skills `reverse-engineering`, `protector-triage`.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
