// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

//! Shared error type for the One ROM CLI library.

use sdrr_fw_parser::SdrrRomType;

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("Hit an error accessing USB:\n  {0}")]
    Usb(String),

    #[error("No One ROMs found")]
    NoDevices,

    #[error("Multiple One ROMs found.  Use --device to select one.\n  Found: {}", .0.join(", "))]
    MultipleDevices(Vec<String>),

    #[error("One ROM not found: {0}")]
    DeviceNotFound(String),

    #[error("Hit an input/output error: {0}")]
    Io(String),

    #[error("{0}")]
    Other(String),

    #[error("Unknown board type: {0}\n  Known board types: {1}")]
    InvalidBoard(String, String),

    #[error("You must not specify both --device and --board together.\n  If --device is specified, this is used to determine the board type automatically if possible.")]
    DeviceAndBoard,

    #[error("The selected operation does not apply to a device.\n  Do not specify --device.")]
    Device,

    #[error("No device was specified.\n  Specify a device using --device.")]
    NoDevice,

    #[error("The '{0}' command has not been implemented")]
    Unimplemented(String),

    #[error(
        "The operation attempted to access an unsupported memory region\n  Address {0:#010x}, length {1:#010x}"
    )]
    InvalidMemoryRange(u32, u32),

    #[error("The specified memory range is not accessible when One ROM isn't running")]
    MemoryDeviceNotRunning,

    #[error("The specificied memory range is not writeable")]
    MemoryNotWriteable,

    #[error("This operation can only be performed on a One ROM that is running")]
    NotRunning,

    #[error("This operation cannot be performed as the ROM type is unknown")]
    UnknownRomType,

    #[error(
        "The operation attempted to access past the end of a live ROM image.\n  The {0} size is {1} bytes"
    )]
    LiveOutOfBounds(SdrrRomType, usize),

    #[error("Cannot determine the board type.\n  Either --board or --device must be specified.")]
    NoBoardOrDevice,

    #[error("Specified version '{0}' not found.\n  Available releases: {1}")]
    VersionNotFound(String, String),

    #[error("No latest release found in manifest.\n  This is likely a bug.  Please report it.")]
    NoLatestRelease,

    #[error("License was not accepted.\n  You must accept the license to proceed with this operation.")]
    LicenseNotAccepted,

    #[error("The base firmware image supplied is larger than the maximum supported\n  {0} bytes supplied vs {1} bytes maximum")]
    BaseFirmwareTooLarge(usize, usize),

    #[error("Assembled firmware has parse errors (use --force to override):\n  {0}\n  This is likely a bug.  Please report it.")]
    FirmwareValidation(String),

    #[error("Failed to stop device, cannot proceed.\n  This is likely a bug.  Please report it.")]
    DeviceStillRunning,

    #[error("Flash verification failed at offset {0:#010x}:\n  Expected {1:#04x}, got {2:#04x}")]
    VerifyFailed(usize, u8, u8),

    #[error("Invalid argument found:\n  {0}")]
    InvalidArgument(String),

    #[error("Cannot program One ROM as no configuration or firmware specified.\n  Use --config, --slot, --firmware, or --base-firmware.")]
    NoFirmwareSource,

    #[error("Unexpected reboot state specified.\n  This is likely a bug.  Please report it.")]
    NoReboot,

    #[error("Unsupported chip type '{0}'.\n  Supported types for this board: {1}")]
    UnsupportedChipType(String, String),

    #[error("This board does not support chip types {1}.\n  Supported types: {2}")]
    UnsupportedBoardChipType(String, String, String),

    #[error("Could not determine board type from the connected device {0}.\n  It may be an unprogrammed One ROM or have corrupt firmware.\n  Supply the board type with --board")]
    NoBoardFromDevice(String),
}

impl Error {
    pub fn io(path: impl AsRef<std::path::Path>, e: std::io::Error) -> Self {
        Self::Io(format!("{}: {e}", path.as_ref().display()))
    }
}

impl From<onerom_fw::Error> for Error {
    fn from(e: onerom_fw::Error) -> Self {
        Self::Other(e.to_string())
    }
}

impl From<onerom_config::Error> for Error {
    fn from(e: onerom_config::Error) -> Self {
        Self::Other(format!("{e}"))
    }
}
