# Проверка, safety-аудит и релиз

## Структура JAR

Проверь:

- ZIP/CRC и неожиданные duplicate entries;
- отсутствие необъяснённых opaque protected entries;
- `CAFEBABE` каждого `.class` и совпадение internal name с path;
- classfile version и CP indices/tags;
- duplicate fields/methods с одинаковым name+descriptor;
- manifest, Fabric, mixins, services, resources;
- удаление invalid signatures;
- ожидаемое количество classes/resources.

Используй `scripts/jar_class_inventory.py` как быстрый precheck, но не как замену ASM.

## Bytecode и linkage

ASM должен разобрать все классы и проверить каждый concrete method через `BasicVerifier` или строже. Production gate — zero failures. Затем проверь member owners/descriptors, hierarchy/interfaces, bootstrap+invokedynamic, nest consistency, mixin targets/entrypoints/access wideners и корректность frames относительно реальной восстановленной иерархии.

Decompiler — только usability check. `Couldn't be decompiled` может означать ограничение декомпилятора, malformed CFG/frames, unresolved CP или оставшуюся защиту. Смотри bytecode и verifier output.

## Сравнение с dump-oracle

Сравни normalized name/super/interfaces/access, CP tags/values, fields/ConstantValue/annotations, methods/bytecode/exceptions и bootstrap/nest/inner/record metadata. Объясни различия runtime rewrite, generated classes, debug metadata, frame recomputation и native reconstruction. Нельзя исправлять mismatch копированием oracle class в independent output.

## Safety audit

Ищи и классифицируй:

- HTTP/WebSocket/socket endpoints кроме ожидаемых игровых сервисов;
- destructive file/raw disk/shell/process APIs;
- native extraction/loading и JNI;
- screen/clipboard/camera/microphone;
- cookies, credentials и tokens;
- persistence, injection и process manipulation;
- update/download-execute;
- obfuscated reflection/method handles и encoded endpoints.

Для каждого укажи reachability, trigger, данные и patch decision. String search один не доказывает безопасность.

## Чистый запуск

Запускай только independently decoded+patched JAR через обычную Java в новой game directory. Не копируй accounts, tokens, worlds, server list, screenshots, crash logs или старый layout config.

Логируй exact Java/Fabric/classpath, первый fatal cause, mixin/classloading/verifier/linkage, GUI lifecycle, assets/natives и network destinations. Тестируй bootstrap, title/menu, target GUI, commands, modules, world/server behavior, persistence и shutdown. Exit code 0 до появления UI не является успешным launch.

## Типовые причины

- Null: valid null, missing dump page, lost `ConstantValue`, missing `<clinit>`, side channel или native failure.
- GUI мигнул и остался blur: screen close/exception/null factory/event bridge/layout state; добавь lifecycle logging.
- Missing anonymous class: выясни archive-protected, runtime-generated или framework-generated происхождение.
- Empty decompile: проверь flags и наличие `Code`.
- VerifyError: исправляй serializer/decoder — CP operands, branches, handlers, hierarchy, frames.
- Missing assets: сравни все nonclass entries и external asset index.

## Toolchain и package

Одна команда должна: проверить original SHA, декодировать temp semantic JAR, проверить его, применить versioned native/safety patches, проверить классы и атомарно создать final JAR/report.

Decoder ZIP должен содержать tables, sources, runtime/dependency instructions, audit maps, README и hashes. Если используется большой entry-level payload, объясни его происхождение и проверяй base/payload/output entry hashes.

Portable client должен содержать final JAR, ordered classpath, Java runtime или requirement, natives, assets/index, clean game dir, `START.bat`, debug launcher, README и `SHA256SUMS.txt`.

Перед релизом найди absolute workspace paths и references на original launcher/runtime, проверь ZIP CRC, внутренний release manifest, extracted launcher paths и smoke launch из packaged folder.
