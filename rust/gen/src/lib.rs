// Copyright (C) 2025 Piers Finlayson <piers@piers.rocks>
//
// MIT License

//! Generates firmware artifacts for One ROM.

#![no_std]

extern crate alloc;

pub mod builder;
pub mod firmware;
pub mod image;
pub mod meta;

pub use builder::{Builder, ChipConfig, ChipSetConfig, Config, FileData, FileSpec, License};
pub use image::{Chip, ChipSet, ChipSetType, CsConfig, CsLogic, SizeHandling};
pub use image::{PAD_BLANK_BYTE, PAD_NO_CHIP_BYTE};
pub use meta::{MAX_METADATA_LEN, Metadata, PAD_METADATA_BYTE};

use alloc::string::String;
use onerom_config::chip::ChipType;
use onerom_config::fw::{FirmwareVersion, ServeAlg};
use onerom_config::mcu::Family;

/// Version of metadata produced by this version of the crate
pub const METADATA_VERSION: u32 = 1;
const METADATA_VERSION_STR: &str = "1";

/// Firmware size reserved at the start of flash, before metadata
pub const FIRMWARE_SIZE: usize = 48 * 1024; // 48KB

pub const MIN_FIRMWARE_OVERRIDES_VERSION: FirmwareVersion = FirmwareVersion::new(0, 6, 0, 0);

/// Error type
#[derive(Debug, serde::Serialize, serde::Deserialize)]
pub enum Error {
    RightSize {
        size: usize,
    },
    ImageTooSmall {
        index: usize,
        expected: usize,
        actual: usize,
    },
    ImageTooLarge {
        image_size: usize,
        expected_size: usize,
    },
    DuplicationNotExactDivisor {
        image_size: usize,
        expected_size: usize,
    },
    BufferTooSmall {
        location: &'static str,
        expected: usize,
        actual: usize,
    },
    NoChips,
    TooManyChips {
        expected: usize,
        actual: usize,
    },
    TooFewChips {
        expected: usize,
        actual: usize,
    },
    MissingCsConfig {
        line: &'static str,
    },
    MissingPointer {
        id: usize,
    },
    InvalidServeAlg {
        serve_alg: ServeAlg,
    },
    InconsistentCsLogic {
        first: CsLogic,
        other: CsLogic,
    },
    InvalidConfig {
        error: String,
    },
    UnsupportedConfigVersion {
        version: u32,
    },
    DuplicateFile {
        id: usize,
    },
    InvalidFile {
        id: usize,
        total: usize,
    },
    MissingFile {
        id: usize,
    },
    UnsupportedChipType {
        chip_type: ChipType,
    },
    InvalidLicense {
        id: usize,
    },
    UnvalidatedLicense {
        id: usize,
    },
    BadLocation {
        id: usize,
        reason: String,
    },
    UnsupportedFrequency {
        frequency_mhz: u32,
    },
    FirmwareTooOld {
        version: FirmwareVersion,
        minimum: FirmwareVersion,
    },
    FirmwareTooNew {
        version: FirmwareVersion,
        maximum: FirmwareVersion,
    },
    WrongMcuFamily {
        actual: Family,
        required: Family,
    },
    Base64,
    Base16,
}
type Result<T> = core::result::Result<T, Error>;

pub fn crate_version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}

pub fn metadata_version() -> &'static str {
    METADATA_VERSION_STR
}
