// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

//! Plugin specification parsing, manifest structs, and ROM configuration
//! JSON generation.
//!
//! # Plugin slot positions
//!
//! Plugin slot positions within the firmware image are fixed:
//!   - System plugin: always chip_set 0
//!   - User plugin:   always chip_set 1
//!   - ROM images:    chip_set 2 onwards
//!
//! A user plugin requires a system plugin to be present. At most one of each
//! type is supported per firmware image.
//!
//! # Processing pipeline
//!
//! Plugin specs are processed in two phases:
//!
//! ## Phase 1 - Parse ([`parse_plugins`])
//!
//! Converts raw `--plugin` strings into [`PluginSpec`] values. This phase
//! validates syntax and performs best-effort semantic checks for specs where
//! the plugin type is already known (i.e. `system/usb` or `user/foo` forms).
//! It cannot fully validate specs where the type is not yet known (bare name
//! or file= forms), as the type must be resolved from the manifest or binary
//! header first.
//!
//! ## Phase 2 - Resolution (caller responsibility)
//!
//! For [`PluginSpec::Named`] specs, the caller fetches the plugin manifest to
//! resolve the binary URL and confirm the type. For [`PluginSpec::File`]
//! specs, the caller fetches the binary and parses its header to determine the
//! type.
//!
//! Once all types are known, the caller must call
//! [`validate_resolved_plugin_types`] to perform the full semantic check
//! (no duplicates, user requires system).
//!
//! After resolution, [`plugin_to_chip_set_json`] produces the chip_set JSON
//! entry for each plugin, ready to be prepended to the chip_sets array before
//! the ROM slot entries.

use onerom_config::fw::FirmwareVersion;
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};

use crate::Error;

/// Base URL for plugin manifests on the images server.
const PLUGIN_SITE_BASE: &str = "https://images.onerom.org/plugins";

/// Maximum plugin binary size in bytes (64KB).
///
/// Plugins occupy exactly one 64KB slot in the firmware image. Binaries
/// smaller than this are padded; binaries larger are rejected.
const PLUGIN_MAX_SIZE: usize = 64 * 1024;

/// Canonical plugin type string for system plugins, as used in JSON configs.
pub const PLUGIN_TYPE_SYSTEM: &str = "system_plugin";

/// Canonical plugin type string for user plugins, as used in JSON configs.
pub const PLUGIN_TYPE_USER: &str = "user_plugin";

/// Recognised keys in a named `--plugin` spec.
const PLUGIN_SPEC_KEYS: &[&str] = &["version"];

// ============================================================
// Manifest structs
// ============================================================

/// Top-level plugin manifest (`plugins.json`).
///
/// Lists all available first-party plugins. Fetched from
/// `https://images.onerom.org/plugins/plugins.json`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PluginsManifest {
    /// Manifest schema version.
    pub version: u32,
    /// List of available plugins.
    pub plugins: Vec<PluginEntry>,
}

/// A single plugin entry in the top-level manifest.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PluginEntry {
    /// Plugin slug, matches the directory name in the images repo.
    pub name: String,
    /// Canonical plugin type string (e.g. `system_plugin`).
    #[serde(rename = "type")]
    pub plugin_type: String,
    /// Relative path to the plugin directory (e.g. `system/usb`).
    pub path: String,
}

/// Per-plugin release manifest (`releases.json`).
///
/// Contains release history for a single plugin. Fetched from
/// `https://images.onerom.org/plugins/{type}/{name}/releases.json`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PluginReleasesManifest {
    /// Manifest schema version.
    pub version: u32,
    /// Human-readable plugin name.
    pub display_name: String,
    /// Short description of the plugin.
    pub description: String,
    /// Latest released version string (e.g. `0.1.0`), or None if no releases.
    pub latest: Option<String>,
    /// All releases, newest first.
    pub releases: Vec<PluginRelease>,
}

/// A single plugin release entry.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PluginRelease {
    /// Version string (e.g. `0.1.0`).
    pub version: String,
    /// Relative path to the version directory (e.g. `v0.1.0`).
    pub path: String,
    /// Binary filename within the version directory.
    pub filename: String,
    /// SHA256 hex digest of the binary.
    pub sha256: String,
    /// Plugin API version this release targets.
    pub api_version: u32,
    /// Canonical plugin type string.
    pub plugin_type: String,
    /// Minimum One ROM firmware version required to run this plugin.
    #[serde(deserialize_with = "deserialize_fw_version")]
    pub min_fw_version: FirmwareVersion,
}

fn deserialize_fw_version<'de, D>(d: D) -> Result<FirmwareVersion, D::Error>
where
    D: serde::Deserializer<'de>,
{
    let s = String::deserialize(d)?;
    FirmwareVersion::try_from_str(&s).map_err(serde::de::Error::custom)
}

impl PluginRelease {
    /// Full URL to the plugin binary for this release.
    pub fn binary_url(&self, plugin_type: &str, plugin_name: &str) -> String {
        format!(
            "{}/{}/{}/{}/{}",
            PLUGIN_SITE_BASE, plugin_type, plugin_name, self.path, self.filename
        )
    }

    /// Returns true if this release is compatible with the given firmware version.
    pub fn compatible_with_firmware(&self, fw: &FirmwareVersion) -> bool {
        fw >= &self.min_fw_version
    }
}

// ============================================================
// Manifest fetch functions
// ============================================================

/// Fetch the top-level plugins manifest from the images server.
pub async fn fetch_plugins_manifest() -> Result<PluginsManifest, Error> {
    let url = format!("{}/plugins.json", PLUGIN_SITE_BASE);
    fetch_json(&url).await
}

/// Fetch the per-plugin releases manifest from the images server.
///
/// `plugin_type` should be the short form (e.g. `system`, not `system_plugin`).
pub async fn fetch_plugin_releases(
    plugin_type: &str,
    plugin_name: &str,
) -> Result<PluginReleasesManifest, Error> {
    let url = format!(
        "{}/{}/{}/releases.json",
        PLUGIN_SITE_BASE, plugin_type, plugin_name
    );
    fetch_json(&url).await
}

/// Fetch and deserialise JSON from a URL.
async fn fetch_json<T: serde::de::DeserializeOwned>(url: &str) -> Result<T, Error> {
    log::debug!("Fetching {url}");
    let response = reqwest::get(url)
        .await
        .map_err(|e| Error::Network(url.to_string(), e.to_string()))?;

    if !response.status().is_success() {
        return Err(Error::Http(url.to_string(), response.status().as_u16()));
    }

    response
        .json::<T>()
        .await
        .map_err(|e| Error::Json(url.to_string(), e.to_string()))
}

// ============================================================
// PluginSpec parsing
// ============================================================

/// Map a user-supplied type string to the canonical JSON config type string.
///
/// Accepts both short forms (`system`, `user`) and canonical forms
/// (`system_plugin`, `user_plugin`). Returns `None` for unrecognised values.
pub fn canonical_plugin_type(s: &str) -> Option<&'static str> {
    match s {
        "system" | "system_plugin" => Some(PLUGIN_TYPE_SYSTEM),
        "user" | "user_plugin" => Some(PLUGIN_TYPE_USER),
        _ => None,
    }
}

/// Short form of a canonical plugin type, for use in manifest URLs and display.
///
/// e.g. `system_plugin` -> `system`
pub fn short_plugin_type(canonical: &str) -> &str {
    match canonical {
        PLUGIN_TYPE_SYSTEM => "system",
        PLUGIN_TYPE_USER => "user",
        other => other,
    }
}

/// A parsed plugin specification from a `--plugin` argument.
///
/// Produced by [`parse_plugins`] (phase 1). The plugin type and resolved
/// binary path are not available until phase 2 resolution is complete.
///
/// Accepted argument forms:
///
/// ```text
/// usb                            name lookup, latest compatible version
/// system/usb                     name lookup with explicit type
/// usb,version=0.1.0              name lookup, pinned version
/// system/usb,version=0.1.0       name lookup with explicit type, pinned version
/// file=path/to/plugin.bin        local file, type from binary header
/// file=https://example.com/p.bin remote file, type from binary header
/// ```
#[derive(Debug, Clone)]
pub enum PluginSpec {
    /// Look up plugin by name (and optional type) from the release manifest.
    ///
    /// `plugin_type` is `None` when the type was not specified by the user;
    /// the manifest lookup in phase 2 will confirm it.
    Named {
        name: String,
        plugin_type: Option<&'static str>,
        version: Option<String>,
    },
    /// Use a local or remote file directly.
    ///
    /// The plugin type is unknown until phase 2, when the binary header is
    /// parsed.
    File { path: String },
}

/// Parse a single raw `--plugin` string into a [`PluginSpec`].
///
/// Validates syntax only. Does not access the network or filesystem.
fn parse_plugin(s: &str) -> Result<PluginSpec, Error> {
    // file= form: remainder is the path or URL, no further parsing
    if let Some(path) = s.strip_prefix("file=") {
        if path.is_empty() {
            return Err(Error::InvalidArgument(
                "--plugin".to_string(),
                format!("file path must not be empty\n    --plugin '{s}'"),
            ));
        }
        return Ok(PluginSpec::File {
            path: path.to_string(),
        });
    }

    // Named form: split on first comma to separate the name part from
    // any key=value options that follow
    let mut parts = s.splitn(2, ',');
    let name_part = parts.next().unwrap();
    let kv_part = parts.next();

    // Parse key=value options, currently only version= is supported
    let mut version = None;
    if let Some(kv) = kv_part {
        let mut seen = std::collections::HashSet::new();
        for kv in kv.split(',') {
            let (key, value) = kv.split_once('=').ok_or_else(|| {
                Error::InvalidArgument(
                    "--plugin".to_string(),
                    format!(
                        "Plugin option '{kv}' is missing a value - expected '{kv}=<value>'\n    --plugin '{s}'"
                    ),
                )
            })?;
            if !seen.insert(key) {
                return Err(Error::InvalidArgument(
                    "--plugin".to_string(),
                    format!("Duplicate plugin option '{key}'\n    --plugin '{s}'"),
                ));
            }
            match key {
                "version" => {
                    if value.is_empty() {
                        return Err(Error::InvalidArgument(
                            "--plugin".to_string(),
                            format!("version must not be empty\n    --plugin '{s}'"),
                        ));
                    }
                    version = Some(value.to_string());
                }
                other => {
                    let supported = PLUGIN_SPEC_KEYS.join(", ");
                    return Err(Error::InvalidArgument(
                        "--plugin".to_string(),
                        format!(
                            "Unrecognised plugin option '{other}'\n    --plugin '{s}'\n  Supported options: {supported}"
                        ),
                    ));
                }
            }
        }
    }

    // Parse optional type/ prefix from the name part.
    // e.g. "system/usb" -> type=system_plugin, name=usb
    //      "usb"        -> type=None,          name=usb
    let (plugin_type, name) = if let Some((type_str, name_str)) = name_part.split_once('/') {
        let canonical = canonical_plugin_type(type_str).ok_or_else(|| {
            Error::InvalidArgument(
                "--plugin".to_string(),
                format!(
                    "Invalid plugin type '{type_str}': expected 'system' or 'user'\n    --plugin '{s}'"
                ),
            )
        })?;
        if name_str.is_empty() {
            return Err(Error::InvalidArgument(
                "--plugin".to_string(),
                format!("Plugin name must not be empty\n    --plugin '{s}'"),
            ));
        }
        (Some(canonical), name_str.to_string())
    } else {
        if name_part.is_empty() {
            return Err(Error::InvalidArgument(
                "--plugin".to_string(),
                format!("Plugin name must not be empty\n    --plugin '{s}'"),
            ));
        }
        (None, name_part.to_string())
    };

    Ok(PluginSpec::Named {
        name,
        plugin_type,
        version,
    })
}

/// Parse all `--plugin` strings into a vec of [`PluginSpec`] values (phase 1).
///
/// Returns the first parse error encountered.
///
/// Performs best-effort semantic validation for specs where the type is
/// already known at this stage (e.g. `system/usb`). For specs where the type
/// is unknown (bare name or `file=` forms), full validation is deferred to
/// phase 2. After resolving all types, the caller must call
/// [`validate_resolved_plugin_types`].
pub fn parse_plugins(plugins: &[String]) -> Result<Vec<PluginSpec>, Error> {
    let specs: Vec<PluginSpec> = plugins
        .iter()
        .map(|s| parse_plugin(s))
        .collect::<Result<_, _>>()?;

    // Best-effort checks on specs with known types only.
    // Specs with unknown types (file= or bare name) are skipped here.
    let mut seen_system = false;
    let mut seen_user = false;
    for spec in &specs {
        if let PluginSpec::Named {
            plugin_type: Some(t),
            ..
        } = spec
        {
            match *t {
                PLUGIN_TYPE_SYSTEM => {
                    if seen_system {
                        return Err(Error::DuplicatePlugin("system".to_string()));
                    }
                    seen_system = true;
                }
                PLUGIN_TYPE_USER => {
                    if seen_user {
                        return Err(Error::DuplicatePlugin("user".to_string()));
                    }
                    seen_user = true;
                }
                _ => {}
            }
        }
    }
    if seen_user && !seen_system {
        return Err(Error::UserPluginWithoutSystem);
    }

    Ok(specs)
}

/// Validate the full set of resolved plugin types (phase 2).
///
/// Must be called by the caller after all plugin types have been resolved
/// from the manifest or binary headers. `plugin_types` should be a slice of
/// canonical type strings (e.g. `["system_plugin", "user_plugin"]`) in the
/// order the plugins were specified.
///
/// Checks:
/// - At most one system plugin
/// - At most one user plugin
/// - A user plugin requires a system plugin
pub fn validate_resolved_plugin_types(plugin_types: &[&str]) -> Result<(), Error> {
    let system_count = plugin_types
        .iter()
        .filter(|&&t| t == PLUGIN_TYPE_SYSTEM)
        .count();
    let user_count = plugin_types
        .iter()
        .filter(|&&t| t == PLUGIN_TYPE_USER)
        .count();

    if system_count > 1 {
        return Err(Error::DuplicatePlugin("system".to_string()));
    }
    if user_count > 1 {
        return Err(Error::DuplicatePlugin("user".to_string()));
    }
    if user_count > 0 && system_count == 0 {
        return Err(Error::UserPluginWithoutSystem);
    }

    Ok(())
}

// ============================================================
// JSON generation
// ============================================================

/// Determine the appropriate `size_handling` value for a plugin binary.
///
/// Plugin binaries must fit within a 64KB slot:
/// - Exactly 64KB: `"none"` (no padding needed or wanted)
/// - Less than 64KB: `"pad"` (binary padded to fill the slot)
/// - Greater than 64KB: error
pub fn plugin_size_handling(size: usize) -> Result<&'static str, Error> {
    if size > PLUGIN_MAX_SIZE {
        return Err(Error::PluginTooLarge(size, PLUGIN_MAX_SIZE));
    }
    if size == PLUGIN_MAX_SIZE {
        Ok("none")
    } else {
        Ok("pad")
    }
}

/// Build the `chips` array entry JSON object for a single plugin.
fn plugin_to_chip_json(
    file: &str,
    plugin_type: &str,
    description: Option<&str>,
    size_handling: &str,
) -> Value {
    let mut chip = json!({
        "type":          plugin_type,
        "file":          file,
        "size_handling": size_handling,
    });
    if let Some(desc) = description {
        chip.as_object_mut()
            .unwrap()
            .insert("description".to_string(), json!(desc));
    }
    chip
}

/// Build a complete chip_set JSON object for a plugin.
///
/// The returned value is a single-chip chip_set entry ready to be inserted
/// into the `chip_sets` array of a One ROM JSON configuration. System plugins
/// must be inserted at index 0; user plugins at index 1.
///
/// Arguments:
/// - `file`:        resolved path or URL to the plugin binary
/// - `plugin_type`: canonical type string (`system_plugin` or `user_plugin`)
/// - `description`: optional human-readable description for the config
/// - `size`:        binary size in bytes, used to determine `size_handling`
pub fn plugin_to_chip_set_json(
    file: &str,
    plugin_type: &str,
    description: Option<&str>,
    size: usize,
) -> Result<Value, Error> {
    let size_handling = plugin_size_handling(size)?;
    Ok(json!({
        "type": "single",
        "chips": [plugin_to_chip_json(file, plugin_type, description, size_handling)]
    }))
}
