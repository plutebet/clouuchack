# Анализ native-кода, PE и эмуляция

## Сначала восстанови загруженные образы

По module metadata и memory ranges собери анализируемый образ exact `jvm.dll`/protection DLL: сохрани RVA/VA, разложи sections по `VirtualAddress`/`VirtualSize`, явно отметь отсутствующие страницы, учти relocation к captured base, отдели image pages от private executable/unpacked/JIT code и сохрани VA→dump-offset map.

Не отправляй многогигабайтный process dump в полный autoanalysis. Импортируй реконструированный PE или ограниченные executable regions. Если malformed thunks заставляют Ghidra/IDA зацикливаться, ограничь проблемный analyzer, сохрани project/database и продолжи targeted analysis.

## Найти границу защиты

Иди от стабильных HotSpot поверхностей к кастомному коду:

- class definition и parser entrypoints;
- `ClassFileParser` и stream `get_u1/u2/u4/u8/bytes`;
- `SystemDictionary` и class-loader hooks;
- JNI registration и `RegisterNatives`;
- native/@Compile binding tables;
- archive readers и name resolvers;
- opcode dispatch tables;
- constant-pool side-channel accessors.

Используй strings, xrefs, call graph, imports, globals, table stride и узнаваемые shapes исходников HotSpot. Семантическое имя функции давай только при доказательстве, иначе сохраняй RVA alias.

## Виртуализация

Дамп не превращает VMProtect/Themida-подобный код автоматически в обычную функцию. Он может дать handlers, VM bytecode, decrypted constants, unpacked stubs или generated code, но semantics всё равно нужно вывести.

Не девиртуализируй всё без необходимости. Для class decoder приоритетны input reads, key/epoch init, per-entry state, name/descriptor decode, opcode/operand mapping, side tables и результат, который получает HotSpot.

## Unicorn как oracle

Эмулируй небольшую необходимую функцию, а не всю JVM и не каждый класс.

1. Map только нужные sections/private pages по captured VA.
2. Создай stack, input buffer, state/output objects и bounded heap.
3. Инициализируй calling convention/registers по callsite.
4. Детерминированно stub-ь imports, TLS, locks, allocation, timing, JNI и OS calls.
5. Hook unmapped access, instruction count, returns и reads защищённого buffer.
6. Ограничь instructions, memory, recursion и trace.
7. Snapshot начального состояния для быстрых повторов.

Трассировку reads включай только после входа в parser boundary. Пиши:

```text
ordinal, RIP/RVA, input offset, size, raw bytes, state/key
```

Сгруппируй sites в логические `u1/u2/u4/bytes`, сопоставь sequence со структурой oracle-class и перенеси формулу в чистый decoder.

Если один класс эмулируется минуты, массовый Unicorn — неверная финальная архитектура. Эмуляция нужна, чтобы узнать semantics и проверить edge cases; production decoder должен выполнять чистый алгоритм.

## Differential analysis

Сравни классы, различающиеся одним признаком: размер CP, fields, annotations, switch, invokedynamic, nested metadata. Сопоставь protected bytes, read trace, live `ClassModel` и standalone output. Так отделяются stream cipher, field permutation, per-entry keys, opcode substitution и side-channel metadata.

Для каждой таблицы/константы запиши module SHA, base/RVA, surrounding bytes, xrefs/callers, интерпретацию, альтернативы и validation samples. Адреса и ключи другой версии никогда не переноси без нового поиска.

