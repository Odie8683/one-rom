// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

//! Argument definitions for `onerom control`.

use crate::args::CommandTrait;
use crate::utils::{parse_u8, parse_u32};
use clap::{ArgGroup, Args, Subcommand, ValueEnum};
use enum_dispatch::enum_dispatch;

use onerom_cli::usb::RebootArgs;

#[derive(Debug, Args)]
pub struct ControlArgs {
    #[command(subcommand)]
    pub command: ControlCommands,
}

impl CommandTrait for ControlArgs {
    fn requires_device(&self) -> bool {
        self.command.requires_device()
    }
}

#[enum_dispatch(CommandTrait)]
#[derive(Debug, Subcommand)]
pub enum ControlCommands {
    /// Reboot the One ROM.
    ///
    /// Restarts the One ROM firmware. The device will re-initialise and
    /// resume serving ROM images after the reboot.
    ///
    /// By default, this command pauses after a reboot to give the device time
    /// to re-enumerate.
    ///
    /// Example:
    ///
    ///   onerom control reboot
    Reboot(ControlRebootArgs),

    /// Control the status LED on a One ROM.
    ///
    /// Examples:
    ///
    ///   onerom control led on
    ///
    ///   onerom control led off
    #[command(
        subcommand_value_name = "COMMAND",
        subcommand_help_heading = "Commands"
    )]
    Led(ControlLedArgs),

    /// Write data to One ROM's SRAM or the live ROM image.
    ///
    /// Poke provides transient (non-persistent) writes to device memory. Changes
    /// are lost on reboot. Use `update` subcommands for persistent flash writes.
    ///
    /// Data can be written as a single byte value or from a binary file.
    ///
    /// Example:
    ///
    ///   onerom control poke memory --address 0x20000010 --byte 0xFF
    ///
    ///   onerom control poke memory --address 0x20000010 --input patch.bin
    ///
    ///   onerom control poke live --address 0x100 --byte 0xEA
    ///
    ///   onerom control poke live --address 0x100 --input patch.bin
    #[command(
        subcommand_value_name = "COMMAND",
        subcommand_help_heading = "Commands"
    )]
    Poke(ControlPokeArgs),

    /// Assert the host reset signal via the One ROM reset GPIO (not yet supported).
    ///
    /// Drives the reset pin to reset the host system the One ROM is
    /// installed in. Useful in scripted workflows to reset the host after
    /// programming a new ROM image.
    ///
    /// Examples:
    ///
    ///   onerom control reset
    ///
    ///   onerom control reset --hold 500
    Reset(ControlResetArgs),

    /// Select the active ROM slot (not yet supported).
    ///
    /// Switches the device to serving the specified image slot. This takes
    /// effect immediately but does not persist across power cycles unless
    /// the device firmware is configured to do so.
    ///
    /// Example:
    ///
    ///   onerom control select --slot 2
    Select(ControlSelectArgs),

    /// Set the state of a One ROM GPIO pin (not yet supported).
    ///
    /// Sets the specified GPIO pin to high, low, or high-impedance (z).
    ///
    /// Example:
    ///
    ///   onerom control gpio --pin 3 --state high
    ///
    ///   onerom control gpio --pin 3 --state z
    Gpio(ControlGpioArgs),

    /// Erase this One ROM's flash memory.
    ///
    /// Permanently erase flash contents on the device, including firmware,
    /// metadata and ROM images.
    ///
    /// If a One ROM's firmware has been erased it will subsequently boot into
    /// the RP2350 bootloader from where it can be reprogrammed.  However, you
    /// will need to use the --unrecognized to detect and program it.
    ///
    /// It is highly recommended that this command is used when One ROM is
    /// stopped (and the default is this command will reboot the device if
    /// required before erasing to make it so).
    ///
    /// Use with extreme caution while One ROM is running.  Erasing the core
    /// firmware or the system plugin's flash will cause the USB stack to be
    /// non-functional, requiring manually forcing into BOOTSEL mode using One
    /// ROM's header pins.  In addition, erasing flash causes the device to
    /// temporarily suspend interrupts and cause flash to become inaccessible.
    /// Anything else running from flash (like a user plugin) may well crash
    /// as a result.
    ///
    /// For a similar reason, large erase operations while running may cause
    /// One ROM's USB support to become unavailable and then re-enumerate
    /// after the flash erase.  In this case, the flash likely succeeded and
    /// can be checked with `inspect peek memory`.
    ///
    /// You can use this command to erase multiple ranges in a single operation
    /// with multiple --offset/--address and --size arguments.
    ///
    /// Example:
    ///
    ///   onerom control erase -a
    ///
    ///   onerom control erase --offset 0x20000 --length 0x1000
    Erase(ControlEraseArgs),
}

#[derive(Debug, Args)]
pub struct ControlLedArgs {
    #[command(subcommand)]
    pub command: ControlLedCommands,
}

impl CommandTrait for ControlLedArgs {
    fn requires_device(&self) -> bool {
        self.command.requires_device()
    }
}

#[enum_dispatch(CommandTrait)]
#[derive(Debug, Subcommand)]
pub enum ControlLedCommands {
    /// Turn the status LED on.
    On(ControlLedOnArgs),
    /// Turn the status LED off.
    Off(ControlLedOffArgs),
    /// Beacon the status LED to identify a physical One ROM.
    Beacon(ControlLedBeaconArgs),
    /// Flame the status LED.
    Flame(ControlLedFlameArgs),
}

#[derive(Debug, Args)]
pub struct ControlLedOnArgs;

impl CommandTrait for ControlLedOnArgs {
    fn requires_device(&self) -> bool {
        true
    }
}

#[derive(Debug, Args)]
pub struct ControlLedOffArgs;

impl CommandTrait for ControlLedOffArgs {
    fn requires_device(&self) -> bool {
        true
    }
}

#[derive(Debug, Args)]
pub struct ControlLedBeaconArgs;

impl CommandTrait for ControlLedBeaconArgs {
    fn requires_device(&self) -> bool {
        true
    }
}

#[derive(Debug, Args)]
pub struct ControlLedFlameArgs;

impl CommandTrait for ControlLedFlameArgs {
    fn requires_device(&self) -> bool {
        true
    }
}

#[derive(Debug, Args, Clone)]
#[command(group = ArgGroup::new("reboot_mode").required(false).multiple(false))]
pub struct ControlRebootArgs {
    /// Reboot the device into stopped (bootloader) state
    #[arg(long, short = 'p', group = "reboot_mode")]
    pub stopped: bool,

    /// Reboot the device into running (byte serving) state (default).
    #[arg(long, short, group = "reboot_mode")]
    pub running: bool,

    /// Don't pause after reboot for the device to re-enumerate
    #[arg(long)]
    pub fast: bool,

    /// Mount mass storage device when rebooting into stopped mode.
    #[arg(long, short = 'm', conflicts_with = "running")]
    pub msd: bool,
}

impl CommandTrait for ControlRebootArgs {
    fn requires_device(&self) -> bool {
        true
    }
}

impl From<&ControlRebootArgs> for RebootArgs {
    fn from(args: &ControlRebootArgs) -> Self {
        if args.stopped {
            RebootArgs::stopped(args.msd, args.fast)
        } else {
            // Default if unspecified
            RebootArgs::running(args.fast)
        }
    }
}

#[derive(Debug, Args)]
pub struct ControlResetArgs {
    /// Duration in milliseconds to hold the reset signal asserted.
    /// Defaults to 100.
    #[arg(long, value_name = "MS", default_value = "100")]
    pub hold: u32,
}

impl CommandTrait for ControlResetArgs {
    fn requires_device(&self) -> bool {
        true
    }
}

#[derive(Debug, Args)]
pub struct ControlSelectArgs {
    /// Image slot index to activate (0-15).
    #[arg(long, short = 'l', value_name = "INDEX", required = true)]
    pub slot: u8,
}

impl CommandTrait for ControlSelectArgs {
    fn requires_device(&self) -> bool {
        true
    }
}

#[derive(Debug, Clone, ValueEnum)]
pub enum GpioState {
    /// Drive the pin high.
    High,
    /// Drive the pin low.
    Low,
    /// Set the pin to high-impedance (tri-state).
    Z,
}

#[derive(Debug, Args)]
pub struct ControlGpioArgs {
    /// GPIO pin number to control.
    #[arg(long, value_name = "PIN", required = true)]
    pub pin: u8,

    /// Desired pin state: high, low, or z (high-impedance).
    #[arg(long, value_name = "STATE", required = true)]
    pub state: GpioState,
}

impl CommandTrait for ControlGpioArgs {
    fn requires_device(&self) -> bool {
        true
    }
}

#[derive(Debug, Args)]
pub struct ControlPokeArgs {
    #[command(subcommand)]
    pub command: ControlPokeCommands,
}

impl CommandTrait for ControlPokeArgs {
    fn requires_device(&self) -> bool {
        self.command.requires_device()
    }
}

#[derive(Debug, Args)]
#[command(group = ArgGroup::new("erase_target").required(true).args(["all", "offset", "address"]))]
#[command(group = ArgGroup::new("reboot_mode").required(false).args(["reboot_stopped", "reboot_running"]))]
pub struct ControlEraseArgs {
    /// Erase all flash contents.
    #[arg(long, short)]
    pub all: bool,

    /// Erase at offset(s) relative to flash base (0x10000000).
    ///
    /// Must be 4096-aligned. Pair each with a --size.
    /// Can be repeated for multiple ranges.
    /// Mutually exclusive with --address.
    #[arg(long, short, value_name = "OFFSET", value_parser = parse_u32, action = clap::ArgAction::Append, conflicts_with = "address", requires = "size")]
    pub offset: Vec<u32>,

    /// Erase at absolute address(es).
    ///
    /// Must be 4096-aligned. Pair each with a --size.
    /// Can be repeated for multiple ranges.
    /// Mutually exclusive with --offset.
    #[arg(long, value_name = "ADDRESS", value_parser = parse_u32, action = clap::ArgAction::Append, conflicts_with = "offset", requires = "size")]
    pub address: Vec<u32>,

    /// Size of each range to erase (paired with --offset or --address).
    ///
    /// Must be 4096-aligned. Specify once per --offset/--address.
    #[arg(long, value_name = "SIZE", value_parser = parse_u32, action = clap::ArgAction::Append, conflicts_with = "all")]
    pub size: Vec<u32>,

    /// Do not reboot before or after erasing.  This can be risky, if the
    /// device is actively accessing the flash range being erased.
    #[arg(long, short, conflicts_with = "reboot_mode")]
    pub no_reboot: bool,

    /// Reboot into stopped (bootloader) mode after erasing.
    #[arg(long, short = 'p', conflicts_with = "reboot_running")]
    pub reboot_stopped: bool,

    /// Reboot into running mode after erasing.
    #[arg(long, short = 'r', conflicts_with = "reboot_stopped")]
    pub reboot_running: bool,

    /// Mount mass storage device when rebooting into stopped mode.
    #[arg(long, short = 'm', requires = "reboot_stopped")]
    pub msd: bool,

    /// Don't pause after reboot for the device to re-enumerate
    #[arg(long, requires = "reboot_mode")]
    pub fast: bool,
}

impl CommandTrait for ControlEraseArgs {
    fn requires_device(&self) -> bool {
        true
    }
}

impl From<&ControlEraseArgs> for RebootArgs {
    fn from(args: &ControlEraseArgs) -> Self {
        if args.reboot_stopped {
            RebootArgs::stopped(args.msd, args.fast)
        } else if args.reboot_running {
            RebootArgs::running(args.fast)
        } else {
            RebootArgs::none()
        }
    }
}

#[enum_dispatch(CommandTrait)]
#[derive(Debug, Subcommand)]
pub enum ControlPokeCommands {
    /// Write a single byte or binary file to One ROM's SRAM.
    ///
    /// Writes data directly to the device's SRAM at the specified address.
    /// This is a transient operation — changes are lost on reboot.
    ///
    /// The address must be a valid SRAM address. When writing a file, the
    /// entire file contents are written starting at the given address. When
    /// writing a single byte, only that byte is written.
    ///
    /// Note: writing to arbitrary SRAM addresses can corrupt firmware state.
    /// Use with caution.
    ///
    /// Example:
    ///   onerom control poke memory --address 0x20000010 --value 0xFF
    ///   onerom control poke memory --address 0x20000000 --input patch.bin
    Memory(ControlPokeMemoryArgs),

    /// Write a single byte or binary file to the live ROM image.
    ///
    /// Writes data to the ROM image currently being served by the device,
    /// at the specified logical ROM address (starting from 0). This is a
    /// transient operation — changes are lost on reboot.
    ///
    /// This is useful for patching a running ROM image without reflashing.
    /// The address is a logical ROM offset, not a physical memory address.
    ///
    /// Example:
    ///   onerom control poke live --address 0x100 --value 0xEA
    ///   onerom control poke live --address 0 --input patch.bin
    Live(ControlPokeLiveArgs),
}

#[derive(Debug, Args)]
#[command(group = ArgGroup::new("poke_source").required(true).multiple(false))]
pub struct ControlPokeMemoryArgs {
    /// Write to this memory address on the device.
    ///
    /// Accepts decimal and hexadecimal (0x prefix) formats.
    #[arg(long, short, value_name = "ADDRESS", value_parser = parse_u32)]
    pub address: u32,

    /// Write this single byte value.
    ///
    /// Accepts decimal and hexadecimal (0x prefix) formats.
    /// Mutually exclusive with --input.
    #[arg(long, short, value_name = "BYTE", value_parser = parse_u8, group = "poke_source")]
    pub byte: Option<u8>,

    /// Write the contents of this binary file.
    ///
    /// Mutually exclusive with --value.
    #[arg(long, visible_alias = "in", value_name = "FILE", group = "poke_source")]
    pub input: Option<String>,
}

impl CommandTrait for ControlPokeMemoryArgs {
    fn requires_device(&self) -> bool {
        true
    }
}

#[derive(Debug, Args)]
#[command(group = ArgGroup::new("poke_source").required(true).multiple(false))]
pub struct ControlPokeLiveArgs {
    /// Write to this logical ROM address, starting from 0.
    ///
    /// Accepts decimal and hexadecimal (0x prefix) formats.
    #[arg(long, short, value_name = "ADDRESS", value_parser = parse_u32, default_value = "0")]
    pub address: u32,

    /// Write this single byte value.
    ///
    /// Accepts decimal and hexadecimal (0x prefix) formats.
    /// Mutually exclusive with --input.
    #[arg(long, short, visible_alias = "value", value_name = "BYTE", value_parser = parse_u8, group = "poke_source")]
    pub byte: Option<u8>,

    /// Write the contents of this binary file.
    ///
    /// Mutually exclusive with --byte.
    #[arg(long, visible_alias = "in", value_name = "FILE", group = "poke_source")]
    pub input: Option<String>,

    /// Only write bytes that differ from the current device's ROM content.
    ///
    /// Requires --input.
    #[arg(long, requires = "input", visible_alias = "deltas")]
    pub delta: bool,

    /// Show what would be written without actually writing.
    ///
    /// Requires --delta.
    #[arg(long, requires = "delta", visible_alias = "dryrun")]
    pub dry_run: bool,
}

impl CommandTrait for ControlPokeLiveArgs {
    fn requires_device(&self) -> bool {
        true
    }
}
