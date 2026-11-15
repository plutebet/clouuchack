# Open-source protectors as Rosetta stones

Commercial protectors (VMProtect) hide their VM design; open-source protectors
teach the same design in readable source. Study them to predict what a
commercial VM's interpreter must be doing.

## LeoChen-CoreMind/VMPacker (Go + C, AGPL, ARM64 ELF, 522 stars)
github.com/LeoChen-CoreMind/VMPacker — function-level VM protection, the whole
pipeline in source:
1. Decode ARM64 (table-driven decoder) -> translate to a 63-instruction custom ISA.
2. **OpcodeCryptor**: `enc[pc] = op[pc] ^ (key ^ (pc * 0x9E3779B9))` — rolling
   XOR keyed by position and the golden-ratio constant. Same class as VMP's
   rolling key; position-dependence is the shared idea.
3. **Bytecode stored in REVERSE execution order** — the interpreter walks
   backwards (VMP's dir_vip = -1 is the same trick; ours moves -5/instr).
4. **Randomly mapped opcodes** (58+ ops, shuffled per build) — opcode values
   carry no semantics; the dispatch table is the only truth.
5. **Interpreter injection via PT_NOTE segment hijack** + 3-instruction entry
   trampoline + runtime-filled stack jump table (indirect dispatch, breaks IDA
   xrefs). VMP's .beN sections + VMEnter stub are the commercial analogue.
6. C interpreter with modular handlers (ALU/mem/branch/cmp/mov/stack/system),
   CRC32 integrity check, section-scattering macros.

## What reading a source protector gives a siege
- The dispatcher's conditional-jump shape: how a flag selects between deltas
  (the exact question our vm-jcc hunt is asking — the reference shows the
  canonical implementations: CSEL-style select into the delta, or a flag-
  indexed jump table).
- The key-evolution contract (what updates the key and when) in unobfuscated form.
- The handler coverage map (what an ISA of ~60 ops covers: VMP needs fewer).

## Limits
- ARM64 ELF only (x64/PE on the roadmap, not implemented).
- Virtualization-only (no mutation), no devirtualization tooling.
- Not compatible with VMP 3.x bytecode — a design Rosetta stone, not a decoder.
