// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

use crate::{
    args,
    utils::{check_device, check_live_read_write},
};
use onerom_cli::device::{Device, select_device};
use onerom_cli::usb::{
    FLASH_BASE, LedSubCmd, RebootArgs, flash_erase, read_memory, reboot, set_led, write_memory,
};
use onerom_cli::{Error, Options};
use std::io::Write;

pub async fn cmd_led_on(
    options: &Options,
    args: &args::control::ControlLedOnArgs,
) -> Result<(), Error> {
    check_device(options, args)?;
    let device = options.device.as_ref().unwrap();
    set_led(device, 0, LedSubCmd::On).await?;
    if options.verbose {
        println!("LED on");
    }
    Ok(())
}

pub async fn cmd_led_off(
    options: &Options,
    args: &args::control::ControlLedOffArgs,
) -> Result<(), Error> {
    check_device(options, args)?;
    let device = options.device.as_ref().unwrap();
    set_led(device, 0, LedSubCmd::Off).await?;
    if options.verbose {
        println!("LED off");
    }
    Ok(())
}

pub async fn cmd_led_beacon(
    options: &Options,
    args: &args::control::ControlLedBeaconArgs,
) -> Result<(), Error> {
    check_device(options, args)?;
    let device = options.device.as_ref().unwrap();
    set_led(device, 0, LedSubCmd::Beacon).await?;
    if options.verbose {
        println!("LED beacon started");
    }
    Ok(())
}

pub async fn cmd_led_flame(
    options: &Options,
    args: &args::control::ControlLedFlameArgs,
) -> Result<(), Error> {
    check_device(options, args)?;
    let device = options.device.as_ref().unwrap();
    set_led(device, 0, LedSubCmd::Flame).await?;
    if options.verbose {
        println!("LED flame started");
    }
    Ok(())
}

pub async fn cmd_reboot(
    options: &Options,
    args: &args::control::ControlRebootArgs,
) -> Result<(), Error> {
    check_device(options, args)?;
    let device = options.device.as_ref().unwrap();
    assert!(
        !(args.stopped && args.running),
        "Cannot specify both --stopped and --running"
    );
    let reboot_args = args.into();
    if options.verbose {
        println!("Rebooting device: {device}");
    } else {
        println!("Rebooting device...");
    }
    let serial = device.serial.clone();
    reboot(device, &reboot_args).await?;
    println!("Rebooted device into {} mode", reboot_args.mode);

    if options.verbose {
        // Rescan device to show new mode
        let selector = serial.as_deref();
        let device = select_device(selector, options.unrecognised, &options.vid_pid).await?;
        println!("{device}");
    }

    Ok(())
}

pub async fn cmd_reset(
    options: &Options,
    args: &args::control::ControlResetArgs,
) -> Result<(), Error> {
    check_device(options, args)?;
    let _device = options.device.as_ref().unwrap();
    Err(Error::Unimplemented("control reset".to_string()))
}

pub async fn cmd_select(
    options: &Options,
    args: &args::control::ControlSelectArgs,
) -> Result<(), Error> {
    check_device(options, args)?;
    let _device = options.device.as_ref().unwrap();
    Err(Error::Unimplemented("control select".to_string()))
}

pub async fn cmd_gpio(
    options: &Options,
    args: &args::control::ControlGpioArgs,
) -> Result<(), Error> {
    check_device(options, args)?;
    let _device = options.device.as_ref().unwrap();
    Err(Error::Unimplemented("control gpio".to_string()))
}

// Resolve poke input — either a single byte value or the contents of a file.
//
// The ArgGroup on the args structs guarantees exactly one of these is Some.
fn poke_data(value: Option<u8>, input: Option<&String>) -> Result<Vec<u8>, Error> {
    if let Some(byte) = value {
        Ok(vec![byte])
    } else if let Some(path) = input {
        std::fs::read(path).map_err(|e| Error::Other(e.to_string()))
    } else {
        // Clap ArgGroup ensures this is unreachable, but be explicit
        Err(Error::Other("No data source specified".to_string()))
    }
}

pub async fn cmd_poke_memory(
    options: &Options,
    args: &args::control::ControlPokeMemoryArgs,
) -> Result<(), Error> {
    check_device(options, args)?;
    let device = options.device.as_ref().unwrap();

    let data = poke_data(args.byte, args.input.as_ref())?;
    write_memory(device, args.address, &data).await?;

    if options.verbose {
        println!("Wrote {} byte(s) to 0x{:08x}", data.len(), args.address);
    }

    Ok(())
}

pub async fn cmd_poke_live(
    options: &Options,
    args: &args::control::ControlPokeLiveArgs,
) -> Result<(), Error> {
    let data = poke_data(args.byte, args.input.as_ref())?;
    let (address, _length) =
        check_live_read_write(options, args.address, Some(data.len() as u32), args)?;
    let device = options.device.as_ref().unwrap();

    if args.delta {
        let current = read_memory(device, address, data.len() as u32).await?;

        // Build runs of consecutive changed bytes
        let mut runs: Vec<(u32, Vec<u8>)> = Vec::new();
        for (i, b) in data.iter().copied().enumerate() {
            if current.get(i).copied().unwrap_or(!b) != b {
                let addr = address + i as u32;
                #[allow(clippy::collapsible_if)]
                if let Some((start, bytes)) = runs.last_mut() {
                    if *start + bytes.len() as u32 == addr {
                        bytes.push(b);
                        continue;
                    }
                }
                runs.push((addr, vec![b]));
            }
        }

        let dry_run_str = if args.dry_run { "[dry-run] " } else { "" };

        // Write the deltas
        let delta_count: usize = runs.iter().map(|(_, b)| b.len()).sum();
        for (addr, bytes) in &runs {
            if options.verbose {
                println!(
                    "{dry_run_str}Writing {} byte(s) to 0x{addr:08x}",
                    bytes.len()
                );
            }
            if !args.dry_run {
                write_memory(device, *addr, bytes).await?;
            }
        }

        if runs.is_empty() {
            println!("{dry_run_str}No differences found - no data written.");
        } else {
            if options.verbose {
                println!("{dry_run_str}{} contiguous blocks written", runs.len())
            }
            println!(
                "{dry_run_str}Applied {delta_count} delta byte(s) of {} to live ROM offset 0x{:08x}",
                data.len(),
                args.address
            );
        }
    } else {
        write_memory(device, address, &data).await?;
        println!(
            "Wrote {} byte(s) to live ROM offset 0x{:08x}",
            data.len(),
            args.address
        );
    }

    Ok(())
}

const FLASH_SIZE: u32 = 2 * 1024 * 1024;
const SECTOR_SIZE: u32 = 4096;

fn build_erase_ranges(args: &args::control::ControlEraseArgs) -> Result<Vec<(u32, u32)>, Error> {
    if args.all {
        return Ok(vec![(0, FLASH_SIZE)]);
    }

    let offsets: Vec<u32> = if !args.address.is_empty() {
        args.address
            .iter()
            .map(|&a| {
                if a < FLASH_BASE {
                    Err(Error::InvalidArgument(format!(
                        "address {a:#010x} is below flash base {FLASH_BASE:#010x}"
                    )))
                } else {
                    Ok(a - FLASH_BASE)
                }
            })
            .collect::<Result<_, _>>()?
    } else {
        args.offset.clone()
    };

    if offsets.len() != args.size.len() {
        return Err(Error::InvalidArgument(format!(
            "got {} offset/address(es) but {} size(s)",
            offsets.len(),
            args.size.len()
        )));
    }

    Ok(offsets.into_iter().zip(args.size.iter().copied()).collect())
}

fn validate_erase_ranges(ranges: &[(u32, u32)]) -> Result<(), Error> {
    for (offset, size) in ranges {
        if offset % SECTOR_SIZE != 0 {
            return Err(Error::InvalidArgument(format!(
                "offset {offset:#x} is not {SECTOR_SIZE}-byte aligned"
            )));
        }
        if *size == 0 || size % SECTOR_SIZE != 0 {
            return Err(Error::InvalidArgument(format!(
                "size {size:#x} must be a non-zero multiple of {SECTOR_SIZE:#x}"
            )));
        }
        if offset + size > FLASH_SIZE {
            return Err(Error::InvalidArgument(format!(
                "range {offset:#x}+{size:#x} exceeds flash size {FLASH_SIZE:#x}"
            )));
        }
    }
    Ok(())
}

fn confirm_erase(options: &Options, device: &Device, ranges: &[(u32, u32)]) -> Result<bool, Error> {
    let total_kb = ranges.iter().map(|(_, s)| s).sum::<u32>() / 1024;
    println!(
        "This will erase {total_kb}KB across {} range(s) on device: {device}",
        ranges.len()
    );
    if options.verbose {
        for (offset, size) in ranges {
            println!(
                "  {size:#x} bytes ({}KB) at {:#010x}",
                size / 1024,
                FLASH_BASE + offset
            );
        }
    }

    if options.yes {
        println!("Auto-accepted (--yes)");
        return Ok(true);
    }

    print!("Are you sure? (y/N): ");
    std::io::stdout()
        .flush()
        .map_err(|e| Error::Other(e.to_string()))?;

    let mut input = String::new();
    std::io::stdin()
        .read_line(&mut input)
        .map_err(|e| Error::Other(e.to_string()))?;

    Ok(matches!(input.trim().to_lowercase().as_str(), "y" | "yes"))
}

async fn ensure_stopped(options: &mut Options) -> Result<(), Error> {
    let device = options.device.as_ref().unwrap();
    if !device.is_running() {
        return Ok(());
    }

    if options.verbose {
        println!("Device is running, rebooting into stopped mode...");
    }
    let serial = device.serial.clone();
    reboot(device, &RebootArgs::stopped(false, false)).await?;

    let selector = serial.as_deref();
    let new_device = select_device(selector, options.unrecognised, &options.vid_pid).await?;

    if new_device.is_running() {
        return Err(Error::DeviceStillRunning);
    }
    options.device = Some(new_device);
    Ok(())
}

async fn erase_ranges(options: &Options, ranges: &[(u32, u32)]) -> Result<(), Error> {
    let device = options.device.as_ref().unwrap();

    println!("Erasing flash - DO NOT DISCONNECT");

    for (offset, size) in ranges {
        if options.verbose {
            let address = FLASH_BASE + offset;
            println!("  Erasing {size:#x} bytes at {address:#010x}");
        }
        flash_erase(device, *offset, *size).await?;
    }

    let total_kb = ranges.iter().map(|(_, s)| s).sum::<u32>() / 1024;
    println!("Erased {total_kb}KB of flash");
    Ok(())
}

async fn reboot_after_erase(
    options: &Options,
    args: &args::control::ControlEraseArgs,
) -> Result<(), Error> {
    let device = options.device.as_ref().unwrap();

    let reboot_args = args.into();
    reboot(device, &reboot_args).await?;
    if !reboot_args.is_none() {
        println!("Rebooted device into {} mode", reboot_args.mode);
    }

    Ok(())
}

pub async fn cmd_erase(
    options: &mut Options,
    args: &args::control::ControlEraseArgs,
) -> Result<(), Error> {
    check_device(options, args)?;

    let ranges = build_erase_ranges(args)?;
    validate_erase_ranges(&ranges)?;

    if !confirm_erase(options, options.device.as_ref().unwrap(), &ranges)? {
        println!("Aborted");
        return Ok(());
    }

    if !args.no_reboot {
        ensure_stopped(options).await?;
    } else if options.verbose {
        println!("BNot rebooting before erase");
    }
    erase_ranges(options, &ranges).await?;
    if !args.no_reboot {
        reboot_after_erase(options, args).await
    } else {
        if options.verbose {
            println!("Not rebooting after erase");
        }
        Ok(())
    }
}
