# VMProtect 3.x x64 — runtime patterns (field-verified)

Cross-verified patterns from a BattlEye BEClient_x64.dll siege (kernel-emulator
trace, 11M+ instructions decoded bit-exact) + public RE writeups (blast.hk 167637,
yougame 277698). Use when lifting VMP 3.5/3.6 x64 by trace.

## VM frame and dispatch (x64)
- Encrypted instruction RVA sits at RSP+90 inside the VM frame; the frame walks
  down (sub rbp/rsp, 8 per push), cells are rsp/rbp-relative.
- Handler commit / control transfer: `add rDst, rSrc; jmp rDst` or `push rSrc; ret`.
- Next-handler delta: 4-byte dword decoded from the bytecode cursor, then
  `movsxd` + `add self_ref, delta` = handler address (self-ref relative).

## Rolling key
- Key register (rotates per block) updates AFTER a decode: `xor key, decoded_value`.
- Initial key at VMENTER = image_base(preferred, from the PE header) + vip0 —
  NOT the runtime ASLR base.
- Direction: cursor moves -4 or +4 per read; effective stride 5 bytes/instruction
  (dword + opcode byte) is common.

## Transform chains (rkey blocks)
- Per-block short arithmetic on the accumulator: xor key; sub/add const; rol/ror;
  neg; dec/inc; not; bswap — 2-6 ops, POLYMORPHIC per block (same bytecode offset
  re-encrypts with the evolving key).
- In ultra-virtualized builds the chain TAIL executes inline from the bytecode
  section itself (.beN pages are executable, self-mutating x86).

## MBA normalization formulas (collapse junk arithmetic to semantics)
- SUB(x,y) = ~( (~x) + y )
- XOR(x,y) = ~( (~x)&y ) & ~( x&(~y) )
- AND(x,y) = ~( (~x)|(~y) )
- OR(x,y)  = ~( (~x)&(~y) )
- Stack trick: `add r9,rax` = push r9 / add [rsp],rax / pop r9
- Junk inserts: setcc/cmc/stc/clc, single-byte xor of a reg with itself,
  rol by 0x20 (mod 8 = 0), bsf/bsr on live flags.

## Integrity / self-check chain (VMP3 style)
GetModuleFileNameW -> NtOpenFile -> NtCreateSection -> NtMapViewOfSection ->
NtQueryVirtualMemory -> NtUnmapViewOfSection -> NtClose; CRC spread across many
sites (not one loop). Section-mapping answers and KUSER_SHARED_DATA parity are
load-bearing for these checks.

## Syscalls from the VM
Syscall number is decoded like any operand (e.g. 0x169 = NtRaiseHardError on
Win11 26100); control transfer around stubs uses `ret`. Expect the VM to read
KUSER_SHARED_DATA directly (hot loops, no syscall) — page parity is detectable.

## Trace-lifting recipe that worked
1. 144-byte record trace (pc + all 16 GPR + code bytes) per instruction.
2. Locate the anchor (verdict pop / known handler) by pc+rva.
3. Fetch events: `mov eAcc, dword ptr [rCur]` with cursor +/-4 within 4 records
   (either side); capture key at the first `xor eAcc, rKey`.
4. Final value right before the STORE/COMMIT; verify decode(enc, key) == value
   bit-exact across >= 20 samples before trusting the model.
5. The stream is linear per environment: environment values that enter ANY decode
   shift every downstream literal via the key. Sweep environment, diff streams.

## Search sources (where RE writeups actually live)
- **blast.hk** (BLASTHACK forum) — Russian RE/game-hacking forum; threads carry full
  technical writeups (VMProtect internals, MBA patterns, anti-debug). Thread 167637
  = VMP 3.5/3.6 x64 patterns. Search: `site:blast.hk <topic>`.
- **yougame.biz** — the original source of many blast.hk reposts (277698, 264690).
- **t.me/colby5engineering** — author's RE blog.
- **back.engineering** (_xeroxz / can1357 circle) — NoVmp, VTIL, devirt theory.
- **katyscode.wordpress.com** — VMP control-flow case studies (Honkai Impact 3).

## The vm-jcc canonical shape (from open-source VMPacker interpreter source)
Reference: github.com/LeoChen-CoreMind/VMPacker, stub/linux/arm64/vm_handlers/h_branch.h
- Compare handler: sets a FLAGS variable from two operands (`vm_set_flags(vm, R[a], R[b])`,
  FL_ZERO/FL_SIGN bits in one FL word).
- Conditional branch handler: `[opcode][target dword in bytecode]`; tests
  `FL & flag-bit`; taken -> `pc = target` (the packer PRE-REMAPS targets to the
  reversed/encrypted offsets); not taken -> fall-through (in reverse mode the
  dispatcher auto-DECREMENTS the cursor — dir_vip=-1 machines make fall-through
  free).
- Dispatch: handler returns either a step size or "pc already set"; a jump table
  `handlers[opcode](vm_ctx)` dispatches; opcodes are randomly mapped per build.
- Opcode encryption: `enc[pc] = op[pc] ^ (key ^ (pc * 0x9E3779B9))` (golden-ratio
  position mixing) — the same class as VMP's rolling key.
- The bytecode stream is stored in REVERSE execution order in this reference —
  the same design choice as VMP x64's dir_vip=-1.

## Hunting the vm-jcc in a decoded stream
Look for a handler pc where the commit is CONDITIONAL: same handler sometimes
stores to the frame and sometimes falls through, gated by a frame-cell read.
In a linear stream (constant environment) it always takes one side — so hunt it
in the HANDLER CODE (x86 bytes at the commit site): a frame-cell read followed
by an x86 jcc gating the delta store. Then feed the flag cell the passing value.
