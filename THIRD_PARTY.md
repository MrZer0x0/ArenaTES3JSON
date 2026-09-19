# Third-party components

ArenaTES3JSON uses the open-source `tes3` Rust library by Greatness7 for the semantic TES3 object model and serde JSON schema.

- Project: https://github.com/Greatness7/tes3
- Pinned revision: `95cbc2cad2c574b023da879e308d3f6f6c4c3e04`
- Purpose: parse/write TES3 ESP/ESM structures and expose the same semantic object model used by the supplied tes3conv GUI.

ArenaTES3JSON does **not** bundle or execute `tes3conv.exe`. The Qt application and ArenaTES3JSON backend are separate code; the shared object model is used so existing tes3conv-format JSON remains compatible.


## Qt

The GUI uses Qt 6 (Core/Widgets). The Windows one-file package keeps Qt dynamically linked inside the embedded runtime; the launcher only changes deployment into a single downloadable EXE. See the Qt licensing documentation and the repository license/notices when redistributing builds.
