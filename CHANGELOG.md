# Changelog

## 0.1.1 - 2026-09-19

- Fixed Qt 6.8 build failure caused by the deleted `QChar(uchar)` constructor.
- Fixed GitHub Actions toolchain selection: Windows builds now explicitly use Visual Studio 2022 x64/MSVC to match the `win64_msvc2022_64` Qt package.
- Updated Windows build preset, batch build script, test invocation and packaging paths for the multi-config MSVC generator.
- Packaging now accepts GitHub Actions `QT_ROOT_DIR` as well as local `QTDIR`.
- Corrected README build paths and CLI examples to match the implemented command-line interface.
- Text subrecords now also retain `data_b64` + `data_sha256`; deliberate raw edits take precedence over `text`.
- Expanded self-test with full defined CP1251 byte round-trip coverage and raw-edit precedence.

## 0.1.0 - 2026-09-19

- Initial Qt 6 repository.
- Direct TES3 record/subrecord parser and writer.
- Byte-preserving base64 interchange format.
- Built-in Windows-1251 / 1C codec.
- Human-readable editable text fields.
- SHA-256 lossless verification.
- Qt Widgets GUI, drag & drop and CLI.
- Windows GitHub Actions build and packaging.
