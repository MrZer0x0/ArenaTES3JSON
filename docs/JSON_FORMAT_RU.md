# Формат JSON ArenaTES3JSON 0.4

Корень остаётся массивом semantic-объектов TES3, совместимым по основным полям с `tes3conv`:

```json
[
  {
    "type": "Header",
    "flags": "",
    "version": 1.3,
    "file_type": "Esm",
    "author": "...",
    "description": "...",
    "num_objects": 51239,
    "masters": []
  }
]
```

## Служебные поля 0.4

TES3 semantic model не представляет некоторые физически существующие данные: empty subrecord presence, unknown/padding bytes, точную NUL-терминацию и некоторые default-блоки. Поэтому ArenaTES3JSON 0.4 добавляет минимальную служебную информацию только там, где собственный writer библиотеки доказанно изменил байты.

- `Header._arena_encoding` — кодировка исходного текстового payload.
- `Script._arena_source_hash` — позволяет определить, менялся ли `SCTX` перед опциональным ремонтом SCPT.
- `<Object>._arena_binary` — компактная guarded-delta исходного бинарного record относительно нормализованного writer output.

Пример принципа (точная форма может содержать несколько операций):

```json
{
  "type": "Npc",
  "id": "example",
  "...": "обычные semantic-поля",
  "_arena_binary": {
    "v": 1,
    "record": "NPC_",
    "header": [],
    "subs": [
      {"op":"insert","tag":"ANAM","occ":0,"index":4,"payload":"00"},
      {"op":"bytes","tag":"AIDT","occ":0,"changes":[[5,0,6],[6,0,50],[7,0,118]]}
    ]
  }
}
```

При импорте `from`/expected writer bytes проверяются перед каждым патчем. Если semantic-поле было отредактировано и writer уже выдал другие байты, соответствующий raw-патч не применяется. Поэтому `_arena_binary` восстанавливает только потерянное физическое представление, не отменяя пользовательские правки.

Отдельного `.arena-lossless` файла нет.

## Кодировки

JSON — UTF-8. `_arena_encoding` хранит исходную однобайтовую кодировку. Режим `Auto` использует её при обратной конвертации. Поддерживаются кодировки/aliases, которые принимает `encoding_rs`, в том числе Windows-1250…1258, IBM866, KOI8-R/U, ISO-8859 variants, Mac encodings и основные Japanese/Chinese/Korean encodings.

## SCPT

У `Script` обычные поля `header`, `variables`, `bytecode`, `text` остаются semantic-полями tes3conv. `_arena_source_hash` нужен только для определения, изменился ли `text`. При включённом ремонте изменённого скрипта старый raw metadata этого SCPT не накладывается поверх нового состояния.


Если `_arena_binary` вручную удалить у конкретного объекта, он будет собран только из semantic-полей обычным writer без попытки восстановить старое физическое представление этого record. Это удобный явный способ отказаться от binary-preservation для конкретной записи после глубокой ручной правки.
