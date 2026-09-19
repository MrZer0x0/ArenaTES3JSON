# ArenaTES3JSON

**ArenaTES3JSON** — конвертер плагинов Morrowind/TES3:

**ESM / ESP ↔ JSON**

GUI написан на **Qt 6**, семантический backend — на Rust и использует TES3 object model. Формат JSON совпадает по структуре с `tes3conv` и присланным примером `MFR.json`.

## Формат JSON

ArenaTES3JSON создаёт чистый semantic JSON без дополнительных sidecar-файлов и без служебных полей ArenaTES3JSON:

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

Поддерживаются типы, которые поддерживает закреплённая TES3-библиотека: `Header`, `GameSetting`, `GlobalVariable`, `Class`, `Faction`, `Race`, `Script`, `Npc`, `Cell`, `Landscape`, `Dialogue`, `DialogueInfo` и другие записи TES3.

При `JSON → ESM/ESP` плагин всегда пересобирается из semantic JSON. Поэтому бинарный размер и внутреннее представление могут отличаться от исходного файла даже если JSON не редактировался. Это ожидаемое поведение этой версии; смысловые данные плагина должны сохраняться.

## Windows-1251 / 1C

Режим **Windows-1251 / 1C** включён по умолчанию. Русские однобайтовые строки преобразуются в нормальный Unicode UTF-8 в JSON и обратно в представление, ожидаемое TES3 writer.

Таблица включает полную верхнюю половину Windows-1251: А-Я/а-я, Ё/ё, `№`, типографские кавычки, тире и другие символы. Преобразование теперь выполняется по исходному байту через внутреннее Windows-1252-представление TES3 backend, поэтому символы вроде `…`, кавычек и тире корректно собираются обратно в ESM/ESP.

## Упрощённый GUI

В окне оставлены только нужные элементы:

- входной `.esm`, `.esp` или `.json`;
- выходной файл;
- автоматически определяемое направление конвертации;
- кнопка **Конвертировать**;
- прогресс 0–100%;
- короткий итоговый статус;
- Drag & Drop.

Windows-1251/1C применяется автоматически. Никаких `.arena-lossless`, скрытых блоков или дополнительных файлов программа не создаёт.


## Один EXE в Windows-сборке

Release/GitHub Actions теперь выдаёт один файл:

```text
ArenaTES3JSON.exe
```

Qt DLL и `ArenaTES3JSON-core.exe` упакованы внутрь этого файла. При первом запуске встроенный runtime тихо извлекается в `%LOCALAPPDATA%\ArenaTES3JSON\0.3.2\app`; рядом с загруженным EXE никаких дополнительных файлов не требуется. При изменении встроенного payload кэш обновляется автоматически.

Исходная сборка проекта по-прежнему создаёт отдельные GUI/CLI/core для разработки и тестов, но пользовательский Windows-артефакт содержит только один EXE.

## CLI

```bat
ArenaTES3JSON-cli MFR.esm
ArenaTES3JSON-cli MFR.json MFR.esm
ArenaTES3JSON-cli --compact MFR.esm MFR.json
ArenaTES3JSON-cli --raw-encoding plugin.esp plugin.json
```

## Сборка Windows

Нужны:

- Visual Studio 2022 / MSVC x64;
- CMake 3.24+;
- Qt 6.5+ MSVC kit;
- Rust **stable** + Cargo.

Пример:

```bat
rustup toolchain install stable
rustup default stable
set QTDIR=C:\Qt\6.8.3\msvc2022_64
BUILD_WINDOWS.bat
```

Готовая пользовательская сборка появится в:

```text
dist\ArenaTES3JSON.exe
```

GitHub Actions выполняет ту же сборку автоматически и публикует `ArenaTES3JSON.exe` как единственный файл артефакта.

Подробности JSON: `docs/JSON_FORMAT_RU.md`.
