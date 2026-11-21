# UEFI/SMM — boot-phase and ring-2 map (for bootkit-tier operations)

Source: blast.hk thread 257954 (2026-09, sashka123555). Educational map of what
sits below the kernel — the tier ABOVE which every usermode/kernel anticheat
battle happens. Vendor-agnostic; quoted facts cross-checked against tianocore.

## Boot chain (x86-64 PC)
RESET# -> vector 0xFFFFFFF0 (FAR JMP) -> SEC (Cache-as-RAM, long-mode setup,
tianocore/edk2 style) -> PEI (MRC memory training, XIP from flash, TE-format
modules, stack migration) -> DXE (DxeMain, HOB list, Boot/Runtime Services,
PE32+ drivers) -> SMM init via PiSmmIpl.
SPI flash regions: Flash Descriptor, Intel ME / AMD PSP, BIOS/UEFI, GbE, PDR —
PCH reads the descriptor map at boot; who owns which region decides what can
be reflashed.

## SMM (ring -2)
- SMI entry -> State Save Area -> handler -> RSM. SMBASE relocation matters
  (where the entry code lives).
- SMRAM protections: TSEG range, D_LCK (SMRAMC), SMRR, SMMCAC — after lock,
  kernel code cannot read/write SMRAM (that is the whole point of the tier).

## Exploitation classes (from inside SMRAM)
1. **SMM callout** — a vulnerable SMI handler calls gBS (boot services) which
   LIVE IN UNPROTECTED RAM after ExitBootServices-adjacent phases: an attacker
   overwrites gBS function pointers -> SMM executes attacker code (safe
   handlers use gSmst->SmmAllocatePool; vulnerable ones dereference gBS).
2. **Comm Buffer Overflow** — SMI handlers copy *CommBufferSize bytes into a
   fixed 64-byte stack buffer inside SMRAM without validating the size ->
   stack overwrite in ring -2.
- Trigger: software SMI via port 0xB2 (CHIPSEC: cs.regs... send_SW_SMI).

## Anticheat reality check (the thread's core claim, field-agreeable)
- Modern anticheats hold 70-80% of their defense in a kernel driver + usermode
  DLL + exe; SMM-based cheats are largely unnecessary in 2026 — a normal driver
  "хватает по горло".
- Detection of SMM cheats is EASY for the anticheat: it detects the
  communication driver, not SMRAM contents.
- Kernel->user telemetry pattern (universal): **Inverted Call** — driver queues
  pending IRPs; events complete them (IoCompleteRequest); user threads
  replenish with OVERLAPPED/IOCP. Every serious vendor runs this shape.
- Kernel callbacks a vendor driver mounts (expect all of them): ObRegister-
  Callbacks (handle strip), CmRegisterCallbackEx (registry),
  PsSetCreateProcessNotifyRoutine, PsSetCreateThreadNotifyRoutine,
  PsSetLoadImageNotifyRoutine.
- BYOVD is heavily fingerprinted: kdmapper's Intel vulnerable driver is
  detected on sight — mapper builds need cleaner vectors.
- mrac.exe (MyGames/MRAC) — usermode anticheat process talking to its kernel
  driver via IOCTLs + local RPC (MyGames titles).

## Where this fits the tower
UEFI/SMM (this file) -> kernel (windows-kernel-security skill) -> usermode AC
process/service -> game process -> the target. A siege climbs the tower from
the top; a firmware cheat starts at the bottom.
