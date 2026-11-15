---
name: crypto-identification
description: Identify and defeat custom/standard cryptography in binaries and protocols — cipher fingerprinting via magic constants, key extraction, weak PRNG recovery, entropy analysis. Use when a loader config, packet payload, or asset file is encrypted and the key or algorithm is unknown. For packers/protectors routing use protector-triage; for loader auth cracks use cheat-crack-pipeline.
---

# Crypto identification

**Decision tree:** (1) Where does the ciphertext live — file, config blob, packet, memory region? (2) Scan for magic constants → match = standard algorithm, exact implementation (IV/ECB matters, not the math). (3) No constants → custom: XOR-rolling, stream cipher, or homegrown block cipher — cryptanalyze black-box, don't read it as AES. (4) Key hunt in parallel at every step: keys live in `.rdata` blobs, stack-initialized buffers, or derived from HWID/username.

**Constant scan first** — it settles 70% of cases in minutes. YARA or a 20-line scanner over the binary for the table in `references/fingerprint-table.md` (AES S-box/Te tables, SHA/MD5 IVs, ChaCha sigma, TEA delta 0x9E3779B9, CRC polynomials, MT19937, LCG multipliers). A match gives you the algorithm; the *mode and key* still come from context — check IV reuse by encrypting the same plaintext twice.

**No constants → custom cipher.** Black-box it: feed controlled plaintexts, diff outputs (`references/key-extraction-and-prng.md` § cryptanalysis workflow). Most "custom" ciphers in loaders are XOR with rolling position-dependent key — defeat is known-plaintext recovery of the keystream, not breaking the cipher.

**Key extraction paths, cheapest first:**
1. Key sits in plaintext in `.rdata` — scan entropy + `strings` around the cipher's referenced data.
2. Key derived from machine (HWID, username, volume serial) — hook the hash input (`CryptHashData`, `BCryptHashData`, or the custom digest) and read what goes in.
3. Key stack-initialized/decoded at runtime — hook or dump at first use; rolling-decode style (see anti-cheat exemplar) means plaintext exists only at the call site.
4. Key negotiated — hook the derivation, don't attack the exchange.

**Weak PRNG recovery** when keys/seq/nonces come from `rand()`, `time()`, LCGs, MT19937: full recovery recipes in `references/key-extraction-and-prng.md`. Strong sources (`BCryptGenRandom`, `RtlGenRandom`, `/dev/urandom`) — walk away, hook the consumer instead.

Refs in `references/`:
- fingerprint-table.md — magic constants for standard ciphers/hashes/PRNGs + YARA skeleton
- key-extraction-and-prng.md — entropy scans, known-plaintext, HWID-derived keys, PRNG state recovery, custom-cipher cryptanalysis

Route: `protector-triage` (packed blobs), `cheat-crack-pipeline` (ENCKEY/loader decryption), `protocol-teardown` (packet-layer crypto), `frida-instrumentation` (runtime hooks for key capture), atlas `topics/re-tools-cracking.md`, `topics/vmprotect-unpacking.md`.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
