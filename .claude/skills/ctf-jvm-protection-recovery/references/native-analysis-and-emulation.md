# Native Analysis, PE Reconstruction, and Emulation

## Reconstruct loaded images first

From dump module metadata and memory ranges, rebuild an analysis image for the exact loaded `jvm.dll` and protection DLL:

- preserve virtual addresses/RVAs;
- map PE headers and sections by `VirtualAddress`/`VirtualSize`;
- fill absent pages explicitly and record gaps;
- account for relocation to the captured image base;
- separate original image pages from private executable pages and unpacked/JIT code;
- retain a virtual-address-to-dump-offset map.

Do not feed a multi-gigabyte process dump to whole-file autoanalysis and expect useful functions. Import reconstructed module images or bounded executable regions into Ghidra/IDA. Disable or limit pathological analyzers when malformed thunks/functions cause repeated failures; preserve the database before changing analysis options.

## Find the protection boundary

Trace from stable JVM surfaces toward custom code:

- class definition and parser entrypoints;
- `ClassFileParser` constructors and stream readers;
- `ClassFileStream::get_u1/get_u2/get_u4/get_u8/get_u1_buffer`-like helpers;
- `SystemDictionary`/class-loader hooks;
- JNI registration and `RegisterNatives`;
- custom `native`/`@Compile` registration tables;
- archive entry readers and name resolvers;
- opcode dispatch tables and CP side-channel accessors.

Use strings, xrefs, call graphs, import use, global references, repeated table strides, and known HotSpot source shapes. Rename functions only when evidence supports the semantic name; keep address-based aliases otherwise.

## Virtualization reality

A process dump does not automatically turn VMProtect/Themida-style virtualized functions back into ordinary code. The dump may reveal unpacked stubs, VM handlers, bytecode streams, decrypted constants, or dynamically generated code, but handler semantics still need analysis.

Prioritize the narrow interface required for class decoding rather than devirtualizing the entire protection:

- input buffer reads;
- key/epoch initialization;
- per-entry state;
- name/descriptor decoding;
- opcode/operand mapping;
- side-table lookups;
- output consumed by HotSpot.

## Unicorn as an oracle

Use Unicorn when a small native routine is easier to execute than fully translate. Do not emulate the entire JVM for every class.

### Minimal harness

1. Map only required module sections and private code pages at captured virtual addresses.
2. Map stack, input buffer, output/state objects, and a bounded heap.
3. Initialize registers and arguments from the calling convention and a captured callsite.
4. Stub imports, TLS, locks, allocation, timing, JNI, and OS calls with deterministic behavior.
5. Hook unmapped reads/writes, instruction count, returns, and protected-buffer reads.
6. Set hard limits for instructions, memory, recursion, and trace size.
7. Snapshot a known-good initial state so repeated pilots reset cheaply.

### Trace the parser, not everything

For one representative protected class, activate read tracing only after entering the parser boundary. Record:

```text
ordinal, RIP/RVA, input offset, size, raw bytes, relevant state/key
```

Cluster read sites into logical `u1/u2/u4/bytes` primitives. Compare the read sequence with the structure of the dump-serialized class. Derive a pure transformation from input offset/state to logical value, then reimplement that transformation outside Unicorn.

If one class requires minutes, mass emulation is the wrong final architecture. Use emulation to learn semantics and validate edge cases; the production decoder should be native-speed Python/Rust/C++ logic over the archive.

## Differential experiments

Use multiple classes that differ in one structural dimension: CP size, fields, annotations, switch bytecode, invokedynamic, or nested metadata. Compare:

- protected bytes and entry metadata;
- parser read trace;
- live logical class model;
- standalone decoder output.

This distinguishes stream encryption from field permutation, per-entry keys, opcode substitution, and side-channel metadata.

## Tool evidence

For every recovered constant/table/function record:

- module SHA and captured base;
- RVA, not just absolute address;
- bytes/signature around the location;
- xrefs and callers;
- interpretation and alternative hypotheses;
- validation samples.

Never transfer an absolute address or XOR key to another version without rediscovery.

