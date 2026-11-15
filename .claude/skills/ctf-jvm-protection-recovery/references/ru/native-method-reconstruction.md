# Реконструкция native и @Compile методов

## Реалистичный результат

Машинный код обычно нельзя превратить в точный исходный Java bytecode/source: потеряны source structure, names, generics, annotations, exception shapes и optimization boundaries. Цель — семантически эквивалентная Java/bytecode реализация с evidence и tests.

Классифицируй результат:

- `exact recovered bytecode` — оригинальные bytes найдены в metadata/side table;
- `mechanical lift` — native CFG/data flow переведены с высокой точностью;
- `semantic reconstruction` — поведение реализовано по evidence;
- `safe stub` — несущественная или опасная логика намеренно отключена;
- `unresolved` — доказательств недостаточно.

## Полная инвентаризация

Найди все `ACC_NATIVE`, protection annotations (`@Compile`), JNI registrations, binding tables и Java callsites. Для каждой строки запиши owner/name/descriptor/access/annotation, native RVA, callers, module/UI/service reachability, side effects, reconstruction kind, confidence и tests.

Нельзя объявлять метод неиспользуемым только из-за отсутствия direct invoke. Проверь reflection, method handles, lambdas, registries, event buses, mixins, codecs, JNI lookup и native→Java callbacks.

## Восстановление semantics

Для каждого reachable root:

1. восстанови JNI boundary и точный Java descriptor;
2. идентифицируй fields, arrays, constants, strings, math, branches и callees;
3. отдели protection plumbing от бизнес-логики;
4. собери CFG и typed value flow;
5. учти exceptions, null behavior и state mutation;
6. переведи в Java или сгенерируй bytecode;
7. сравни с captured inputs/outputs или invariants.

Для rotation/movement/math сохрани float precision, radians/degrees, angle wrapping, interpolation/clamp order, randomness, tick timing, coordinate frame и side effects. Алгебраически похожая формула может отличаться из-за rounding/order.

Для GUI сохрани coordinate spaces, scale, anchors, window/framebuffer dimensions, animation state, input capture и lifecycle registration. GUI, который мигнул и оставил blur, часто закрылся после exception, получил null factory result, потерял event bridge или повреждённый layout state.

## Замена метода

При замене native declaration:

- убери `ACC_NATIVE` и при необходимости `ACC_ABSTRACT`;
- добавь валидный `Code`;
- используй точные owner/name/descriptor references;
- сохрани annotations, Signature и Exceptions;
- пересчитай max stack, max locals и frames;
- сохрани synchronized/static/bridge/synthetic semantics.

Target выбирай по owner+name+descriptor; неожиданные zero/multiple matches — ошибка.

## Safety patches

В offline CTF можно явно отключить protection-only handshake/shutdown, telemetry/HWID, screenshot/clipboard/process collection, self-update/download-execute, destructive filesystem/commands и token upload. Core module logic молча не заглушай. Документируй callsites и ожидаемый эффект каждой safe stub.

## Проверка

Используй три уровня:

1. static descriptor/linkage/bytecode verification;
2. component tests для codecs, catalogs, storage, math и UI factories;
3. integration tests для menu/module/command paths.

Веди карту каждого native/@Compile метода к модулю или части интерфейса. Старт JAR не означает, что все native roots восстановлены.

