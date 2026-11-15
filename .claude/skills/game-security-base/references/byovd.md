# BYOVD field notes — driver exploitation pipeline

Source: blast.hk thread 258088 (author sashka123555, June article repost, part 2 of 2),
cross-checked against loldrivers.io taxonomy and 2026 kernel-reality corrections.
Scope: driver PE/IOCTL plumbing, SDDL gate, case studies, what the article missed.

## Pipeline (as taught)
1. Source of vulnerable drivers: LOLDrivers (public — every AV/AC vendor knows it,
   short lifetime) or manual bug hunting in old vendor drivers (GPU overclocking
   utilities, AV/anti-exploit drivers, 360 Total Security cited as candidate).
   Uniqueness buys lifetime.
2. First gate — SDDL/ACL on the DEVICE_OBJECT: no descriptor → anyone can open
   `\\.\Name` from usermode. Check imports for
   `ZwSetSecurityObject`, `RtlCreateSecurityDescriptor`,
   `RtlSetDaclSecurityDescriptor`; `SeConvertStringSecurityDescriptorToSecurityDescriptor`
   resolved via `MmGetSystemRoutineAddress` (with manual fallback) = deliberately hardened.
3. Enumerate the IOCTL surface from the IRP_MJ_DEVICE_CONTROL (0x0E) handler
   dispatch table in IDA/Ghidra.
4. Usermode: `CreateFileW` on the symbolic link + `DeviceIoControl`.

## Case studies (from the article)
- **ardrv.sys** (public, LOLDrivers) — no SDDL. Handler copies the user buffer
  (`AssociatedIrp.MasterIrp`) unvalidated, dispatches on a Size field to
  `__inbyte` / `__inword` / `__indword` → arbitrary I/O port read.
  IOCTL decimal 37879865 = 0x02420039 = CTL_CODE(0x242, 0xE, METHOD_IN_DIRECT,
  FILE_ANY_ACCESS). Struct `{USHORT Port; ULONG Value; BYTE Size;}`; port 0x60
  (KBC) demo. Port IO is a building block, not a full RW primitive.
- **NdkPing.sys** (Microsoft RDMA ping tool) — negative example, correctly hardened:
  SDDL `D:(A;;GA;;;SY)(A;;GA;;;BA)` → SYSTEM + Administrators only, non-admin
  CreateFile gets error 5. SDDL blocks: owner (G), group (Q), DACL (D), SACL (S).
  Lesson per author: bug hunting in Microsoft drivers is low ROI; vendor drivers
  are the real targets.
- **hmpalert.sys** (Sophos HitmanPro.Alert anti-exploit) — the money case. Rich
  IOCTL surface: 0x222200 open process with specific rights, 0x222220 get process
  handle by PID (EDR-kill value: terminate AV processes), 0x2222CC direct RW,
  0x222108/0x22210C event management, 0x2220F0 mode switching, 0x222004 read PoC.
  Input struct: `{ULONG_PTR ProcessId; ULONG_PTR BaseAddress; ULONG_PTR Buffer;
  ULONG Size;}`. Bug: driver validates only the OUTPUT buffer (Buffer and
  Buffer+Size < MmHighestUserAddress); BaseAddress is never validated. Read
  routine `sub_18002F820` does KeStackAttachProcess → copy → detach. Result:
  arbitrary read of any process memory including protected processes (lsass).
  Demo read: SumatraPDF, PID 12252, addr 0xFFFFF800042A3000 → `48 89 5C 24 08 ...`.
- Bug class to hunt: IOCTLs where only the output pointer is range-checked and
  the input pointer/addresses are trusted (METHOD_NEITHER especially), plus any
  IOCTL that grants a process handle by PID.

## Corrections / 2026 reality (our notes, not the article's)
- HVCI/Memory Integrity is default-on on modern Win11 builds; the Microsoft
  vulnerable-driver blocklist is enforced by CI regardless of vendor callbacks.
  Known LOL drivers (incl. kdmapper's iqvw64e.sys) refuse to load on HVCI systems.
  Fresh/undocumented vulnerable drivers, or HVCI-off targets, are the realistic pool.
- Version pinning: vendors patch in place, same filename/signature. You must ship
  the exact old signed vulnerable build — the driver name alone is not a primitive.
- Load-time detection surface the article underplays: Sysmon EID 6, vendor
  PsSetLoadImageNotifyRoutine callbacks, ELAM, blocklist cross-check at load.
  Hygiene: minimize loaded window, unload after use; a mapper additionally needs
  PiDDB Cache Table and g_KernelHashBucketHead/HashCache cleanup.
- Access gate: SDDL `(BA)` drivers require an elevated loader — UAC prompt or
  bypass; game processes usually run non-elevated, so the loader runs outside.
- hmpalert IOCTLs above are old-build only; Sophos fixed this class in current
  builds. Still valid archaeology for the bug class.

## Pointers
- kdmapper internals (PiDDB purge, hash cache, header erasure, routine
  resolution) — promised by the article as part 2, NOT covered there. Separate
  chart when we build ours.
- IOActive paper on KMDF driver bug hunting (referenced by article) — worth a read.
