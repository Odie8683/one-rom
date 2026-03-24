// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

//! Slot string parsing and ROM configuration JSON generation.
//!
//! Handles parsing of `--slot file=...,type=...,cs1=...` arguments and
//! converting them into a One ROM JSON configuration suitable for the builder.

use crate::Error;
use onerom_config::chip::{CHIP_TYPE_NAMES_PLUGINS, ChipFunction, ChipType, ControlLineType};
use onerom_config::hw::Board;
use onerom_gen::{
    SizeHandling,
    firmware::{FireCpuFreq, FireVreg},
};
use serde_json::{Value, json};

const DEFAULT_CONFIG_DESCRIPTION: &str = "Created by the One ROM CLI";
const CONFIG_SCHEMA_URL: &str = "https://images.onerom.org/configs/schema.json";

/// The result of checking whether any slot specifications require user
/// confirmation before proceeding.
pub struct ConfirmationsRequired {
    /// True if any slot has a CPU frequency above the stock threshold.
    pub cpu_freq: bool,
    /// True if any slot has a vreg above the stock threshold.
    pub vreg: bool,
}

/// Check whether any slot specifications require user confirmation.
///
/// The caller should inspect the returned flags and prompt the user
/// accordingly before proceeding to build the firmware. The `--yes`
/// flag suppresses both prompts.
pub fn check_confirmations(slots: &[SlotSpec]) -> ConfirmationsRequired {
    ConfirmationsRequired {
        cpu_freq: slots.iter().any(|s| {
            s.cpu_freq
                .map(|f| f > FireCpuFreq::stock_value())
                .unwrap_or(false)
        }),
        vreg: slots.iter().any(|s| {
            s.vreg
                .as_ref()
                .map(|v| *v > FireVreg::stock_value())
                .unwrap_or(false)
        }),
    }
}

/// Parse slot strings and check whether any require user confirmation.
///
/// Slots are parsed purely for validation and confirmation checking.
/// The caller should prompt as needed before proceeding.
pub fn check_slot_confirmations(
    slots: &[String],
    board: &Board,
) -> Result<ConfirmationsRequired, Error> {
    let parsed = parse_slots(slots, board)?;
    Ok(check_confirmations(&parsed))
}

// Handle tilde expansion for file paths in slot specifications, since these
// are passed directly to the builder as-is and won't be expanded by the
// shell.
fn expand_tilde(path: &str) -> std::borrow::Cow<'_, str> {
    if let Some(rest) = path.strip_prefix("~/")
        && let Some(home) = std::env::var_os("HOME")
    {
        format!("{}/{}", home.to_string_lossy(), rest).into()
    } else {
        path.into()
    }
}

/// Parsed and validated slot specification from a `--slot` argument.
pub struct SlotSpec {
    pub file: Option<String>,
    pub chip_type: ChipType,
    pub cs1: Option<String>,
    pub cs2: Option<String>,
    pub cs3: Option<String>,
    size_handling: Option<String>,
    pub cpu_freq: Option<FireCpuFreq>,
    pub vreg: Option<FireVreg>,
    pub led: Option<bool>,
    pub force_16bit: Option<bool>,
}

/// Parse a CS logic value, accepting active_low/0 and active_high/1.
fn parse_cs_logic(slot: &str, key: &str, value: &str) -> Result<String, Error> {
    match value {
        "active_low" | "0" => Ok("active_low".to_string()),
        "active_high" | "1" => Ok("active_high".to_string()),
        other => Err(Error::InvalidArgument(
            "--slot".to_string(),
            format!(
                "Invalid CS logic '{other}': expected {key}=active_low|active_high|0|1\n   --slot '{slot}'"
            ),
        )),
    }
}

// Use the SizeHandling deserialization to validate the value and get a
// normalized string.
fn parse_size_handling(slot: &str, _key: &str, value: &str) -> Result<String, Error> {
    serde_json::from_str::<SizeHandling>(&format!("\"{value}\""))
        .map(|sh| {
            serde_json::to_string(&sh)
                .unwrap()
                .trim_matches('"')
                .to_string()
        })
        .map_err(|_| {
            let variants_err = serde_json::from_str::<SizeHandling>("\"?\"").unwrap_err();
            let supported_variants = SizeHandling::supported_values()
                .iter()
                .map(|v| {
                    serde_json::to_string(v)
                        .unwrap()
                        .trim_matches('"')
                        .to_string()
                })
                .collect::<Vec<_>>()
                .join(", ");
            Error::InvalidArgument(
                "--slot".to_string(),
                format!("Invalid size_handling '{value}': {variants_err}\n    --slot '{slot}'\n  Supported values {supported_variants}"),
            )
        })
}

fn parse_bool(slot: &str, key: &str, value: &str) -> Result<bool, Error> {
    match value.to_lowercase().as_str() {
        "true" | "on" | "1" => Ok(true),
        "false" | "off" | "0" => Ok(false),
        other => Err(Error::InvalidArgument(
            "--slot".to_string(),
            format!(
                "Invalid boolean '{other}': expected {key}=true|false|on|off|1|0\n    --slot '{slot}'"
            ),
        )),
    }
}

fn parse_cpu_freq(slot: &str, key: &str, value: &str) -> Result<FireCpuFreq, Error> {
    let digits = if value.to_lowercase().ends_with("mhz") {
        &value[..value.len() - 3]
    } else {
        value
    };
    let mhz = digits.parse::<u16>().map_err(|_| {
        Error::InvalidArgument(
            "--slot".to_string(),
            format!("Invalid CPU frequency '{value}': expected formats {key}=150|150MHz\n    --slot '{slot}'"),
        )
    })?;
    FireCpuFreq::mhz(mhz).map_err(|_| {
        Error::InvalidArgument(
            "--slot".to_string(),
            format!(
                "CPU frequency {mhz}MHz out of range ({}-{}MHz)\n    --slot '{slot}'",
                FireCpuFreq::MIN_MHZ,
                FireCpuFreq::MAX_MHZ,
            ),
        )
    })
}

fn parse_vreg(slot: &str, key: &str, value: &str) -> Result<FireVreg, Error> {
    let stripped = if value.ends_with('v') || value.ends_with('V') {
        &value[..value.len() - 1]
    } else {
        value
    };
    let canonical = match stripped.split_once('.') {
        Some((int, frac)) => {
            let padded = format!("{frac:0<2}");
            if padded.len() > 2 {
                return Err(Error::InvalidArgument(
                    "--slot".to_string(),
                    format!(
                        "Invalid VReg '{value}': too many decimal places, max 2\n    --slot '{slot}'"
                    ),
                ));
            }
            format!("{int}.{padded}V")
        }
        None => {
            return Err(Error::InvalidArgument(
                "--slot".to_string(),
                format!(
                    "Invalid VReg '{value}': expected format {key}=1.1|1.10|1.10V\n    --slot '{slot}'"
                ),
            ));
        }
    };
    serde_json::from_str::<FireVreg>(&format!("\"{canonical}\"")).map_err(|_| {
        let levels = FireVreg::supported_levels()
            .iter()
            .map(|v| {
                serde_json::to_string(v)
                    .unwrap()
                    .trim_matches('"')
                    .to_string()
            })
            .collect::<Vec<_>>()
            .join(", ");
        Error::InvalidArgument(
            "--slot".to_string(),
            format!(
                "Unsupported VReg '{value}'\n    --slot '{slot}'\n  Supported levels: {levels}"
            ),
        )
    })
}

const SLOT_KEYS: &[&str] = &[
    "file",
    "type",
    "cs1",
    "cs2",
    "cs3",
    "size_handling",
    "size",
    "cpu-freq",
    "cpu-vreg",
    "led",
    "force_16bit",
];

/// Parse a single `--slot` string into a [`SlotSpec`], validating against the given board.
fn parse_slot(slot: &str, board: &Board) -> Result<SlotSpec, Error> {
    let mut file = None;
    let mut chip_type_str = None;
    let mut cs1 = None;
    let mut cs2 = None;
    let mut cs3 = None;
    let mut size_handling = None;
    let mut cpu_freq = None;
    let mut vreg = None;
    let mut led = None;
    let mut force_16bit = None;

    //
    // Parse
    //
    let mut seen = std::collections::HashSet::new();
    for part in slot.split(',') {
        let (key, value) = part.split_once('=').ok_or_else(|| {
            Error::InvalidArgument("--slot".to_string(), format!("Slot key '{part}' is missing a value - expected '{part}=<value>'\n    --slot '{slot}'"))
        })?;
        let key = key.trim();
        if !seen.insert(key) {
            return Err(Error::InvalidArgument(
                "--slot".to_string(),
                format!("Duplicate slot key '{key}' found.\n    --slot '{slot}'"),
            ));
        }
        match key {
            "file" | "path" | "url" => file = Some(expand_tilde(value).into_owned()),
            "type" | "rom-type" | "rom_type" | "chip_type" | "chip-type" => {
                chip_type_str = Some(value.to_string())
            }
            "cs1" => cs1 = Some(parse_cs_logic(slot, key, value)?),
            "cs2" => cs2 = Some(parse_cs_logic(slot, key, value)?),
            "cs3" => cs3 = Some(parse_cs_logic(slot, key, value)?),
            "size_handling" | "size" => {
                size_handling = Some(parse_size_handling(slot, key, value)?)
            }
            "cpu" | "freq" | "frequency" | "cpu-freq" | "cpu_freq" | "cpu_frequency"
            | "cpu-frequency" => cpu_freq = Some(parse_cpu_freq(slot, key, value)?),
            "vreg" | "cpu-vreg" | "cpu_vreg" => vreg = Some(parse_vreg(slot, key, value)?),
            "led" | "status_led" | "status-led" => led = Some(parse_bool(slot, key, value)?),
            "16bit" | "force_16bit" | "force_16_bit" | "force-16bit" | "force-16-bit" => {
                force_16bit = Some(parse_bool(slot, key, value)?)
            }
            other => {
                let supported_keys = SLOT_KEYS.join(", ");
                return Err(Error::InvalidArgument(
                    "--slot".to_string(),
                    format!(
                        "Unrecognised slot key '{other}'\n    --slot '{slot}'\n  Supported keys: {supported_keys}"
                    ),
                ));
            }
        }
    }

    //
    // Validate
    //
    let chip_type_str = chip_type_str.ok_or_else(|| {
        Error::InvalidArgument(
            "--slot".to_string(),
            format!("slot missing 'type' key\n    --slot '{slot}'"),
        )
    })?;
    let chip_type = ChipType::try_from_str(&chip_type_str).ok_or_else(|| {
        let supported = supported_chip_names_for_board(board);
        Error::UnsupportedChipType(chip_type_str.clone(), supported)
    })?;

    if !board.supports_chip_type(chip_type) {
        let supported = supported_chip_names_for_board(board);
        return Err(Error::UnsupportedBoardChipType(
            chip_type.name().to_string(),
            chip_type.aliases().join(", "),
            supported,
        ));
    }

    if chip_type.chip_function() != ChipFunction::Ram && file.is_none() {
        return Err(Error::InvalidArgument(
            "--slot".to_string(),
            format!("Missing 'file' key for ROM chip.\n    --slot '{slot}'"),
        ));
    }

    validate_cs_lines(
        slot,
        &chip_type,
        cs1.as_deref(),
        cs2.as_deref(),
        cs3.as_deref(),
    )?;

    if force_16bit.is_some() && board.chip_pins() != 40 {
        return Err(Error::InvalidArgument(
            "--slot".to_string(),
            format!("force_16bit is only valid on 40-pin boards\n    --slot '{slot}'"),
        ));
    }

    Ok(SlotSpec {
        file,
        chip_type,
        cs1,
        cs2,
        cs3,
        size_handling,
        cpu_freq,
        vreg,
        led,
        force_16bit,
    })
}

/// Validate that CS lines are provided for all configurable control lines,
/// and not provided for fixed active-low lines.
fn validate_cs_lines(
    slot: &str,
    chip_type: &ChipType,
    cs1: Option<&str>,
    cs2: Option<&str>,
    cs3: Option<&str>,
) -> Result<(), Error> {
    let cs_values = [("cs1", cs1), ("cs2", cs2), ("cs3", cs3)];

    for line in chip_type.control_lines() {
        let supplied = cs_values
            .iter()
            .find(|(name, _)| *name == line.name)
            .map(|(_, v)| v.is_some())
            .unwrap_or(false);

        match line.line_type {
            ControlLineType::Configurable if !supplied => {
                return Err(Error::InvalidArgument(
                    "--slot".to_string(),
                    format!(
                        "Chip type {} requires {} to be specified\n    --slot '{slot}'",
                        chip_type.name(),
                        line.name
                    ),
                ));
            }
            ControlLineType::FixedActiveLow if supplied => {
                return Err(Error::InvalidArgument(
                    "--slot".to_string(),
                    format!(
                        "Chip type {} has fixed active-low {}, do not specify it\n    --slot '{slot}'",
                        chip_type.name(),
                        line.name
                    ),
                ));
            }
            _ => {}
        }
    }

    for (cs_name, cs_val) in &cs_values {
        if cs_val.is_some() && !chip_type.control_lines().iter().any(|l| l.name == *cs_name) {
            return Err(Error::InvalidArgument(
                "--slot".to_string(),
                format!(
                    "Chip type {} has no {} line\n    --slot '{slot}'",
                    chip_type.name(),
                    cs_name
                ),
            ));
        }
    }

    Ok(())
}

/// Build a human-readable sorted list of chip type names supported by a board,
/// including plugins.
pub fn supported_chip_names_for_board(board: &Board) -> String {
    let mut names: Vec<&str> = board.supported_chip_type_names().to_vec();
    names.extend_from_slice(CHIP_TYPE_NAMES_PLUGINS);
    names.sort_unstable();
    names.join(", ")
}

/// Parse all `--slot` strings against a resolved board, returning a vec of
/// [`SlotSpec`] or the first error.
pub fn parse_slots(slots: &[String], board: &Board) -> Result<Vec<SlotSpec>, Error> {
    slots.iter().map(|s| parse_slot(s, board)).collect()
}

/// Build a chip JSON object from a [`SlotSpec`].
fn slot_to_chip_json(slot: &SlotSpec) -> Value {
    let mut chip = json!({
        "type": slot.chip_type.name(),
    });

    let obj = chip.as_object_mut().unwrap();
    if let Some(file) = &slot.file {
        obj.insert("file".to_string(), json!(file));
    }
    if let Some(cs1) = &slot.cs1 {
        obj.insert("cs1".to_string(), json!(cs1));
    }
    if let Some(cs2) = &slot.cs2 {
        obj.insert("cs2".to_string(), json!(cs2));
    }
    if let Some(cs3) = &slot.cs3 {
        obj.insert("cs3".to_string(), json!(cs3));
    }
    if let Some(sh) = &slot.size_handling {
        obj.insert("size_handling".to_string(), json!(sh));
    }

    chip
}

/// Build a firmware_overrides JSON object from a [`SlotSpec`], if any overrides
/// are set. Returns None if no overrides are present.
fn slot_to_firmware_overrides_json(slot: &SlotSpec) -> Option<Value> {
    let has_fire = slot.cpu_freq.is_some() || slot.vreg.is_some() || slot.force_16bit.is_some();
    let has_led = slot.led.is_some();

    if !has_fire && !has_led {
        return None;
    }

    let mut overrides = json!({});
    let obj = overrides.as_object_mut().unwrap();

    if has_fire {
        let mut fire = json!({});
        let fire_obj = fire.as_object_mut().unwrap();
        if let Some(freq) = slot.cpu_freq {
            fire_obj.insert("cpu_freq".to_string(), json!(format!("{}MHz", freq.get())));
            fire_obj.insert(
                "overclock".to_string(),
                json!(freq > FireCpuFreq::stock_value()),
            );
        }
        if let Some(vreg) = &slot.vreg {
            let vreg_str = serde_json::to_string(vreg)
                .unwrap()
                .trim_matches('"')
                .to_string();
            fire_obj.insert("vreg".to_string(), json!(vreg_str));
        }
        if let Some(f) = slot.force_16bit {
            fire_obj.insert("force_16_bit".to_string(), json!(f));
        }
        obj.insert("fire".to_string(), fire);
    }

    if let Some(enabled) = slot.led {
        obj.insert("led".to_string(), json!({ "enabled": enabled }));
    }

    Some(overrides)
}

/// Generate a One ROM JSON configuration string from a list of slot specs.
pub fn slots_to_config_json(
    slots: &[SlotSpec],
    name: Option<&str>,
    description: Option<&str>,
) -> Result<String, Error> {
    let chip_sets: Vec<Value> = slots
        .iter()
        .map(|slot| {
            let mut chip_set = json!({
                "type": "single",
                "chips": [slot_to_chip_json(slot)]
            });
            if let Some(fw) = slot_to_firmware_overrides_json(slot) {
                chip_set
                    .as_object_mut()
                    .unwrap()
                    .insert("firmware_overrides".to_string(), fw);
            }
            chip_set
        })
        .collect();

    let mut config = json!({
        "$schema": CONFIG_SCHEMA_URL,
        "version": 1,
        "description": description.unwrap_or(DEFAULT_CONFIG_DESCRIPTION),
        "chip_sets": chip_sets,
    });

    if let Some(name) = name {
        config
            .as_object_mut()
            .unwrap()
            .insert("name".to_string(), json!(name));
    }

    serde_json::to_string_pretty(&config).map_err(|e| Error::Other(e.to_string()))
}

/// Save a config JSON string to a file.
pub fn save_config(path: &str, json: &str) -> Result<(), Error> {
    std::fs::write(path, json).map_err(|e| Error::io(path, e))
}
