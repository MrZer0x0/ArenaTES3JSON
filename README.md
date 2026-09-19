# ArenaTES3JSON

ArenaTES3JSON converts Morrowind/TES3 plugins between **ESM/ESP and tes3conv-style semantic JSON**.

Version **0.3.2** writes a clean semantic JSON array with the normal record layout (`Header`, `GameSetting`, `Class`, `Npc`, `Cell`, `DialogueInfo`, etc.). It does not create `.arena-lossless` sidecars and does not add private metadata fields to JSON.

When converting JSON back to ESM/ESP, the plugin is rebuilt from the semantic data. Binary file size and layout may therefore differ from the original even when the JSON was not edited. Semantic plugin data is the compatibility target for this mode.

Windows-1251 / Russian 1C text conversion is enabled by default and covers the full upper half of the CP1251 table. Conversion is bridged by the original byte value through the TES3 backend's Windows-1252 string transport, so punctuation such as `…`, smart quotes and dashes remains writable during JSON -> plugin conversion.

## GUI

The Qt 6 GUI is intentionally small: input, output, auto-detected direction, one Convert button, a 0–100% progress bar, result status, and drag-and-drop.


## One-file Windows release

The Windows release artifact contains a single `ArenaTES3JSON.exe`. Qt runtime files and `ArenaTES3JSON-core.exe` are bundled inside it. On first launch the runtime is silently extracted to `%LOCALAPPDATA%\ArenaTES3JSON\0.3.2\app`, so no DLLs or companion executables need to sit beside the downloaded EXE. The cache is refreshed automatically when the embedded payload changes.

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
