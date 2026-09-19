# Changelog

## 0.3.6

- Embedded the ArenaTES3JSON application icon into the Windows GUI executable and the final one-file launcher.
- Set the Qt application icon globally so title bar, task switcher and dialogs use the same icon.
- The final `ArenaTES3JSON.exe` now exposes the icon directly to Windows Explorer/shortcuts without external icon files.

## 0.3.5

- Fixed a real semantic data-loss case in JSON -> ESM/ESP for CELL references.
- The TES3 backend canonicalizes local references (`mast_index = 0`) by omitting explicit `NAM9/object_count = 1`; ArenaTES3JSON now restores that subrecord whenever it was explicitly present in JSON.
- The supplied MFR regression contains 18,025 affected references. Restoring them adds exactly 216,300 bytes (`18,025 × 12`) and prevents the second ESM -> JSON pass from dropping `object_count`.
- Added strict FRMR identity validation before applying the repair; references are matched by encoded FRMR rather than list position because the TES3 object model may reorder CELL references.
- Added regression tests for inserting, preserving, and not inventing `NAM9`.
- Kept the v0.3.4 native-CAB one-file EXE launcher and automatic RU/EN UI selection.

## 0.3.4

- Added automatic RU/EN UI selection from the Windows/Qt system UI language. Russian locales use Russian; all other locales use English.
- Localization is compiled into the executable/runtime; no external `.qm` files are required.
- Replaced the one-file launcher's PowerShell `Expand-Archive` extraction with native Windows CAB extraction through SetupAPI.
- The one-file launcher no longer requires PowerShell on the target PC to unpack its bundled Qt/runtime files.
- Runtime cache paths are keyed by the embedded payload SHA-256, avoiding stale/locked cache collisions between builds.
- Launcher extraction/startup errors are localized automatically and now include the Windows error code and system message.

## 0.3.3

- Fixed a false GitHub Actions failure in the final one-file verification step.
- Windows GUI executables are now verified with `Start-Process -Wait -PassThru`, so CI waits for `--onefile-selftest` and reads the real process exit code instead of relying on `$LASTEXITCODE`.
- Added a reusable `scripts/verify_onefile.ps1` check and run it from both GitHub Actions and `BUILD_WINDOWS.bat`.
- The produced release artifact remains exactly one `ArenaTES3JSON.exe`.

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
