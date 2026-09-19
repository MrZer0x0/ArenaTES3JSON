# ArenaTES3JSON

**ArenaTES3JSON** — конвертер плагинов Morrowind/TES3:

**ESM / ESP ↔ JSON**

GUI написан на **Qt 6**, семантический backend — на Rust и использует TES3 object model. Формат JSON совместим с `tes3conv` и с присланным примером `MFR.json`.

## Главное отличие 0.2

JSON больше не выглядит как низкоуровневые `records/subrecords/data_b64`.

Он выглядит так же, как tes3conv:

```json
[
  {
    "type": "Header",
    "flags": "",
    "version": 1.3,
    "file_type": "Esm",
    "author": "aL & TES Community",
    "description": "...",
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

Поддерживаются все типы, которые поддерживает закреплённая TES3-библиотека: `Header`, `GameSetting`, `GlobalVariable`, `Class`, `Faction`, `Race`, `Script`, `Npc`, `Cell`, `Landscape`, `Dialogue`, `DialogueInfo` и остальные записи TES3.

## Windows-1251 / 1C

Режим **Windows-1251 / 1C** включён по умолчанию. Русские однобайтовые строки преобразуются в нормальный Unicode UTF-8 в JSON и обратно в представление, ожидаемое TES3 writer.

В отличие от старого GUI, таблица включает не только А-Я/а-я и Ё/ё, но полную верхнюю половину Windows-1251, включая `№`, типографские кавычки и тире.

## Почему исходный ESM/ESP больше не должен уменьшаться без правок

Семантический JSON tes3conv не содержит всей информации о физическом бинарном представлении плагина. Поэтому полностью пересобранный файл может иметь другой размер даже при тех же объектах.

ArenaTES3JSON решает это без изменения JSON-схемы. При экспорте:

```text
MFR.esm
  ↓
MFR.json
MFR.arena-lossless
```

`MFR.json` остаётся обычным tes3conv-совместимым JSON. В `.arena-lossless` хранится сжатый исходный бинарник и контрольные суммы.

При обратной конвертации:

- если JSON **семантически не изменён**, исходный ESM/ESP восстанавливается **byte-for-byte**, с тем же размером и SHA-256;
- изменение пробелов/отступов или порядка ключей JSON не считается изменением;
- если данные JSON реально отредактированы, создаётся новый ESM/ESP через TES3 writer;
- если sidecar отсутствует, JSON всё равно можно собрать в ESM/ESP обычным способом.

Lossless-sidecar можно отключить в GUI или ключом `--no-lossless`.

## GUI

- выбор ESM/ESP/JSON;
- автоматическое имя выходного файла;
- Drag & Drop;
- Windows-1251/1C или raw-режим;
- compact JSON;
- lossless sidecar;
- проверка byte-for-byte пути.

## CLI

```bat
ArenaTES3JSON-cli MFR.esm
ArenaTES3JSON-cli MFR.json MFR.esm
ArenaTES3JSON-cli --verify MFR.esm
ArenaTES3JSON-cli --compact MFR.esm MFR.json
ArenaTES3JSON-cli --raw-encoding plugin.esp plugin.json
ArenaTES3JSON-cli --no-lossless plugin.esp plugin.json
```

## Сборка Windows

Нужны:

- Visual Studio 2022 / MSVC x64;
- CMake 3.24+;
- Qt 6.5+ MSVC kit;
- Rust **nightly** + Cargo.

Пример:

```bat
rustup toolchain install nightly
rustup default nightly
set QTDIR=C:\Qt\6.8.3\msvc2022_64
BUILD_WINDOWS.bat
```

Готовый архив появится в:

```text
dist\ArenaTES3JSON-windows-x64.zip
```

GitHub Actions выполняет ту же сборку автоматически.

Подробности JSON: `docs/JSON_FORMAT_RU.md`.
