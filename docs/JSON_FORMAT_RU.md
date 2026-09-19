# Формат JSON ArenaTES3JSON 0.2

ArenaTES3JSON 0.2 использует **семантическую схему tes3conv**, а не низкоуровневый список TES3 record/subrecord.

Корневой элемент — массив объектов. Первый объект обычно `Header`:

```json
[
  {
    "type": "Header",
    "flags": "",
    "version": 1.3,
    "file_type": "Esm",
    "author": "aL & TES Community",
    "description": "Описание",
    "num_objects": 51239,
    "masters": [
      ["Morrowind.esm", 79764287]
    ]
  },
  {
    "type": "GameSetting",
    "flags": "",
    "id": "sExpelled",
    "value": {
      "type": "String",
      "data": "ИЗГНАНЫ"
    }
  }
]
```

Имена типов и полей определяются TES3 object model и совместимы с JSON tes3conv: `Header`, `GameSetting`, `GlobalVariable`, `Class`, `Faction`, `Race`, `Script`, `Npc`, `Cell`, `Landscape`, `DialogueInfo` и другие.

## Windows-1251 / 1C

TES3-моды с русской 1C-локализацией исторически содержат однобайтовый текст Windows-1251. В режиме `Windows-1251 / 1C` ArenaTES3JSON преобразует такой текст в нормальный Unicode в JSON и обратно при сборке ESP/ESM. Поддерживается полная верхняя половина таблицы Windows-1251, включая `Ё/ё`, `№`, кавычки и тире.

## Lossless sidecar

В сам JSON **не добавляются** поля `data_b64`, `raw`, `record`, `subrecord` и прочая служебная информация.

При включённом lossless-режиме рядом с `MFR.json` создаётся:

`MFR.arena-lossless`

Sidecar содержит сжатый исходный плагин, SHA-256 и семантический hash JSON. Если JSON не менялся (переформатирование/пробелы не считаются изменением), обратная конвертация восстанавливает исходный ESP/ESM byte-for-byte. Если семантические данные изменены, ArenaTES3JSON собирает новый плагин из JSON.

Такой подход нужен потому, что семантический JSON намеренно не описывает все особенности исходного бинарного представления. Добавление этих байтов в JSON нарушило бы требуемую совместимость с tes3conv.
