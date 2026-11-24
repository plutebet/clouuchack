---
name: reverser-overview
description: Root pointer for the binary reversing lane — routes to triage, firmware, packer unpacking, ROP, anti-debug sub-playbooks.
---

# Reverser Skill Catalog

Installed sub-skills: `reverser-triage` (first-pass ELF/PE/Mach-O), `reverser-firmware` (router/IoT extraction). Packer unpacking → `protector-triage`. Not installed (upstream only): rop-chain, anti-debug-bypass playbooks.

Workflow: identify (format/arch/NX/PIE) → packer check (entropy+signature; packed → unpack, re-identify) → categorized strings (url/ip/crypto/secret/version) → symbols/imports risk → version strings → CVE lookup → deeper via Ghidra/r2 scripts on disk.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
