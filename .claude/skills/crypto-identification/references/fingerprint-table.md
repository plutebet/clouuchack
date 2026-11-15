# Fingerprint table — magic constants

Scan binary for these; a hit names the algorithm. Endianness: constants listed as memory-dword (little-endian byte order in file).

## Block ciphers / stream

| Constant | Bytes (hex) | Algorithm |
|---|---|---|
| AES S-box first byte | `63` at table start, full box `63 7c 77 7b f2 6b 6f c5` | AES |
| AES Te0[0] | `c66363a5` | AES (T-table impl) |
| ChaCha/Salsa sigma | `61707865 3320646e 79622d32 6b206574` ("expand 32-byte k") | ChaCha20, Salsa20 |
| TEA/XTEA/XXTEA delta | `9e3779b9` | TEA family |
| Blowfish P-array[0] | `243f6a88` | Blowfish |
| RC5/RC6 magic | `b7e15163 9e3779b9` | RC5/RC6 |
| SM4 FK | `a3b1bac6 56aa3350 677d9197 b27022dc` | SM4 |
| DES weak markers | IP table `58 50 42 34 26 18 0a 02` | DES |

## Hashes

| Constant | Bytes | Algorithm |
|---|---|---|
| IV[0..4] | `67452301 efcdab89 98badcfe 10325476` | MD5 **or** SHA-1 (distinguish: SHA-1 has 5th `c3d2e1f0`, MD5 has 64-entry sine table) |
| SHA-256 IV | `6a09e667 bb67ae85 3c6ef372 a54ff53a` | SHA-256 |
| SHA-512 IV | `6a09e667f3bcc908 bb67ae8584caa73b` | SHA-512 |
| SHA-256 round constants | `428a2f98 71374491 b5c0fbcf e9b5dba5` | SHA-256 |
| CRC32 poly | `edb88320` (reflected) / `04c11db7` (normal) | CRC-32 |
| CRC-16-CCITT poly | `1021` | CRC-16 |
| Adler-32 modulus | `fff1` (65521) | Adler-32 |
| MurmurHash3 constants | `cc9e2d51 1b873593` | Murmur3 |
| xxHash64 prime | `9e3779b185ebca87` | xxHash64 |
| FNV-1a 32 | prime `01000193`, offset `811c9dc5` | FNV-1a |
| djb2 | seed `1505` (5381), mult `21` (33) | djb2 |
| Base64 alphabet | `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/` | Base64 (nonstandard alphabets = custom, diff against it) |

## PRNG

| Constant | Bytes | Generator |
|---|---|---|
| MT19937 matrix A | `9908b0df` | Mersenne Twister |
| MT init multiplier | `6c078965` (1812433253) | MT19937 init |
| PCG/MMIX multiplier | `5851f42d4c957f2d` (6364136223846793005) | PCG-XSH-RR, MMIX LCG |
| MSVC rand | `045d9f3b` (214013), addend `02741` (2531011) | MSVC `rand()` |
| glibc rand | `41c64e6d` (1103515245), addend `3039` | glibc TYPE_0 |
| Numerical Recipes | `0019660d` (1664525), addend `3c6ef35f` (1013904223) | NR LCG |
| Java Random | `5deece66d` (25214903917) | `java.util.Random` |
| xorshift64* multiplier | `2685821657736338717` | xorshift64* |

## No-constant algorithms (behavioral ID)

- **RC4:** 256-byte identity-initialized S array, KGA shuffle loop (i/j swap pattern). No table in `.rdata`; find by the 256-iteration init loop or `S[i]` swap pattern in the disasm.
- **XOR rolling:** no constants; ciphertext plaintext-position dependent. Identify by black-box (see key-extraction ref).
- **ChaCha without sigma:** stripped sigma — timing/instruction pattern (ARX quarter-round) is the tell.

## YARA skeleton

```yara
rule loader_crypto_constants {
    strings:
        $sha256 = { 67 E6 09 6A 85 AE 67 BB }        // IV LE
        $md5sha1 = { 01 23 45 67 89 AB CD EF }
        $tea_delta = { B9 79 37 9E }
        $chacha = { 65 78 70 61 6E 64 20 33 32 2D 62 79 74 65 20 6B } // "expand 32-byte k"
        $crc32 = { 20 83 B8 ED }
        $mt_a = { DF B0 08 99 }
        $msvc_rand = { 3B 9F 5D 04 }
    condition:
        uint16(0) == 0x5A4D and 2 of them
}
```

Match → confirm in disassembly at the xref: constant referenced from a loop = algorithm used, constant present but unreferenced = dead table from a linked-in lib (check xrefs before celebrating).
