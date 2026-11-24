# Anti-Frida hardening — detection vectors and evasion

## What production targets actually check

| Vector | Method | Note |
|---|---|---|
| Default port | TCP connect to 27042/27043 | cheapest check, most common |
| Listening sockets | `/proc/net/tcp` on Android, `GetTcpTable` on Windows | catches renamed binary on default port |
| Maps/image scan | `/proc/self/maps` for `frida-agent`, `gum`, `gadget` strings | |
| Thread names | `gum-js-loop`, `gmain`, `gdbus`, `pool-frida-*` | via `/proc/self/task/*/comm` |
| Named pipes (Windows) | `\\.\pipe\frida-*` enumeration | |
| Ptrace guard | `TracerPid` in `/proc/self/status`, self-`ptrace` | |
| Inline-hook checks | prologue bytes of libc exports vs on-disk copy | catches naive Interceptor.attach on hot functions |
| D-Bus/probe on port | Frida's own protocol handshake on 27042 | |
| Gadget export scan | `frida_gadget` exported symbol in loaded images | |

## Evasion stack, in order of effort

1. **Rename + recompile frida-server** — build from source (or use strongR-frida/hluda prebuilts) with renamed symbols, strings, and thread names. Kills string/name scans; port still yours to choose (`-l 0.0.0.0:1337`).
2. **Gadget, listen config, abstract socket:**
```json
{
  "interaction": {
    "type": "listen",
    "address": "127.0.0.1",
    "port": 0,
    "on_port_change": "script"
  }
}
```
Packaged as `libgadget.config.so` next to the injected `libgadget.so`. Port 0 = random — defeats port scans by default.
3. **Gadget preload instead of server:** repack target APK with gadget in `lib/arm64-v8a/` + `System.loadLibrary` in smali — no external frida-server process exists to find. Thread names still present: patch them post-load or compile gum with renamed threads.
4. **Zygisk/LSPosed route** (Android): run instrumentation as a Zygisk module — in-process, loaded by the system loader, no injection artifacts. Cleanest against maps/pipe scans; defeats prologue checks only if you avoid hooking checked functions (hook callers instead).
5. **Windows targets:** manual-map the agent, rename pipes, and keep `Interceptor` off the functions the target hashes — hook at a trampoline you install inside a syscall wrapper instead.

## Hardened targets — how to work when hooks are checked

- **Hook the caller, not the checked function.** Target hashes `SSL_read` prologue → hook the app's wrapper that calls it (find via Stalker trace of one TLS send).
- **Hardware-breakpoint hooks** (via exception-based debuggers, not inline patch) leave prologues intact.
- **Delay activation:** load scripts after the integrity check runs (the check usually runs once at startup) — spawn gated: start with `--pause`, let init finish, resume + attach.
- **Memory-only scripts:** `frida --runtime=v8` with everything in RAM; gadget configs that don't touch disk (`interaction: script` loads JS from the config itself).

## Reality check

Deterministic anti-Frida (rooted detection + port + strings) is fully defeatable with the stack above. Behavioral anti-tamper (target notices thread-count anomalies, timing of hook installation) is statistical — same discipline as `behavioral-evasion`: look like the population, which on a stock device means "no unusual threads at all" — prefer Zygisk/gadget-preload over live server injection on targets that fingerprint processes.
