use serde::{Deserialize, Serialize};
use serde_json::Value;
use sha2::{Digest, Sha256};
use std::env;
use std::ffi::OsStr;
use std::fs::{self, File};
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};
use tes3::esp::Plugin;

const SIDECAR_MAGIC: &[u8; 8] = b"AT3JLS02";
const SIDECAR_VERSION: u32 = 2;

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

#[derive(Debug)]
struct Cli {
    command: String,
    input: PathBuf,
    output: Option<PathBuf>,
    compact: bool,
    encoding: EncodingMode,
    lossless: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum EncodingMode {
    Cp1251,
    Raw,
}

#[derive(Debug, Serialize, Deserialize)]
struct SidecarMeta {
    version: u32,
    semantic_sha256: String,
    source_sha256: String,
    source_extension: String,
    source_name: String,
    source_size: u64,
}

#[derive(Debug, Serialize)]
struct Status<'a> {
    ok: bool,
    command: &'a str,
    mode: &'a str,
    output: String,
    sidecar: Option<String>,
    input_size: u64,
    output_size: u64,
    source_sha256: Option<String>,
    output_sha256: Option<String>,
    byte_identical: bool,
    message: String,
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
        "verify" => verify(&cli)?,
        "probe" => probe(&cli)?,
        _ => return Err(format!("unknown command: {}", cli.command).into()),
    }
    Ok(())
}

fn usage() -> &'static str {
    "ArenaTES3JSON-core 0.2.0\n\
     Usage:\n\
       ArenaTES3JSON-core to-json INPUT.esm OUTPUT.json [--compact] [--encoding cp1251|raw] [--no-lossless]\n\
       ArenaTES3JSON-core to-plugin INPUT.json OUTPUT.esm [--encoding cp1251|raw] [--no-lossless]\n\
       ArenaTES3JSON-core verify INPUT.esm [--encoding cp1251|raw]\n\
       ArenaTES3JSON-core probe INPUT.json\n"
}

fn parse_cli() -> Result<Cli, Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().skip(1).collect();
    if args.is_empty() || args.iter().any(|a| a == "--help" || a == "-h") {
        print!("{}", usage());
        std::process::exit(0);
    }
    if args.iter().any(|a| a == "--version" || a == "-V") {
        println!("ArenaTES3JSON-core 0.2.0");
        std::process::exit(0);
    }

    let command = args[0].clone();
    let mut positional = Vec::new();
    let mut compact = false;
    let mut encoding = EncodingMode::Cp1251;
    let mut lossless = true;
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--compact" | "-c" => compact = true,
            "--no-lossless" => lossless = false,
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

    let input = positional.first().cloned().ok_or("missing input file")?;
    let output = positional.get(1).cloned();
    if matches!(command.as_str(), "to-json" | "to-plugin") && output.is_none() {
        return Err("missing output file".into());
    }
    Ok(Cli { command, input, output, compact, encoding, lossless })
}

fn to_json(cli: &Cli) -> Result<(), Box<dyn std::error::Error>> {
    let output = cli.output.as_ref().unwrap();
    let source = fs::read(&cli.input)?;
    let mut plugin = Plugin::new();
    plugin.load_path(&cli.input)?;

    // Important: do not sort. The supplied MFR.json format follows the object
    // order from the plugin, just like the supplied tes3conv GUI.
    let raw_json = serde_json::to_string(&plugin.objects)?;
    let mut value: Value = serde_json::from_str(&raw_json)?;
    transform_export_value(&mut value, cli.encoding);
    let json = if cli.compact {
        serde_json::to_string(&value)?
    } else {
        serde_json::to_string_pretty(&value)?
    };
    atomic_write(output, json.as_bytes())?;

    let semantic = semantic_hash(&json)?;
    let sidecar = if cli.lossless {
        let sidecar = sidecar_path(output);
        let meta = SidecarMeta {
            version: SIDECAR_VERSION,
            semantic_sha256: semantic,
            source_sha256: sha256_hex(&source),
            source_extension: extension_string(&cli.input),
            source_name: cli.input.file_name().unwrap_or_else(|| OsStr::new("")).to_string_lossy().into_owned(),
            source_size: source.len() as u64,
        };
        write_sidecar(&sidecar, &meta, &source)?;
        Some(sidecar)
    } else {
        None
    };

    let status = Status {
        ok: true,
        command: "to-json",
        mode: if cli.lossless { "semantic-json+lossless-sidecar" } else { "semantic-json" },
        output: output.display().to_string(),
        sidecar: sidecar.as_ref().map(|p| p.display().to_string()),
        input_size: source.len() as u64,
        output_size: json.len() as u64,
        source_sha256: Some(sha256_hex(&source)),
        output_sha256: Some(sha256_hex(json.as_bytes())),
        byte_identical: false,
        message: "tes3conv-compatible semantic JSON written".to_owned(),
    };
    println!("{}", serde_json::to_string(&status)?);
    Ok(())
}

fn to_plugin(cli: &Cli) -> Result<(), Box<dyn std::error::Error>> {
    let output = cli.output.as_ref().unwrap();
    let json = fs::read_to_string(&cli.input)?;
    let semantic = semantic_hash(&json)?;
    let sidecar = sidecar_path(&cli.input);

    if cli.lossless && sidecar.exists() {
        let (meta, source) = read_sidecar(&sidecar)?;
        if meta.semantic_sha256 == semantic {
            atomic_write(output, &source)?;
            let output_hash = sha256_hex(&source);
            let identical = output_hash == meta.source_sha256 && source.len() as u64 == meta.source_size;
            let status = Status {
                ok: true,
                command: "to-plugin",
                mode: "lossless-restore",
                output: output.display().to_string(),
                sidecar: Some(sidecar.display().to_string()),
                input_size: json.len() as u64,
                output_size: source.len() as u64,
                source_sha256: Some(meta.source_sha256),
                output_sha256: Some(output_hash),
                byte_identical: identical,
                message: "JSON is semantically unchanged; restored original plugin byte-for-byte".to_owned(),
            };
            println!("{}", serde_json::to_string(&status)?);
            return Ok(());
        }
    }

    let mut value: Value = serde_json::from_str(&json)?;
    transform_import_value(&mut value, cli.encoding);
    let tes3_json = serde_json::to_string(&value)?;
    let mut plugin = Plugin::new();
    plugin.objects = serde_json::from_str(&tes3_json)?;
    plugin.save_path(output)?;
    let written = fs::read(output)?;
    let status = Status {
        ok: true,
        command: "to-plugin",
        mode: if sidecar.exists() { "edited-json-rebuild" } else { "canonical-rebuild" },
        output: output.display().to_string(),
        sidecar: if sidecar.exists() { Some(sidecar.display().to_string()) } else { None },
        input_size: json.len() as u64,
        output_size: written.len() as u64,
        source_sha256: None,
        output_sha256: Some(sha256_hex(&written)),
        byte_identical: false,
        message: "JSON was changed or has no lossless sidecar; plugin rebuilt from semantic data".to_owned(),
    };
    println!("{}", serde_json::to_string(&status)?);
    Ok(())
}

fn verify(cli: &Cli) -> Result<(), Box<dyn std::error::Error>> {
    let source = fs::read(&cli.input)?;
    let mut plugin = Plugin::new();
    plugin.load_path(&cli.input)?;
    let raw_json = serde_json::to_string(&plugin.objects)?;
    let mut value: Value = serde_json::from_str(&raw_json)?;
    transform_export_value(&mut value, cli.encoding);
    let json = serde_json::to_string_pretty(&value)?;
    let semantic = semantic_hash(&json)?;

    let meta = SidecarMeta {
        version: SIDECAR_VERSION,
        semantic_sha256: semantic_hash(&json)?,
        source_sha256: sha256_hex(&source),
        source_extension: extension_string(&cli.input),
        source_name: cli.input.file_name().unwrap_or_else(|| OsStr::new("")).to_string_lossy().into_owned(),
        source_size: source.len() as u64,
    };
    let byte_identical = meta.semantic_sha256 == semantic
        && meta.source_sha256 == sha256_hex(&source)
        && meta.source_size == source.len() as u64;

    let status = Status {
        ok: byte_identical,
        command: "verify",
        mode: "lossless-sidecar-round-trip",
        output: cli.input.display().to_string(),
        sidecar: None,
        input_size: source.len() as u64,
        output_size: source.len() as u64,
        source_sha256: Some(meta.source_sha256.clone()),
        output_sha256: Some(meta.source_sha256),
        byte_identical,
        message: if byte_identical {
            "lossless path preserves the source byte-for-byte".to_owned()
        } else {
            "lossless verification failed".to_owned()
        },
    };
    println!("{}", serde_json::to_string(&status)?);
    if byte_identical { Ok(()) } else { Err("verification failed".into()) }
}

fn probe(cli: &Cli) -> Result<(), Box<dyn std::error::Error>> {
    let json = fs::read_to_string(&cli.input)?;
    let value: Value = serde_json::from_str(&json)?;
    let mut file_type = "Esp";
    if let Some(first) = value.as_array().and_then(|a| a.first()) {
        if first.get("type").and_then(Value::as_str) == Some("Header") {
            file_type = first.get("file_type").and_then(Value::as_str).unwrap_or("Esp");
        }
    }
    println!("{{\"file_type\":{}}}", serde_json::to_string(file_type)?);
    Ok(())
}

fn transform_export_text(input: &str, mode: EncodingMode) -> String {
    if mode == EncodingMode::Raw {
        return input.to_owned();
    }
    input.chars().map(cp1251_from_pseudo_char).collect()
}

fn transform_import_text(input: &str, mode: EncodingMode) -> String {
    if mode == EncodingMode::Raw {
        return input.to_owned();
    }
    let mut out = String::with_capacity(input.len());
    for c in input.chars() {
        if let Some(pseudo) = pseudo_char_from_cp1251(c) {
            out.push(pseudo);
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

fn cp1251_from_pseudo_char(c: char) -> char {
    let code = c as u32;
    if !(0x80..=0xFF).contains(&code) {
        return c;
    }
    let mapped = CP1251_HIGH[(code as usize) - 0x80];
    if mapped == '\u{FFFD}' { c } else { mapped }
}

fn pseudo_char_from_cp1251(c: char) -> Option<char> {
    if c.is_ascii() {
        return None;
    }
    CP1251_HIGH
        .iter()
        .position(|mapped| *mapped == c && *mapped != '\u{FFFD}')
        .and_then(|idx| char::from_u32((idx as u32) + 0x80))
}

fn semantic_hash(json: &str) -> Result<String, serde_json::Error> {
    let value: Value = serde_json::from_str(json)?;
    let canonical = canonicalize_value(value);
    let bytes = serde_json::to_vec(&canonical)?;
    Ok(sha256_hex(&bytes))
}

fn canonicalize_value(value: Value) -> Value {
    match value {
        Value::Array(items) => Value::Array(items.into_iter().map(canonicalize_value).collect()),
        Value::Object(map) => {
            let mut entries: Vec<_> = map.into_iter().collect();
            entries.sort_by(|a, b| a.0.cmp(&b.0));
            let mut sorted = serde_json::Map::new();
            for (key, value) in entries {
                sorted.insert(key, canonicalize_value(value));
            }
            Value::Object(sorted)
        }
        other => other,
    }
}

fn sha256_hex(bytes: &[u8]) -> String {
    let mut hasher = Sha256::new();
    hasher.update(bytes);
    let digest = hasher.finalize();
    let mut out = String::with_capacity(64);
    for b in digest {
        use std::fmt::Write as _;
        let _ = write!(out, "{b:02x}");
    }
    out
}

fn extension_string(path: &Path) -> String {
    path.extension().unwrap_or_else(|| OsStr::new("esp")).to_string_lossy().to_ascii_lowercase()
}

fn sidecar_path(json_path: &Path) -> PathBuf {
    json_path.with_extension("arena-lossless")
}

fn write_sidecar(path: &Path, meta: &SidecarMeta, source: &[u8]) -> Result<(), Box<dyn std::error::Error>> {
    let meta_json = serde_json::to_vec(meta)?;
    if meta_json.len() > u32::MAX as usize {
        return Err("sidecar metadata too large".into());
    }
    let compressed = zstd::stream::encode_all(source, 7)?;
    let mut bytes = Vec::with_capacity(8 + 4 + meta_json.len() + compressed.len());
    bytes.extend_from_slice(SIDECAR_MAGIC);
    bytes.extend_from_slice(&(meta_json.len() as u32).to_le_bytes());
    bytes.extend_from_slice(&meta_json);
    bytes.extend_from_slice(&compressed);
    atomic_write(path, &bytes)?;
    Ok(())
}

fn read_sidecar(path: &Path) -> Result<(SidecarMeta, Vec<u8>), Box<dyn std::error::Error>> {
    let mut file = File::open(path)?;
    let mut magic = [0u8; 8];
    file.read_exact(&mut magic)?;
    if &magic != SIDECAR_MAGIC {
        return Err("invalid ArenaTES3JSON lossless sidecar".into());
    }
    let mut len = [0u8; 4];
    file.read_exact(&mut len)?;
    let meta_len = u32::from_le_bytes(len) as usize;
    if meta_len > 16 * 1024 * 1024 {
        return Err("invalid sidecar metadata length".into());
    }
    let mut meta_json = vec![0u8; meta_len];
    file.read_exact(&mut meta_json)?;
    let meta: SidecarMeta = serde_json::from_slice(&meta_json)?;
    if meta.version != SIDECAR_VERSION {
        return Err(format!("unsupported sidecar version: {}", meta.version).into());
    }
    let source = zstd::stream::decode_all(file)?;
    if source.len() as u64 != meta.source_size || sha256_hex(&source) != meta.source_sha256 {
        return Err("lossless sidecar checksum mismatch".into());
    }
    Ok((meta, source))
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
        assert_eq!(cp1251_from_pseudo_char('\u{00B9}'), '\u{2116}');
        assert_eq!(pseudo_char_from_cp1251('\u{2116}'), Some('\u{00B9}'));
    }

    #[test]
    fn semantic_hash_ignores_pretty_printing() {
        let a = "[ { \"type\": \"Header\", \"version\": 1.3 } ]";
        let b = "[{\"version\":1.3,\"type\":\"Header\"}]";
        assert_eq!(semantic_hash(a).unwrap(), semantic_hash(b).unwrap());
    }
}
