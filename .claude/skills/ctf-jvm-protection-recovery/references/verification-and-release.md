# Verification, Safety Audit, and Release

## Structural JAR checks

Require:

- valid ZIP with no unexpected duplicate entries;
- no protected opaque entries remain unless documented resources;
- every `.class` begins with `CAFEBABE` and its internal name matches its path;
- legal classfile version and CP indices/tags;
- no duplicate fields/methods with the same name+descriptor;
- manifest/Fabric/mixin/service/resource entries remain present;
- invalid signatures are removed;
- expected class/resource counts match inventory.

Run `scripts/jar_class_inventory.py` as a fast precheck.

## Bytecode and linkage verification

Use ASM to parse all classes and analyze every concrete method with `BasicVerifier` or a stricter verifier. Report class/method/descriptor and exact analyzer failure. Zero failures is the production gate.

Then check:

- field/method owners and descriptors exist;
- inheritance and interface relationships resolve;
- bootstrap method and invokedynamic indices are valid;
- nest host/member consistency;
- mixin targets, entrypoints and access wideners resolve;
- frame computation used the real reconstructed hierarchy.

When practical, run JVM verification/class loading in isolation. Avoid loading untrusted application code merely to compute frames; use an offline hierarchy resolver.

Decompiler output is a usability check, not correctness proof. `Couldn't be decompiled` can be a decompiler limitation, malformed control flow, illegal frames, unresolved CP operands, or remaining protection. Inspect bytecode and verifier output before deciding.

## Compare against the dump oracle

For classes present in both sources compare logical fingerprints:

- internal name, superclass, interfaces and access;
- CP tags and normalized values;
- fields/ConstantValue/annotations;
- method descriptors, flags, normalized bytecode and exception tables;
- bootstrap/nest/inner/record metadata.

Explain expected differences for runtime rewriting, generated classes, debug metadata, frame recomputation and semantically reconstructed native methods. Never hide mismatches by copying the oracle class.

## Safety audit before launch

Perform static callsite/string/import analysis for:

- HTTP/WebSocket/custom socket endpoints other than expected game services;
- file deletion, recursive traversal, raw disk/device paths and shell/process execution;
- native library extraction/loading and JNI registration;
- screenshot, screen capture, clipboard, webcam/microphone, browser cookie, credential or token access;
- registry, scheduled task, service, startup, injection and process-manipulation APIs;
- update/download/execute flows;
- obfuscated reflection/method-handle dispatch and encoded endpoints.

Classify each finding by reachability, trigger, data sent/modified, and patch decision. Do not claim safety from string search alone.

## Clean launch test

Launch only the independently decoded and patched build using a standard Java entrypoint in a new game directory. Do not reuse account databases, tokens, worlds, server lists, screenshots, crash logs, or layout/config state from the analysis environment.

Capture:

- Java/Fabric version and exact classpath;
- stdout/stderr and game logs;
- first fatal exception plus causal chain;
- classloading/mixin/VerifyError/LinkageError failures;
- GUI lifecycle/open-close events;
- asset/native loading;
- network destinations during startup.

Test incrementally: bootstrap, title/menu, target GUI, commands, modules, world/server behavior, persistence, and clean shutdown. A code-0 process that never reached the UI is not a successful launch.

## Diagnose common failures globally

- **Null fields:** distinguish legitimate null, missing dump page, lost ConstantValue, missing `<clinit>`, side-channel injection, and failed native reconstruction.
- **GUI flashes then blur remains:** log screen factory, setScreen calls, render/input lifecycle, exceptions, and close reason; verify centering math and clean layout config.
- **Missing anonymous/hidden class:** determine whether it was archive-protected, runtime-generated, or a framework-generated dependency before copying anything.
- **Decompiler empty body:** inspect access flags and Code attribute; native/abstract methods legitimately have no Code, malformed concrete methods do not.
- **VerifyError:** repair CP operands, branches, exception ranges, hierarchy or frames at the serializer/decoder layer rather than patching individual crashes blindly.
- **Missing assets:** compare all nonclass JAR entries and external asset indexes; classes alone are not a runnable mod.

## Reproducible toolchain

Provide one build command that:

1. validates the original JAR SHA;
2. decodes to a temporary semantic JAR;
3. validates its hash or logical manifest;
4. applies versioned reconstruction/safety patches;
5. verifies all classfiles/methods;
6. atomically writes the final JAR and report.

The decoder package should contain tables, source code, minimal runtime/dependency documentation, audit maps, README and hashes. If a large entry-level payload is used, state clearly which transformations it represents and verify base, payload, every output entry, and supported version.

## Portable client package

Include or document:

- final client JAR;
- exact ordered dependency classpath;
- compatible Java runtime or explicit Java version requirement;
- native libraries for the target architecture;
- assets and asset index, or a documented external assets path;
- clean game/config directories;
- `START.bat`, console/debug launcher, README and `SHA256SUMS.txt`.

Audit the staged directory for absolute workspace paths and forbidden original-runtime references. Verify ZIP CRC, internal release manifest, extracted launcher paths, and a smoke launch from the packaged directory.

