use base64::{engine::general_purpose::STANDARD as BASE64, Engine as _};
use chardetng::EncodingDetector;
use encoding_rs::{Encoding, WINDOWS_1251, WINDOWS_1252};
use serde_json::{json, Value};
use std::collections::{HashMap, HashSet, VecDeque};
use std::env;
use std::ffi::OsStr;
use std::fs::{self, File};
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use tes3::esp::Plugin;

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
    encoding: String,
    repair_scripts: RepairScriptsMode,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum RepairScriptsMode {
    Off,
    Changed,
    All,
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
        "inspect" => inspect_input(&cli)?,
        _ => return Err(format!("unknown command: {}", cli.command).into()),
    }
    Ok(())
}

fn usage() -> &'static str {
    "ArenaTES3JSON-core 0.4.0\n\
     Usage:\n\
       ArenaTES3JSON-core to-json INPUT.esm OUTPUT.json [--compact] [--encoding auto|LABEL]\n\
       ArenaTES3JSON-core to-plugin INPUT.json OUTPUT.esm [--encoding auto|LABEL] [--repair-scripts off|changed|all]\n\
       ArenaTES3JSON-core inspect INPUT.esm [--encoding auto|LABEL]\n"
}

fn parse_cli() -> Result<Cli, Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().skip(1).collect();
    if args.is_empty() || args.iter().any(|a| a == "--help" || a == "-h") {
        print!("{}", usage());
        std::process::exit(0);
    }
    if args.iter().any(|a| a == "--version" || a == "-V") {
        println!("ArenaTES3JSON-core 0.4.0");
        std::process::exit(0);
    }

    let command = args[0].clone();
    if !matches!(command.as_str(), "to-json" | "to-plugin" | "inspect") {
        return Err(format!("unknown command: {command}").into());
    }

    let mut positional = Vec::new();
    let mut compact = false;
    let mut encoding = String::from("auto");
    let mut repair_scripts = RepairScriptsMode::Off;
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--compact" | "-c" => compact = true,
            "--encoding" => {
                i += 1;
                let value = args.get(i).ok_or("--encoding requires a value")?;
                encoding = value.to_owned();
                validate_encoding_label(&encoding)?;
            }
            "--repair-scripts" => {
                i += 1;
                let value = args.get(i).ok_or("--repair-scripts requires off|changed|all")?;
                repair_scripts = match value.as_str() {
                    "off" => RepairScriptsMode::Off,
                    "changed" => RepairScriptsMode::Changed,
                    "all" => RepairScriptsMode::All,
                    _ => return Err(format!("unsupported --repair-scripts mode: {value}").into()),
                };
            }
            value if value.starts_with('-') => return Err(format!("unknown option: {value}").into()),
            value => positional.push(PathBuf::from(value)),
        }
        i += 1;
    }

    let required = if command == "inspect" { 1 } else { 2 };
    if positional.len() < required {
        return Err(if command == "inspect" {
            "missing input file".into()
        } else {
            "missing input or output file".into()
        });
    }

    Ok(Cli {
        command,
        input: positional[0].clone(),
        output: positional.get(1).cloned().unwrap_or_default(),
        compact,
        encoding,
        repair_scripts,
    })
}

fn report_progress(percent: u8, stage: &str) {
    eprintln!("AT3J_PROGRESS\t{}\t{}", percent.min(100), stage);
    let _ = io::stderr().flush();
}


fn to_json(cli: &Cli) -> Result<(), Box<dyn std::error::Error>> {
    report_progress(4, "read-plugin");
    let input_size = fs::metadata(&cli.input)?.len();
    let original_bytes = fs::read(&cli.input)?;

    report_progress(12, "parse-plugin");
    let mut plugin = Plugin::new();
    plugin.load_path(&cli.input)?;

    report_progress(34, "analyze-binary-roundtrip");
    let normalized_path = temporary_plugin_path(&cli.input);
    let _ = fs::remove_file(&normalized_path);
    plugin.save_path(&normalized_path)?;
    let normalized_bytes = fs::read(&normalized_path)?;
    let _ = fs::remove_file(&normalized_path);
    let binary_metadata = build_binary_metadata(&original_bytes, &normalized_bytes)?;
    drop(original_bytes);
    drop(normalized_bytes);

    report_progress(52, "serialize-json");
    let mut value = serde_json::to_value(&plugin.objects)?;
    drop(plugin);

    let selected_encoding = resolve_export_encoding(&cli.encoding, &value)?;
    report_progress(65, "decode-text");
    transform_export_value(&mut value, selected_encoding)?;

    attach_header_encoding(&mut value, selected_encoding.name())?;
    attach_script_source_hashes(&mut value)?;
    attach_binary_metadata(&mut value, &binary_metadata)?;

    report_progress(86, "write-json");
    let json = if cli.compact {
        serde_json::to_string(&value)?
    } else {
        serde_json::to_string_pretty(&value)?
    };
    atomic_write(&cli.output, json.as_bytes())?;

    report_progress(100, "done");
    emit_status_extra(
        "to-json",
        "semantic-json-lossless-delta",
        &cli.output,
        input_size,
        json.len() as u64,
        "semantic JSON written with compact binary-preservation metadata",
        Some(selected_encoding.name()),
        None,
    );
    Ok(())
}

fn to_plugin(cli: &Cli) -> Result<(), Box<dyn std::error::Error>> {
    report_progress(4, "read-json");
    let json_text = fs::read_to_string(&cli.input)?;
    let json_input_size = json_text.len() as u64;

    report_progress(12, "parse-json");
    let mut value: Value = serde_json::from_str(&json_text)?;
    drop(json_text);
    let selected_encoding = resolve_import_encoding(&cli.encoding, &value)?;
    let binary_metadata = detach_binary_metadata(&mut value)?;
    let repaired_script_indices = repair_scripts_if_requested(&mut value, cli.repair_scripts)?;
    let repaired_scripts = repaired_script_indices.len();
    remove_arena_metadata(&mut value)?;
    let reference_counts = collect_reference_count_expectations(&value)?;

    report_progress(28, "encode-text");
    transform_import_value(&mut value, selected_encoding)?;

    report_progress(44, "deserialize-plugin");
    let mut plugin = Plugin::new();
    plugin.objects = serde_json::from_value(value)?;

    report_progress(64, "write-plugin");
    plugin.save_path(&cli.output)?;

    report_progress(76, "restore-reference-counts");
    let restored_nam9 = restore_explicit_reference_counts(&cli.output, &reference_counts)?;

    report_progress(84, "restore-binary-details");
    let binary_repairs = apply_binary_metadata_to_file(&cli.output, &binary_metadata, &repaired_script_indices)?;

    report_progress(96, "finish");
    let output_size = fs::metadata(&cli.output)?.len();
    report_progress(100, "done");
    let message = format!(
        "plugin rebuilt; encoding={}; script repairs={}; binary repairs={}; NAM9 restored={}",
        selected_encoding.name(), repaired_scripts, binary_repairs, restored_nam9
    );
    emit_status_extra(
        "to-plugin",
        "semantic-rebuild",
        &cli.output,
        json_input_size,
        output_size,
        &message,
        Some(selected_encoding.name()),
        Some(repaired_scripts as u64),
    );
    Ok(())
}

fn inspect_input(cli: &Cli) -> Result<(), Box<dyn std::error::Error>> {
    let ext = cli.input.extension().and_then(OsStr::to_str).unwrap_or("").to_ascii_lowercase();
    let input_size = fs::metadata(&cli.input)?.len();
    if ext == "json" {
        let text = fs::read_to_string(&cli.input)?;
        let value: Value = serde_json::from_str(&text)?;
        let enc = resolve_import_encoding(&cli.encoding, &value)?;
        let objects = value.as_array().map(|v| v.len()).unwrap_or(0);
        println!("{}", json!({
            "ok": true,
            "command": "inspect",
            "kind": "JSON",
            "size": input_size,
            "encoding": enc.name(),
            "objects": objects,
        }));
        return Ok(());
    }

    let mut plugin = Plugin::new();
    plugin.load_path(&cli.input)?;
    let value = serde_json::to_value(&plugin.objects)?;
    let enc = resolve_export_encoding(&cli.encoding, &value)?;
    println!("{}", json!({
        "ok": true,
        "command": "inspect",
        "kind": ext.to_ascii_uppercase(),
        "size": input_size,
        "encoding": enc.name(),
        "objects": plugin.objects.len(),
    }));
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


fn validate_encoding_label(label: &str) -> Result<(), Box<dyn std::error::Error>> {
    if matches!(label.to_ascii_lowercase().as_str(), "auto" | "raw" | "none") {
        return Ok(());
    }
    if Encoding::for_label(label.as_bytes()).is_some() {
        return Ok(());
    }
    Err(format!("unsupported encoding label: {label}").into())
}

fn resolve_export_encoding(label: &str, value: &Value) -> Result<&'static Encoding, Box<dyn std::error::Error>> {
    let lower = label.to_ascii_lowercase();
    if lower == "raw" || lower == "none" {
        return Ok(WINDOWS_1252);
    }
    if lower != "auto" {
        return Encoding::for_label(label.as_bytes()).ok_or_else(|| format!("unsupported encoding label: {label}").into());
    }
    let mut bytes = Vec::new();
    collect_transport_bytes(value, &mut bytes);
    if bytes.is_empty() {
        return Ok(WINDOWS_1252);
    }
    let mut detector = EncodingDetector::new();
    detector.feed(&bytes, true);
    let guessed = detector.guess(None, true);
    // chardetng can be conservative for Cyrillic game data. Prefer CP1251 when
    // the same bytes decode to a substantial amount of Cyrillic text.
    let (cp1251_text, _, _) = WINDOWS_1251.decode(&bytes);
    let cyr = cp1251_text.chars().filter(|c| ('\u{0400}'..='\u{052F}').contains(c)).count();
    let letters = cp1251_text.chars().filter(|c| c.is_alphabetic()).count();
    if letters > 20 && cyr * 3 > letters {
        return Ok(WINDOWS_1251);
    }
    Ok(guessed)
}

fn resolve_import_encoding(label: &str, value: &Value) -> Result<&'static Encoding, Box<dyn std::error::Error>> {
    let lower = label.to_ascii_lowercase();
    if lower == "raw" || lower == "none" {
        return Ok(WINDOWS_1252);
    }
    if lower != "auto" {
        return Encoding::for_label(label.as_bytes()).ok_or_else(|| format!("unsupported encoding label: {label}").into());
    }
    if let Some(enc) = stored_header_encoding(value) {
        if let Some(found) = Encoding::for_label(enc.as_bytes()) {
            return Ok(found);
        }
    }
    let mut cyr = 0usize;
    let mut greek = 0usize;
    let mut hebrew = 0usize;
    let mut arabic = 0usize;
    scan_unicode_scripts(value, &mut cyr, &mut greek, &mut hebrew, &mut arabic);
    if cyr > 0 { return Ok(WINDOWS_1251); }
    if greek > 0 { return Ok(encoding_rs::WINDOWS_1253); }
    if hebrew > 0 { return Ok(encoding_rs::WINDOWS_1255); }
    if arabic > 0 { return Ok(encoding_rs::WINDOWS_1256); }
    Ok(WINDOWS_1252)
}

fn stored_header_encoding(value: &Value) -> Option<String> {
    value.as_array()?.iter().find_map(|o| {
        if o.get("type").and_then(Value::as_str) == Some("Header") {
            o.get("_arena_encoding").and_then(Value::as_str).map(ToOwned::to_owned)
        } else { None }
    })
}

fn attach_header_encoding(value: &mut Value, name: &str) -> Result<(), Box<dyn std::error::Error>> {
    let objects = value.as_array_mut().ok_or("semantic JSON root must be an array")?;
    if let Some(header) = objects.iter_mut().find(|o| o.get("type").and_then(Value::as_str) == Some("Header")) {
        if let Some(map) = header.as_object_mut() {
            map.insert("_arena_encoding".into(), Value::String(name.to_owned()));
        }
    }
    Ok(())
}

fn scan_unicode_scripts(value: &Value, cyr: &mut usize, greek: &mut usize, hebrew: &mut usize, arabic: &mut usize) {
    match value {
        Value::String(s) => for ch in s.chars() {
            let u = ch as u32;
            if (0x0400..=0x052F).contains(&u) { *cyr += 1; }
            if (0x0370..=0x03FF).contains(&u) { *greek += 1; }
            if (0x0590..=0x05FF).contains(&u) { *hebrew += 1; }
            if (0x0600..=0x06FF).contains(&u) { *arabic += 1; }
        },
        Value::Array(a) => for v in a { scan_unicode_scripts(v,cyr,greek,hebrew,arabic); },
        Value::Object(m) => for (k,v) in m { if !k.starts_with("_arena_") { scan_unicode_scripts(v,cyr,greek,hebrew,arabic); } },
        _ => {}
    }
}

fn collect_transport_bytes(value: &Value, out: &mut Vec<u8>) {
    match value {
        Value::String(s) => {
            if s.len() > 3 && !looks_like_base64(s) {
                for ch in s.chars() {
                    if let Some(b) = transport_char_to_byte(ch) { out.push(b); }
                }
                out.push(b'\n');
            }
        }
        Value::Array(a) => for v in a { collect_transport_bytes(v, out); },
        Value::Object(m) => for (k,v) in m { if !k.starts_with("_arena_") { collect_transport_bytes(v, out); } },
        _ => {}
    }
}

fn looks_like_base64(s: &str) -> bool {
    s.len() >= 8 && s.len() % 4 == 0 && s.bytes().all(|b| b.is_ascii_alphanumeric() || matches!(b,b'+'|b'/'|b'='))
}

fn transport_char_to_byte(c: char) -> Option<u8> {
    if c.is_ascii() { return Some(c as u8); }
    CP1252_HIGH.iter().position(|x| *x == c).map(|i| (i + 0x80) as u8)
}

fn byte_to_transport_char(b: u8) -> char {
    if b < 0x80 { b as char } else { CP1252_HIGH[(b - 0x80) as usize] }
}

fn transform_export_text(input: &str, encoding: &'static Encoding) -> Result<String, Box<dyn std::error::Error>> {
    if encoding == WINDOWS_1252 { return Ok(input.to_owned()); }
    let mut bytes = Vec::with_capacity(input.len());
    for c in input.chars() {
        if let Some(b) = transport_char_to_byte(c) { bytes.push(b); }
        else { return Err(format!("TES3 transport character U+{:04X} cannot be mapped to a raw byte", c as u32).into()); }
    }
    let (decoded, _, _) = encoding.decode(&bytes);
    Ok(decoded.into_owned())
}

fn transform_import_text(input: &str, encoding: &'static Encoding) -> Result<String, Box<dyn std::error::Error>> {
    if encoding == WINDOWS_1252 { return Ok(input.to_owned()); }
    let (encoded, _, had_errors) = encoding.encode(input);
    if had_errors {
        return Err(format!("text contains characters that cannot be encoded as {}: {:?}", encoding.name(), input.chars().take(80).collect::<String>()).into());
    }
    Ok(encoded.iter().map(|b| byte_to_transport_char(*b)).collect())
}

fn transform_export_value(value: &mut Value, encoding: &'static Encoding) -> Result<(), Box<dyn std::error::Error>> {
    match value {
        Value::String(text) => *text = transform_export_text(text, encoding)?,
        Value::Array(items) => for item in items { transform_export_value(item, encoding)?; },
        Value::Object(map) => for (key,item) in map.iter_mut() { if !key.starts_with("_arena_") { transform_export_value(item, encoding)?; } },
        _ => {}
    }
    Ok(())
}

fn transform_import_value(value: &mut Value, encoding: &'static Encoding) -> Result<(), Box<dyn std::error::Error>> {
    match value {
        Value::String(text) => *text = transform_import_text(text, encoding)?,
        Value::Array(items) => for item in items { transform_import_value(item, encoding)?; },
        Value::Object(map) => for (key,item) in map.iter_mut() { if !key.starts_with("_arena_") { transform_import_value(item, encoding)?; } },
        _ => {}
    }
    Ok(())
}

fn emit_status_extra(command: &str, mode: &str, output: &Path, input_size: u64, output_size: u64, message: &str, encoding: Option<&str>, repaired_scripts: Option<u64>) {
    let mut status = json!({
        "ok": true, "command": command, "mode": mode, "output": output.display().to_string(),
        "input_size": input_size, "output_size": output_size, "message": message,
    });
    if let Some(map) = status.as_object_mut() {
        if let Some(e) = encoding { map.insert("encoding".into(), Value::String(e.to_owned())); }
        if let Some(n) = repaired_scripts { map.insert("repaired_scripts".into(), Value::Number(n.into())); }
    }
    println!("{}", status);
}

fn fnv1a64_bytes(bytes: &[u8]) -> String {
    let mut h: u64 = 0xcbf29ce484222325;
    for b in bytes { h ^= *b as u64; h = h.wrapping_mul(0x100000001b3); }
    format!("{h:016x}")
}

fn fnv1a64(text: &str) -> String { fnv1a64_bytes(text.as_bytes()) }

fn attach_script_source_hashes(value: &mut Value) -> Result<(), Box<dyn std::error::Error>> {
    let objects = value.as_array_mut().ok_or("semantic JSON root must be an array")?;
    for o in objects {
        if o.get("type").and_then(Value::as_str) != Some("Script") { continue; }
        let text = o.get("text").and_then(Value::as_str).unwrap_or("");
        let hash = fnv1a64(text);
        if let Some(m) = o.as_object_mut() { m.insert("_arena_source_hash".into(), Value::String(hash)); }
    }
    Ok(())
}

fn parse_script_variables(src: &str) -> (Vec<String>, Vec<String>, Vec<String>) {
    let mut shorts=Vec::new(); let mut longs=Vec::new(); let mut floats=Vec::new(); let mut seen=HashSet::new();
    for line in src.lines() {
        let code = line.split(';').next().unwrap_or("").trim();
        let mut it = code.split_whitespace();
        let ty = it.next().unwrap_or("").to_ascii_lowercase();
        if !matches!(ty.as_str(), "short"|"long"|"float") { continue; }
        let name = it.next().unwrap_or("");
        let mut chars = name.chars();
        let valid_start = chars.next().is_some_and(|c| c.is_ascii_alphabetic() || c == '_');
        let valid_rest = chars.all(|c| c.is_ascii_alphanumeric() || c == '_');
        if !valid_start || !valid_rest || !seen.insert(name.to_ascii_lowercase()) { continue; }
        match ty.as_str() { "short"=>shorts.push(name.to_owned()), "long"=>longs.push(name.to_owned()), _=>floats.push(name.to_owned()) }
    }
    (shorts,longs,floats)
}

fn serde_byte_vec_base64(raw: &[u8]) -> String {
    let mut wrapped=Vec::with_capacity(raw.len()+4);
    wrapped.extend_from_slice(&(raw.len() as u32).to_le_bytes());
    wrapped.extend_from_slice(raw);
    BASE64.encode(wrapped)
}

fn repair_one_script(obj: &mut Value) -> Result<(), Box<dyn std::error::Error>> {
    let text = obj.get("text").and_then(Value::as_str).unwrap_or("").to_owned();
    let (shorts,longs,floats)=parse_script_variables(&text);
    let mut scvr=Vec::new();
    for n in shorts.iter().chain(longs.iter()).chain(floats.iter()) { scvr.extend_from_slice(n.as_bytes()); scvr.push(0); }
    if let Some(map)=obj.as_object_mut() {
        map.insert("variables".into(), Value::String(serde_byte_vec_base64(&scvr)));
        // Lightweight TES3ZER0EDIT-compatible repair: clear stale SCDT and rebuild
        // SCHD/SCVR. A true vanilla TESCS opcode compiler is not embedded.
        map.insert("bytecode".into(), Value::String(serde_byte_vec_base64(&[])));
        let header = map.entry("header").or_insert_with(|| json!({}));
        if let Some(h)=header.as_object_mut() {
            h.insert("num_shorts".into(), Value::Number((shorts.len() as u64).into()));
            h.insert("num_longs".into(), Value::Number((longs.len() as u64).into()));
            h.insert("num_floats".into(), Value::Number((floats.len() as u64).into()));
            h.insert("bytecode_length".into(), Value::Number(0u64.into()));
            h.insert("variables_length".into(), Value::Number((scvr.len() as u64).into()));
        }
    }
    Ok(())
}

fn repair_scripts_if_requested(
    value: &mut Value,
    mode: RepairScriptsMode,
) -> Result<HashSet<usize>, Box<dyn std::error::Error>> {
    let mut repaired = HashSet::new();
    if mode == RepairScriptsMode::Off {
        return Ok(repaired);
    }

    let objects = value.as_array_mut().ok_or("semantic JSON root must be an array")?;
    for (index, object) in objects.iter_mut().enumerate() {
        if object.get("type").and_then(Value::as_str) != Some("Script") {
            continue;
        }

        let current = fnv1a64(object.get("text").and_then(Value::as_str).unwrap_or(""));
        let original = object.get("_arena_source_hash").and_then(Value::as_str);
        let should_repair = match mode {
            RepairScriptsMode::Off => false,
            RepairScriptsMode::All => true,
            // For ArenaTES3JSON-exported JSON we can tell precisely whether SCTX
            // changed. For third-party tes3conv JSON there is no source hash, so
            // Changed mode deliberately leaves bytecode alone rather than guessing.
            RepairScriptsMode::Changed => original.map(|hash| hash != current).unwrap_or(false),
        };

        if should_repair {
            repair_one_script(object)?;
            repaired.insert(index);
        }
    }
    Ok(repaired)
}

fn remove_arena_metadata(value: &mut Value) -> Result<(), Box<dyn std::error::Error>> {
    let objects = value.as_array_mut().ok_or("semantic JSON root must be an array")?;
    for o in objects { if let Some(m)=o.as_object_mut() { m.remove("_arena_encoding"); m.remove("_arena_source_hash"); m.remove("_arena_binary"); } }
    Ok(())
}

#[derive(Clone)]
struct RawSub { tag:[u8;4], payload:Vec<u8> }
#[derive(Clone)]
struct RawRecord { tag:[u8;4], header_tail:[u8;8], subs:Vec<RawSub> }

fn parse_records(data:&[u8]) -> io::Result<Vec<RawRecord>> {
    let mut out=Vec::new(); let mut off=0usize;
    while off<data.len() {
        if data.len()-off<16 { return Err(invalid_data("truncated record header")); }
        let mut tag=[0;4]; tag.copy_from_slice(&data[off..off+4]);
        let size=read_u32_le(data,off+4)? as usize; let end=off+16+size;
        if end>data.len() { return Err(invalid_data("truncated record")); }
        let mut tail=[0;8]; tail.copy_from_slice(&data[off+8..off+16]);
        let spans=parse_subrecords(&data[off+16..end])?;
        let subs=spans.into_iter().map(|sp| RawSub{tag:sp.tag,payload:data[off+16+sp.payload_start..off+16+sp.payload_start+sp.payload_len].to_vec()}).collect();
        out.push(RawRecord{tag,header_tail:tail,subs}); off=end;
    }
    Ok(out)
}

fn write_records(records:&[RawRecord]) -> io::Result<Vec<u8>> {
    let mut out=Vec::new();
    for r in records {
        let body_len:usize=r.subs.iter().map(|s|8+s.payload.len()).sum();
        out.extend_from_slice(&r.tag); out.extend_from_slice(&(u32::try_from(body_len).map_err(|_|invalid_data("record too large"))?).to_le_bytes()); out.extend_from_slice(&r.header_tail);
        for s in &r.subs { out.extend_from_slice(&s.tag); out.extend_from_slice(&(u32::try_from(s.payload.len()).map_err(|_|invalid_data("subrecord too large"))?).to_le_bytes()); out.extend_from_slice(&s.payload); }
    }
    Ok(out)
}

fn tag_string(tag:[u8;4])->String { String::from_utf8_lossy(&tag).to_string() }
fn hex_encode(bytes:&[u8])->String { const H:&[u8;16]=b"0123456789abcdef"; let mut s=String::with_capacity(bytes.len()*2); for b in bytes { s.push(H[(b>>4) as usize] as char); s.push(H[(b&15) as usize] as char); } s }
fn hex_decode(s:&str)->io::Result<Vec<u8>> { if s.len()%2!=0{return Err(invalid_data("odd hex length"));} let b=s.as_bytes(); let mut out=Vec::with_capacity(b.len()/2); for i in (0..b.len()).step_by(2){ let h=(b[i] as char).to_digit(16).ok_or_else(||invalid_data("bad hex"))?; let l=(b[i+1] as char).to_digit(16).ok_or_else(||invalid_data("bad hex"))?; out.push(((h<<4)|l) as u8);} Ok(out) }

fn find_occurrence(subs:&[RawSub], tag:[u8;4], occ:usize)->Option<usize> { let mut n=0; for (i,s) in subs.iter().enumerate(){if s.tag==tag{if n==occ{return Some(i)} n+=1;}} None }
fn parse_tag(s:&str)->io::Result<[u8;4]>{ let b=s.as_bytes(); if b.len()!=4{return Err(invalid_data("tag must be 4 bytes"));} let mut t=[0;4];t.copy_from_slice(b);Ok(t)}

fn build_binary_metadata(original: &[u8], normalized: &[u8]) -> io::Result<Vec<Option<Value>>> {
    let original_records = parse_records(original)?;
    let normalized_records = parse_records(normalized)?;
    if original_records.len() != normalized_records.len() {
        return Err(invalid_data(format!(
            "writer changed record count {} -> {}",
            original_records.len(),
            normalized_records.len()
        )));
    }

    let mut result = Vec::with_capacity(original_records.len());
    for (original_record, normalized_record) in original_records.iter().zip(normalized_records.iter()) {
        if original_record.tag != normalized_record.tag {
            return Err(invalid_data("writer changed record order/type"));
        }

        let mut operations = Vec::new();
        let mut header_changes = Vec::new();
        for index in 0..8 {
            if original_record.header_tail[index] != normalized_record.header_tail[index] {
                header_changes.push(json!([
                    index,
                    normalized_record.header_tail[index],
                    original_record.header_tail[index]
                ]));
            }
        }

        let mut tags = HashSet::<[u8; 4]>::new();
        for sub in &original_record.subs {
            tags.insert(sub.tag);
        }
        for sub in &normalized_record.subs {
            tags.insert(sub.tag);
        }
        let mut tags: Vec<_> = tags.into_iter().collect();
        tags.sort_unstable();

        for tag in tags {
            // CELL/NAM9 is handled from the semantic object_count field below.
            // Keeping it out of generic raw metadata means deleting/changing
            // object_count in JSON is respected instead of being resurrected
            // by an old insert patch.
            if original_record.tag == *b"CELL" && tag == *b"NAM9" {
                continue;
            }

            let original: Vec<_> = original_record
                .subs
                .iter()
                .enumerate()
                .filter(|(_, sub)| sub.tag == tag)
                .collect();
            let normalized: Vec<_> = normalized_record
                .subs
                .iter()
                .enumerate()
                .filter(|(_, sub)| sub.tag == tag)
                .collect();
            let common = original.len().min(normalized.len());

            for occurrence in 0..common {
                let original_payload = &original[occurrence].1.payload;
                let normalized_payload = &normalized[occurrence].1.payload;
                if original_payload == normalized_payload {
                    continue;
                }

                // The semantic writer often normalizes an unterminated string by
                // appending a trailing NUL. Keep a tiny guarded instruction rather
                // than storing the whole original string again.
                if normalized_payload.len() == original_payload.len() + 1
                    && normalized_payload.starts_with(original_payload)
                    && normalized_payload.last() == Some(&0)
                {
                    operations.push(json!({
                        "op": "strip_nul",
                        "tag": tag_string(tag),
                        "occ": occurrence,
                        "from_len": normalized_payload.len(),
                        "from_hash": fnv1a64_bytes(normalized_payload)
                    }));
                } else if original_payload.len() == normalized_payload.len() {
                    let changes: Vec<Value> = original_payload
                        .iter()
                        .zip(normalized_payload.iter())
                        .enumerate()
                        .filter_map(|(index, (original_byte, normalized_byte))| {
                            (original_byte != normalized_byte)
                                .then(|| json!([index, *normalized_byte, *original_byte]))
                        })
                        .collect();
                    operations.push(json!({
                        "op": "bytes",
                        "tag": tag_string(tag),
                        "occ": occurrence,
                        "changes": changes
                    }));
                } else {
                    operations.push(json!({
                        "op": "replace",
                        "tag": tag_string(tag),
                        "occ": occurrence,
                        "from": hex_encode(normalized_payload),
                        "to": hex_encode(original_payload)
                    }));
                }
            }

            // Present in the original but discarded by the semantic writer. This
            // covers empty NPC_:ANAM/ACTI:FNAM/LIGH:MODL and any future equivalent.
            for occurrence in common..original.len() {
                let (index, sub) = original[occurrence];
                operations.push(json!({
                    "op": "insert",
                    "tag": tag_string(tag),
                    "occ": occurrence,
                    "index": index,
                    "payload": hex_encode(&sub.payload)
                }));
            }

            // Invented by the semantic writer although absent in the source. This
            // covers e.g. default NPC_:AIDT and LAND:VTEX blocks.
            for occurrence in common..normalized.len() {
                let (_, sub) = normalized[occurrence];
                operations.push(json!({
                    "op": "remove",
                    "tag": tag_string(tag),
                    "occ": occurrence,
                    "payload": hex_encode(&sub.payload)
                }));
            }
        }

        let original_order: Vec<String> = original_record.subs.iter().map(|s| tag_string(s.tag)).collect();
        let normalized_order: Vec<String> = normalized_record.subs.iter().map(|s| tag_string(s.tag)).collect();
        let order = (original_order != normalized_order).then_some(original_order);

        if operations.is_empty() && header_changes.is_empty() && order.is_none() {
            result.push(None);
        } else {
            let mut metadata = json!({
                "v": 1,
                "record": tag_string(original_record.tag),
                "header": header_changes,
                "subs": operations
            });
            if let (Some(map), Some(order)) = (metadata.as_object_mut(), order) {
                map.insert("order".into(), serde_json::to_value(order).unwrap());
            }
            result.push(Some(metadata));
        }
    }
    Ok(result)
}

fn attach_binary_metadata(
    value: &mut Value,
    metadata: &[Option<Value>],
) -> Result<(), Box<dyn std::error::Error>> {
    let objects = value.as_array_mut().ok_or("semantic JSON root must be an array")?;
    if objects.len() != metadata.len() {
        return Err(format!(
            "object/record count mismatch {} vs {}",
            objects.len(),
            metadata.len()
        )
        .into());
    }
    for (object, item) in objects.iter_mut().zip(metadata) {
        if let (Some(item), Some(map)) = (item, object.as_object_mut()) {
            map.insert("_arena_binary".into(), item.clone());
        }
    }
    Ok(())
}

fn detach_binary_metadata(
    value: &mut Value,
) -> Result<Vec<Option<Value>>, Box<dyn std::error::Error>> {
    let objects = value.as_array_mut().ok_or("semantic JSON root must be an array")?;
    let mut metadata = Vec::with_capacity(objects.len());
    for object in objects {
        metadata.push(object.as_object_mut().and_then(|map| map.remove("_arena_binary")));
    }
    Ok(metadata)
}

fn reorder_subrecords(subs: &mut Vec<RawSub>, order: &[Value]) -> io::Result<bool> {
    if order.len() != subs.len() {
        return Ok(false);
    }

    let mut actual_counts: HashMap<[u8; 4], usize> = HashMap::new();
    for sub in subs.iter() {
        *actual_counts.entry(sub.tag).or_default() += 1;
    }
    let mut desired_tags = Vec::with_capacity(order.len());
    let mut desired_counts: HashMap<[u8; 4], usize> = HashMap::new();
    for item in order {
        let tag = parse_tag(item.as_str().ok_or_else(|| invalid_data("bad subrecord order tag"))?)?;
        desired_tags.push(tag);
        *desired_counts.entry(tag).or_default() += 1;
    }
    if actual_counts != desired_counts {
        return Ok(false);
    }

    let mut buckets: HashMap<[u8; 4], VecDeque<RawSub>> = HashMap::new();
    for sub in subs.drain(..) {
        buckets.entry(sub.tag).or_default().push_back(sub);
    }

    let mut reordered = Vec::with_capacity(desired_tags.len());
    for tag in desired_tags {
        let sub = buckets
            .get_mut(&tag)
            .and_then(|bucket| bucket.pop_front())
            .ok_or_else(|| invalid_data("subrecord order count changed unexpectedly"))?;
        reordered.push(sub);
    }
    *subs = reordered;
    Ok(true)
}

fn apply_binary_metadata_to_file(
    path: &Path,
    metadata: &[Option<Value>],
    skip_record_indices: &HashSet<usize>,
) -> io::Result<usize> {
    if metadata.iter().all(Option::is_none) {
        return Ok(0);
    }

    let bytes = fs::read(path)?;
    let mut records = parse_records(&bytes)?;
    if records.len() != metadata.len() {
        return Err(invalid_data("output record count differs from JSON metadata"));
    }

    let mut repairs = 0usize;
    for (record_index, (record, meta)) in records.iter_mut().zip(metadata).enumerate() {
        if skip_record_indices.contains(&record_index) {
            // Script source was deliberately changed/repaired. Never restore its
            // old SCHD/SCVR/SCDT/SCTX representation over the new script data.
            continue;
        }
        let Some(meta) = meta else { continue };

        if let Some(expected_tag) = meta.get("record").and_then(Value::as_str) {
            if tag_string(record.tag) != expected_tag {
                // User changed the semantic object type/order. Do not apply raw
                // metadata to a different record.
                continue;
            }
        }

        if let Some(changes) = meta.get("header").and_then(Value::as_array) {
            for change in changes {
                let fields = change.as_array().ok_or_else(|| invalid_data("bad header patch"))?;
                if fields.len() != 3 { continue; }
                let Some(index_u64) = fields[0].as_u64() else { continue };
                let Some(from_u64) = fields[1].as_u64() else { continue };
                let Some(to_u64) = fields[2].as_u64() else { continue };
                if index_u64 >= 8 || from_u64 > u8::MAX as u64 || to_u64 > u8::MAX as u64 { continue; }
                let index = index_u64 as usize;
                let from = from_u64 as u8;
                let to = to_u64 as u8;
                if record.header_tail[index] == from {
                    record.header_tail[index] = to;
                    repairs += 1;
                }
            }
        }

        let operations = meta
            .get("subs")
            .and_then(Value::as_array)
            .cloned()
            .unwrap_or_default();

        // Payload repairs are guarded by the writer-produced byte(s). If a user
        // edited the corresponding semantic value, the guard no longer matches and
        // the edit wins instead of being overwritten by raw preservation data.
        for operation in operations.iter().filter(|op| {
            !matches!(op.get("op").and_then(Value::as_str), Some("insert") | Some("remove"))
        }) {
            let kind = operation.get("op").and_then(Value::as_str).unwrap_or("");
            let tag = parse_tag(operation.get("tag").and_then(Value::as_str).unwrap_or("????"))?;
            let occurrence = operation.get("occ").and_then(Value::as_u64).unwrap_or(0) as usize;
            let Some(index) = find_occurrence(&record.subs, tag, occurrence) else { continue };

            match kind {
                "strip_nul" => {
                    let payload = &mut record.subs[index].payload;
                    let same_length = operation.get("from_len").and_then(Value::as_u64)
                        == Some(payload.len() as u64);
                    let same_content = operation.get("from_hash").and_then(Value::as_str)
                        == Some(fnv1a64_bytes(payload).as_str());
                    // Never strip a terminator from a string edited by the user.
                    if same_length && same_content && payload.last() == Some(&0) {
                        payload.pop();
                        repairs += 1;
                    }
                }
                "bytes" => {
                    if let Some(changes) = operation.get("changes").and_then(Value::as_array) {
                        for change in changes {
                            let fields = change.as_array().ok_or_else(|| invalid_data("bad byte patch"))?;
                            if fields.len() != 3 { continue; }
                            let Some(position_u64) = fields[0].as_u64() else { continue };
                            let Some(from_u64) = fields[1].as_u64() else { continue };
                            let Some(to_u64) = fields[2].as_u64() else { continue };
                            if from_u64 > u8::MAX as u64 || to_u64 > u8::MAX as u64 { continue; }
                            let Ok(position) = usize::try_from(position_u64) else { continue };
                            let from = from_u64 as u8;
                            let to = to_u64 as u8;
                            if position < record.subs[index].payload.len()
                                && record.subs[index].payload[position] == from
                            {
                                record.subs[index].payload[position] = to;
                                repairs += 1;
                            }
                        }
                    }
                }
                "replace" => {
                    let from = hex_decode(operation.get("from").and_then(Value::as_str).unwrap_or(""))?;
                    if record.subs[index].payload == from {
                        record.subs[index].payload = hex_decode(
                            operation.get("to").and_then(Value::as_str).unwrap_or("")
                        )?;
                        repairs += 1;
                    }
                }
                _ => {}
            }
        }

        // Remove from the largest occurrence number down so removing one repeated
        // tag cannot shift the occurrence number of the next one.
        let mut removals: Vec<&Value> = operations
            .iter()
            .filter(|op| op.get("op").and_then(Value::as_str) == Some("remove"))
            .collect();
        removals.sort_by_key(|op| std::cmp::Reverse(op.get("occ").and_then(Value::as_u64).unwrap_or(0)));
        for operation in removals {
            let tag = parse_tag(operation.get("tag").and_then(Value::as_str).unwrap_or("????"))?;
            let occurrence = operation.get("occ").and_then(Value::as_u64).unwrap_or(0) as usize;
            let Some(index) = find_occurrence(&record.subs, tag, occurrence) else { continue };
            let expected = hex_decode(operation.get("payload").and_then(Value::as_str).unwrap_or(""))?;
            if record.subs[index].payload == expected {
                record.subs.remove(index);
                repairs += 1;
            }
        }

        let mut inserts: Vec<&Value> = operations
            .iter()
            .filter(|op| op.get("op").and_then(Value::as_str) == Some("insert"))
            .collect();
        inserts.sort_by_key(|op| op.get("index").and_then(Value::as_u64).unwrap_or(u64::MAX));
        for operation in inserts {
            let tag = parse_tag(operation.get("tag").and_then(Value::as_str).unwrap_or("????"))?;
            let occurrence = operation.get("occ").and_then(Value::as_u64).unwrap_or(0) as usize;
            if find_occurrence(&record.subs, tag, occurrence).is_some() {
                continue;
            }
            let index = (operation
                .get("index")
                .and_then(Value::as_u64)
                .unwrap_or(record.subs.len() as u64) as usize)
                .min(record.subs.len());
            let payload = hex_decode(operation.get("payload").and_then(Value::as_str).unwrap_or(""))?;
            record.subs.insert(index, RawSub { tag, payload });
            repairs += 1;
        }

        if let Some(order) = meta.get("order").and_then(Value::as_array) {
            let before: Vec<[u8; 4]> = record.subs.iter().map(|s| s.tag).collect();
            if reorder_subrecords(&mut record.subs, order)? {
                let after: Vec<[u8; 4]> = record.subs.iter().map(|s| s.tag).collect();
                if before != after {
                    repairs += 1;
                }
            }
        }
    }

    if repairs > 0 {
        atomic_write(path, &write_records(&records)?)?;
    }
    Ok(repairs)
}

fn temporary_plugin_path(input:&Path)->PathBuf{
    let mut p=env::temp_dir(); let ext=input.extension().and_then(OsStr::to_str).unwrap_or("esp"); p.push(format!("ArenaTES3JSON-{}-roundtrip.{}",std::process::id(),ext)); p
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
        let unicode = transform_export_text(pseudo, WINDOWS_1251).unwrap();
        assert_eq!(unicode, "Привет, Ёжик!");
        assert_eq!(transform_import_text(&unicode, WINDOWS_1251).unwrap(), pseudo);
    }

    #[test]
    fn cp1251_punctuation_stays_encodable() {
        assert_eq!(transform_import_text("Это честь…", WINDOWS_1251).unwrap(), "Ýòî ÷åñòü…");
    }

    #[test]
    fn supports_multiple_encodings() {
        let enc = Encoding::for_label(b"windows-1250").unwrap();
        let pseudo = transform_import_text("Příliš žluťoučký kůň", enc).unwrap();
        assert_eq!(transform_export_text(&pseudo, enc).unwrap(), "Příliš žluťoučký kůň");
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
    fn raw_record(tag: &[u8; 4], subs: &[(&[u8; 4], &[u8])]) -> Vec<u8> {
        let mut body = Vec::new();
        for (sub_tag, payload) in subs {
            body.extend(subrecord(sub_tag, payload));
        }
        let mut out = Vec::new();
        out.extend_from_slice(tag);
        out.extend_from_slice(&(body.len() as u32).to_le_bytes());
        out.extend_from_slice(&0u32.to_le_bytes());
        out.extend_from_slice(&0u32.to_le_bytes());
        out.extend(body);
        out
    }

    #[test]
    fn binary_delta_restores_empty_subrecords_padding_and_string_termination() {
        let aidt_original = [0u8, 0, 10, 30, 90, 6, 50, 118, 0, 0, 0, 0];
        let aidt_normalized = [0u8, 0, 10, 30, 90, 0, 0, 0, 0, 0, 0, 0];
        let original = raw_record(
            b"NPC_",
            &[(b"NAME", b"npc\0"), (b"ANAM", b"\0"), (b"AIDT", &aidt_original), (b"SCTX", b"begin x")],
        );
        let normalized = raw_record(
            b"NPC_",
            &[(b"NAME", b"npc\0"), (b"AIDT", &aidt_normalized), (b"SCTX", b"begin x\0")],
        );
        let metadata = build_binary_metadata(&original, &normalized).unwrap();
        assert!(metadata[0].is_some());

        let path = env::temp_dir().join(format!("ArenaTES3JSON-test-{}.esp", std::process::id()));
        fs::write(&path, &normalized).unwrap();
        let repaired = apply_binary_metadata_to_file(&path, &metadata, &HashSet::new()).unwrap();
        assert!(repaired > 0);
        assert_eq!(fs::read(&path).unwrap(), original);
        let _ = fs::remove_file(path);
    }


    #[test]
    fn binary_delta_does_not_strip_nul_from_edited_text() {
        let original = raw_record(b"INFO", &[(b"BNAM", b"first")]);
        let normalized = raw_record(b"INFO", &[(b"BNAM", b"first\0")]);
        let edited = raw_record(b"INFO", &[(b"BNAM", b"second\0")]);
        let metadata = build_binary_metadata(&original, &normalized).unwrap();
        let path = env::temp_dir().join(format!(
            "ArenaTES3JSON-text-edit-test-{}.esp", std::process::id()));
        fs::write(&path, &edited).unwrap();
        apply_binary_metadata_to_file(&path, &metadata, &HashSet::new()).unwrap();
        assert_eq!(fs::read(&path).unwrap(), edited);
        let _ = fs::remove_file(path);
    }

    #[test]
    fn changed_script_repair_is_hash_guarded() {
        let original_text = "begin Test\nshort counter\nend";
        let mut value = json!([{
            "type": "Script",
            "id": "Test",
            "header": {
                "num_shorts": 0, "num_longs": 0, "num_floats": 0,
                "bytecode_length": 2, "variables_length": 0
            },
            "variables": serde_byte_vec_base64(&[]),
            "bytecode": serde_byte_vec_base64(&[1, 1]),
            "text": original_text,
            "_arena_source_hash": fnv1a64(original_text)
        }]);

        let mut unchanged_value = value.clone();
        let unchanged = repair_scripts_if_requested(&mut unchanged_value, RepairScriptsMode::Changed).unwrap();
        assert!(unchanged.is_empty());

        value[0]["text"] = Value::String("begin Test\nshort counter\nlong other\nend".into());
        let repaired = repair_scripts_if_requested(&mut value, RepairScriptsMode::Changed).unwrap();
        assert_eq!(repaired.len(), 1);
        assert!(repaired.contains(&0));
        assert_eq!(value[0]["header"]["num_shorts"].as_u64(), Some(1));
        assert_eq!(value[0]["header"]["num_longs"].as_u64(), Some(1));
        assert_eq!(value[0]["header"]["bytecode_length"].as_u64(), Some(0));
        assert_eq!(value[0]["bytecode"].as_str(), Some("AAAAAA=="));
    }

    #[test]
    fn cell_nam9_is_not_duplicated_in_generic_binary_metadata() {
        let mut original_body = Vec::new();
        original_body.extend(subrecord(b"NAME", b"\0"));
        original_body.extend(subrecord(b"FRMR", &1u32.to_le_bytes()));
        original_body.extend(subrecord(b"NAME", b"crate\0"));
        original_body.extend(subrecord(b"NAM9", &1u32.to_le_bytes()));
        original_body.extend(subrecord(b"DATA", &[0u8; 24]));
        let original = one_cell_plugin(&original_body);

        let mut normalized_body = Vec::new();
        normalized_body.extend(subrecord(b"NAME", b"\0"));
        normalized_body.extend(subrecord(b"FRMR", &1u32.to_le_bytes()));
        normalized_body.extend(subrecord(b"NAME", b"crate\0"));
        normalized_body.extend(subrecord(b"DATA", &[0u8; 24]));
        let normalized = one_cell_plugin(&normalized_body);

        let metadata = build_binary_metadata(&original, &normalized).unwrap();
        let subs = metadata[0].as_ref().unwrap().get("subs").and_then(Value::as_array).unwrap();
        assert!(!subs.iter().any(|op| op.get("tag").and_then(Value::as_str) == Some("NAM9")));
    }

    #[test]
    fn binary_delta_preserves_semantic_edits_while_restoring_unknown_bytes() {
        let original_payload = [1u8, 2, 3, 4, 5, 77, 88, 99, 0, 0, 0, 0];
        let normalized_payload = [1u8, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0];
        let edited_payload = [9u8, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0];
        let original = raw_record(b"NPC_", &[(b"AIDT", &original_payload)]);
        let normalized = raw_record(b"NPC_", &[(b"AIDT", &normalized_payload)]);
        let edited = raw_record(b"NPC_", &[(b"AIDT", &edited_payload)]);
        let metadata = build_binary_metadata(&original, &normalized).unwrap();

        let path = env::temp_dir().join(format!("ArenaTES3JSON-edit-test-{}.esp", std::process::id()));
        fs::write(&path, &edited).unwrap();
        apply_binary_metadata_to_file(&path, &metadata, &HashSet::new()).unwrap();
        let result = parse_records(&fs::read(&path).unwrap()).unwrap();
        assert_eq!(result[0].subs[0].payload[0], 9); // semantic edit survives
        assert_eq!(&result[0].subs[0].payload[5..8], &[77, 88, 99]); // raw padding restored
        let _ = fs::remove_file(path);
    }


}
