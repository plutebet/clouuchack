# Hooking patterns

## SSL pinning bypass — Android, per layer

Work bottom-up; retest traffic after each patch. Spawn-mode only (`-f`), attach is too late for most pins.

**OkHttp3 CertificatePinner** (most common):
```javascript
Java.perform(() => {
  const P = Java.use('okhttp3.CertificatePinner');
  P.check.overload('java.lang.String', 'java.util.List').implementation = function () {};
});
```

**TrustManagerImpl (system-level verification):**
```javascript
Java.perform(() => {
  const T = Java.use('com.android.org.conscrypt.TrustManagerImpl');
  T.checkTrustedRecursive.implementation = function (a, b, c, d, e, f) {
    return Java.use('java.util.ArrayList').$new();   // empty chain = trusted
  };
});
```
(Class name varies by API level — enumerate: `Java.enumerateLoadedClasses` filtered on `TrustManager`.)

**Conscrypt/Platform verify chain:** hook `com.android.org.conscrypt.Platform.checkServerTrusted` similarly.

**Native BoringSSL** (games, Flutter, anything shipping its own libssl):
```javascript
// ssl_verify_peer_cert / SSL_CTX_set_verify → force success
Interceptor.attach(Module.getExportByName('libssl.so', 'ssl_verify_peer_cert'),
  { onLeave(r) { r.replace(1); } });
```
If the lib is renamed/packed, pattern-scan for the verify function or skip verification entirely — dump plaintext with `SSL_read`/`SSL_write` hooks (SKILL.md skeleton) and let the connection fail where it fails.

**Repack route (no runtime frida):** apktool decode → add `network_security_config.xml` with `trust-anchors: system+user` → repackage + sign. Kills Java-layer pins, not native ones.

## SSL pinning bypass — iOS

Hook the trust chain, then delegates:
```javascript
// Force trust evaluation to succeed
const SecTrust = Module.getExportByName(null, 'SecTrustEvaluateWithError');
Interceptor.attach(SecTrust, { onLeave(r) { r.replace(1); } });

// NSURLSession delegate pinning (didReceiveChallenge handlers)
Interceptor.attach(ObjC.classes.NSURLSession['- dataTaskWithRequest:completionHandler:'].implementation, {
  onEnter(a) { console.log('[req] ' + new ObjC.Object(a[2]).toString()); }
});
```
Objection covers the standard set: `ios sslpinning disable` (patches SecTrust + common delegate patterns). For TLS-Toolkit-style pins, hook `ssl_verify_cert` in the shipping libssl the same way as Android native.

## Native hooks beyond exports

- Exported: `Module.getExportByName(lib, sym)`.
- Offset from IDA/Ghidra: `Module.getBaseAddress('libtarget.so').add(0x1234)` — offsets are per-build, re-derive.
- No symbols: `Memory.scan(base, size, '48 8B ?? 8B ?? ?? 8D', { onMatch })` with the pattern from the disasm; keep patterns 5+ wildcards loose, verify in `onMatch` by reading surrounding instructions.
- Replacement, not just observation: `Interceptor.replace(target, new NativeCallback(fn, ret, args))` for full control; `NativeFunction` to call arbitrary routines with crafted args.

## Tracing with Stalker

```javascript
Stalker.follow(Process.getCurrentThreadId(), {
  events: { call: true },
  onReceive(events) { /* parse into call log */ }
});
```
Use for: finding the actual check function in an obfuscated validate routine (call counts spike at the compare), building execution traces of license checks. Expensive — scope to a thread and a time window, then `Stalker.unfollow`.

## Root/jailbreak detection bypass — Android

Hook the common probes; one script, all of them:
- `java.io.File.exists` → return false for path containing `/su`, `magisk`, `supersu`, `xposed`, `frida`.
- `Runtime.exec`/`ProcessBuilder.start` → intercept `su`, `which`.
- `System.getProperty`/`__system_property_get` → `ro.build.tags` → return `release-keys` (not `test-keys`), `ro.debuggable` → `0`, `ro.secure` → `1`.
- `PackageManager.getInstalledApplications` → filter magisk/substrate package IDs.
- Magisk hide layer underneath (denylist/Zygisk) — client-side hooks plus mount-namespace isolation, not either alone.

Hardware attestation (Play Integrity hardware-backed) is NOT bypassable client-side — spoofing software attestation only; server decides. Say so when a target requires it; the route is the server request, not the device.

## iOS jailbreak detection

- Hook `stat`/`lstat`/`fopen` for `/Applications/Cydia.app`, `/var/lib/apt`, `/bin/sh`.
- `dyld` image list scan → filter suspicious paths (`Substrate`, `TweakInject`).
- `csops` flags check → forge `CS_GET_STATUS` result.
- ObjC `canOpenURL` on `cydia://`.
