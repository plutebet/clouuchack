# Reconstructing Native and @Compile Methods

## Set the correct expectation

Native machine code generally cannot be converted back into the exact original Java bytecode or source. Compilation erases source-level structure, names, generics, annotations, exception shapes, and many optimization boundaries. The achievable target is a semantically equivalent Java/bytecode implementation supported by native-code evidence and tests.

Distinguish:

- **exact recovered bytecode:** original bytecode was found in metadata/side tables;
- **mechanical lift:** native control/data flow was translated with high fidelity;
- **semantic reconstruction:** behavior was reimplemented from evidence;
- **safe stub:** nonessential or dangerous behavior intentionally disabled;
- **unresolved:** insufficient evidence; do not invent behavior.

## Build a complete inventory

Enumerate every method with `ACC_NATIVE`, protection annotations such as `@Compile`, JNI registration entries, native binding tables, and Java callsites. For each record:

```text
owner, name, descriptor, access, annotation
registration/native address and module RVA
direct/indirect callers
reachable module/UI/service
side effects and external endpoints
reconstruction class and confidence
tests and remaining gaps
```

Do not call a native method unused merely because it has no obvious direct invoke. Check reflection, method handles, lambdas, registries, event buses, mixins, serialization codecs, JNI lookup, and native-to-Java callbacks.

## Recover semantics

For each reachable root:

1. recover the JNI/JVM calling boundary and exact Java descriptor;
2. identify object fields, array layouts, constants, strings, math operations, branches and callees;
3. distinguish protection plumbing from business logic;
4. construct a control-flow graph and typed value flow;
5. model exceptional/null behavior and state mutation;
6. translate to Java or generate bytecode;
7. test against captured inputs/outputs or invariant-based oracles.

For rotation/movement/math logic, preserve floating-point precision, degree/radian conversions, angle wrapping, interpolation/clamping order, random distributions, tick timing, coordinate frames, and side effects. Algebraically similar code can behave differently because of float rounding or operation order.

For UI methods, preserve coordinate spaces, scaling, anchoring, framebuffer/window dimensions, animation state, input capture, and lifecycle registration. A GUI rendering briefly and leaving a blur often means the screen closed after an exception, a factory returned null, an event bridge was not registered, or layout state was corrupted—not merely a renderer problem.

## Re-emit methods safely

When replacing a native declaration:

- clear `ACC_NATIVE` and `ACC_ABSTRACT` as appropriate;
- emit a valid `Code` attribute;
- use correct owner/name/descriptor references;
- preserve method annotations/signature/exceptions;
- recompute max stack, max locals and frames;
- preserve synchronization and static/instance semantics;
- keep bridge/synthetic flags when required by dispatch.

Apply changes by exact owner/name/descriptor, never name alone. Fail if zero or multiple targets match unexpectedly.

## Safety-oriented reconstruction

In an offline CTF release, explicitly neutralize protection-only behavior that is not required for gameplay:

- license/server handshake and forced shutdown;
- telemetry or hardware fingerprint submission;
- screenshot/clipboard/process enumeration unrelated to the challenge;
- self-update/download-and-execute paths;
- destructive filesystem or command execution;
- token/account upload.

Do not silently stub core module logic. Record each safe stub, callsites, expected effect, and why it is nonessential.

## Validation

Use three levels:

1. static: descriptor, field/call linkage, bytecode verification;
2. component: invoke reconstructed codecs, catalogs, storage, math and UI factories with controlled fixtures;
3. integration: exercise menu/module/command paths in a clean launch and inspect detailed lifecycle logs.

Keep a method map that ties every reconstruction to its module or interface component. Completion means every protection-owned root is classified, not merely that the JAR starts.

