# ArenaTES3JSON

ArenaTES3JSON converts Morrowind/TES3 plugins between **ESM/ESP and tes3conv-style semantic JSON**.

Version **0.3.6** writes a clean semantic JSON array with the normal record layout (`Header`, `GameSetting`, `Class`, `Npc`, `Cell`, `DialogueInfo`, etc.). It does not create `.arena-lossless` sidecars and does not add private metadata fields to JSON.

When converting JSON back to ESM/ESP, the plugin is rebuilt from the semantic data. Binary layout may still differ because the underlying writer normalizes records, but JSON-visible semantic fields must be preserved.

### Preserving `Cell.references[].object_count`

Version 0.3.6 also embeds the application icon into both the Qt GUI and the final one-file Windows launcher, so Explorer, shortcuts, Alt+Tab and the taskbar can display it.

Version 0.3.5 fixes the concrete data-loss case found in MFR: the TES3 writer canonicalized local references (`mast_index = 0`) by omitting an explicit `NAM9/object_count = 1`. ArenaTES3JSON now restores `NAM9` from JSON after the semantic writer and validates the matching `FRMR` before changing bytes. The supplied MFR has **18,025** affected references, exactly **216,300 bytes** of missing subrecords. No sidecar or private JSON metadata is used.

Windows-1251 / Russian 1C text conversion is enabled by default and covers the full upper half of the CP1251 table. Conversion is bridged by the original byte value through the TES3 backend's Windows-1252 string transport, so punctuation such as `…`, smart quotes and dashes remains writable during JSON -> plugin conversion.

## GUI

The Qt 6 GUI is intentionally small: input, output, auto-detected direction, one Convert button, a 0–100% progress bar, result status, and drag-and-drop.


## Automatic RU / EN

The GUI selects its language automatically from the Windows/Qt system UI language: Russian locales use **RU**, all other locales use **EN**. No external `.qm` translation files are required. The one-file launcher uses the same automatic RU/EN rule for startup and extraction errors.


## One-file Windows release

The Windows release artifact contains a single `ArenaTES3JSON.exe`. Qt runtime files and `ArenaTES3JSON-core.exe` are bundled inside it. On first launch the runtime is extracted natively with Windows SetupAPI/CAB to `%LOCALAPPDATA%\ArenaTES3JSON\Runtime\<payload-id>`, so no DLLs or companion executables need to sit beside the downloaded EXE. PowerShell/7-Zip are not required on the target PC, and each payload uses its own cache directory.

The source build still creates the separate GUI/CLI/core targets for development and testing; only the end-user release is packed into one EXE.

## CLI

```text
ArenaTES3JSON-cli MFR.esm
ArenaTES3JSON-cli MFR.json MFR.esm
ArenaTES3JSON-cli --compact MFR.esm MFR.json
ArenaTES3JSON-cli --raw-encoding plugin.esp plugin.json
```

## Build on Windows

Requirements: Visual Studio 2022/MSVC x64, CMake 3.24+, Qt 6.5+ MSVC kit, and stable Rust/Cargo.

```bat
rustup toolchain install stable
rustup default stable
set QTDIR=C:\Qt\6.8.3\msvc2022_64
BUILD_WINDOWS.bat
```

GitHub Actions uses Qt 6.8.3, MSVC 2022 x64 and stable Rust.

Local packaging with `BUILD_WINDOWS.bat` produces `dist\ArenaTES3JSON.exe`.
