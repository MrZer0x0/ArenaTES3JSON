# Changelog

## 0.4.1

- Store the original plugin filesystem mtime in Header._arena_file_mtime_utc (RFC3339 UTC) on ESM/ESP → JSON.
- Restore saved, current or custom mtime after JSON → plugin binary repairs; local Windows date/time picker in RU/EN GUI.
- Allow explicit ESP/ESM TES3 Header.file_type change, enforce matching output suffix, and provide GUI/CLI options.
- Add backend tests for timestamp round-trip, metadata cleanup and header type override.

## 0.4.0

- Added guarded low-level TES3 binary preservation without a separate sidecar file.
- Preserves empty-but-present subrecords such as `NPC_:ANAM`, `ACTI:FNAM`, `LIGH:MODL`, `RACE:FNAM` and `SOUN:FNAM`.
- Preserves writer-lost unknown/padding bytes in fixed binary structures (`AIDT`, short `NPDT`, AI packages, `RNAM`, `SNAM`, `PGRP`, etc.).
- Preserves original trailing-NUL behavior and removes writer-invented default subrecords when unchanged.
- Preserves original subrecord order when the semantic writer normalizes it.
- Keeps the explicit `CELL/NAM9 object_count=1` restoration from 0.3.5.
- Added `Auto` plus manual text-encoding selection using the encoding_rs/WHATWG encoding set.
- Added asynchronous file inspection after selecting an input: type, size, object count and detected encoding are shown in the GUI.
- Added optional changed-script SCPT repair: source hashes detect modified SCTX, SCHD/SCVR are rebuilt and stale SCDT is cleared in a TES3ZER0EDIT-compatible way.
- Script records repaired in that mode are excluded from binary restoration so old SCDT cannot be restored over modified source.
- GUI remains automatically localized RU/EN.
- Windows artifact remains one `ArenaTES3JSON.exe` with icon and native SetupAPI/CAB runtime extraction.
- Updated CLI to `--encoding LABEL` and `--repair-scripts`.

## 0.3.6

- Embedded the application icon into the GUI and one-file Windows launcher.

## 0.3.5

- Restored explicit local `CELL/NAM9 object_count=1` values dropped by the TES3 writer.

## 0.3.3

- Fixed one-file CI verification for Windows GUI subsystem executables.

## 0.3.2

- Fixed CP1251/Windows-1252 byte transport for punctuation such as ellipsis.
- Added native one-file Windows packaging.

## 0.3.0

- Simplified semantic JSON workflow and GUI.
