# HotSpot Class Discovery and Classfile Serialization

This procedure is for reconstructing classes already loaded in an authorized CTF JVM dump. HotSpot internal layouts change across versions and custom builds. Every offset below is a semantic field name, not a universal numeric offset.

## 1. Establish the exact VM layout

Prefer evidence in this order:

1. matching OpenJDK/HotSpot source for the exact build;
2. symbols/PDB/DWARF or exported VMStructs data;
3. decompiled accessors and constructors in the recovered JVM image;
4. cross-validation against many known classes in the dump.

Use `gHotSpotVMStructs`, `gHotSpotVMTypes`, `gHotSpotVMIntConstants`, and `gHotSpotVMLongConstants` when present and trustworthy. A protection fork may remove or alter them. If so, recover offsets from code that reads/writes `Klass`, `InstanceKlass`, `ConstantPool`, `Method`, `ConstMethod`, `Symbol`, `Array<T>`, and `ClassLoaderData`.

Record for every inferred field: offset, size, pointer/value interpretation, evidence address/function, JVM build, and confidence. Validate on ordinary known classes before target classes.

## 2. Find candidate classes

### Preferred: ClassLoaderDataGraph

Locate the global class-loader-data graph head, then walk each `ClassLoaderData` and its linked `Klass` list. Depending on the exact build, relevant semantic links include the graph `_head`, CLD `_next`, CLD `_klasses`, and `Klass::_next_link` or an equivalent list node.

For each pointer:

- require alignment and a mapped readable region;
- verify the vtable points into the expected JVM image or valid private executable range;
- bound every `Array<T>` length before iterating;
- detect cycles and cap traversal;
- retain the defining loader/CLD as provenance.

### Fallback: vtable scan

Find the `InstanceKlass` vtable from constructors, RTTI, known objects, or repeated first pointers. Scan plausible metaspace/class-space ranges for that vtable, then apply strict structural validation. A vtable match alone is insufficient because stale/free objects and unrelated data can collide.

### Candidate validation

A credible `InstanceKlass` should satisfy most of these independently:

- decoded `Klass::_name` is a valid HotSpot `Symbol` with bounded length and plausible modified UTF-8 internal name;
- `_super` is null only for `java/lang/Object`, otherwise points to a valid `Klass`;
- access flags are legal for a class/interface/annotation/enum/record/module;
- `_constants` points to a valid `ConstantPool` whose holder resolves back to the class;
- method array and interface arrays have sane lengths and valid elements;
- every `Method` has a valid `ConstMethod`, name index, signature index and holder;
- class state/init state and layout helper are plausible;
- target package/name correlations agree with archive evidence.

If the protection encrypts or XORs the name pointer, infer the transform using known class names and accessor code. Confirm one process-wide rule across many classes before applying it globally.

## 3. Read HotSpot primitives safely

Implement one dump reader with:

- `read(addr, size)` across adjacent ranges;
- checked `u1/u2/u4/u8` and pointer reads using target endianness;
- pointer-to-module/range lookup;
- bounded HotSpot `Array<T>` decoding;
- `Symbol` decoding using exact header/length/hash layout and modified UTF-8;
- cycle detection, recursion caps, and per-object diagnostics.

Never silently replace a failed read with zero. A missing page must become an explicit partial/failure state; otherwise nulls propagate into apparently valid but semantically broken classes.

## 4. Extract a logical class model

Do not write classfile bytes directly while chasing pointers. First build a versioned intermediate representation:

```text
ClassModel
  version, access, this_name, super_name, interfaces
  ConstantPoolModel[] with original slot numbers
  FieldModel[]
  MethodModel[]
  ClassAttributeModel[]
  provenance and unresolved items
```

Preserve original constant-pool slot numbers whenever possible. Bytecode, annotations, bootstrap methods, exception tables and metadata already refer to those indices. Compacting/reordering the pool prematurely creates widespread corruption.

## 5. Reconstruct the constant pool

Read `ConstantPool::_length`, tag storage, slots, holder, cache/resolved-reference companions, operands/bootstrap data, and any fork-specific side tables. Handle standard classfile tags:

- Utf8, Integer, Float, Long, Double;
- Class, String;
- Fieldref, Methodref, InterfaceMethodref;
- NameAndType;
- MethodHandle, MethodType;
- Dynamic, InvokeDynamic;
- Module and Package.

Long and Double occupy two classfile slots. Keep the following phantom slot empty.

### Resolved entries

HotSpot mutates portions of the runtime constant pool and stores resolved objects in `ConstantPoolCache`, resolved references, or fork-specific companions. A slot that no longer looks like a raw classfile index may be:

- a pointer to `Klass`, `Symbol`, `java.lang.String`, `Method`, or field metadata;
- a cache index whose original CP index is recoverable through cache entry metadata;
- an encoded pair of indices;
- a protection-owned side-channel value.

Recover the logical classfile form, not the in-memory pointer. Use tags, CP-cache reverse mappings, bytecode operand use, holder relationships, and parser/decompiler evidence. Never serialize a raw process pointer into a `.class` entry.

### Modified UTF-8

Classfile Utf8 uses modified UTF-8, not ordinary UTF-8. Preserve embedded NUL encoding, surrogate handling, and byte lengths. A standard JSON/string round trip can corrupt names and string constants.

## 6. Reconstruct fields

Determine the exact build's field representation. Older builds often expose a packed `Array<u2>` with fixed groups; newer builds may use `FieldInfoStream`/compressed records and separate generic-signature or annotation tables. Do not assume six `u2` values per field.

For each field recover:

- access flags;
- name and descriptor CP indices;
- optional generic signature;
- `ConstantValue` index for static final compile-time constants;
- runtime visible/invisible annotations and type annotations;
- synthetic/deprecated metadata when represented;
- field order.

A field whose Java value is null at dump time is not proof its initializer was null. `ConstantValue` comes from class metadata; non-constant assignments live in `<clinit>` or constructors. If the fork injects values through a side channel, locate the writer/parser logic and model it explicitly.

## 7. Reconstruct methods

### Method and ConstMethod

For each `Method*`, recover semantic fields from the exact build:

- holder through `ConstMethod -> ConstantPool -> pool_holder`;
- name index and signature index;
- access flags and method kind;
- `ConstMethod` flags;
- bytecode size and bytecode start;
- `max_stack`, `max_locals`, parameter size;
- exception table;
- checked exceptions;
- generic signature and method parameters;
- annotation/default/type-annotation blobs;
- line number and local-variable tables where retained.

`Method` also contains interpreter/compiled entrypoints, counters, adapters, profiling data and possibly native function pointers. Those are runtime artifacts and do not belong in a classfile. A non-null compiled entry is not Java bytecode.

`ConstMethod` trailing sections are controlled by flags and are layout-sensitive. Derive their order and alignment from the exact source/decompiled accessors. Validate all sizes against the allocation boundary or neighboring metadata.

### Bytecode normalization

HotSpot may rewrite bytecodes after loading (`fast_*`, resolved invokes/fields, breakpoint opcodes, native-endian operands) and the protection may add its own opcode substitution. Convert every internal opcode back to a legal classfile opcode and restore classfile-endian operands.

Special care:

- branch targets are relative to the opcode address;
- `tableswitch`/`lookupswitch` padding depends on bytecode offset;
- `wide` changes operand width;
- `invokeinterface`, `invokedynamic`, `multianewarray`, `newarray`, and switch formats have extra operands;
- rewritten field/method/invoke operands may be CP-cache indices, not CP indices;
- exception handlers must land on instruction boundaries.

Build a complete instruction-boundary map before rewriting branches.

## 8. Reconstruct class attributes

Recover or rebuild all attributes required by the target framework/version:

- `SourceFile`, `Signature`, `Synthetic`, `Deprecated`;
- runtime visible/invisible annotations and type annotations;
- `InnerClasses` and `EnclosingMethod`;
- `BootstrapMethods` for dynamic/invokedynamic constants;
- `NestHost` and `NestMembers`;
- `Record` components and their nested attributes;
- `PermittedSubclasses`;
- module attributes for module descriptors;
- custom framework metadata that was originally a classfile attribute.

Missing nest/inner/mixin metadata can allow parsing yet fail later during reflection, lambda bootstrap, access checks, or framework initialization.

## 9. Emit a standard classfile

Write in JVM classfile order:

1. `CAFEBABE`;
2. minor and major version;
3. constant pool count and entries;
4. class access flags, this class, super class;
5. interfaces;
6. fields and attributes;
7. methods and attributes;
8. class attributes.

For each non-abstract, non-native method emit `Code` with `max_stack`, `max_locals`, code bytes, exception table, and nested attributes. Do not invent an empty body for a missing method: classify it unresolved or reconstruct it separately.

If runtime metadata no longer preserves `StackMapTable`, first serialize a complete hierarchy, then recompute frames with ASM `COMPUTE_FRAMES` using a class writer whose common-superclass lookup reads from the reconstructed JAR rather than loading untrusted application classes. Never use `java/lang/Object` as a universal superclass unless verified safe for every merge.

## 10. Handle runtime-generated classes

Lambda forms, hidden classes, proxies, mixin-generated classes and anonymous runtime adapters may exist in the dump but not the original archive. Mark them `runtime-generated`. They may help understand behavior, but blindly packaging them can collide with framework generation or embed unstable names.

## 11. Pilot validation

Choose pilots that exercise different features:

- simple class with constants and fields;
- inheritance/interface class;
- annotations and generic signatures;
- invokedynamic/lambda/bootstrap methods;
- inner/nest/record metadata;
- complex switches and exception handlers;
- a target protected module class.

For each pilot require valid parsing, correct internal name, legal CP references, instruction-boundary validation, ASM data-flow verification, and comparison against the live oracle. Scale only after pilots pass.

