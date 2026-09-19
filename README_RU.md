# ArenaTES3JSON 0.4.1

**ArenaTES3JSON** — Qt-конвертер плагинов Morrowind/TES3:

**ESM / ESP ↔ semantic JSON**

Формат основных объектов совместим со схемой `tes3conv` (`Header`, `GameSetting`, `Class`, `Npc`, `Cell`, `Script`, `DialogueInfo` и т. д.), но версия 0.4.1 дополнительно сохраняет только те бинарные детали, которые semantic writer сам теряет.

## Дата файла и тип ESP/ESM (0.4.1)

При экспорте `.esm/.esp → .json` ArenaTES3JSON записывает в **первый объект `Header`** дополнительное поле `_arena_file_mtime_utc` (UTC, RFC3339):

```json
{
  "type": "Header",
  "file_type": "Esm",
  "_arena_file_mtime_utc": "2002-05-01T12:34:56.1234567Z"
}
```

Это именно **время последнего изменения файла** Windows (mtime), влияющее на сортировку плагинов TES Construction Set. Это не TES3-поле `HEDR` и не время создания. Дата сохраняется непосредственно внутри одного JSON — отдельного файла рядом нет. `Header._arena_file_mtime_utc` удаляется из semantic-модели перед записью ESM/ESP. Редактировать дату в JSON можно вручную в формате RFC3339.

При обратной конвертации в интерфейсе выбирается режим:

- **Восстановить из JSON** (по умолчанию). Если JSON создан сторонней программой и поле отсутствует — используется время создания нового выходного файла.
- **Текущая дата и время**.
- **Своя дата и время** — календарь и время в местной зоне Windows, при передаче backend значение переводится в UTC.

Дата устанавливается **после** записи и всех бинарных исправлений, чтобы порядок TESCS не менялся в результате работы конвертера. Точность восстановления зависит от файловой системы (NTFS поддерживает интервалы 100 нс, GUI задаёт свою дату с точностью до секунды).

При `JSON → ESM/ESP` можно выбрать **тип плагина: из JSON, ESP или ESM**. Программа меняет одновременно поле `Header.file_type` в семантической модели TES3 и расширение результата. Простое переименование `.esm` в `.esp` здесь не используется. Мастер-зависимости и другие свойства плагина при такой смене типа автоматически не перерабатываются.

В CLI:

```bat
ArenaTES3JSON-cli MFR.json MFR.esp --file-type esp --file-date original
ArenaTES3JSON-cli MFR.json MFR.esm --file-type esm --file-date now
ArenaTES3JSON-cli MFR.json MFR.esp --file-type esp --file-date 2002-05-01T12:34:56Z
```

Параметры backend: `--file-type original|esp|esm`, `--file-date original|now|RFC3339`. Если выбранный тип заголовка не соответствует расширению, конвертация завершится ошибкой вместо создания ошибочно размеченного файла.

## Что исправлено в 0.4.0

На тестовом `MFR.esm` была найдена точная причина уменьшения файла после `ESM → JSON → ESM`: writer выбрасывал пустые subrecord'ы, обнулял unknown/padding-байты, добавлял NUL к некоторым строкам и иногда создавал отсутствовавшие default-subrecord'ы. ArenaTES3JSON теперь сравнивает исходный plugin с нормализованным выводом TES3 writer и сохраняет компактную дельту только для реально отличающихся record'ов.

Исправляется в том числе:

- пустой, но присутствующий `NPC_:ANAM`;
- пустые `ACTI:FNAM`, `LIGH:MODL`, `RACE:FNAM`, `SOUN:FNAM` и аналогичные случаи;
- unknown/padding bytes в `NPC_/CREA:AIDT`, коротком `NPC_:NPDT`, `AI_T`, `AI_F`, `AI_E` и других фиксированных структурах;
- хвостовые байты `FACT:RNAM`, `REGN:SNAM`, `PGRD:PGRP` и других структур;
- лишние завершающие NUL, добавленные semantic writer к строковым subrecord'ам;
- writer-created default blocks, которых не было в исходном plugin;
- исходный порядок subrecord'ов, если writer его нормализует;
- явный `Cell.references[].object_count = 1` / `NAM9`.

Никакого `.arena-lossless` sidecar рядом с JSON нет. Для точного восстановления небольшой `_arena_binary` добавляется **только к тем semantic-объектам, где writer реально меняет бинарное представление**. Патчи защищены ожидаемыми writer-байтами: если пользователь меняет соответствующее semantic-поле, его правка имеет приоритет и старые байты поверх неё не возвращаются.

## Скрипты / SCPT

В GUI есть галочка **«Ремонтировать байткод изменённых скриптов»**.

При экспорте ArenaTES3JSON сохраняет хэш исходного `SCTX`. При обратной конвертации с включённой галочкой программа определяет, у каких `Script` реально изменился текст, пересобирает список локальных переменных `SCVR` и счётчики `SCHD`, а устаревший `SCDT` очищает по схеме, используемой TES3ZER0EDIT. Для таких SCPT старые raw-патчи не применяются поверх нового состояния.

Это **ремонт структуры SCPT, а не полный компилятор опкодов Bethesda TESCS**. Поэтому опция не притворяется полноценной компиляцией произвольного MWScript в новый исполняемый vanilla-bytecode. Если нужен именно новый исполняемый SCDT для оригинального движка Morrowind, скрипт после изменения всё ещё следует скомпилировать TES Construction Set/MWEdit. Старый несовместимый байткод при этом ArenaTES3JSON не оставит случайно привязанным к новому тексту.

## Кодировки

Вместо жёстко заданной CP1251 появился режим **Auto** и ручной выбор кодировки. После выбора `.esm/.esp/.json` программа сразу показывает:

- тип файла;
- размер;
- число semantic-объектов;
- определённую/выбранную кодировку.

Поддерживаются кодировки `encoding_rs`/WHATWG, применимые к TES3, включая Windows-1250…1258, Windows-874, IBM866, KOI8-R/U, ISO-8859 family, Mac Cyrillic/Macintosh, Shift-JIS, EUC-JP, ISO-2022-JP, GBK/GB18030, Big5, EUC-KR, UTF-8 и их поддерживаемые aliases. Поле выбора редактируемое: можно ввести любое имя, принимаемое backend. Для русских Morrowind/1C plugin'ов Auto предпочитает Windows-1251 при уверенном обнаружении кириллицы.

JSON всегда записывается как нормальный UTF-8 Unicode.

## GUI

Окно остаётся компактным:

- входной файл;
- выходной файл;
- `Auto` / ручная кодировка;
- галочка ремонта изменённых SCPT;
- направление конвертации;
- информация о выбранном файле;
- прогресс 0–100%;
- короткий итоговый статус;
- Drag & Drop.

Язык интерфейса выбирается автоматически: русская системная locale → **RU**, остальные → **EN**.

## Один EXE

GitHub Actions/Windows packaging выдаёт только:

```text
ArenaTES3JSON.exe
```

Иконка встроена в EXE и отображается в Проводнике, ярлыке, заголовке окна, Alt+Tab и панели задач. Qt runtime и Rust backend упакованы внутрь CAB payload. Launcher извлекает его нативно через Windows SetupAPI в `%LOCALAPPDATA%\ArenaTES3JSON\Runtime\<payload-id>`, без PowerShell/7-Zip.

## CLI

```bat
ArenaTES3JSON-cli MFR.esm
ArenaTES3JSON-cli MFR.esm MFR.json --encoding auto
ArenaTES3JSON-cli MFR.json MFR.esm --encoding windows-1251
ArenaTES3JSON-cli MFR.json MFR.esm --repair-scripts
ArenaTES3JSON-cli --compact MFR.esm MFR.json
```

Низкоуровневый backend также умеет:

```bat
ArenaTES3JSON-core inspect MFR.esm --encoding auto
```

## Сборка Windows

Требуется Visual Studio 2022, Qt 6.8.x MSVC x64, CMake и Rust stable.

```bat
set QTDIR=C:\Qt\6.8.3\msvc2022_64
BUILD_WINDOWS.bat
```

Итоговый пользовательский файл появится в `dist\ArenaTES3JSON.exe`.
