# Changelog

## 0.2.1

- Fixed current Windows CI failure in `hashbrown 0.14.5`: `cannot specialize on trait Copy`.
- Removed the optional TES3 `nightly` and `simd` features from ArenaTES3JSON; the semantic backend now enables only `esp` and `serde`.
- Switched the backend and GitHub Actions from moving Rust nightly to Rust stable.
- Added `rust-toolchain.toml` and CI Rust version diagnostics.
- Kept the pinned TES3 revision and the tes3conv-compatible JSON schema unchanged.

## 0.2.0

- Replaced the incorrect low-level Arena JSON schema with tes3conv-compatible semantic JSON.
- JSON root is now the TES3 object array (`Header`, `GameSetting`, `Npc`, `Cell`, `DialogueInfo`, etc.).
- Pinned the TES3 object model to the revision used by the supplied tes3conv GUI for schema compatibility.
- Added Windows-1251/1C translation for the complete high half of CP1251.
- Added `.arena-lossless` sidecar so an unedited semantic JSON can restore the exact original ESP/ESM bytes and size.
- Semantic sidecar hash ignores JSON formatting and object-key order.
- Qt GUI now exposes Windows-1251/raw and lossless options.
- Added Rust semantic backend to the CMake/GitHub Actions build.

## 0.1.1

- Fixed Qt 6.8 `QChar(uchar)` build error.
- Fixed MSVC/MinGW toolchain mismatch in CI.
- Fixed `QT_ROOT_DIR` packaging support.

## 0.1.0

- Initial low-level prototype. Its JSON schema is deprecated and is not compatible with 0.2.
