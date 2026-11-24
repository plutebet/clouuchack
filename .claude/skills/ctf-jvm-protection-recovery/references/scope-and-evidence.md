# Scope, Evidence, and Milestones

## Authorization record

Write one short statement before technical work: target, CTF/lab name if available, files/processes in scope, allowed mutations, and explicit prohibitions. CTF authorization does not imply permission to attack challenge infrastructure, reuse recovered credentials, or publish unrelated secrets.

If a process must remain frozen, record its PID and the operations forbidden against it. Enumerating the process list is not permission to open handles to that process.

## Evidence ledger

For every input record:

- absolute path and stable logical label;
- byte length and SHA-256;
- creation/source description supplied by the user;
- whether it is trusted, untrusted, or generated;
- whether execution/loading is forbidden;
- format and architecture detected from bytes, not filename.

Keep originals read-only. Write generated files under a dedicated workspace. Never overwrite the only dump or archive. Preserve a chain such as:

```text
original.jar -> semantic-decoded.jar -> native-restored.jar
             -> safety-patched.jar -> verified-release.jar
```

Each stage needs an input hash, output hash, tool/version, transformation summary, and validation result.

## Initial inventory

### Archive

Measure:

- ZIP/JAR validity, duplicate entries, entry order and compression methods;
- manifest, Fabric metadata, mixin configs, access wideners and service descriptors;
- count of normal `CAFEBABE` classes versus opaque entries;
- naming patterns, record sizes, entropy, repeated headers/trailers, and correlations with class counts;
- assets and native libraries embedded as resources;
- signatures that will become invalid after rewriting.

Do not assume an opaque 32-hex entry is encrypted class data merely from its name. Correlate count, size distribution, runtime class names, and parser behavior.

### Dump

Record dump format and flags, memory-range coverage, module list, thread contexts, exception stream, and whether private executable pages are present. Process working set and dump file size are not expected to match: uncommitted, shared, excluded, zero, guard, mapped, and compressed pages affect the result.

For anti-dump behavior that makes a dump grow indefinitely, prefer a region-aware collector that snapshots the virtual memory map and reads only committed readable ranges with explicit caps. Preserve allocation base, protection, type, and partial-read status. Do not resume a protected frozen process merely to obtain a conventional minidump.

### Native images

Identify exact JVM build, PE timestamp, image size, section layout, imports/exports, and relocation state. A memory-loaded image reconstructed from dump pages differs from its disk PE: headers or relocations may be modified and not all sections are resident.

## Milestone gates

Do not advance based on visual plausibility alone.

### Gate A: reliable memory access

- address lookup maps virtual addresses to the correct dump ranges;
- reads across range boundaries are handled;
- module base and PE sections are consistent;
- pointer canonicality and page permissions are checked.

### Gate B: reliable class discovery

- several ordinary JDK/game classes validate under the inferred layout;
- target classes validate through name, superclass, constant pool, and method arrays;
- false-positive rate is measured on random candidates.

### Gate C: pilot serialization

- classfile parser accepts the output;
- internal name/path agree;
- constant pool references are in range;
- bytecode disassembles and verifies;
- annotations and structural attributes needed by the framework survive.

### Gate D: independent archive decoder

- fresh output is produced from the original archive only;
- output class count and entry mapping are complete;
- no dump/old JAR class bytes appear in the input list;
- hashes or structural fingerprints match dump-derived oracle classes where both exist.

### Gate E: runnable release

- all concrete methods pass bytecode/data-flow checks;
- class loading/mixins complete in a clean directory;
- native roots are reconstructed, explicitly safe-stubbed, or documented unresolved;
- dangerous network/filesystem/process functionality has been reviewed;
- assets, metadata, libraries and natives are present or documented dependencies.

## Communicating status

Use exact counters rather than optimism:

```text
archive protected entries: 25,683
decoded successfully:       25,683
serialized classfiles:      25,684
concrete methods verified:  195,327
verification failures:      0
native roots classified:    119/119
```

Estimate time only after measuring throughput on a representative batch. Separate CPU-bound emulation time, I/O, whole-JAR frame computation, and manual native reconstruction.

