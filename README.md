# ArenaTES3JSON 0.4.1

ArenaTES3JSON is a Qt 6 / Rust converter for Morrowind TES3 plugins:

**ESM / ESP ↔ tes3conv-compatible semantic JSON**

Version 0.4.1 adds guarded binary-preservation metadata for only the records that the semantic TES3 writer normalizes. This preserves empty-but-present subrecords, unknown/padding bytes, original string termination, absent default blocks, subrecord order, and explicit `CELL/NAM9` values without a separate `.arena-lossless` file. Semantic edits still win because every raw repair is guarded by the writer-produced bytes it expects to replace.

The GUI provides automatic/manual text encoding selection, selected-file information, progress, automatic RU/EN UI language, embedded application icon, and an optional TES3ZER0EDIT-compatible repair of changed SCPT structure (SCHD/SCVR rebuild plus stale SCDT removal). The latter is not a full Bethesda TESCS opcode compiler.

The Windows release is one user-facing `ArenaTES3JSON.exe`. Qt and the Rust backend are packaged into a native CAB payload and extracted by the Win32 launcher through SetupAPI; no PowerShell or 7-Zip is required on the target machine.

## Build

Requirements: Visual Studio 2022, Qt 6.8.x MSVC x64, CMake, Rust stable.

```bat
set QTDIR=C:\Qt\6.8.3\msvc2022_64
BUILD_WINDOWS.bat
```

GitHub Actions performs Rust tests, CMake/MSVC build, CTest, one-file packaging and one-file runtime self-test.

## File modification time and ESP/ESM type (0.4.1)

Export adds `_arena_file_mtime_utc` (RFC3339 UTC) to the semantic `Header` object in the same JSON. Import removes this metadata before TES3 deserialization, then sets the output file **modification time (mtime)** after the final binary patch; the default is to restore the source timestamp. GUI also offers “Current date/time” and a custom date/time picker (local Windows time). For old third-party JSON without this field, original-date mode leaves the newly created output time intact. This is filesystem modification time, not the TES3 HEDR field or Windows creation time. Filesystem timestamp resolution may differ.

On JSON → plugin the GUI offers “From JSON header / ESP / ESM”. The explicit choice updates both the semantic TES3 `Header.file_type` and the output filename extension. The backend refuses mismatches. Changing plugin type does not rewrite master dependencies.

CLI examples:

```console
ArenaTES3JSON-cli MFR.json MFR.esp --file-type esp --file-date original
ArenaTES3JSON-cli MFR.json MFR.esm --file-type esm --file-date now
ArenaTES3JSON-cli MFR.json MFR.esp --file-type esp --file-date 2002-05-01T12:34:56Z
```
