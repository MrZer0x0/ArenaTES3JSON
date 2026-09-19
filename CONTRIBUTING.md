# Contributing

ArenaTES3JSON has a few compatibility goals:

1. TES3 records must keep the tes3conv-style semantic JSON structure.
2. JSON must stay clean: no ArenaTES3JSON-only metadata fields and no lossless sidecar files.
3. Windows-1251/1C conversion must remain reversible for the supported CP1251 byte range.
4. JSON → ESM/ESP must rebuild valid TES3 plugins from edited semantic data.
5. The GUI should remain intentionally small and must not block the event loop during conversion.
6. Conversion progress must continue to be exposed to the Qt frontend.

Before submitting changes, run the Rust tests, CMake build, and CTest workflow used by GitHub Actions.
