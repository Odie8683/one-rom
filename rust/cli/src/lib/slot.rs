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
use onerom_gen::SizeHandling;
use serde_json::{Value, json};

const DEFAULT_CONFIG_DESCRIPTION: &str = "Created by the One ROM CLI";
const CONFIG_SCHEMA_URL: &str = "https://images.onerom.org/configs/schema.json";

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
}

/// Parse a CS logic value, accepting active_low/0 and active_high/1.
fn parse_cs_logic(value: &str) -> Result<String, Error> {
    match value {
        "active_low" | "0" => Ok("active_low".to_string()),
        "active_high" | "1" => Ok("active_high".to_string()),
        other => Err(Error::InvalidArgument(format!(
            "invalid CS logic '{other}': expected active_low, active_high, 0, or 1"
        ))),
    }
}

// Use the SizeHandling deserialization to validate the value and get a
// normalized string.
fn parse_size_handling(value: &str) -> Result<String, Error> {
    serde_json::from_str::<SizeHandling>(&format!("\"{value}\""))
        .map(|sh| {
            serde_json::to_string(&sh)
                .unwrap()
                .trim_matches('"')
                .to_string()
        })
        .map_err(|_| {
            let variants_err = serde_json::from_str::<SizeHandling>("\"?\"").unwrap_err();
            Error::InvalidArgument(format!("invalid size_handling '{value}': {variants_err}"))
        })
}

/// Parse a single `--slot` string into a [`SlotSpec`], validating against the given board.
fn parse_slot(s: &str, board: &Board) -> Result<SlotSpec, Error> {
    let mut file = None;
    let mut chip_type_str = None;
    let mut cs1 = None;
    let mut cs2 = None;
    let mut cs3 = None;
    let mut size_handling = None;

    for part in s.split(',') {
        let (key, value) = part.split_once('=').ok_or_else(|| {
            Error::InvalidArgument(format!("invalid slot key=value pair '{part}'"))
        })?;
        match key.trim() {
            "file" => file = Some(expand_tilde(value).into_owned()),
            "type" => chip_type_str = Some(value.to_string()),
            "cs1" => cs1 = Some(parse_cs_logic(value)?),
            "cs2" => cs2 = Some(parse_cs_logic(value)?),
            "cs3" => cs3 = Some(parse_cs_logic(value)?),
            "size_handling" | "size" => size_handling = Some(parse_size_handling(value)?),
            other => {
                return Err(Error::InvalidArgument(format!(
                    "unknown slot key '{other}'"
                )));
            }
        }
    }

    let chip_type_str = chip_type_str
        .ok_or_else(|| Error::InvalidArgument("slot missing 'type' key".to_string()))?;
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
            "slot missing 'file' key for ROM chip".to_string(),
        ));
    }

    validate_cs_lines(&chip_type, cs1.as_deref(), cs2.as_deref(), cs3.as_deref())?;

    Ok(SlotSpec {
        file,
        chip_type,
        cs1,
        cs2,
        cs3,
        size_handling,
    })
}

/// Validate that CS lines are provided for all configurable control lines,
/// and not provided for fixed active-low lines.
fn validate_cs_lines(
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
                return Err(Error::InvalidArgument(format!(
                    "chip type {} requires {} to be specified",
                    chip_type.name(),
                    line.name
                )));
            }
            ControlLineType::FixedActiveLow if supplied => {
                return Err(Error::InvalidArgument(format!(
                    "chip type {} has fixed active-low {}, do not specify it",
                    chip_type.name(),
                    line.name
                )));
            }
            _ => {}
        }
    }

    for (cs_name, cs_val) in &cs_values {
        if cs_val.is_some() && !chip_type.control_lines().iter().any(|l| l.name == *cs_name) {
            return Err(Error::InvalidArgument(format!(
                "chip type {} has no {} line",
                chip_type.name(),
                cs_name
            )));
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

/// Generate a One ROM JSON configuration string from a list of slot specs.
pub fn slots_to_config_json(
    slots: &[SlotSpec],
    name: Option<&str>,
    description: Option<&str>,
) -> Result<String, Error> {
    let chip_sets: Vec<Value> = slots
        .iter()
        .map(|slot| {
            json!({
                "type": "single",
                "chips": [slot_to_chip_json(slot)]
            })
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
