---
name: frida-instrumentation
description: Dynamic instrumentation with Frida/objection — Java/ObjC/native hooking, SSL pinning bypass, runtime key/trace capture, gadget injection, anti-Frida hardening and evasion. Use for any "hook X", "bypass SSL pinning", "trace function", "dump at runtime" task on Android/iOS/desktop.
---

# Frida instrumentation

**Spawn > attach:** `frida -U -f com.target.app -l script.js` (spawn with script loaded before app code runs — mandatory for pinning bypass and early hooks). Attach (`-n`/`-p`) only when spawn breaks (anti-tamper at zygote). List targets: `frida-ps -Ua`. Quick wins before writing scripts: `objection explore` (patchable: `android sslpinning disable`, `ios hooking watch class`).

**Script skeletons — the three bridges:**

```javascript
// Java (Android)
Java.perform(() => {
  const C = Java.use('com.target.Class');
  C.method.overload('java.lang.String').implementation = function (arg) {
    console.log('[+] ' + arg);
    return this.method(arg);          // or return false / modified value
  };
});

// Native
Interceptor.attach(Module.getExportByName(null, 'SSL_read'), {
  onEnter(a) { this.buf = a[1]; this.n = a[2].toInt32(); },
  onLeave(r) { if (this.n > 0) console.log(hexdump(this.buf, { length: this.n })); }
});

// ObjC (iOS)
Interceptor.attach(ObjC.classes.NSURLSession['- dataTaskWithRequest:completionHandler:'].implementation, {
  onEnter(a) { console.log(new ObjC.Object(a[2]).toString()); }
});
```

**Pinning bypass is layered, not global** — the app pins at whatever layer it uses: OkHttp `CertificatePinner`, `TrustManagerImpl`, Conscrypt, native BoringSSL `ssl_verify_peer_cert`, or iOS NSURLSession delegates. Kill bottom-up: patch `checkServerTrusted` to empty body, `CertificatePinner.check` to return, and if traffic still fails, hook `SSL_read`/`SSL_write` to dump plaintext and stop fighting verification entirely. Full per-layer recipes: `references/hooking-patterns.md`.

**Runtime key capture** is the highest-value use in loader/crack work — `CryptHashData`/`BCryptHashData`/custom digest input dumps are one-liners, see `crypto-identification` ref for the snippet.

**Anti-Frida is now standard on production games/apps**: port scan 27042, `gum-js-loop` thread names, maps/pipe string scans, inline-hook prologue checks. Countermeasure stack and stealth gadget configs: `references/anti-frida-hardening.md`.

Refs in `references/`:
- hooking-patterns.md — per-layer pinning bypass (OkHttp/TrustManager/Conscrypt/BoringSSL/iOS), native pattern-scanning hooks, Stalker tracing, root-detection bypass
- anti-frida-hardening.md — detection vectors, renamed frida-server builds, gadget stealth config, Zygisk/LSPosed alternatives

Route: `crypto-identification` (what to hook once keys appear), `cheat-crack-pipeline` (loader context), `protocol-teardown` (pinned-TLS traffic capture).

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
