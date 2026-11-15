# Поиск HotSpot-классов и сериализация classfile

Этот раздел описывает восстановление уже загруженных классов из дампа JVM в разрешённой CTF. Layout внутренних структур меняется между версиями и кастомными build. Названия полей ниже семантические; числовые offsets нужно заново вывести для конкретной JVM.

## 1. Вывести точный layout JVM

Источники в порядке предпочтения:

1. исходники OpenJDK/HotSpot точного build;
2. PDB/DWARF/symbols или VMStructs;
3. декомпилированные accessors/constructors восстановленной `jvm.dll`;
4. проверка на множестве известных объектов в дампе.

Ищи `gHotSpotVMStructs`, `gHotSpotVMTypes`, `gHotSpotVMIntConstants`, `gHotSpotVMLongConstants`. Защищённый fork мог удалить или изменить их. Тогда offsets выводятся из функций, читающих/записывающих `Klass`, `InstanceKlass`, `ConstantPool`, `Method`, `ConstMethod`, `Symbol`, `Array<T>` и `ClassLoaderData`.

Для каждого поля записывай offset, размер, pointer/value смысл, адрес функции-доказательства, build JVM и confidence. Сначала проверяй обычные классы, затем защищённые.

## 2. Найти InstanceKlass

### Через ClassLoaderDataGraph

Найди head глобального графа загрузчиков, обойди каждый `ClassLoaderData` и связанный список `Klass`. Семантически нужны graph `_head`, CLD `_next`, CLD `_klasses` и `Klass::_next_link` либо их аналоги конкретного build.

При обходе:

- проверяй alignment и readable mapped range;
- vtable должна вести в ожидаемую JVM или подтверждённую private executable область;
- длины всех `Array<T>` ограничивай до чтения;
- отслеживай циклы и лимиты;
- сохраняй defining loader/CLD как provenance.

### Через vtable-scan

Если граф недоступен, найди vtable `InstanceKlass` по constructor/RTTI/известному объекту и сканируй только metaspace/class-space ranges. Совпадение первой pointer с vtable недостаточно: возможны stale/free objects и ложные совпадения.

### Строгая валидация кандидата

Настоящий `InstanceKlass` должен подтверждаться несколькими независимыми признаками:

- `Klass::_name` ведёт на валидный `Symbol` с ограниченной длиной и правдоподобным modified UTF-8 internal name;
- `_super` равен null только у `java/lang/Object`, иначе ведёт на валидный `Klass`;
- access flags допустимы;
- `_constants` ведёт на `ConstantPool`, а `pool_holder` возвращается к тому же классу;
- methods/interfaces arrays имеют разумную длину и валидные элементы;
- каждый `Method` имеет валидный `ConstMethod`, name/signature indices и holder;
- init state/layout helper правдоподобны;
- package/name коррелируют с архивом и framework metadata.

Если pointer имени зашифрован/XOR-нут, выведи формулу по accessor-коду и известным классам. Подтверди одну process-wide схему на множестве объектов.

## 3. Надёжный dump reader

Сделай единый слой чтения:

- `read(addr,size)` через соседние memory ranges;
- checked `u1/u2/u4/u8` и pointer reads;
- поиск module/range по адресу;
- bounded decoding HotSpot `Array<T>`;
- разбор `Symbol` по точному header/length/hash layout и modified UTF-8;
- cycle/recursion limits и diagnostics на объект.

Нельзя молча возвращать ноль при отсутствующей странице. Иначе повреждение превращается в фальшивые null-поля и ломает сериализацию далеко от причины.

## 4. Сначала логическая модель

Не пиши classfile прямо во время pointer chasing. Сначала собери IR:

```text
ClassModel
  version/access/this/super/interfaces
  ConstantPoolModel[] с исходными slot numbers
  FieldModel[]
  MethodModel[]
  ClassAttributeModel[]
  provenance и unresolved data
```

По возможности сохраняй исходные номера CP slots: bytecode, annotations, bootstrap methods и exception metadata уже на них ссылаются. Ранняя компактизация CP массово ломает ссылки.

## 5. ConstantPool

Прочитай `_length`, tags, slots, holder, cache/resolved references, operands/bootstrap data и fork-specific side tables. Поддержи все стандартные tags: Utf8, числа, Class/String, Field/Method/Interface refs, NameAndType, MethodHandle/MethodType, Dynamic/InvokeDynamic, Module/Package. Long/Double занимают два slots.

### Уже resolved entries

HotSpot изменяет runtime-представление CP. Вместо classfile index там может оказаться pointer на `Klass`, `Symbol`, `String`, `Method`, field metadata; CP-cache index; encoded pair; либо side-channel значение защиты.

Нужно восстановить логическую classfile-форму. Используй tag, reverse mapping из `ConstantPoolCache`, bytecode operand use, holder relations, parser code и companion tables. Никогда не записывай process pointer в `.class`.

### Modified UTF-8

Classfile Utf8 — modified UTF-8. Корректно обрабатывай NUL, surrogate pairs и byte length. Обычный UTF-8/JSON round-trip способен испортить имена и строковые constants.

## 6. Fields

Сначала определи формат конкретного build. В старых JVM поля часто лежат в packed `Array<u2>`; новые могут применять `FieldInfoStream`/compressed records и отдельные таблицы signatures/annotations. Нельзя универсально считать по шесть `u2` на поле.

Восстанови access, name/descriptor CP indices, generic Signature, `ConstantValue`, visible/invisible annotations и type annotations, synthetic/deprecated metadata и порядок полей.

Runtime null не доказывает нулевой initializer. `ConstantValue` хранится в metadata, прочие присваивания — в `<clinit>`/constructors. Если fork подставляет значения через side channel, найди writer/parser и сериализуй источник явно.

## 7. Method и ConstMethod

Для каждого `Method*` восстанови:

- holder через `ConstMethod -> ConstantPool -> pool_holder`;
- name/signature indices;
- access/method flags;
- bytecode size/start;
- `max_stack`, `max_locals`, parameter size;
- exception table и checked exceptions;
- generic signature, MethodParameters;
- annotations/default/type annotations;
- LineNumber/LocalVariable tables, если сохранились.

Interpreter/compiled entrypoints, adapters, counters, profiling data и native function pointers — runtime artifacts, в classfile они не записываются. Ненулевой compiled entry не является Java bytecode.

Trailing sections `ConstMethod` зависят от flags и alignment. Их порядок выводи из точного исходника или accessors. Размер проверяй относительно allocation boundary/соседних metadata.

### Нормализация bytecode

HotSpot мог заменить opcodes на `fast_*`, resolved invoke/field формы, breakpoints и native-endian operands; защита могла добавить свою substitution table. Верни legal classfile opcodes и big-endian operands.

Особенно проверь relative branches, padding `tableswitch/lookupswitch`, `wide`, `invokeinterface`, `invokedynamic`, `multianewarray`, CP-cache operands и exception-handler boundaries. Сначала построй карту всех instruction boundaries, затем переписывай targets.

## 8. Атрибуты

Восстанови `SourceFile`, `Signature`, annotations/type annotations, `InnerClasses`, `EnclosingMethod`, `BootstrapMethods`, `NestHost/NestMembers`, `Record`, `PermittedSubclasses`, module attributes и нужные framework-specific данные. Потеря nest/inner/mixin metadata часто проявляется только в reflection, lambda bootstrap, access checks или старте framework.

## 9. Запись classfile

Порядок: magic `CAFEBABE`, version, CP, class flags/this/super, interfaces, fields, methods, class attributes. Для каждого concrete method запиши `Code`: stack/locals, bytes, exception table и nested attributes. Не выдумывай пустое тело для отсутствующего метода — пометь unresolved или реконструируй отдельно.

Если `StackMapTable` не сохранился, сначала собери полную иерархию, потом пересчитай frames через ASM `COMPUTE_FRAMES` с offline common-superclass resolver по восстановленному JAR. Не загружай недоверенные классы и не подставляй всегда `java/lang/Object` без доказательства.

## 10. Runtime-generated classes

Lambda forms, hidden/proxy/mixin-generated classes могут быть в дампе, но не в архиве. Помечай их `runtime-generated`. Они полезны как evidence, но упаковка может вызвать collision с повторной генерацией.

## 11. Пилоты

Возьми классы с constants/fields, inheritance/interfaces, annotations/generics, invokedynamic, inner/nest/record, сложными switches/exceptions и один целевой protected-класс. Для каждого нужны parse success, correct name/path, legal CP, instruction boundaries, ASM verification и сравнение с oracle. Только потом масштабируй.
