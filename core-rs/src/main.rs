use serde_json::Value;
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
    "ArenaTES3JSON-core 0.3.0\n\
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
        println!("ArenaTES3JSON-core 0.3.0");
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

    report_progress(30, "encode-windows-1251");
    transform_import_value(&mut value, cli.encoding);

    report_progress(48, "deserialize-plugin");
    let tes3_json = serde_json::to_string(&value)?;
    let mut plugin = Plugin::new();
    plugin.objects = serde_json::from_str(&tes3_json)?;

    report_progress(70, "write-plugin");
    plugin.save_path(&cli.output)?;

    report_progress(96, "finish");
    let output_size = fs::metadata(&cli.output)?.len();
    report_progress(100, "done");
    emit_status(
        "to-plugin",
        "semantic-rebuild",
        &cli.output,
        json.len() as u64,
        output_size,
        "plugin rebuilt from semantic JSON",
    );
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
    fn cp1251_high_table_round_trip() {
        for byte in 0x80u32..=0xFF {
            if byte == 0x98 {
                continue;
            }
            let pseudo = char::from_u32(byte).unwrap();
            let unicode = cp1251_from_pseudo_char(pseudo);
            assert_eq!(pseudo_char_from_cp1251(unicode), Some(pseudo));
        }
    }
}
