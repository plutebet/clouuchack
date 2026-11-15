# Injector taxonomy — researched 2026-10 (UC / yougame / MPGH / GitHub census)

Field-researched map of injector families: mechanism, artifacts, verdict 2024–2026.
Primary census: UC threads 469426/698776/701167/665677, GH Injector source
(Injection.h/Start Routine.h), BlackBone MMap.h, Extreme Injector, KDMapper,
yougame.biz kernel/marketplace tags. Full citations in the originating session.

## Families (ladder of stealth)

1. **Loader-based (LoadLibrary family)** — `VirtualAllocEx` path → `WriteProcessMemory`
   → `CreateRemoteThread(LoadLibraryW)` / `NtCreateThreadEx` / `RtlCreateUserThread`
   / `LdrLoadDll` / `LdrpLoadDll` / `LdrpLoadDllInternal` stubs / `SetWindowsHookEx`
   (hook-DLL loaded by loader into every hooked proc). REAL loader runs: SEC_IMAGE
   mapping, imports, PEB lists + `LdrpHashTable`, TLS, `PsSetLoadImageNotifyRoutine`
   path event, thread start = LoadLibrary. DEAD vs any kernel AC; alive vs VAC +
   no-AC titles. Disk artifacts → hash/signature flagging of public injector binaries.

2. **Manual mapping (usermode)** — no loader: parse PE, alloc private commit
   (`NtAllocateVirtualMemory` direct-syscall), apply relocs, copy sections with
   per-`IMAGE_SCN_MEM_*` page protections, resolve imports (walk remote
   `InMemoryOrderModuleList` or GetProcAddress), register x64 exceptions
   (`RtlAddFunctionTable` / InvertedFunctionTable), run TLS + DllMain. Kills PEB/notify
   artifacts; leaves: private executable unbacked memory (VAD scans — Moneta/PE-sieve
   class), thread start in anonymous memory, unbacked callstacks, RWX if lazy.
   Hardening flags (GH Injector): `INJ_ERASE_HEADER` 0x1000 wipe,
   `INJ_FAKE_HEADER` (copy ntdll header over mapped), `INJ_MM_CLEAN_DATA_DIR`,
   `INJ_MM_SET_PAGE_PROTECTIONS`, `INJ_MM_SHIFT_MODULE_BASE`, `INJ_UNLINK_FROM_PEB`,
   `INJ_MM_RUN_UNDER_LDR_LOCK`; thread cloak: `INJ_CTF_FAKE_START_ADDRESS`
   (decoy = RtlExitUserThread), `INJ_CTF_HIDE_FROM_DEBUGGER`,
   `INJ_CTF_SKIP_THREAD_ATTACH`, `INJ_CTF_FAKE_TEB_CLIENT_ID`.
   BlackBone extras: `HideVAD` (PAGE_NOACCESS disguise), `MapInHighMem`, `NoThreads`,
   `ManualImports`, `CreateLdrRef` (double-edged — re-adds loader artifacts),
   `WipeHeader`, `ForceRemap`, PartialExcept.

3. **Execution triggers** (orthogonal to how code got in):
   - New thread: `NtCreateThreadEx` flags 0x1/0x2/0x4(HIDE_FROM_DEBUGGER) —
     `PsSetCreateThreadNotifyRoutine` fires regardless; kernel-side start-address
     resolution defeats TEB/ETH edits. Verdict: flagged.
   - **Thread hijack**: `SuspendThread` → `Get/SetThreadContext` → `ResumeThread`,
     shellcode calls entry then restores RIP. No thread event; but foreign
     Get/SetThreadContext pair is monitored (context callbacks, Sysmon 8), RIP
     transiently in anonymous memory; EAC flags CONTEXT writes from foreign handle.
     Kernel variant `PsGetContextThread/PsSetContextThread` hides the caller origin.
   - **APC**: `QueueUserAPC`/`NtQueueApcThread(Ex2)` on alertable thread; kernel
     `KeInitializeApc`+`KeInsertQueueApc` from driver. No thread artifact. Protected
     processes block foreign APC delivery. **Early Cascade Injection** (Outflank
     2024): APC on main thread of suspended fresh process — runs before AC's
     usermode hooks load.
   - **Threadless/callback**: PEB `KernelCallbackTable` overwrite (GH LM_KernelCallback),
     **FakeVEH** (payload = VEH handler; fake LIST_ENTRY + LdrProtectMrdata tricks),
     fiber injection, atom bombing (`GlobalAddAtom`→`GlobalGetAtomName` overwriting
     RtlAddVectoredExceptionHandler), SetWinEventHook entry (TTKKO), TLS callbacks.
     Each leaves PEB-adjacent artifact (callback table entry, VEH chain node).
   - **Handle hijack**: steal existing handle from another process's table
     (GH `INJ_HIJACK_HANDLE`; SandFoxy repo) — defeats ObRegisterCallbacks
     handle-stripping + handle enumeration (no open event at all).

4. **Section mapping / stomping** — `NtCreateSection(SEC_IMAGE)`+
   `NtMapViewOfSection`: image VAD with file backing but NO PEB entry (inverse
   trade of LoadLibrary). **Module stomping / DLL hollowing**: map legit signed
   carrier, wait for its image-notify, overwrite .text — region VAD-backed + listed
   + signed host: defeats private-memory AND header scans; loses to module hash
   checks (only if host is hashed). Phantom variant: never-loaded system DLL's space.
   Current frontier per UC 2025 threads (Rivals/CS2/Ricochet threads).

5. **Kernel injection (ring-0)** — loaded driver: `KeStackAttachProcess` →
   `ZwAllocateVirtualMemory` → `KeInsertQueueApc`/`ZwCreateThreadEx`/memcpy into
   existing game module. Beats all usermode enumeration; detection moves to the
   driver itself (signature, DSE path, mapping traces, `MmUnloadedDrivers`,
   `PiDDBCacheTable`, `g_KernelHashBucketList`, WdFilter traces — all need wiping,
   see kdmapper). Strongest UM-invisible form: memcpy into legit module memory.
   Rogue619Z choreography: driver suspends AC threads → disables image-load
   callbacks → usermode maps → restore → resume.
   Delivery = **KDMapper** (iqvw64e.sys; cert revoked → blocked on default Win11;
   forks rotate BYOVD; HVCI/blocklist enforced when HVCI/SmartAppControl on).

6. **No injection at all** — DMA (pcileech-fpga/ScreamerM2 + kmbox; counterplay:
   PCIe enumeration vs known firmware, IOMMU, BAR fingerprinting, Vanguard hardware
   bans), hidden hypervisor (EPT; loses to RDTSC timing + artifact probes),
   second-PC + AI-vision + hardware input.

## Detection vector → artifact → counter map

| Vector | Catches | Counter |
|---|---|---|
| PEB Ldr lists / LdrpHashTable | loader-injected modules | manual map, or unlink (Xenos) |
| Image VAD w/o loader entry | section-mapped, stomped-not-listed | stomp a loader-registered module |
| Private executable MEM_COMMIT | classic manual map | stomping, HideVAD, per-section prots |
| Thread start outside modules | CreateRemoteThread/NtCreateThreadEx | hijack/APC/callbacks, fake start addr |
| Foreign Get/SetThreadContext | thread hijack | kernel PsSetContextThread, APC |
| PsSetLoadImageNotifyRoutine path | LoadLibrary family | manual/section map |
| ETW Kernel-Process/Threading | remote thread + image load | direct syscalls, kernel injection |
| Unbacked callstacks (2023+ EAC, Ricochet) | mapped code calling APIs | execute from stomped module; ret-addr spoof |
| Module .text hashing | stomping, IAT/EAT patches | pick unhashed host module |
| Handle enumeration / ObCallbacks stripping | OpenProcess-based injectors | handle hijack, kernel PsSetContextThread |
| On-disk signatures / mapper traces | public binaries, kdmapper leftovers | private builds, trace wiping |

## Reference tool matrix

- **GuidedHacking Injector** (Broihon) — modes IM_LoadLibraryExW/IM_LdrLoadDll/
  IM_LdrpLoadDll/IM_LdrpLoadDllInternal/IM_ManualMap × launches LM_NtCreateThreadEx/
  LM_HijackThread/LM_SetWindowsHookEx/LM_QueueUserAPC/LM_KernelCallback/LM_FakeVEH;
  Hook Scanner repairs AC hooks over remote stubs. The educational reference.
- **BlackBone** (DarthTon, dormant 2023) — library: MT_Native/MT_Manual, HideVAD,
  cross-thread remote exec, remote hooking, driver-assisted mapping.
- **Xenos** (DarthTon) — Blackbone GUI: LoadLibrary/LdrLoadDll(hijack opt)/manual/
  kernel-hide, WOW64 injection, IL injection, module unlink.
- **Extreme Injector** (master131, C#) — Standard/LdrLoadDll/LdrpLoadDll/Hijack/
  Manual Map + scramble presets, Erase PE, Hide Module. MPGH legacy standard.
- **KDMapper** (TheCruZ) — kernel driver mapper; PiDDB/unloaded-driver trace wipe,
  atom hook, PDB-driven offset adaptation; STATUS_IMAGE_CERT_REVOKED on modern Win11.
- Kernel teaching skeletons: Rhydon1337 (ring0 KeInsertQueueApc → LoadLibraryW),
  cybryk (Win11 23H2+, XOR payload), TTKKO (SetWinEventHook entry), utoni/KMemDriver,
  SandFoxy (handle hijack), Rogue619Z (AC-thread freeze choreography, archived 2026).

## Verdicts 2024–2026

Dead: LoadLibrary+CreateRemoteThread, SetWindowsHookEx-DLL vs kernel ACs,
default-form manual map (private RWX + new thread).
Alive: hardened manual map (stomped/host-backed, threadless, syscall-based),
thread hijack w/ stack discipline, early-cascade APC, kernel injection from
clean-driver path, hypervisor, DMA. Next flank: server-side ML (FACEIT Human
Input Detection, VACnet) — injection-agnostic.

## Detail bank — second pass (thread ledger, counterplay, census dates)

### UC thread ledger (per-technique citations)
- 698776 — DLL injection methods 2025 (hernos: SetWindowsHookEx still works vs some AC)
- 701167, 271757 — manual map detection state 2025; Suspend→Get/SetThreadContext chain itself a signature
- 469426 — canonical manual-map detection sweep (threads, PE headers, LdrpHashTable/InvertedFunctionTable)
- 592629 — hiding usermode thread of manually mapped DLL: LSTAR/TEB start-address rewrite limits vs kernel-side resolution
- 337247, 623992 — manualmap-detection bypass Q&A
- 3820684 — PsSetCreateThreadNotifyRoutine fires before CreateRemoteThread returns
- 236505 — NtCreateThreadEx detected despite ThreadHideFromDebugger
- 579254 — EAC system-thread detection vectors; 508137 — kernel ACs walk KTHREAD.ThreadListEntry (catches unlinked)
- 262312, 442417, 307721 — thread hijack vs EAC ("easy af to block from ring0"), kernel PsGet/PsSetContextThread
- 696381 — shellcode via SetWindowsHookEx/QueueUserAPC vs vanilla EAC; protected processes block foreign APC
- 122192 — QueueUserAPC basics
- 711299 — Sastasha Injector v1.7 (legacy-compat method set)
- 764300 — Marvel Rivals: map into legit signed carrier as SEC_IMAGE (stomping)
- 725356 — Ricochet: load signed DLL, wait image-notify, stomp .text
- 593057 — DLL injection vs manual map vs APC comparison
- 665677 — manual map without creating a thread
- 739335 — CS2 kernel-bypass triggerbot technique roster (fiber, atom bombing listed)
- 739720 — injecting into EAC process via kernel driver (hijack "heavily monitored")
- 462290 — RWX hook pages flagged by EAC
- 438804 — mapper-artifact traces on disk scanned by EAC/BE
- 148364 — ObRegisterCallbacks countermeasures; handle-presence itself an artifact
- 420149 (Vanguard 32-bit), 95281 (Xigncode) — image-notify path event catches LoadLibrary even in old ACs

### GH Injector source files to study
Injection.h (INJECTION_MODE × LAUNCH_METHOD grid, all INJ_* flags), Start Routine.h
(SR_REMOTE_DATA_VEH fake LIST_ENTRY, cloak flags), QueueUserAPC.cpp, KernelCallback.cpp,
FakeVEH.cpp + VEH Shell.cpp, Handle Hijacking.cpp, Hook Scanner.cpp
(ValidateInjectionFunctions/RestoreInjectionFunctions).

### BlackBone subsystems
MT_Native via NtLoader.cpp (LdrLoadDll internals in target) vs MT_Manual; remote code
exec on new/existing threads (cdecl/stdcall/thiscall/fastcall incl. FPU args); remote
hooking (int3/hw bp); driver-assisted usermode mapping; module enumeration three ways
(loader list / section objects / PE headers) — same strategies ACs use.

### Extreme Injector notes
Scramble presets None/Basic/Standard/Extreme (new entrypoint x86-only, strip section
characteristics, shift section memory — the last significantly aids evasion on x86 DLLs);
Erase PE breaks .NET DLLs (metadata reads fail); Secure Mode; known MPGH recipe:
Manual Map + Erase PE + Hide Module.

### Detection-theory sources (callstacks / VAD / kernel AC)
patchi.fyi (Peregrine: VAD scanning; callstack validation), Elastic "Call Stacks: No
More Free Passes", IBM X-Force reflective call-stack detections, offensivecraft return
address spoofing x64, s4dbrd (kernel AC overview; BEDaisy static analysis), hypercall.net
BattlEye, ACM 2024/2025 BattlEye paper (dl.acm.org/doi/10.1145/3733817.3762701),
MeekoLab kernel-AC research (16MB proximity heuristic → exact range validation),
"RWX Dilemma" (bassam.hossam69, Medium), RX-INT arXiv 2508.03879 (2025 kernel detector
covering stomping), Threat Hunter Playbook CreateRemoteThread hunt, Sysmon EID 7/8/10/22,
Elastic EID 8 prebuilt rules, AvantGuard Threadless Ops, anticheat.ac/blog/mappeddrivers.

### Hypervisor / DMA / no-injection counterplay
secret.club "How anti-cheats detect system emulation" (RDTSC timing, artifact probes);
arXiv 2502.12322 (VIC, 2025 — VM detection); qemu-anti-detection issue #80; EAC weaker
than BE vs hypervisors historically. DMA: hwidchange.com DMA-detection overview (PCIe
enumeration vs known firmware, IOMMU enforcement, BAR fingerprinting); GH DMA guide
(threads/20627); UC DMA newbie guide (638326); Vanguard 2025 wave bricked/banned $6k
DMA boxes (shattered.io Vanguard timeline); Riot "Vanguard On-Demand".

### Blocklist / HVCI / BYOVD (kernel delivery constraint)
learn.microsoft.com recommended driver block rules (updated 2026-05): blocklist ON by
default since Win11 2022 update; enforced under HVCI / Smart App Control / S mode;
quarterly + monthly updates. KDMapper README: iqvw64e.sys cert revoked →
0xC0000603 STATUS_IMAGE_CERT_REVOKED; workaround VulnerableDriverBlocklistEnable=0;
FACEIT causes map failure 0xC0000022/0xC000009A (actively blocks the known path).
KB5020779 (Oct 2022 blocklist). BYOVD rotation beats quarterly blocklists (Expel,
Halcyon analyses); MS push = full WDAC allowlists. Vanguard/FACEIT refuse to run with
HVCI off or blocklisted drivers present → pushes those titles to hypervisor/hardware.

### Tool census (verified commit dates, Oct 2026)
TheCruZ/kdmapper — active, last commit 2026-08-23, README supports Win11 25H2 (26200.x).
GuidedHacking-Injector — semi-maintained (code PR 2024-03, README 2025-04/05, GUI 4.8 2023).
DarthTon/Blackbone — dormant (last commit 2023-07-17, final push Win11 21H2/22H2).
mmozeiko manual mapper — no maintained repo (gist/blog code only).
"RemapInjector"/"Sneaky Injector" — not located as maintained public repos; remap
survives inside private/commercial injectors.
KDMapper forks in RU circulation: skadro-official/kdmapper, zH4x/kdmapper-1,
Zysen/node-kdmapper (rotate the loader driver as iqvw64e.sys dies).
Also: SandFoxy/Handle-Hijacking-Injector, Rogue619Z/Manual-Map-Injector (archived 2026-03),
TTKKO/Kernel-Manual-Map-Injector (SetWinEventHook entry), cybryk/kernelmodeinjector
(Win11 23H2+, XOR payload, BE/EAC target), Rhydon1337/windows-kernel-dll-injector
(324★, ring0 KeInsertQueueApc→LoadLibraryW, Win10 1909), utoni/KMemDriver,
0mWindyBug/KernelInjector (CR3 switch + RX shellcode PoC),
MicrosoftARMAssembler/Undetected-Easy — NOT an injector: pre-map scanner checking
your .sys against reversed EAC/BE detections.

### yougame.biz / MPGH thread ledger (preview-tier, login-walled full text)
yougame 381528 — APC Injection + Kernel Manual Mapping (Rust/Apex, Win 25H2); marketplace
pricing $80 driver+injector, $140 with mapper. 385848 — kernel injector for Rust: weak
link = the driver itself (signature + mapping detectability), not the injection step.
55252 — early HideVAD lineage: pages mapped invisible to NtQueryVirtualMemory-class API.
49850 — DOLBOEB INJECTOR (2018 usermode, BE/EAC). Tags /tags/injector /tags/kernel
/tags/battleye = commercial SOTA (DMA firmware Artix-7 75T/100T "Heino 1.2" for
Vanguard titles; mobile root-kernel loaders: Uncode kernel 4.6, PUBG Mobile,
/data/local/tmp). mrac (MyGames) grouped with Faceit/EAC as hard kernel target.
MPGH: 1324169 (Extreme Injector 3.7.3), 1266182 (UD recipe), 1113672 + 764401 + 911153
(consensus: LdrLoadDll stubs are still the loader, not manual mapping; VAC flags public
injector binaries more than the technique), 109161 + 500627 (doctrine: compile your own,
don't ship binaries), 1290287 (Xenos 2.3.0), 1392153 (Project-X 2.4 mod). MPGH is
usermode-centric; kernel threads rare. RU community runs a full tier deeper.

## Key sources
UC: unknowncheats.me/forum/anti-cheat-bypass/698776 (methods 2025), /c-and-c-/469426
(manual map detection), /anti-cheat-bypass/701167 (manual map 2025), 665677
(no-thread manual map), 592629 (hiding mapped threads), 593057 (section vs manual vs APC).
GH: guidedhacking.com/threads/manual-mapping-dll-injection-tutorial-how-to-manual-map.10009.
Detection theory: patchi.fyi (VAD scan, callstack validation), s4dbrd.github.io
(kernel AC, BEDaisy), hypercall.net/posts/Battleye, MeekoLab kernel AC research.
Hollowing: ired.team module stomping, TrustedSec DLL hollowing.
Kernel delivery: learn.microsoft.com driver block rules (2026-05), TheCruZ/kdmapper.
