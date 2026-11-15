---
name: protector-triage
description: Identify packer/protector via DIE and route to the matching unpacker. Only for "what is this packed with" + unpacking; auth cracks → cheat-crack-pipeline; VMP devirt theory → atlas.
---

# Protector Triage

**DIE** ([horsicq/Detect-It-Easy](https://github.com/horsicq/Detect-It-Easy)): read (1) direct name, (2) entropy — exec section >7.0 = packed, uniform-high = cryptor, (3) unnamed: empty IAT + RWX + odd sections = protector, (4) `(Heur)` outweighs stale sigs, (5) name≠section names (UPX0/1) = fake. Sigs: DiE-JS in `db/PE|ELF|Binary`; custom → `db_custom/`.

| DIE says | Unpack |
|---|---|
| UPX/MPRESS/ASPack | auto (`upx -d`) |
| .NET reactor/ConfuserEx | de4dot→dnSpy |
| VMProtect | [VMPStatic](https://github.com/notsnakesilent/VMPStatic) (no-run) → [NoVmp](https://github.com/can1357/NoVmp) x64 / [MogVMP](https://github.com/eversinc33/MogVMP) x86 / [titan](https://github.com/archercreat/titan) |
| Themida/WinLicense | [unlicense](https://github.com/ergrelet/unlicense) (Frida, auto OEP+IAT) or [Magicmida](https://github.com/Hendi48/Magicmida) `/unpack`; both run target → VM |
| Enigma | [DimitarSerg](https://github.com/DimitarSerg) + x64dbg→Scylla |
| Safengine | UnSafengine64 (MDPI 2024) |
| ASProtect/Obsidium | Tuts4You/at4re scripts |
| nothing | trust structure; manual x64dbg |

Rules: packer restores fully; protector dump = static material, not runnable. Index: [awesome-executable-packing](https://github.com/packing-box/awesome-executable-packing). Refs: atlas `topics/protector-identification-unpacking.md`, `vmprotect-unpacking.md`.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
