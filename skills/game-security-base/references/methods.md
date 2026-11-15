# Field-proven methods

Each method: what it does, what tool implemented it, what it defeated, and its
failure mode. All verified in real operations (see project research logs).

## Kernel-emulator siege (kernemul — Unicorn-based Windows usermode emulator)

1. **Ground-truth probing** — minimal C++ exe per syscall surface, same shapes
   the target uses, against the LIVE kernel; encode the real answers into the
   emulator. Defeated: NtTraceControl (per-code answers), ProcessInstrumentation-
   Callback (Version validation), KSecDD 0x390400 (BUFFER_OVERFLOW + body),
   WNF state names, KnownDlls sections, registry seeds. Failure mode: probing
   from a bare process misses session context (KSecDD answers are input-pure,
   service-independent — but client-built inputs differ upstream).

2. **Trace-lifting a VMP VM** — 144-byte/instruction trace (pc + 16 GPR + code
   bytes); locate anchor by pc+rva; fetch events = `mov eAcc,[rCur]` with cursor
   +/-4 within 4 records either side; key captured at first `xor eAcc,rKey`;
   verify decode(enc, key) == final bit-exact over >= 20 samples. See
   `reverse-engineering-tools` skill → vmprotect-x64-patterns.md.

3. **Environment sweep with verdict extraction** — vary one environment knob per
   run (TSC ns/GHz, KUSD fields), extract the target metric (verdict register at
   the anchor pop) from each trace. A shift = the knob feeds the stream.
   Failure mode: knobs that scale together (TSC + interrupt time) keep internal
   ratios constant — sweep must break RATIOS, not scales.

4. **Point patches via env var** — BE_PATCH="rva:regcol:value" (hex, regcol =
   record column 1=rax..16=r15) forces a register at a pc. Instant experiments
   without rebuilds. Failure mode: NEVER patch generic handlers (one patch
   corrupted the VM's generic address adder and manufactured a false crash).

5. **Section-mapped module capture** — modules arriving via NtCreateSection +
   NtMapViewOfSection bypass name-based module callbacks; capture by unique
   SizeOfImage at the map, derive entry from the mapped PE header.

6. **DllMain-return forcing** — remember the entry at the section map, hook the
   return, zero eax. Lets the load complete after a failed init — BUT the
   client is then half-initialized (its unpacker aborted); deeper stages hit
   undecrypted code. Use for observation, not for a working client.

7. **SEH visibility** — log the guest exception chain (TEB+0 walk, 3 frames) at
   every exception delivery. Answers "does the target register a recovery
   handler" without touching the walk itself.

## KUSER_SHARED_DATA parity
- Dump the live page (usermode-readable at 0x7FFE0000), diff field-by-field
  against the emulator's builder. A 2026 sweep found 82 divergent dwords incl.
  a transcription bug worth 32 days of TimeZoneBias. Fields a VM reads directly
  (no syscall) are fingerprints; time fields feed decode streams.
- Verify the build's syscall-stub prologue against KUSD: some builds test
  byte [0x7FFE0308] — the flag value is build- and machine-dependent
  (measure, don't assume).

## What does NOT work (closed branches — do not retry)
- TSC-rate sweeps against a VMP verdict that does not absorb TSC (18 points,
  all identical).
- Patching encrypted literals in files (rolling key desync).
- Patching pop/relay carriers of a verdict (copies inherit from upstream).
- kernelbase-level API hooks against services that call ntdll directly.
- ntdll syscall-stub patching and DR-register observation against BattlEye
  service (self-integrity + DR checks, self-terminates).
