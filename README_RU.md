# ArenaTES3JSON

**ArenaTES3JSON** — новый Qt 6 конвертер плагинов TES3/Morrowind (`.esp`, `.esm`) в JSON и обратно, сделанный с упором на сохранение исходного бинарного файла.

## Зачем он нужен

Обычный конвертер, который загружает плагин в объектную модель и затем полностью сохраняет его заново, может изменить бинарное представление даже без правок. ArenaTES3JSON работает иначе: он сохраняет исходные байты каждой подзаписи и не нормализует нетронутые данные.

Главная проверка проекта:

```text
plugin.esp -> plugin.json -> plugin.esp
SHA-256 до == SHA-256 после
```

Если JSON не редактировался, конвертация должна быть **byte-for-byte** идентичной.

## Возможности

- ESP/ESM → JSON и JSON → ESP/ESM;
- lossless round-trip без уменьшения/пересборки нетронутых данных;
- встроенная Windows-1251 / 1C кодировка;
- отображение строк в Unicode в поле `text`;
- исходные байты всегда хранятся в `data_b64`;
- изменение `text` перекодирует только изменённую подзапись;
- изменение `data_b64` позволяет редактировать бинарные данные напрямую;
- сохранение неизвестных/неразобранных payload как opaque base64;
- SHA-256 проверка round-trip;
- GUI на Qt 6 Widgets;
- отдельный CLI для скриптов и CI;
- drag & drop;
- GitHub Actions для Windows x64.

## Важное отличие от tes3conv

ArenaTES3JSON v0.1 не пытается превратить все 40+ типов TES3 record в красивую семантическую объектную модель. JSON отражает реальную структуру `record/subrecord` и поэтому может гарантировать сохранение неизвестных данных. Читаемые строковые подзаписи дополнительно получают поле `text`.

Это сознательная база для дальнейшего добавления типизированных представлений (`CELL`, `NPC_`, `DIAL`, `INFO`, `SCPT` и т.д.) без потери lossless-слоя.

## Сборка Windows

Нужно:

- Visual Studio 2022 Build Tools / MSVC;
- CMake 3.24+;
- Qt 6.8.x MSVC 2022 x64.

В `cmd.exe`:

```bat
set QTDIR=C:\Qt\6.8.3\msvc2022_64
BUILD_WINDOWS.bat
```

После сборки исполняемые файлы находятся в:

```text
build\windows-msvc\Release\ArenaTES3JSON.exe
build\windows-msvc\Release\ArenaTES3JSON-cli.exe
```

Для portable-пакета:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_windows.ps1
```

## CLI

```bat
ArenaTES3JSON-cli.exe Morrowind.esm Morrowind.json
ArenaTES3JSON-cli.exe Morrowind.json Morrowind.esm
ArenaTES3JSON-cli.exe --verify MyPlugin.esp
```

Полезные флаги:

```text
--compact       компактный JSON
--verify        round-trip и byte-for-byte проверка
```

## Как редактировать строки

Пример:

```json
{
  "type": "NAME",
  "data_b64": "...",
  "data_sha256": "...",
  "text": "Русское имя",
  "trailing_nuls": 1
}
```

Если изменить только `text`, программа кодирует новую строку в Windows-1251 и заменит данные только этой подзаписи. Если изменить `data_b64`, именно raw-байты считаются намеренной правкой.

Подробнее: `docs/JSON_FORMAT_RU.md`.
