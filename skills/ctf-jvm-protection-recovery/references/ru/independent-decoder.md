# Самостоятельный декодер защищённого JAR

## Критерий независимости

Разрешённые inputs:

- exact original protected JAR;
- документированные derived tables, keys и algorithms;
- обычные deterministic libraries.

Запрещены как inputs: dump pages, dump-serialized classes, предыдущий decoded JAR, launcher, protected JVM/DLL, server responses и tokens. В каждом отчёте перечисляй фактически прочитанные файлы.

## Сопоставление entries

Построй opaque-entry → internal-class-name mapping через name algorithm/parser trace, relations/CP names, oracle fingerprints и framework metadata. Проверяй output collisions до записи. Сохраняй nonprotected resources, Fabric metadata, mixin configs, access wideners, services, manifest и assets. Удаляй ставшие недействительными signatures.

## Слои decoder

Раздели и тестируй отдельно:

1. выбор container entries;
2. per-entry key/state initialization;
3. structural record/field permutation;
4. scalar/byte-range decoding;
5. symbol/name/descriptor recovery;
6. ConstantPool reconstruction;
7. fields и methods;
8. opcode/operand normalization;
9. attributes и side-channel metadata;
10. standard classfile serialization.

Каждая ошибка должна содержать entry, offset, ожидаемую структуру, actual value и table/rule version. Нельзя заменять invalid reference нулём и продолжать как production-success.

## Вывод transformations

Ищи:

- field permutations;
- XOR/add/rotate/ARX scalar transforms;
- process-wide и per-class keys;
- encrypted `Symbol*`/name IDs;
- descriptor masks;
- opcode substitution tables;
- CP-cache/side-pool indirection;
- metadata, убранную из main stream и injected native side channel.

Правило подтверждай на разных классах. Совпадение одного пилота — oracle adapter, а не decoder.

Большая permutation table допустима, если versioned, имеет evidence/hash и полное покрытие exact input. Неизвестный SHA должен отклоняться. Partial/diagnostic mode может сохранить удачные классы, но не называть JAR готовым.

## CP и bytecode вместе

Решай их совместно: operands помогают восстановить CP-cache reverse mapping, а tags/descriptors ограничивают legal opcodes. Проверь instruction lengths, branch targets, switch padding, exception boundaries, local indices и CP tag compatibility.

## Side-channel metadata

При потере `ConstantValue`, initializers, annotations, names или bodies выясни, находятся ли данные:

- в другой части protected entry;
- в native global table;
- в parser injection;
- в позднем native вычислении;
- только семантически в runtime.

Трассируй writer к конкретному HotSpot field и коррелируй index с archive entry. Экспортируй минимальную стабильную таблицу/формулу. Глобальный «fix all nulls» недопустим: null может быть валидным, отсутствующим или следствием failed read.

## Production CLI

CLI должен выглядеть примерно так:

```text
decode INPUT.jar OUTPUT.jar --tables tables/ --report decode-report.json
```

Он обязан валидировать input SHA, по умолчанию запрещать incomplete coverage/failures, писать во временный output, создавать JSON report с input/output/table hashes и атомарно заменять результат. В отчёте independent run должно быть `dump_or_old_jar_inputs: []`.

## Доказательство независимости

Запусти packaged toolchain в чистой папке только с original JAR. Сделай oracle/dump/intermediate paths недоступными. Проверь:

- полное покрытие protected entries;
- отсутствие fallback copies;
- совпадение class fingerprints с oracle samples;
- только разрешённые inputs в report;
- одинаковое логическое содержимое повторных JAR.

Общий ZIP SHA может различаться из-за DEFLATE metadata, поэтому сравнивай names/order/content hashes entries.

