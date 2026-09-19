# ArenaTES3JSON

ArenaTES3JSON is a Qt 6 application for **TES3 ESM/ESP ↔ JSON** conversion.

Version 0.2 outputs the same semantic JSON shape as tes3conv (the supplied `MFR.json` style): a top-level array containing objects such as `Header`, `GameSetting`, `Class`, `Npc`, `Cell`, and `DialogueInfo`. The JSON contains no Arena-specific raw record fields.

## Goals

- tes3conv-compatible semantic JSON;
- Windows-1251 / Russian 1C text conversion to real Unicode JSON;
- byte-for-byte restoration when an exported JSON has not been semantically edited;
- Qt GUI plus command-line frontend;
- reproducible Windows build with GitHub Actions.

## Lossless mode

Semantic JSON cannot represent every physical detail of the original plugin binary. ArenaTES3JSON therefore stores lossless data outside the JSON:

```text
MFR.esm -> MFR.json + MFR.arena-lossless
```

If the JSON is semantically unchanged, importing it restores the original plugin bytes exactly. Whitespace and JSON object-key order do not invalidate the semantic hash. If the JSON was edited, or the sidecar is unavailable, the plugin is rebuilt from the semantic TES3 data.

## Windows-1251 / 1C

The default text mode maps the single-byte Russian Windows-1251 representation used by 1C localizations to Unicode in JSON and back during plugin generation. A raw mode is also available.

## Build

Requirements: Visual Studio 2022, CMake 3.24+, Qt 6.5+ MSVC x64, and Rust nightly.

```bat
rustup toolchain install nightly
rustup default nightly
set QTDIR=C:\Qt\6.8.3\msvc2022_64
BUILD_WINDOWS.bat
```

See `README_RU.md` and `docs/JSON_FORMAT_RU.md` for details.
