# Changelog

## 0.3.2

- Fixed JSON -> ESM/ESP for CP1251 punctuation and extended characters using byte-exact CP1251 <-> Windows-1252 transport mapping.
- Added regression coverage for `Arena_Dealer_script` text containing `…`; the transport bytes now match the original Windows-1251 bytes exactly.
- Windows release packaging now produces one portable `ArenaTES3JSON.exe` instead of a folder/ZIP with Qt DLLs and a separate core executable.
- The one-file launcher caches its bundled runtime under LocalAppData and refreshes it automatically when the embedded payload changes.

## 0.3.1

- Fixed JSON -> ESM/ESP encode failure on scripts containing CP1251 punctuation such as `…`.
- Replaced the numeric pseudo-Latin-1 conversion with an exact byte-preserving Windows-1251 <-> Windows-1252 transport bridge used by the TES3 backend.
- Added regression coverage for `Arena_Dealer_script`-style text, `№`, `Ё/ё`, punctuation, and the full defined CP1251 high-byte table.

## 0.3.0

- Removed `.arena-lossless` completely.
- Removed the experimental embedded `_arena_lossless` JSON block.
- JSON is now a clean tes3conv-style semantic array with no ArenaTES3JSON metadata.
- Removed zstd/Base64 lossless storage and its dependencies, reducing backend complexity and package size.
- JSON → ESM/ESP always rebuilds the plugin from semantic JSON.
- Added backend progress events and a responsive Qt progress bar.
- Simplified the GUI to input, output, direction, progress, status and one Convert button.
- Windows-1251/1C remains automatic in the GUI.
- Kept `--raw-encoding` and `--compact` as CLI-only advanced options.

## 0.2.1

- Switched the Rust backend from nightly to stable Rust.
- Disabled optional TES3 nightly/SIMD features that caused `hashbrown` specialization build failures.

## 0.2.0

- Switched from low-level record/subrecord JSON to tes3conv-compatible semantic JSON.
- Added the Rust TES3 semantic backend.
- Added Windows-1251/1C translation over semantic JSON string values.
- Added the first byte-identical round-trip experiment using an external sidecar.

## 0.1.1

- Fixed Qt 6.8 `QChar(uchar)` compatibility.
- Fixed Windows CI toolchain selection and packaging environment handling.

## 0.1.0

- Initial Qt/C++ prototype.
