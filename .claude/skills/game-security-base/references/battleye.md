# BattlEye empirical map (Unturned, 2026 siege)

Everything below was verified in operations against BEClient_x64.dll (VMProtect
3.x x64, sections .be0/.be1/.be2), BEService, and the BE Master channel.
Full operation log: any project research/ folder carrying the siege diary.

## Architecture (Unturned specifically)
- No BEDaisy.sys for this title — the fortress is purely usermode.
- Launcher file-blocks foreign DLLs (proxy + version.dll blocked).
- The game loads the SERVICE-STAGED client (Temp\BEClient2.dll), not the
  BattlEye folder file — a foreign BEClient_x64.dll is never loaded by the game.
- BEService self-protection: ntdll stub integrity (kills on patch), DR-register
  checks (kills hardware hooks), kernelbase NOT checked but bypassed (calls
  ntdll directly). Injecting with SeDebugPrivilege works; observation via hooks
  does not.
- Service pipe: \\.\pipe\BattlEye. Client handshake [06], session init [02]
  (response format lives in the service's VM'd dispatcher; region 0047/0051).
- Client validation = Authenticode publisher chain ("BattlEye Innovations e.K.",
  "Smartly Dressed Games Ltd."), staged via Temp\BEClient2.dll.
- Service uses BlackBone-style manual mapping to inject the client.

## BEClient_x64.dll internals
- VMProtect 3.x x64: sections .be0/.be1/.be2, ~13 real handlers, rolling key
  (key ^= decoded payload), self_ref + delta dispatch, DllMain fully
  virtualized (entry point == VMEnter, no native prelude).
- Kill path: verdict 0x30 (decoded literals) -> NtRaiseHardError(0x50000018)
  -> silent unwind -> terminate. Unconditional relay — no branch. Environment
  enters via VM handlers executing syscalls (six result cells in the VM frame).
- The client talks KSecDD RAW (Ioctl 0x390400 = CNG dispatch) bypassing
  bcrypt.dll; polls two WNF state names hundreds of times; checks
  ProcessInstrumentationCallback (sets NULL), ProcessDebugPort/ObjectHandle.
- Self-check: maps its own image via sections (NtOpenFile -> NtCreateSection ->
  NtMapViewOfSection chain) — section-object answers are load-bearing.

## BE Master channel
- sub_1800032F0 sends to BE Master over TCP (1020-byte chunks). The server is
  a pure courier: assembled [05] payloads forward to the master; verdicts come
  back as cmd-0 kick strings routed by a 4-byte player key.
- Verdicts observed: solver-computed [05] condemned fast ("Game restart
  required"); genuine in-game accepted. Attestation binds to live BEService
  co-signature and/or in-session facts — module path and host name alone
  (P1/P2) proved insufficient.

## Packet protocol (server side, verified live)
- [00] handshake, [02 seq,sub,guid32] GUID+ping, [04,seq,00,payload] challenge
  (264 B), [05,seq,nchunks,chunkidx,data<=1024] chunked response, [09] sync,
  [0B] script restrictions, [0C] legacy GUID.
- Kill timers: "Game restart required" (state 5, 10 s), "Client not responding"
  (config/2 or /5 retries at 2-5 s), "Ping too high".
- Sending [04,0x01] directly forces state 5 — never do that.
- The [05] body is ciphertext (325/330 bytes differ between two solves of the
  same challenge) — no plaintext fields to patch; must be computed genuinely.

## Academic teardown — Sabt, CheckMATE'25 (driver side)
Paper: M. Sabt (Univ Rennes/CNRS/IRISA), "Battling The Eye: Exploring the Anti-Cheat
Techniques of BattlEye", CheckMATE'25, doi 10.1145/3733817.3762701, hal-05482209.
Kernel PoC: zenodo.org/records/17037068. Disclosed to B. Suter (CEO); mitigation
shipped. Analysis targets BEDaisy.sys — the complementary half of our service-side map.

BEDaisy.sys internals (their NoVmp devirt of VMP3; matches our .be0/.be1/.be2 VMP3 findings on the client):
- Minifilter (FltGetRoutineAddress import); all imports via MmGetSystemRoutineAddress.
- ObRegisterCallbacks: PsProcessType + PsThreadType, ops = HANDLE_CREATE|DUPLICATE,
  shared PreOp. Usermode-handle strip mask 0xfffffbc5 → clears 0x33A =
  VM_OPERATION|VM_READ|VM_WRITE|CREATE_THREAD|SET_QUOTA|SET_INFORMATION.
  Kernel handles (KernelHandle bit) untouched.
- PsSetLoadImageNotifyRoutine: compares PE TimeDateStamp against a hardcoded
  denylist array of vulnerable-driver timestamps. Their claim "no driver (even
  signed) loads while BE active" — mechanism unexplained; notify callbacks cannot
  veto loads (PsSetLoadImageNotifyRoutine is notify-only). Likely detect+kill,
  treat as unverified.
- DriverEntry integrity checks: win32k/win32kfull/win32kbase, dxgkrnl, hal.dll,
  ACPI.sys, pci.sys — DMA-hardware-cheat oriented. No global check of all loaded
  kernel modules → a driver loaded BEFORE BE launch stays invisible.
- Anti-VM/anti-debug: cpuid 0x40000000 hypervisor vendor string (catches
  ill-configured hv: hvpp, Hyperplatform); repeated xgetbv timing (introspection
  overhead); kernel-debug device detection → VM shutdown (Hyper-V/VMware/VBox crash).

Attack chain (their PoC): DSE-Patcher (github.com/Cesusius/Dse-Patcher) loads a
vulnerable signed driver → patches g_CiOptions in ci.dll (0=off, 6=on); install
unsigned cheat driver; restore g_CiOptions=6 (no reboot; PatchGuard does not cover
ci.dll); launch BE — the pre-loaded driver is invisible to the image-load denylist.
- Blocked by: VBS (ci.dll in secure world read-only → BSOD
  ATTEMPTED_WRITE_TO_READONLY_MEMORY), Defender (flags DSE-Patcher), blocklist.
- External cheat: userland client + kernel driver, IOCTLs; READ_MEMORY struct
  {ULONG ProcessId; ULONGLONG addressSrc; PVOID addressDst; ULONG size};
  PsLookupProcessByProcessId + MmCopyVirtualMemory, PreviousMode=KernelMode →
  no handle → Ob callbacks blind. Persistent RW for minutes, undetected (2025
  build); multiplayer oxygen cheat worked, life only in single-player (server-side).
- Internal cheat (2025, now dead per disclosure): OB_CALLBACK_ENTRY.Enabled=false
  on PsProcessType CallbackList (struct via Vergilius); PspLoadImageNotifyRoutine
  entries nulled; or manual mapping (not implemented by them). Post-disclosure BE
  detects callback deactivation → injection now crashes the game.
- Thesis: kernel-AC security reduces to DSE — DSE bypass ⇒ AC bypass. Future-work
  pointer: "security-hole-as-a-service" (EchOh-No!, echo-ac Minecraft-AC writeup,
  ioctl.fail) — a trusted driver exposing an MmCopyVirtualMemory gadget, no
  malicious driver needed.

### Conflicts / deltas vs our map
- Paper claims BEDaisy.sys runs with Unturned 3.23.12 (Win11 23H2, 2025); our
  live siege (Win10 22H2, 2026) found NO BEDaisy for this title. Per-game BE
  config or build drift — unresolved; both are primary observations, keep both.
- Paper never touches BEService/BEMaster channel, KSecDD raw, WNF polling —
  service side is ours, driver side is theirs. Complementary, no overlap.
- Their threat model (Defender/VBS/blocklist disabled, Win11 23H2) ≈ our bench
  (HVCI off, blocklist dormant, Win10 22H2) — the DSE-Patcher chain replicates
  locally as-is; build-specific offsets differ (19045 vs 22631).

## Emulation state (kernemul, 2026-09)
- Client loads (LoadLibrary success via section-map entry capture + DllMain
  return forcing), VM init runs, kill switch neutralizable by point patches,
  post-DllMain second stage requires an honest verdict (undecrypted .be0/.be1
  otherwise). Syscall surfaces at live-kernel parity: TraceControl 15/27/30,
  Set/Query(40), KSecDD 0x390400, WNF (2 polled names), KnownDlls sections,
  registry seeds, syscall 0x21. Open front: the VM-level conditional
  (vm-jcc handler) selecting pass/kill deltas from frame cells.
