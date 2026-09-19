# Contributing

ArenaTES3JSON has two compatibility contracts:

1. The exported `.json` must remain compatible with the tes3conv semantic object schema.
2. The lossless sidecar must restore an unedited export to the exact original plugin bytes.

Before a pull request:

1. Run `cargo test --manifest-path core-rs/Cargo.toml` with Rust stable.
2. Build the Qt project.
3. Run `ctest --test-dir build -C Release --output-on-failure` on Windows/MSVC.
4. Test at least one Russian Windows-1251/1C plugin.
5. Verify `ESM/ESP -> JSON -> ESM/ESP` with `ArenaTES3JSON-cli --verify`.

Do not add Arena-specific raw fields to the semantic JSON. Binary preservation metadata belongs in `.arena-lossless`.
