// Copyright (c) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT licence

#![no_std]
#![no_main]
#![feature(type_alias_impl_trait)]
#![feature(impl_trait_in_assoc_type)]

extern crate alloc;

#[allow(unused_imports)]
use log::{debug, error, info, trace, warn};

use alloc::format;
use alloc::vec::Vec;

use embassy_executor::Spawner;
use embassy_executor::main as embassy_main;
use embassy_rp::{clocks::ClockConfig, config::Config};
use embassy_time::Timer;

use embedded_alloc::LlffHeap as Heap;
use panic_rtt_target as _;

mod error;
mod hw;
mod logs;
mod rom;

use hw::Board;
#[cfg(feature = "eprom-16bit")]
use rom::Rom27C400;
#[cfg(feature = "eprom-8bit")]
use rom::RomEprom8;

const PKG_VERSION: &str = env!("CARGO_PKG_VERSION");

#[global_allocator]
static HEAP: Heap = Heap::empty();

#[embassy_main]
async fn main(_spawner: Spawner) -> ! {
    // Initialize the heap allocator
    {
        use core::mem::MaybeUninit;
        const HEAP_SIZE: usize = 1024;
        static mut HEAP_MEM: [MaybeUninit<u8>; HEAP_SIZE] = [MaybeUninit::uninit(); HEAP_SIZE];
        unsafe { HEAP.init(&raw mut HEAP_MEM as usize, HEAP_SIZE) }
    }

    // Init logging
    logs::init_rtt();

    info!("-----");
    info!("One ROM Lab NEW v{}", PKG_VERSION);
    info!("Copyright (c) 2026 Piers Finlayson");

    info!("-----");
    debug!("RP2350 target");

    // Initialize peripherals with clocks set to 150MHz
    let mut config = Config::default();
    let clocks = ClockConfig::system_freq(150_000_000).expect("Failed to configure clocks");
    config.clocks = clocks;
    let _p = embassy_rp::init(config);

    debug!("Clocks configured to 150MHz");

    // Get the board object
    let board = hw::get_board();

    // Set up the LED
    let led_pins = board.led_pins();
    let [mut led] = led_pins;

    // Flash LED to show we're alive
    led.set_as_output();
    for _ in 0..2 {
        led.set_high();
        Timer::after_millis(200).await;
        led.set_low();
        Timer::after_millis(200).await;
    }

    // Get the other pins
    let addr_pins = board.addr_pins();
    let data_pins = board.data_pins();
    let cs_pins = board.cs_pins();
    #[cfg(feature = "eprom-16bit")]
    let special_pins = board.special_pins();

    // Create the ROM object
    #[cfg(feature = "eprom-16bit")]
    let mut rom = Rom27C400::new(addr_pins, data_pins, cs_pins, special_pins);
    #[cfg(feature = "eprom-8bit")]
    let mut rom = RomEprom8::new(addr_pins, data_pins, cs_pins);
    rom.init();

    debug!("-----");

    loop {
        info!("Reading {} ...", rom.type_as_str());
        let results = rom.read();
        for r in &results {
            info!(
                "{} SHA1: {} checksum: {:#010X}",
                r.mode,
                hex::encode(r.sha1),
                r.checksum
            );
        }
        if results.len() >= 2 {
            let match_ok = results
                .windows(2)
                .all(|w| w[0].sha1 == w[1].sha1 && w[0].checksum == w[1].checksum);
            info!("Match: {}", match_ok);
        }
        let ts_failures = results
            .iter()
            .map(|r| format!("{}: {}", r.mode, r.failures))
            .collect::<Vec<_>>()
            .join(", ");
        info!("Tristate failures: {ts_failures}");
        info!("-----");
        embassy_time::Timer::after_secs(1).await;
    }
}
