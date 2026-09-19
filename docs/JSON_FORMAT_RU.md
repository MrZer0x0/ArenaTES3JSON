# Формат JSON ArenaTES3JSON 0.3

ArenaTES3JSON использует **семантическую схему tes3conv**. Корневой элемент — массив TES3-объектов, первый объект обычно `Header`.

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
  }
]
```

Имена типов и полей определяются TES3 object model: `Header`, `GameSetting`, `GlobalVariable`, `Class`, `Faction`, `Race`, `Script`, `Npc`, `Cell`, `Landscape`, `DialogueInfo` и другие.

ArenaTES3JSON **не добавляет** в JSON собственные служебные поля и не создаёт отдельные lossless-sidecar файлы. JSON можно открывать и редактировать как обычный semantic JSON tes3conv.

## Обратная конвертация

При `JSON → ESM/ESP` бинарный plugin создаётся заново из semantic-модели. Поэтому размер и физическое расположение записей могут отличаться от исходного плагина даже без смысловых изменений JSON. Это не считается ошибкой само по себе.

Если после round-trip обнаружится потеря данных, изменение поведения мода или слишком большое необъяснимое отличие, такой файл следует использовать как тестовый пример для исправления writer/backend.

## Windows-1251 / 1C

В режиме по умолчанию ArenaTES3JSON преобразует однобайтовый Windows-1251 текст в нормальный Unicode UTF-8 в JSON и обратно. Поддерживается полная верхняя половина CP1251, включая `Ё/ё`, `№`, кавычки и тире.


## CELL reference object_count

Поле `Cell.references[].object_count` соответствует явному subrecord `NAM9` в TES3. ArenaTES3JSON 0.3.5 сохраняет наличие этого поля даже для локальных ссылок (`mast_index = 0`) со значением `1`, хотя базовая TES3-библиотека считает такое значение значением по умолчанию и обычно не записывает `NAM9`.
