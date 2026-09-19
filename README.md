# ArenaTES3JSON

Qt/C++ конвертер **Morrowind TES3 plugins**: `.esm/.esp ↔ .json`.

Цель проекта — заменить сценарий `tes3conv`, где обратная сериализация через объектную модель может нормализовать plugin и менять его размер даже без ручных правок. ArenaTES3JSON работает на уровне оригинальных TES3 record/subrecord и не зависит от `tes3conv`.

## Главное

- ESM/ESP → JSON и JSON → ESM/ESP.
- Lossless round-trip: неизменённый JSON должен собираться **byte-for-byte** как исходный plugin.
- Сохраняются порядок records/subrecords, `unknown`, `flags`, неизвестные бинарные payload и хвостовые данные.
- Нативное декодирование/кодирование **Windows-1251** для русской 1C-локализации; JSON остаётся стандартным UTF-8.
- Qt 6 GUI + отдельный CLI.
- Drag & Drop в GUI.
- Проверка `--verify`, которая делает ESM/ESP → JSON model → ESM/ESP в памяти и сравнивает байты.
- GitHub Actions для Windows 2022 / MSVC 2022 / Qt 6.8.3.

> В проекте нет BSA-конвертации. ArenaTES3JSON работает только с TES3 `.esm/.esp` plugins и JSON.

## Сборка Windows

Требуется Qt 6.5+ (рекомендуется Qt 6.8.x MSVC 2022), CMake 3.24+ и Visual Studio 2022 Build Tools.

```bat
set QTDIR=C:\Qt\6.8.3\msvc2022_64
BUILD_WINDOWS.bat
```

После сборки:

```text
build\windows-msvc\Release\ArenaTES3JSON.exe
build\windows-msvc\Release\ArenaTES3JSON-cli.exe
```

Portable ZIP:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_windows.ps1
```

## CLI

```text
ArenaTES3JSON-cli MyMod.esp MyMod.json
ArenaTES3JSON-cli MyMod.json MyMod.esp
ArenaTES3JSON-cli --compact MyMod.esm MyMod.json
ArenaTES3JSON-cli --verify MyMod.esp
```

При JSON → plugin расширение `.esm`/`.esp` берётся из `source.extension`, если выходной путь не задан.

## Про Windows-1251 / 1C

Старые русские плагины Morrowind часто содержат строки в Windows-1251. ArenaTES3JSON содержит собственную обратимую таблицу CP1251 и не использует трюк «прочитать байты как Latin-1, а затем визуально заменить символы». В JSON русский текст хранится нормальным Unicode UTF-8, а при обратной сборке кодируется в исходную Windows-1251.

## Формат JSON

См. [docs/JSON_FORMAT_RU.md](docs/JSON_FORMAT_RU.md).

## Ограничение v0.1

Это **lossless низкоуровневый** JSON: все records/subrecords доступны, строки автоматически раскрываются как CP1251, остальные данные сохраняются как Base64. Полная семантическая расшифровка каждого бинарного TES3-типа (NPC_, CELL, LAND и т. д.) может добавляться поверх этого слоя без риска потерять исходные байты.

## Лицензия

MIT.
