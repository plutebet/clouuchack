# Building an Independent Protected-JAR Decoder

## Independence criterion

The final decoder may consume:

- the exact supported original protected JAR;
- documented tables/keys/algorithms derived during analysis;
- ordinary deterministic libraries.

It must not consume dump pages, serialized dump classes, a previous decoded JAR, the original launcher, the protected JVM, the protection DLL, live server responses, or secret tokens. List actual input files in every decode report.

## Map protected entries

Build a table from opaque archive entry to expected internal class name using independent evidence:

- parser trace and decoded name logic;
- class relationships and constant-pool names;
- dump oracle fingerprints;
- framework metadata and resource references.

Detect output collisions before writing. Preserve nonprotected resources, Fabric metadata, mixin configs, access wideners, services, manifests, and asset paths. Remove invalid JAR signatures after modifications.

## Separate decoding layers

Implement and test layers independently:

1. container/entry selection;
2. per-entry key/state initialization;
3. structural record or field permutation;
4. scalar/byte-range decoding;
5. symbol/name/descriptor recovery;
6. constant-pool reconstruction;
7. fields and methods;
8. bytecode opcode/operand normalization;
9. attributes and side-channel metadata;
10. standard classfile serialization.

Do not hide uncertainty behind a permissive parser. Each layer should produce bounded diagnostics with entry, offset, expected structure, actual value, and rule/table version.

## Recover transformations

Common protection designs include:

- per-entry permutations of logical fields;
- XOR/add/rotate/ARX scalar transforms;
- process-wide plus per-class keys;
- encrypted `Symbol*` or name identifiers;
- descriptor masks;
- opcode substitution tables;
- CP-cache or side-pool indirection;
- metadata removed from the main stream and injected by a native side channel.

Derive rules from several diverse classes. A rule that only reproduces one pilot is an oracle adapter, not a decoder.

When a large permutation table is unavoidable, store entry name, table version, source evidence, and hash. Validate full coverage against the supported input hash. Refuse unknown inputs by default; an explicit diagnostic/partial mode may emit successes but must never label the result production-ready.

## Constant-pool and bytecode coupling

Decode CP and bytecode as a coupled system. Field/method/invoke operands can reveal CP-cache reverse mappings; CP tags and descriptors constrain legal opcodes and operand widths. Iterate until both are self-consistent rather than replacing invalid references with zero.

For opcode substitution, recover the entire mapping required by the input, not only opcodes seen in one class. Validate instruction lengths, branch targets, switch padding, exception handler boundaries, local indices, and CP tag compatibility.

## Side-channel metadata

If serialization loses `ConstantValue`, initializers, annotations, class names, or method bodies, determine whether the information is:

- present elsewhere in the protected entry;
- derived from a global table in the native module;
- injected during parsing;
- computed later by native code;
- genuinely absent and only observable semantically at runtime.

Trace writers to the corresponding HotSpot field and correlate table indices with archive entries. Export the minimal stable table/algorithm into decoder data. Do not patch nulls globally: null may be valid, missing, or a failed read.

## Deterministic CLI contract

A production decoder should resemble:

```text
decode INPUT.jar OUTPUT.jar --tables tables/ --report decode-report.json
```

It should:

- validate the supported input SHA-256;
- refuse incomplete table coverage or decode failures by default;
- use a temporary output and atomically replace only on success;
- report decoded/skipped/failed entries and restored features;
- record input/output/table hashes;
- list `dump_or_old_jar_inputs: []` for an independent run;
- preserve deterministic entry ordering and normalized metadata where practical.

## Proving independence

Run the packaged toolchain in a clean directory containing only the original JAR and toolchain. Temporarily make dump/oracle/intermediate paths unavailable. Then verify:

- all expected protected entries decode;
- no output entry was copied from a fallback JAR;
- class names and content fingerprints match oracle samples;
- the report names only permitted inputs;
- rerunning produces identical logical JAR entries, even if container-level DEFLATE bytes differ.

