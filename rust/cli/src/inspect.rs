// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

use crate::args::inspect::{
    InspectGpioArgs, InspectImageArgs, InspectInfoArgs, InspectPeekLiveArgs, InspectPeekMemoryArgs,
    InspectSlotsArgs, InspectTelemetryArgs,
};
use crate::utils::{check_device, check_live_read_write, print_hex_dump};
use onerom_cli::LIVE_ROM_BASE;
use onerom_cli::usb::read_memory;
use onerom_cli::{Device, Error, Options};
use sdrr_fw_parser::SdrrCsState;

pub async fn cmd_info(options: &Options, args: &InspectInfoArgs) -> Result<(), Error> {
    // Print the device summary
    check_device(options, args)?;
    let device = options.device.as_ref().unwrap();

    println!("{device}");

    // Print the detailed device information as JSON if available
    if let Some(onerom) = device.onerom.as_ref() {
        if let Some(info) = onerom.flash.as_ref() {
            let json =
                serde_json::to_string_pretty(info).map_err(|e| Error::Other(e.to_string()))?;
            println!("Flash information:");
            println!("{json}");
        }
        if let Some(info) = onerom.ram.as_ref() {
            let json =
                serde_json::to_string_pretty(info).map_err(|e| Error::Other(e.to_string()))?;
            println!("Runtime information:");
            println!("{json}");
        }
    }

    Ok(())
}

pub async fn cmd_telemetry(options: &Options, args: &InspectTelemetryArgs) -> Result<(), Error> {
    check_device(options, args)?;
    let _device = options.device.as_ref().unwrap();
    Err(Error::Unimplemented("inspect telemetry".into()))
}

pub async fn cmd_slots(options: &Options, args: &InspectSlotsArgs) -> Result<(), Error> {
    check_device(options, args)?;
    let device = options.device.as_ref().unwrap();

    println!("{device}");
    if let Some(onerom) = device.onerom.as_ref()
        && let Some(info) = onerom.flash.as_ref()
    {
        let verbose = options.verbose;
        let set_count = info.rom_set_count;
        println!(
            "Configured with {set_count} slot{}:",
            if set_count == 1 { "" } else { "s" }
        );
        for (i, set) in info.rom_sets.iter().enumerate() {
            println!("  Slot {i}:");
            let set_location = set.data_ptr;
            let set_image_size = set.size;
            for (j, rom) in set.roms.iter().enumerate() {
                let mut cs = String::new();
                if rom.cs1_state != SdrrCsState::NotUsed {
                    cs.push_str(&format!("Chip Select 1: {} ", rom.cs1_state));
                }
                if rom.cs2_state != SdrrCsState::NotUsed {
                    cs.push_str(&format!("Chip Select 2: {} ", rom.cs2_state));
                }
                if rom.cs3_state != SdrrCsState::NotUsed {
                    cs.push_str(&format!("Chip Select 3: {} ", rom.cs3_state));
                }
                let rom_type = rom.rom_type;
                println!("    ROM {j}: {rom_type} {cs}");
                if verbose {
                    println!(
                        "      Flash location 0x{set_location:08x} size 0x{set_image_size:08x} bytes"
                    );
                }
                if let Some(filename) = &rom.filename {
                    println!("      Image source: {filename}");
                }
            }
        }
        Ok(())
    } else {
        Err(Error::Other(
            "No recognised information found on device flash".to_string(),
        ))
    }
}

pub async fn cmd_image(options: &Options, args: &InspectImageArgs) -> Result<(), Error> {
    check_device(options, args)?;
    let _device = options.device.as_ref().unwrap();
    Err(Error::Unimplemented("inspect image".into()))
}

// Outputs some bytes of data read from the device, either to the console as a
// hex dump or to a file if an output path is provided.
//
// addr_offset is subtracted from the displayed addresses in the hex dump, so
// it can be used to convert from a physical memory address to an offset within
// a range.
async fn read_and_output(
    device: &Device,
    address: u32,
    length: u32,
    addr_offset: u32,
    out: Option<&String>,
) -> Result<(), Error> {
    let data = read_memory(device, address, length).await?;

    if let Some(filename) = out {
        std::fs::write(filename, &data).map_err(|e| Error::io(filename, e))?;
    } else {
        print_hex_dump(address - addr_offset, &data);
    }

    Ok(())
}

pub async fn cmd_peek_live(options: &Options, args: &InspectPeekLiveArgs) -> Result<(), Error> {
    let (address, length) = check_live_read_write(options, args.address, args.length, args)?;

    let device = options.device.as_ref().unwrap();
    read_and_output(device, address, length, LIVE_ROM_BASE, args.output.as_ref()).await
}

pub async fn cmd_peek_memory(options: &Options, args: &InspectPeekMemoryArgs) -> Result<(), Error> {
    check_device(options, args)?;
    let device = options.device.as_ref().unwrap();
    read_and_output(device, args.address, args.length, 0, args.output.as_ref()).await
}

pub async fn cmd_gpio(options: &Options, args: &InspectGpioArgs) -> Result<(), Error> {
    check_device(options, args)?;
    let _device = options.device.as_ref().unwrap();
    Err(Error::Unimplemented("inspect gpio".into()))
}
