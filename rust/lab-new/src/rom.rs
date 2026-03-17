// Copyright (c) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT licence

// Expansion notes:
//
// RomCore<ADDR, DATA> is the shared inner struct for all EPROM types.  It owns
// the address lines, data lines, CE, and OE pins and provides common init,
// addressing, byte-reading, and tristate-testing logic.
//
// RomEprom8<N> is a thin wrapper around RomCore<N, 8> for all 8-bit EPROMs
// (27xx series).  N is the address line count:
//   N=10 -> 2708, N=11 -> 2716, N=12 -> 2732, N=13 -> 2764, N=14 -> 27128,
//   N=15 -> 27256, N=16 -> 27512, N=17 -> 27C010, N=18 -> 27C020, N=19 -> 27C040
//
// Rom27C400 wraps RomCore<19, 16> with an additional BYTE# pin and adds
// dual 8-bit/16-bit read mode logic.
//
// Mask ROMs (23xxx series) will require a separate RomMask type due to
// configurable CS line polarity and potentially multiple CS lines.

#[allow(unused_imports)]
use log::{debug, error, info, trace, warn};

#[cfg(feature = "eprom-8bit")]
use alloc::vec;
use alloc::vec::Vec;
use core::num::Wrapping;
use embassy_rp::gpio::{Flex, Pull};
use sha1::{Digest, Sha1};

/// sha1 digest, wrapping 32-bit checksum, tristate failure count
pub struct ModeResult {
    pub mode: BitMode,
    pub sha1: [u8; 20],
    pub checksum: u32,
    pub failures: u32,
}

pub type ReadResult = Vec<ModeResult>;

struct ChecksumState(Wrapping<u32>);

impl ChecksumState {
    fn new() -> Self {
        Self(Wrapping(0))
    }

    #[inline]
    fn update(&mut self, byte: u8) {
        self.0 = self.0 + Wrapping(byte as u32);
    }

    fn finish(self) -> u32 {
        self.0 .0
    }
}

#[derive(Debug, Clone, Copy)]
pub enum BitMode {
    Eight,
    #[cfg(feature = "eprom-16bit")]
    Sixteen,
}

impl core::fmt::Display for BitMode {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            BitMode::Eight => write!(f, "8-bit"),
            #[cfg(feature = "eprom-16bit")]
            BitMode::Sixteen => write!(f, "16-bit"),
        }
    }
}

// ---------------------------------------------------------------------------
// RomCore — shared inner struct
// ---------------------------------------------------------------------------

/// Shared inner struct for all EPROM types.
///
/// ADDR: number of address lines.
/// DATA: number of data lines (8 for 8-bit EPROMs, 16 for 27C400).
///
/// Data lines are assumed to be arranged as contiguous 8-bit bytes, so
/// DATA must be a multiple of 8.  read_data_byte(n) reads bits [n*8..(n+1)*8].
struct RomCore<const ADDR: usize, const DATA: usize> {
    addr: [Flex<'static>; ADDR],
    data: [Flex<'static>; DATA],
    ce: Flex<'static>,
    oe: Flex<'static>,
}

impl<const ADDR: usize, const DATA: usize> RomCore<ADDR, DATA> {
    fn new(
        addr: [Flex<'static>; ADDR],
        data: [Flex<'static>; DATA],
        ce: Flex<'static>,
        oe: Flex<'static>,
    ) -> Self {
        Self { addr, data, ce, oe }
    }

    fn init(&mut self) {
        for pin in self.addr.iter_mut() {
            pin.set_as_output();
            pin.set_low();
        }
        for pin in self.data.iter_mut() {
            pin.set_pull(Pull::Down);
            pin.set_as_input();
        }
        self.ce.set_as_output();
        self.ce.set_high();
        self.oe.set_as_output();
        self.oe.set_high();
    }

    #[inline(always)]
    fn set_addr(&mut self, addr: usize) {
        for (i, pin) in self.addr.iter_mut().enumerate() {
            if addr & (1 << i) != 0 {
                pin.set_high();
            } else {
                pin.set_low();
            }
        }
    }

    /// Reads 8 data bits starting at pin data[byte_index * 8].
    #[inline(always)]
    fn read_data_byte(&self, byte_index: usize) -> u8 {
        let mut val = 0u8;
        let offset = byte_index * 8;
        for (i, pin) in self.data[offset..offset + 8].iter().enumerate() {
            if pin.is_high() {
                val |= 1 << i;
            }
        }
        val
    }

    /// Tests OE and CE tristate independently, checking the first `byte_count`
    /// data bytes after each de-assertion.  Assumes both CE and OE are active
    /// (low) on entry; restores that state on exit.
    ///
    /// Returns the number of failures (0..=2).
    fn test_tristate(&mut self, byte_count: usize, settle_cycles: u32) -> u32 {
        let mut failures = 0u32;

        self.oe.set_high();
        cortex_m::asm::delay(settle_cycles);
        if (0..byte_count).any(|i| self.read_data_byte(i) != 0) {
            failures += 1;
        }
        self.oe.set_low();

        self.ce.set_high();
        cortex_m::asm::delay(settle_cycles);
        if (0..byte_count).any(|i| self.read_data_byte(i) != 0) {
            failures += 1;
        }
        self.ce.set_low();

        failures
    }
}

// ---------------------------------------------------------------------------
// RomEprom8 — 8-bit EPROMs
// ---------------------------------------------------------------------------

/// 8-bit EPROM reader.  N is the number of address lines.
///
/// Supported devices (by N):
///   12 -> 2732,  13 -> 2764,
///   14 -> 27128, 15 -> 27256, 16 -> 27512,
///   17 -> 27C010, 18 -> 27C020, 19 -> 27C040
#[cfg(feature = "eprom-8bit")]
pub struct RomEprom8<const N: usize> {
    core: RomCore<N, 8>,
}

#[cfg(feature = "eprom-8bit")]
impl<const N: usize> RomEprom8<N> {
    // Empirically determined read delay cycles for stable reads at 150MHz.
    const READ_DELAY_CYCLES: u32 = 8;
    const TRISTATE_SETTLE_CYCLES: u32 = 100;

    pub fn new(
        addr: [Flex<'static>; N],
        data: [Flex<'static>; 8],
        mut cs_pins: Vec<Flex<'static>>,
    ) -> Self {
        if cs_pins.len() != 2 {
            panic!("Expected exactly two CS pins for CE and OE, got {}", cs_pins.len());
        }
        let ce = cs_pins.pop().expect("No CE pin found");
        let oe = cs_pins.pop().expect("No OE pin found");
        Self { core: RomCore::new(addr, data, ce, oe) }
    }

    pub fn init(&mut self) {
        self.core.init();
    }

    pub fn type_as_str(&self) -> &'static str {
        match N {
            19 => "27C040",
            18 => "27C020",
            17 => "27C010",
            16 => "27512",
            15 => "27256",
            14 => "27128",
            13 => "2764",
            12 => "2732",
            _ => "unknown",
        }
    }

    // Don't inline so the function can be analysed in map/dis files.
    #[inline(never)]
    fn read_all(&mut self, sha: &mut Sha1, csum: &mut ChecksumState) -> u32 {
        let mut failures = 0u32;
        self.core.ce.set_low();
        self.core.oe.set_low();

        for addr in 0..(1usize << N) {
            self.core.set_addr(addr);
            cortex_m::asm::delay(Self::READ_DELAY_CYCLES);

            let byte = self.core.read_data_byte(0);
            sha.update([byte]);
            csum.update(byte);

            failures += self.core.test_tristate(1, Self::TRISTATE_SETTLE_CYCLES);
        }

        self.core.oe.set_high();
        self.core.ce.set_high();

        failures
    }

    pub fn read(&mut self) -> ReadResult {
        let mut sha = Sha1::new();
        let mut csum = ChecksumState::new();
        let failures = self.read_all(&mut sha, &mut csum);
        let mut sha1 = [0u8; 20];
        sha1.copy_from_slice(&sha.finalize());

        vec![ModeResult {
            mode: BitMode::Eight,
            sha1,
            checksum: csum.finish(),
            failures,
        }]
    }
}

// ---------------------------------------------------------------------------
// Rom27C400 — 40-pin 512KB EPROM
// ---------------------------------------------------------------------------

/// 27C400 40-pin 512KB EPROM reader.
///
/// addr[0]   = A-1 (byte-select address line)
/// addr[1..19] = A0..A17
/// data[0..7]  = D0-D7  (low byte)
/// data[8..15] = D8-D15 (high byte)
///
/// 8-bit mode:  BYTE# low, 524288 byte addresses, A-1 selects low/high byte.
/// 16-bit mode: BYTE# high, 262144 word addresses, both bytes read per cycle.
///
/// In 8-bit mode the read delay is longer than 16-bit because A-1 participates
/// in address decoding and requires additional settling time.
#[cfg(feature = "eprom-16bit")]
pub struct Rom27C400 {
    core: RomCore<19, 16>,
    byte_n: Flex<'static>,
}

#[cfg(feature = "eprom-16bit")]
impl Rom27C400 {
    // Empirically determined read delay cycles for stable reads at 150MHz.
    // 8-bit delay is longer: A-1 participates in address decoding and needs
    // additional settling time compared to the static BYTE# in 16-bit mode.
    const READ_DELAY_CYCLES_8_BIT: u32 = 12;
    const READ_DELAY_CYCLES_16_BIT: u32 = 8;
    // Longer settle than 8-bit EPROMs due to weaker pull-downs and test-fixture
    // capacitance/inductance (e.g. pogo pins).
    const TRISTATE_SETTLE_CYCLES: u32 = 200;

    pub fn new(
        addr: [Flex<'static>; 19],
        data: [Flex<'static>; 16],
        mut cs_pins: Vec<Flex<'static>>,
        mut special_pins: Vec<Flex<'static>>,
    ) -> Self {
        if special_pins.len() != 1 {
            panic!("Expected exactly one special pin for 27C400 BYTE#, got {}", special_pins.len());
        }
        let byte_n = special_pins.pop().unwrap();
        if cs_pins.len() != 2 {
            panic!("Expected exactly two CS pins for 27C400 CE and OE, got {}", cs_pins.len());
        }
        let ce = cs_pins.pop().expect("No CE pin found");
        let oe = cs_pins.pop().expect("No OE pin found");
        Self { core: RomCore::new(addr, data, ce, oe), byte_n }
    }

    pub const fn type_as_str(&self) -> &'static str {
        "27C400"
    }

    pub fn init(&mut self) {
        self.core.init();
        self.byte_n.set_as_output();
        self.byte_n.set_high();
    }

    // Don't inline so the function can be analysed in map/dis files.
    #[inline(never)]
    fn read_mode(&mut self, mode: BitMode, sha: &mut Sha1, csum: &mut ChecksumState) -> u32 {
        let mut failures = 0u32;

        let (addr_count, addr_shift, read_delay, byte_count) = match mode {
            BitMode::Eight => (1 << 19, 0, Self::READ_DELAY_CYCLES_8_BIT, 1usize),
            BitMode::Sixteen => (1 << 18, 1, Self::READ_DELAY_CYCLES_16_BIT, 2usize),
        };

        match mode {
            BitMode::Eight => self.byte_n.set_low(),
            BitMode::Sixteen => self.byte_n.set_high(),
        }
        self.core.ce.set_low();
        self.core.oe.set_low();

        for addr in 0..addr_count {
            self.core.set_addr(addr << addr_shift);
            cortex_m::asm::delay(read_delay);

            let lo = self.core.read_data_byte(0);
            sha.update([lo]);
            csum.update(lo);

            if let BitMode::Sixteen = mode {
                let hi = self.core.read_data_byte(1);
                sha.update([hi]);
                csum.update(hi);
            }

            failures += self.core.test_tristate(byte_count, Self::TRISTATE_SETTLE_CYCLES);
        }

        self.core.oe.set_high();
        self.core.ce.set_high();
        self.byte_n.set_high();

        failures
    }

    /// Reads the ROM in both 8-bit byte mode and 16-bit word mode.
    ///
    /// Returns results for both modes.  If the ROM and wiring are correct,
    /// both SHA1s and checksums will be identical, and both failure counts
    /// will be zero.
    pub fn read(&mut self) -> ReadResult {
        let mut results = Vec::new();

        for mode in [BitMode::Eight, BitMode::Sixteen] {
            let mut sha = Sha1::new();
            let mut csum = ChecksumState::new();
            let failures = self.read_mode(mode, &mut sha, &mut csum);
            let mut sha1 = [0u8; 20];
            sha1.copy_from_slice(&sha.finalize());
            results.push(ModeResult { mode, sha1, checksum: csum.finish(), failures });
        }

        results
    }
}