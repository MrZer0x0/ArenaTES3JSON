# Third-party components

ArenaTES3JSON uses the open-source `tes3` Rust library by Greatness7 for the semantic TES3 object model and serde JSON schema.

- Project: https://github.com/Greatness7/tes3
- Pinned revision: `95cbc2cad2c574b023da879e308d3f6f6c4c3e04`
- Purpose: parse/write TES3 ESP/ESM structures and expose the semantic object model used by tes3conv-style JSON.

ArenaTES3JSON does **not** bundle or execute `tes3conv.exe`.

The Rust backend also uses `serde_json`, `encoding_rs`, `chardetng` and `base64` from crates.io. Their upstream licenses apply to their source/binaries in accordance with Cargo dependency licensing.

## TES3ZER0EDIT reference

The user-supplied TES3ZER0EDIT 1.9.2 was used as a behavioral reference for its lightweight SCPT repair approach: rebuild script-variable metadata and remove stale compiled SCDT when source text is changed. No TES3ZER0EDIT HTML/JavaScript code is bundled in ArenaTES3JSON. ArenaTES3JSON does not claim that this mode is a complete Bethesda TESCS MWScript bytecode compiler.

## Qt

The GUI uses Qt 6 (Core/Widgets). The Windows one-file package keeps Qt dynamically linked inside the embedded runtime; the launcher changes deployment into a single user-facing EXE. See the Qt licensing documentation and repository notices when redistributing builds.
