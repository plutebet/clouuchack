# Per-vendor detection vectors and evasion

## EAC (EasyAntiCheat / EOS)

Architecture: `EasyAntiCheat_EOS.exe` (usermode module, launched with game) + `EasyAntiCheat_EOS.sys` (kernel driver, loaded at game start, unloaded after). Weakest kernel presence of the commercial vendors — most titles are winnable from usermode-external.

Vectors:
- Module enumeration: `EnumProcesses`/toolhelp snapshots, then kernel-side VAD walk to catch PEB-unlinked images.
- Thread audit: start addresses, TEB client ID spoofing, suspicious suspend counts.
- Handle scan: any process holding a handle to the game with VM_READ → checks ACL via `ObGetObjectTypeId`-adjacent logic; counter = kernel callback `ObRegisterCallbacks` stripping VM_READ/WRITE from foreign handles.
- Window/foreground scan, known cheat-window class names.
- Game memory integrity: hash of code sections at intervals.

Evasion notes:
- External with kernel read path (physical memory or mapped communication) beats internal for most EAC titles: nothing touches game address space from ring 3.
- If internal is required (SDK-adjacent features), hide from VAD too, not just PEB — manual map with image cleanup, wipe PE header, break the `MMVAD` link, see atlas `topics/manual-map-injector-teardown.md`.
- Heartbeats: EAC module must keep talking to EOS servers; do not suspend the module, feed it a clean environment instead.

## BattlEye (BEService / BEDaisy)

Architecture: `BEService.exe` (service, usermode) + `BEService64.exe` companion + `BEDaisy.sys` (kernel driver, loaded on game launch, stays until game exits). Usermode module injected into game (`BEClient.dll`).

Vectors:
- Deep module walk incl. driver-linked module list (`PsLoadedModuleList` integrity).
- Handle enumeration with request to close foreign handles; tracks handle-granting processes over time.
- Screenshot requests (periodic, composited — overlays show up).
- Heartbeat packets to BattlEye master servers, signed.
- Memory scan for known signatures, including outside the game process (global scan).
- Startup: early load means it sees boot-time artifacts other vendors miss.

Evasion notes:
- Kernel read path mandatory on actively-monitored titles; direct physical reads or hypervisor backing beat driver-hook approaches.
- Never patch `BEDaisy` in place — integrity-checked, and the service re-verifies from disk.
- Screenshot-safe overlay: render only when no capture request is in flight (hook the screenshot request path), or render on a layer it does not composite.
- HWID bans common — pair with spoofer work, atlas `topics/spoofer-methods.md`.

## Vanguard (vgk.sys)

Architecture: `vgk.sys` loads at **boot**, before the game exists; `vgtray.exe`/`vgc.exe` usermode parts; driver stays resident. Enforces Microsoft Vulnerable Driver Blocklist, requires TPM 2.0 + Secure Boot on Win11, blocks its own driver from being unloaded cleanly.

Vectors:
- Boot-time driver verification: any loaded driver's signature/pages checked; unsigned/mapped drivers compared against on-disk copies.
- Memory integrity of game + own components, kernel and usermode.
- VM/hypervisor detection (timing artifacts, CPUID leaf inspection).
- Fast HWID bans; Riot's legal posture means signature packs update aggressively.

Evasion notes:
- The only realistic kernel path is a legitimately signed, non-blocklisted driver used as read primitive, or a hypervisor that itself defeats their VM checks — high effort, re-derive every patch.
- Zero disk persistence, zero registry footprint, no service remnants: their scans run even when Valorant is closed.
- Do not run inside the same boot as development tooling (kernel debuggers, test-signed drivers) — that alone is a flag.

## VAC / VACnet

Architecture: usermode modules injected into Source/Source 2 games on Valve's schedule; VACnet = server-side ML classifiers consuming gameplay + uploaded binary fingerprints. Signature updates are staged; bans issue in waves days-to-weeks after detection.

Vectors:
- Signature scans of game process memory (known public cheat patterns).
- VACnet: aim/behavior features from server POV — covered in `behavioral-evasion`, this is where most modern VAC bans come from, not the client scan.
- Uploaded suspicious binaries get fingerprinted fleet-wide.

Evasion notes:
- External reads from a process with no injected module: client-side VAC has little to see.
- Ban waves mean a working bypass proves nothing for weeks — validate on throwaway accounts with known flagged-marker tests, never on main.
- Do not bother bypassing the client scan for behavior-only detections: if VACnet flags, the client was never the problem.

## Ricochet (CoD)

Architecture: kernel driver + usermode client, plus server-side **Shield** (aims to detect/enforce against aimbot-class behavior from the server), **Detect**, **Telemetry**.

Vectors:
- Client integrity + known cheat signatures.
- Server-side behavioral: hit-through-cover, aim-correlation with unseen positions (wallhack inference), reaction profiles.
- Telemetry sampled and shipped for offline scoring.

Evasion notes:
- Client-side bypass alone loses to Shield: keep every observable in human distribution — `behavioral-evasion` is not optional on Ricochet titles.
- HWID + account linkage bans; expect spoofer requirement.

## FACEIT / ESEA

Architecture: `faceitclient.exe` + service + `faceit.sys` kernel driver (ESEA similar, older lineage). Strictest commercial usermode scanning; client also validates game files.

Vectors:
- Full process/thread/handle audit at high frequency, global memory scan for cheat clients (external overlays included).
- Driver-level: blocks debuggers, checks driver load events, flags test-signed modules.
- Screenshot on demand, HWID fingerprinting, instant hardware bans.

Evasion notes:
- Kernel separation mandatory: read path in ring 0, absolutely nothing resident in game process.
- Client runs even outside matches — artifacts persist in memory while the client lives; time your operations.
- HWID bans are the default punishment; spoofing infrastructure is part of the stack, not an optional extra.

## Derivation workflow (any vendor, live-patched build)

1. Copy the shipped AC module + driver from disk; note version/hash.
2. Diff against previous build (BinDiff on the usermode module, or string/scan-pattern delta on the driver).
3. Re-locate: integrity-check ranges (game sections hashed), heartbeat interval and endpoint, handle-audit cadence, screenshot request path.
4. Re-check the Microsoft Vulnerable Driver Blocklist and vendor blocklist against every driver in your chain.
5. Re-validate on a clean account before any real session — ban-wave lag means silence is not evidence.
