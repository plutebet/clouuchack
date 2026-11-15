---
name: ctf-jvm-protection-recovery
description: "Recover protected Java/JVM challenge artifacts in an authorized CTF: analyze dumps and native JVM/protection code, serialize live HotSpot classes, derive an independent archive decoder, reconstruct native methods, verify classfiles, and package an offline runnable JAR. Do not use for unlicensed software cracking or real credentials."
---

# CTF JVM Protection Recovery

## Language / Язык

Match the user's language. English references are under `references/`. For a Russian request, use the complete Russian mirror under `references/ru/`. Отвечай на языке пользователя; русская и английская версии должны сохранять одинаковые технические требования.

Russian reference routing / Маршрут русской версии:

- [область задачи и доказательства](references/ru/scope-and-evidence.md);
- [поиск и сериализация классов HotSpot](references/ru/hotspot-class-recovery.md);
- [анализ native-кода и эмуляция](references/ru/native-analysis-and-emulation.md);
- [самостоятельный декодер исходного JAR](references/ru/independent-decoder.md);
- [реконструкция native/@Compile-методов](references/ru/native-method-reconstruction.md);
- [проверка, аудит и релиз](references/ru/verification-and-release.md).

Use this skill only for a CTF, malware-analysis lab, owned binary, or another target for which the user explicitly has authorization. Record the authorization context in the work log. If the request is actually license circumvention, credential extraction, persistence, or access to a third-party system, stop and ask for a legitimate scoped objective.

## Required outcome

Clarify which result is requested; do not conflate these milestones:

1. **Dump recovery:** serialize only classes already present in a captured JVM.
2. **Independent decoder:** transform the original protected archive using only that archive plus derived tables/algorithms. The dump may validate results but must not supply output class bytes.
3. **Semantic native reconstruction:** replace protection-owned `native`/`@Compile` methods with reviewed Java bytecode or safe explicit stubs. Exact original Java source is generally unrecoverable.
4. **Offline runnable release:** start with a normal Java/Fabric entrypoint, preserve assets/resources, remove protected runtime dependencies, audit dangerous side effects, and package reproducibly.

The strongest completion claim requires all four. State partial results honestly and name the remaining blockers.

## Non-negotiable invariants

- Hash every original JAR, dump, JVM DLL, protection DLL, table, intermediate JAR, and final artifact. Never reuse addresses, offsets, masks, or keys from a merely similar sample.
- Treat an untrusted launcher/runtime as data. Do not execute or load native protection unless the user explicitly authorizes a contained dynamic test. Prefer dump reconstruction, static analysis, or bounded emulation.
- Respect frozen/live-process constraints exactly. Never resume, kill, inject into, or inspect a process that the user marked untouchable.
- Keep provenance per class and method: `archive-decoded`, `dump-oracle`, `runtime-generated`, `native-lifted`, `semantic-reconstruction`, `safe-stub`, or `unresolved`.
- A dump-derived class may be an oracle for names, layouts, method hashes, and comparison. It is not acceptable evidence that an independent decoder works if its bytes were copied into the final JAR.
- Do not expose tokens, accounts, hardware identifiers, screenshots, private paths, or unrelated memory contents. Redact them from reports and never package them.
- Do not report progress percentages without a measured denominator. Prefer counts: protected entries decoded, methods verified, native roots classified, failures remaining.

## Workflow router

Select one language set and stay with it: `references/*.md` for English or `references/ru/*.md` for Russian. Start with the corresponding `scope-and-evidence.md`; it defines evidence handling, milestones, and the decision tree. The links below point to the English set, while the Russian equivalents are listed in **Language / Язык** above.

- For dump carving, HotSpot object discovery, `InstanceKlass` validation, and exact classfile serialization, read [references/hotspot-class-recovery.md](references/hotspot-class-recovery.md).
- For Ghidra/IDA analysis, PE reconstruction, parser tracing, and Unicorn usage, read [references/native-analysis-and-emulation.md](references/native-analysis-and-emulation.md).
- For deriving a decoder that works from the original archive without dump bytes, read [references/independent-decoder.md](references/independent-decoder.md).
- For `native`/`@Compile` methods and semantic lifting, read [references/native-method-reconstruction.md](references/native-method-reconstruction.md).
- For validation, safety review, launch testing, and release packaging, read [references/verification-and-release.md](references/verification-and-release.md).

Read only the references needed for the active milestone, but read the selected reference completely before acting.

## Default execution shape

1. Inventory and hash inputs; establish exact JVM version, architecture, archive format, dump type, and protection modules.
2. Build a read-only memory map and reconstruct loaded PE images from dump ranges before relying on a decompiler.
3. Locate and validate a small set of target `InstanceKlass` objects. Derive layouts from the exact JVM build, never from remembered offsets.
4. Serialize pilot classes from HotSpot into standard `.class`; compare structurally against known ordinary classes and run bytecode verification.
5. Use pilots as an oracle to infer the protected archive transformation. Trace protected-buffer reads if static inference is insufficient.
6. Implement a standalone deterministic decoder and prove it consumes only the original archive plus documented derived tables.
7. Inventory every native/protection-owned method and classify it before reconstructing behavior.
8. Verify the entire JAR, audit side effects, launch in a clean isolated game directory, and package with README and hashes.

## Deliverables

Maintain these artifacts when applicable:

- `evidence.json`: input hashes, versions, base addresses, provenance, and explicit exclusions.
- `class_inventory.tsv`: class source, internal name, loader, address or archive entry, serialization status, class hash, failure.
- `layout_notes.md`: exact-build structure offsets with evidence and confidence, not copied folklore.
- `decode-report.json`: protected entries, decoded names/hashes, table inputs, failures, and proof that no dump/old-JAR bytes were inputs.
- `native_method_map.tsv`: owner/name/descriptor, callers, native root, reconstruction kind, confidence, tests.
- `bytecode-audit.tsv` and `safety-audit.tsv`.
- one command that produces the patched JAR from the original supported input hash.
- a portable or dependency-documented launch directory with `START.bat`, README, and `SHA256SUMS.txt`.

Use `scripts/jar_class_inventory.py` for a quick archive/class-magic inventory. It is a structural precheck, not a substitute for ASM verification or a real launch test.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
