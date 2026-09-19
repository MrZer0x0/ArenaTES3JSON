# ArenaTES3JSON 0.4.0

ArenaTES3JSON is a Qt 6 / Rust converter for Morrowind TES3 plugins:

**ESM / ESP ↔ tes3conv-compatible semantic JSON**

Version 0.4.0 adds guarded binary-preservation metadata for only the records that the semantic TES3 writer normalizes. This preserves empty-but-present subrecords, unknown/padding bytes, original string termination, absent default blocks, subrecord order, and explicit `CELL/NAM9` values without a separate `.arena-lossless` file. Semantic edits still win because every raw repair is guarded by the writer-produced bytes it expects to replace.

The GUI provides automatic/manual text encoding selection, selected-file information, progress, automatic RU/EN UI language, embedded application icon, and an optional TES3ZER0EDIT-compatible repair of changed SCPT structure (SCHD/SCVR rebuild plus stale SCDT removal). The latter is not a full Bethesda TESCS opcode compiler.

The Windows release is one user-facing `ArenaTES3JSON.exe`. Qt and the Rust backend are packaged into a native CAB payload and extracted by the Win32 launcher through SetupAPI; no PowerShell or 7-Zip is required on the target machine.

## Build

Requirements: Visual Studio 2022, Qt 6.8.x MSVC x64, CMake, Rust stable.

```bat
set QTDIR=C:\Qt\6.8.3\msvc2022_64
BUILD_WINDOWS.bat
```

GitHub Actions performs Rust tests, CMake/MSVC build, CTest, one-file packaging and one-file runtime self-test.
