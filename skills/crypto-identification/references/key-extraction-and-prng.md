# Key extraction and PRNG recovery

## Entropy mapping

- `binwalk -E file`, or radare2 `p=e` on sections: flat high entropy (.rdata region) = keys, ciphertext, or packed code; distinguish by xrefs — key blobs are referenced once at init, packed code is the section itself.
- Key-sized blob hunting: filter .rdata for 16/24/32-byte aligned runs with entropy > 5 bits/byte, referenced from the same function that references the cipher constants. AES key expansion buffer = 176 bytes (AES-128, 11 round keys), 240 (AES-256) — a 176-byte writable buffer next to a 32-byte key blob is the expanded key, original sits elsewhere.
- Stack keys: key never in .rdata — built on stack before first use. Find via hooking the cipher entry and dumping its key argument (see frida hooks, bottom).

## Known-plaintext key recovery

For XOR/stream (incl. rolling XOR with position-dependent byte `key[i % n] + i`-style):

1. Anchor plaintexts: PNG header `89 50 4E 47 0D 0A 1A 0A`, MZ `4D 5A 90 00`, PK `50 4B 03 04`, JPEG `FF D8 FF`, ELF `7F 45 4C 46`, HTML `<html`, JSON `{"`, XML `<?xml`.
2. `keystream = ciphertext ⊕ known_plain` at those offsets; repeat across multiple anchors; look for period (repeating keystream) — period found = key recovered regardless of cipher "strength".
3. Rolling keys (`enc[i] ^ (key + i)`): keystream is linear in i — recover base from two anchors.
4. Interpolate missing region, decrypt, verify against expected structure (headers parse, strings readable).

## HWID / environment-derived keys

Loader encrypts config with key = hash(machine ID, username, volume serial). Paths:

- Hook hash input, not output: `CryptHashData`, `BCryptHashData` (bcrypt.dll), `CryptCreateHash`/`CryptDeriveKey` (advapi32), or the custom digest function found via constant scan. Input bytes going in = pre-image; then re-derive offline for any machine.
- String assembly: break on `sprintf`/`wsprintfA`/`std::to_string` calls feeding a hash function — concatenation order is the salt format.
- If derived from networked auth server response (KeyAuth `ENCKEY` pattern): see `cheat-crack-pipeline` — match the emulator's key, or patch the validation to skip decryption (rare — usually you need the plaintext).

## Custom-cipher cryptanalysis (black-box)

1. **Determinism:** same input twice → same output? No = IV/nonce present, find it (usually prepended or in packet header).
2. **ECB test:** encrypt `P` and `P'` differing in one 16-byte block; equal other blocks = ECB-mode block cipher.
3. **Diffusion depth:** flip 1 bit of input; count output bits changed. Full diffusion (≈50%) after k transformations = k rounds. Homegrown ciphers typically 1–4 rounds — recoverable by algebra or brute per-round.
4. **Linearity probe:** flip high bits of first byte; if output changes are confined to one block/byte, cipher is byte-local (recovered by table, not cryptanalysis).
5. **Keystream reuse:** encrypt all-zero plaintext → output = keystream directly (for stream constructions).

## PRNG state recovery

| Source | Recovery |
|---|---|
| MSVC `rand()` (LCG 2^32) | state = `(out - 2531011) * inv(214013) mod 2^32`; inv exists (odd modulus factors) |
| glibc TYPE_0 | state = `(out - 12345) * inv(1103515245) mod 2^31` |
| LCG truncated to k high bits | solve via lattice / 2–3 consecutive outputs (Hacker's Delight §lattice method) |
| MT19937 | 624 consecutive outputs → untemper each → full state; then predict everything |
| Java Random | 48-bit state; `nextLong` leaks 64 bits of 2 calls → solve; seed = `(out_high - 11) * inv(0x5DEECE66D) mod 2^48` trick with `nextInt` |
| `time()`-seeded | brute seed window around build/run timestamp (±1 day = 86400 candidates) |
| Session-random (urandom/GenRandom) | not recoverable — hook the consumer instead |

Untemper MT19937: reverse the three tempering transforms (right-shift xor, left-shift xor with mask, final right-shift) — standard 30-line routine; generate on request.

## Runtime key capture hooks (frida one-liners)

```javascript
// CryptHashData: dump input buffer
Interceptor.attach(Module.getExportByName(null, 'CryptHashData'), {
  onEnter(a) { console.log(hexdump(a[1], {length: a[2].toInt32()})); }
});
// BCryptHashData same shape: (hHash, pbInput, cbInput, flags)
```

Dump-at-first-use beats static key hunting whenever the key is runtime-derived — which is the default in anything above toy loaders.
