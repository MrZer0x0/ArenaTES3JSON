use serde_json::Value;
use std::collections::{HashMap, HashSet};
use std::env;
use std::ffi::OsStr;
use std::fs::{self, File};
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use tes3::esp::Plugin;

// Windows-1251 bytes 0x80..0xFF. Undefined 0x98 is U+FFFD and is deliberately
// not converted so the original pseudo-Latin-1 code point remains reversible.
const CP1251_HIGH: [char; 128] = [
    '\u{0402}', '\u{0403}', '\u{201A}', '\u{0453}', '\u{201E}', '\u{2026}', '\u{2020}', '\u{2021}',
    '\u{20AC}', '\u{2030}', '\u{0409}', '\u{2039}', '\u{040A}', '\u{040C}', '\u{040B}', '\u{040F}',
    '\u{0452}', '\u{2018}', '\u{2019}', '\u{201C}', '\u{201D}', '\u{2022}', '\u{2013}', '\u{2014}',
    '\u{FFFD}', '\u{2122}', '\u{0459}', '\u{203A}', '\u{045A}', '\u{045C}', '\u{045B}', '\u{045F}',
    '\u{00A0}', '\u{040E}', '\u{045E}', '\u{0408}', '\u{00A4}', '\u{0490}', '\u{00A6}', '\u{00A7}',
    '\u{0401}', '\u{00A9}', '\u{0404}', '\u{00AB}', '\u{00AC}', '\u{00AD}', '\u{00AE}', '\u{0407}',
    '\u{00B0}', '\u{00B1}', '\u{0406}', '\u{0456}', '\u{0491}', '\u{00B5}', '\u{00B6}', '\u{00B7}',
    '\u{0451}', '\u{2116}', '\u{0454}', '\u{00BB}', '\u{0458}', '\u{0405}', '\u{0455}', '\u{0457}',
    '\u{0410}', '\u{0411}', '\u{0412}', '\u{0413}', '\u{0414}', '\u{0415}', '\u{0416}', '\u{0417}',
    '\u{0418}', '\u{0419}', '\u{041A}', '\u{041B}', '\u{041C}', '\u{041D}', '\u{041E}', '\u{041F}',
    '\u{0420}', '\u{0421}', '\u{0422}', '\u{0423}', '\u{0424}', '\u{0425}', '\u{0426}', '\u{0427}',
    '\u{0428}', '\u{0429}', '\u{042A}', '\u{042B}', '\u{042C}', '\u{042D}', '\u{042E}', '\u{042F}',
    '\u{0430}', '\u{0431}', '\u{0432}', '\u{0433}', '\u{0434}', '\u{0435}', '\u{0436}', '\u{0437}',
    '\u{0438}', '\u{0439}', '\u{043A}', '\u{043B}', '\u{043C}', '\u{043D}', '\u{043E}', '\u{043F}',
    '\u{0440}', '\u{0441}', '\u{0442}', '\u{0443}', '\u{0444}', '\u{0445}', '\u{0446}', '\u{0447}',
    '\u{0448}', '\u{0449}', '\u{044A}', '\u{044B}', '\u{044C}', '\u{044D}', '\u{044E}', '\u{044F}',
];


// The TES3 crate transports plugin strings through Windows-1252. ArenaTES3JSON
// presents the same raw bytes as Windows-1251 in JSON. Therefore conversion must
// bridge by BYTE VALUE, not by casting 0x80..0xFF to Unicode code points.
//
// Important example: byte 0x85 is U+2026 (ellipsis) in both code pages. The old
// numeric pseudo-char approach converted it to U+0085 on import and the TES3
// writer rejected the script as an encode error.
const CP1252_HIGH: [char; 128] = [
    '\u{20AC}', '\u{0081}', '\u{201A}', '\u{0192}', '\u{201E}', '\u{2026}', '\u{2020}', '\u{2021}',
    '\u{02C6}', '\u{2030}', '\u{0160}', '\u{2039}', '\u{0152}', '\u{008D}', '\u{017D}', '\u{008F}',
    '\u{0090}', '\u{2018}', '\u{2019}', '\u{201C}', '\u{201D}', '\u{2022}', '\u{2013}', '\u{2014}',
    '\u{02DC}', '\u{2122}', '\u{0161}', '\u{203A}', '\u{0153}', '\u{009D}', '\u{017E}', '\u{0178}',
    '\u{00A0}', '\u{00A1}', '\u{00A2}', '\u{00A3}', '\u{00A4}', '\u{00A5}', '\u{00A6}', '\u{00A7}',
    '\u{00A8}', '\u{00A9}', '\u{00AA}', '\u{00AB}', '\u{00AC}', '\u{00AD}', '\u{00AE}', '\u{00AF}',
    '\u{00B0}', '\u{00B1}', '\u{00B2}', '\u{00B3}', '\u{00B4}', '\u{00B5}', '\u{00B6}', '\u{00B7}',
    '\u{00B8}', '\u{00B9}', '\u{00BA}', '\u{00BB}', '\u{00BC}', '\u{00BD}', '\u{00BE}', '\u{00BF}',
    '\u{00C0}', '\u{00C1}', '\u{00C2}', '\u{00C3}', '\u{00C4}', '\u{00C5}', '\u{00C6}', '\u{00C7}',
    '\u{00C8}', '\u{00C9}', '\u{00CA}', '\u{00CB}', '\u{00CC}', '\u{00CD}', '\u{00CE}', '\u{00CF}',
    '\u{00D0}', '\u{00D1}', '\u{00D2}', '\u{00D3}', '\u{00D4}', '\u{00D5}', '\u{00D6}', '\u{00D7}',
    '\u{00D8}', '\u{00D9}', '\u{00DA}', '\u{00DB}', '\u{00DC}', '\u{00DD}', '\u{00DE}', '\u{00DF}',
    '\u{00E0}', '\u{00E1}', '\u{00E2}', '\u{00E3}', '\u{00E4}', '\u{00E5}', '\u{00E6}', '\u{00E7}',
    '\u{00E8}', '\u{00E9}', '\u{00EA}', '\u{00EB}', '\u{00EC}', '\u{00ED}', '\u{00EE}', '\u{00EF}',
    '\u{00F0}', '\u{00F1}', '\u{00F2}', '\u{00F3}', '\u{00F4}', '\u{00F5}', '\u{00F6}', '\u{00F7}',
    '\u{00F8}', '\u{00F9}', '\u{00FA}', '\u{00FB}', '\u{00FC}', '\u{00FD}', '\u{00FE}', '\u{00FF}',
];

#[derive(Debug)]
struct Cli {
    command: String,
    input: PathBuf,
    output: PathBuf,
    compact: bool,
    encoding: EncodingMode,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum EncodingMode {
    Cp1251,
    Raw,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct ReferenceCountExpectation {
    mast_index: u32,
    refr_index: u32,
    object_count: Option<u32>,
}

fn main() {
    if let Err(error) = real_main() {
        eprintln!("ArenaTES3JSON-core: {error}");
        std::process::exit(1);
    }
}

fn real_main() -> Result<(), Box<dyn std::error::Error>> {
    let cli = parse_cli()?;
    match cli.command.as_str() {
        "to-json" => to_json(&cli)?,
        "to-plugin" => to_plugin(&cli)?,
        _ => return Err(format!("unknown command: {}", cli.command).into()),
    }
    Ok(())
}

fn usage() -> &'static str {
    "ArenaTES3JSON-core 0.3.6\n\
     Usage:\n\
       ArenaTES3JSON-core to-json INPUT.esm OUTPUT.json [--compact] [--encoding cp1251|raw]\n\
       ArenaTES3JSON-core to-plugin INPUT.json OUTPUT.esm [--encoding cp1251|raw]\n"
}

fn parse_cli() -> Result<Cli, Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().skip(1).collect();
    if args.is_empty() || args.iter().any(|a| a == "--help" || a == "-h") {
        print!("{}", usage());
        std::process::exit(0);
    }
    if args.iter().any(|a| a == "--version" || a == "-V") {
        println!("ArenaTES3JSON-core 0.3.6");
        std::process::exit(0);
    }

    let command = args[0].clone();
    if !matches!(command.as_str(), "to-json" | "to-plugin") {
        return Err(format!("unknown command: {command}").into());
    }

    let mut positional = Vec::new();
    let mut compact = false;
    let mut encoding = EncodingMode::Cp1251;
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--compact" | "-c" => compact = true,
            "--encoding" => {
                i += 1;
                let value = args.get(i).ok_or("--encoding requires a value")?;
                encoding = match value.as_str() {
                    "cp1251" | "windows-1251" | "1c" => EncodingMode::Cp1251,
                    "raw" | "none" => EncodingMode::Raw,
                    _ => return Err(format!("unsupported encoding mode: {value}").into()),
                };
            }
            value if value.starts_with('-') => return Err(format!("unknown option: {value}").into()),
            value => positional.push(PathBuf::from(value)),
        }
        i += 1;
    }

    if positional.len() < 2 {
        return Err("missing input or output file".into());
    }

    Ok(Cli {
        command,
        input: positional[0].clone(),
        output: positional[1].clone(),
        compact,
        encoding,
    })
}

fn report_progress(percent: u8, stage: &str) {
    eprintln!("AT3J_PROGRESS\t{}\t{}", percent.min(100), stage);
    let _ = io::stderr().flush();
}

fn emit_status(command: &str, mode: &str, output: &Path, input_size: u64, output_size: u64, message: &str) {
    let status = serde_json::json!({
        "ok": true,
        "command": command,
        "mode": mode,
        "output": output.display().to_string(),
        "input_size": input_size,
        "output_size": output_size,
        "message": message,
    });
    println!("{}", status);
}

fn to_json(cli: &Cli) -> Result<(), Box<dyn std::error::Error>> {
    report_progress(4, "read-plugin");
    let input_size = fs::metadata(&cli.input)?.len();

    report_progress(12, "parse-plugin");
    let mut plugin = Plugin::new();
    plugin.load_path(&cli.input)?;

    report_progress(58, "serialize-json");
    // Do not sort. The supplied MFR.json and tes3conv preserve plugin object order.
    let raw_json = serde_json::to_string(&plugin.objects)?;
    let mut value: Value = serde_json::from_str(&raw_json)?;

    report_progress(70, "decode-windows-1251");
    transform_export_value(&mut value, cli.encoding);

    report_progress(86, "write-json");
    let json = if cli.compact {
        serde_json::to_string(&value)?
    } else {
        serde_json::to_string_pretty(&value)?
    };
    atomic_write(&cli.output, json.as_bytes())?;

    report_progress(100, "done");
    emit_status(
        "to-json",
        "semantic-json",
        &cli.output,
        input_size,
        json.len() as u64,
        "tes3conv-compatible semantic JSON written",
    );
    Ok(())
}

fn to_plugin(cli: &Cli) -> Result<(), Box<dyn std::error::Error>> {
    report_progress(4, "read-json");
    let json = fs::read_to_string(&cli.input)?;

    report_progress(15, "parse-json");
    let mut value: Value = serde_json::from_str(&json)?;
    let reference_counts = collect_reference_count_expectations(&value)?;

    report_progress(30, "encode-windows-1251");
    transform_import_value(&mut value, cli.encoding);

    report_progress(48, "deserialize-plugin");
    let tes3_json = serde_json::to_string(&value)?;
    let mut plugin = Plugin::new();
    plugin.objects = serde_json::from_str(&tes3_json)?;

    report_progress(68, "write-plugin");
    plugin.save_path(&cli.output)?;

    report_progress(84, "restore-reference-counts");
    let restored = restore_explicit_reference_counts(&cli.output, &reference_counts)?;

    report_progress(96, "finish");
    let output_size = fs::metadata(&cli.output)?.len();
    report_progress(100, "done");
    let message = if restored == 0 {
        "plugin rebuilt from semantic JSON".to_owned()
    } else {
        format!(
            "plugin rebuilt from semantic JSON; restored {restored} explicit CELL reference object_count/NAM9 subrecords"
        )
    };
    emit_status(
        "to-plugin",
        "semantic-rebuild",
        &cli.output,
        json.len() as u64,
        output_size,
        &message,
    );
    Ok(())
}

fn collect_reference_count_expectations(
    value: &Value,
) -> Result<Vec<Vec<ReferenceCountExpectation>>, Box<dyn std::error::Error>> {
    let objects = value
        .as_array()
        .ok_or("semantic JSON root must be an array")?;
    let mut cells = Vec::new();

    for object in objects {
        if object.get("type").and_then(Value::as_str) != Some("Cell") {
            continue;
        }

        let mut references = Vec::new();
        if let Some(items) = object.get("references") {
            let items = items
                .as_array()
                .ok_or("Cell.references must be an array")?;
            references.reserve(items.len());

            for reference in items {
                let mast_index = json_u32(reference, "mast_index")?;
                let refr_index = json_u32(reference, "refr_index")?;
                let object_count = match reference.get("object_count") {
                    Some(Value::Number(number)) => {
                        let value = number
                            .as_u64()
                            .ok_or("Cell reference object_count must be an unsigned integer")?;
                        Some(u32::try_from(value).map_err(|_| {
                            "Cell reference object_count is outside the uint32 range"
                        })?)
                    }
                    Some(Value::Null) | None => None,
                    Some(_) => {
                        return Err("Cell reference object_count must be an unsigned integer".into())
                    }
                };
                references.push(ReferenceCountExpectation {
                    mast_index,
                    refr_index,
                    object_count,
                });
            }
        }
        cells.push(references);
    }

    Ok(cells)
}

fn json_u32(value: &Value, key: &str) -> Result<u32, Box<dyn std::error::Error>> {
    let number = value
        .get(key)
        .and_then(Value::as_u64)
        .ok_or_else(|| format!("Cell reference {key} must be an unsigned integer"))?;
    Ok(u32::try_from(number)
        .map_err(|_| format!("Cell reference {key} is outside the uint32 range"))?)
}

fn restore_explicit_reference_counts(
    path: &Path,
    cells: &[Vec<ReferenceCountExpectation>],
) -> io::Result<usize> {
    let bytes = fs::read(path)?;
    let (patched, restored) = patch_plugin_reference_counts(&bytes, cells)?;
    if restored != 0 {
        atomic_write(path, &patched)?;
    }
    Ok(restored)
}

fn patch_plugin_reference_counts(
    input: &[u8],
    cells: &[Vec<ReferenceCountExpectation>],
) -> io::Result<(Vec<u8>, usize)> {
    let mut output = Vec::with_capacity(input.len());
    let mut offset = 0usize;
    let mut cell_index = 0usize;
    let mut restored = 0usize;

    while offset < input.len() {
        if input.len() - offset < 16 {
            return Err(invalid_data("truncated TES3 record header"));
        }

        let record_type = &input[offset..offset + 4];
        let size = read_u32_le(input, offset + 4)? as usize;
        let body_start = offset + 16;
        let body_end = body_start
            .checked_add(size)
            .ok_or_else(|| invalid_data("TES3 record size overflow"))?;
        if body_end > input.len() {
            return Err(invalid_data("truncated TES3 record body"));
        }

        if record_type == b"CELL" {
            let expected = cells
                .get(cell_index)
                .ok_or_else(|| invalid_data("writer produced more CELL records than JSON contains"))?;
            let (body, count) = patch_cell_reference_counts(&input[body_start..body_end], expected)?;
            restored += count;

            output.extend_from_slice(&input[offset..offset + 4]);
            let new_size = u32::try_from(body.len())
                .map_err(|_| invalid_data("CELL record exceeds uint32 size"))?;
            output.extend_from_slice(&new_size.to_le_bytes());
            output.extend_from_slice(&input[offset + 8..offset + 16]);
            output.extend_from_slice(&body);
            cell_index += 1;
        } else {
            output.extend_from_slice(&input[offset..body_end]);
        }

        offset = body_end;
    }

    if cell_index != cells.len() {
        return Err(invalid_data(format!(
            "writer produced {cell_index} CELL records, but JSON contains {}",
            cells.len()
        )));
    }

    Ok((output, restored))
}

#[derive(Clone, Copy)]
struct SubrecordSpan {
    start: usize,
    end: usize,
    payload_start: usize,
    payload_len: usize,
    tag: [u8; 4],
}

fn patch_cell_reference_counts(
    body: &[u8],
    expected: &[ReferenceCountExpectation],
) -> io::Result<(Vec<u8>, usize)> {
    let spans = parse_subrecords(body)?;
    let frmrs: Vec<usize> = spans
        .iter()
        .enumerate()
        .filter_map(|(index, span)| (span.tag == *b"FRMR").then_some(index))
        .collect();

    if frmrs.len() != expected.len() {
        return Err(invalid_data(format!(
            "CELL reference count mismatch: writer has {}, JSON has {}",
            frmrs.len(),
            expected.len()
        )));
    }

    if frmrs.is_empty() {
        return Ok((body.to_vec(), 0));
    }

    // The TES3 object model may reorder references while loading/saving a CELL.
    // FRMR is the stable per-cell identity, so match JSON expectations by the
    // encoded master/reference number rather than by list position.
    let mut expected_by_frmr = HashMap::with_capacity(expected.len());
    for item in expected {
        let key = reference_key(*item)?;
        if expected_by_frmr.insert(key, *item).is_some() {
            return Err(invalid_data(format!(
                "duplicate FRMR 0x{key:08X} in JSON CELL references"
            )));
        }
    }

    let mut matched = HashSet::with_capacity(expected.len());
    let mut output = Vec::with_capacity(body.len());
    let mut restored = 0usize;

    // Preserve all CELL-level subrecords before the first FRMR byte-for-byte.
    output.extend_from_slice(&body[..spans[frmrs[0]].start]);

    for (reference_index, &group_start_index) in frmrs.iter().enumerate() {
        let group_end_index = frmrs
            .get(reference_index + 1)
            .copied()
            .unwrap_or(spans.len());
        let group = &spans[group_start_index..group_end_index];
        let actual_frmr = read_frmr(body, group[0])?;
        let expectation = *expected_by_frmr.get(&actual_frmr).ok_or_else(|| {
            invalid_data(format!(
                "writer produced FRMR 0x{actual_frmr:08X} that is not present in JSON CELL references"
            ))
        })?;
        if !matched.insert(actual_frmr) {
            return Err(invalid_data(format!(
                "writer produced duplicate FRMR 0x{actual_frmr:08X} in one CELL"
            )));
        }

        let existing_nam9 = group.iter().position(|span| span.tag == *b"NAM9");
        let insert_after = group.iter().position(|span| span.tag == *b"INTV");
        let fallback_before = group
            .iter()
            .position(|span| span.tag == *b"XSOL" || span.tag == *b"DATA")
            .unwrap_or(group.len());

        for (local_index, span) in group.iter().enumerate() {
            if span.tag == *b"NAM9" {
                if let Some(expected_count) = expectation.object_count {
                    if span.payload_len != 4 {
                        return Err(invalid_data("NAM9 subrecord must be 4 bytes"));
                    }
                    let current = read_u32_le(body, span.payload_start)?;
                    if current != expected_count {
                        append_nam9(&mut output, expected_count);
                        restored += 1;
                        continue;
                    }
                }
            }

            output.extend_from_slice(&body[span.start..span.end]);

            if expectation.object_count.is_some()
                && existing_nam9.is_none()
                && insert_after == Some(local_index)
            {
                append_nam9(&mut output, expectation.object_count.unwrap());
                restored += 1;
            }

            if expectation.object_count.is_some()
                && existing_nam9.is_none()
                && insert_after.is_none()
                && local_index + 1 == fallback_before
            {
                append_nam9(&mut output, expectation.object_count.unwrap());
                restored += 1;
            }
        }

        if expectation.object_count.is_some()
            && existing_nam9.is_none()
            && insert_after.is_none()
            && fallback_before == group.len()
        {
            append_nam9(&mut output, expectation.object_count.unwrap());
            restored += 1;
        }
    }

    if matched.len() != expected.len() {
        return Err(invalid_data("not all JSON CELL references were matched to writer FRMR records"));
    }

    Ok((output, restored))
}

fn reference_key(expected: ReferenceCountExpectation) -> io::Result<u32> {
    if expected.mast_index > 0xFF || expected.refr_index > 0x00FF_FFFF {
        return Err(invalid_data("JSON CELL reference index cannot be represented by FRMR"));
    }
    Ok((expected.mast_index << 24) | expected.refr_index)
}

fn read_frmr(body: &[u8], span: SubrecordSpan) -> io::Result<u32> {
    if span.tag != *b"FRMR" || span.payload_len != 4 {
        return Err(invalid_data("FRMR subrecord must be 4 bytes"));
    }
    read_u32_le(body, span.payload_start)
}

fn append_nam9(output: &mut Vec<u8>, count: u32) {
    output.extend_from_slice(b"NAM9");
    output.extend_from_slice(&4u32.to_le_bytes());
    output.extend_from_slice(&count.to_le_bytes());
}

fn parse_subrecords(body: &[u8]) -> io::Result<Vec<SubrecordSpan>> {
    let mut spans = Vec::new();
    let mut offset = 0usize;
    while offset < body.len() {
        if body.len() - offset < 8 {
            return Err(invalid_data("truncated TES3 subrecord header"));
        }
        let mut tag = [0u8; 4];
        tag.copy_from_slice(&body[offset..offset + 4]);
        let payload_len = read_u32_le(body, offset + 4)? as usize;
        let payload_start = offset + 8;
        let end = payload_start
            .checked_add(payload_len)
            .ok_or_else(|| invalid_data("TES3 subrecord size overflow"))?;
        if end > body.len() {
            return Err(invalid_data("truncated TES3 subrecord body"));
        }
        spans.push(SubrecordSpan {
            start: offset,
            end,
            payload_start,
            payload_len,
            tag,
        });
        offset = end;
    }
    Ok(spans)
}

fn read_u32_le(data: &[u8], offset: usize) -> io::Result<u32> {
    let end = offset
        .checked_add(4)
        .ok_or_else(|| invalid_data("integer offset overflow"))?;
    let bytes: [u8; 4] = data
        .get(offset..end)
        .ok_or_else(|| invalid_data("truncated uint32 field"))?
        .try_into()
        .map_err(|_| invalid_data("invalid uint32 field"))?;
    Ok(u32::from_le_bytes(bytes))
}

fn invalid_data(message: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message.into())
}

fn transform_export_text(input: &str, mode: EncodingMode) -> String {
    if mode == EncodingMode::Raw {
        return input.to_owned();
    }
    input.chars().map(cp1251_from_transport_char).collect()
}

fn transform_import_text(input: &str, mode: EncodingMode) -> String {
    if mode == EncodingMode::Raw {
        return input.to_owned();
    }
    let mut out = String::with_capacity(input.len());
    for c in input.chars() {
        if let Some(transport) = transport_char_from_cp1251(c) {
            out.push(transport);
        } else {
            out.push(c);
        }
    }
    out
}

fn transform_export_value(value: &mut Value, mode: EncodingMode) {
    match value {
        Value::String(text) => *text = transform_export_text(text, mode),
        Value::Array(items) => {
            for item in items {
                transform_export_value(item, mode);
            }
        }
        Value::Object(map) => {
            for item in map.values_mut() {
                transform_export_value(item, mode);
            }
        }
        _ => {}
    }
}

fn transform_import_value(value: &mut Value, mode: EncodingMode) {
    match value {
        Value::String(text) => *text = transform_import_text(text, mode),
        Value::Array(items) => {
            for item in items {
                transform_import_value(item, mode);
            }
        }
        Value::Object(map) => {
            for item in map.values_mut() {
                transform_import_value(item, mode);
            }
        }
        _ => {}
    }
}

fn cp1251_from_transport_char(c: char) -> char {
    if c.is_ascii() {
        return c;
    }

    // Find which byte the TES3 crate's Windows-1252 string represents, then
    // interpret that exact byte as Windows-1251 for the JSON view.
    if let Some(idx) = CP1252_HIGH.iter().position(|mapped| *mapped == c) {
        let mapped = CP1251_HIGH[idx];
        if mapped != '\u{FFFD}' {
            return mapped;
        }
    }
    c
}

fn transport_char_from_cp1251(c: char) -> Option<char> {
    if c.is_ascii() {
        return None;
    }

    // Reverse of cp1251_from_transport_char(): choose the Windows-1252
    // character that the TES3 writer will encode to the same byte value.
    CP1251_HIGH
        .iter()
        .position(|mapped| *mapped == c && *mapped != '\u{FFFD}')
        .map(|idx| CP1252_HIGH[idx])
}

fn atomic_write(path: &Path, data: &[u8]) -> io::Result<()> {
    let parent = path.parent().unwrap_or_else(|| Path::new("."));
    fs::create_dir_all(parent)?;
    let name = path.file_name().unwrap_or_else(|| OsStr::new("output"));
    let temp = parent.join(format!(".{}.arenates3json.tmp", name.to_string_lossy()));
    {
        let mut file = File::create(&temp)?;
        file.write_all(data)?;
        file.sync_all()?;
    }
    if path.exists() {
        fs::remove_file(path)?;
    }
    fs::rename(temp, path)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cp1251_russian_round_trip() {
        let pseudo = "Ïðèâåò, ¨æèê!";
        let unicode = transform_export_text(pseudo, EncodingMode::Cp1251);
        assert_eq!(unicode, "Привет, Ёжик!");
        assert_eq!(transform_import_text(&unicode, EncodingMode::Cp1251), pseudo);
    }

    #[test]
    fn cp1251_extended_symbols() {
        assert_eq!(cp1251_from_transport_char('\u{00B9}'), '\u{2116}');
        assert_eq!(transport_char_from_cp1251('\u{2116}'), Some('\u{00B9}'));
    }

    #[test]
    fn cp1251_punctuation_stays_encodable() {
        // Regression for Arena_Dealer_script: CP1251 and CP1252 both encode
        // U+2026 as byte 0x85, so the transport character must stay U+2026.
        assert_eq!(transport_char_from_cp1251('…'), Some('…'));
        assert_eq!(cp1251_from_transport_char('…'), '…');
        assert_eq!(transform_import_text("Это честь…", EncodingMode::Cp1251), "Ýòî ÷åñòü…");
    }

    #[test]
    fn cp1251_high_table_round_trip() {
        for byte in 0x80u32..=0xFF {
            if byte == 0x98 {
                continue;
            }
            let idx = (byte - 0x80) as usize;
            let transport = CP1252_HIGH[idx];
            let unicode = cp1251_from_transport_char(transport);
            assert_eq!(transport_char_from_cp1251(unicode), Some(transport));
        }
    }


    fn subrecord(tag: &[u8; 4], payload: &[u8]) -> Vec<u8> {
        let mut out = Vec::new();
        out.extend_from_slice(tag);
        out.extend_from_slice(&(payload.len() as u32).to_le_bytes());
        out.extend_from_slice(payload);
        out
    }

    fn one_cell_plugin(body: &[u8]) -> Vec<u8> {
        let mut out = Vec::new();
        out.extend_from_slice(b"CELL");
        out.extend_from_slice(&(body.len() as u32).to_le_bytes());
        out.extend_from_slice(&0u32.to_le_bytes());
        out.extend_from_slice(&0u32.to_le_bytes());
        out.extend_from_slice(body);
        out
    }

    #[test]
    fn restores_explicit_nam9_for_local_reference_count_one() {
        let mut body = Vec::new();
        body.extend(subrecord(b"NAME", b"\0"));
        body.extend(subrecord(b"FRMR", &1u32.to_le_bytes()));
        body.extend(subrecord(b"NAME", b"crate\0"));
        body.extend(subrecord(b"INTV", &0u32.to_le_bytes()));
        body.extend(subrecord(b"DATA", &[0u8; 24]));
        let plugin = one_cell_plugin(&body);
        let expected = vec![vec![ReferenceCountExpectation {
            mast_index: 0,
            refr_index: 1,
            object_count: Some(1),
        }]];

        let (patched, restored) = patch_plugin_reference_counts(&plugin, &expected).unwrap();
        assert_eq!(restored, 1);
        assert_eq!(patched.len(), plugin.len() + 12);
        assert!(patched.windows(4).any(|bytes| bytes == b"NAM9"));
    }

    #[test]
    fn does_not_invent_nam9_when_json_omits_object_count() {
        let mut body = Vec::new();
        body.extend(subrecord(b"NAME", b"\0"));
        body.extend(subrecord(b"FRMR", &1u32.to_le_bytes()));
        body.extend(subrecord(b"NAME", b"crate\0"));
        body.extend(subrecord(b"INTV", &0u32.to_le_bytes()));
        body.extend(subrecord(b"DATA", &[0u8; 24]));
        let plugin = one_cell_plugin(&body);
        let expected = vec![vec![ReferenceCountExpectation {
            mast_index: 0,
            refr_index: 1,
            object_count: None,
        }]];

        let (patched, restored) = patch_plugin_reference_counts(&plugin, &expected).unwrap();
        assert_eq!(restored, 0);
        assert_eq!(patched, plugin);
    }

    #[test]
    fn matches_references_by_frmr_not_json_list_order() {
        let mut body = Vec::new();
        body.extend(subrecord(b"NAME", b"\0"));
        body.extend(subrecord(b"FRMR", &1u32.to_le_bytes()));
        body.extend(subrecord(b"NAME", b"first\0"));
        body.extend(subrecord(b"INTV", &0u32.to_le_bytes()));
        body.extend(subrecord(b"DATA", &[0u8; 24]));
        body.extend(subrecord(b"FRMR", &2u32.to_le_bytes()));
        body.extend(subrecord(b"NAME", b"second\0"));
        body.extend(subrecord(b"INTV", &0u32.to_le_bytes()));
        body.extend(subrecord(b"DATA", &[0u8; 24]));
        let plugin = one_cell_plugin(&body);

        // Deliberately reverse JSON order. Some real CELLs are normalized by
        // the TES3 model, so positional matching would be unsafe.
        let expected = vec![vec![
            ReferenceCountExpectation {
                mast_index: 0,
                refr_index: 2,
                object_count: None,
            },
            ReferenceCountExpectation {
                mast_index: 0,
                refr_index: 1,
                object_count: Some(1),
            },
        ]];

        let (patched, restored) = patch_plugin_reference_counts(&plugin, &expected).unwrap();
        assert_eq!(restored, 1);
        assert_eq!(patched.len(), plugin.len() + 12);
    }

    #[test]
    fn does_not_duplicate_existing_nam9() {
        let mut body = Vec::new();
        body.extend(subrecord(b"NAME", b"\0"));
        body.extend(subrecord(b"FRMR", &1u32.to_le_bytes()));
        body.extend(subrecord(b"NAME", b"crate\0"));
        body.extend(subrecord(b"INTV", &0u32.to_le_bytes()));
        body.extend(subrecord(b"NAM9", &1u32.to_le_bytes()));
        body.extend(subrecord(b"DATA", &[0u8; 24]));
        let plugin = one_cell_plugin(&body);
        let expected = vec![vec![ReferenceCountExpectation {
            mast_index: 0,
            refr_index: 1,
            object_count: Some(1),
        }]];

        let (patched, restored) = patch_plugin_reference_counts(&plugin, &expected).unwrap();
        assert_eq!(restored, 0);
        assert_eq!(patched, plugin);
    }
}
