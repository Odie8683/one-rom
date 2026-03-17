// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

//! One ROM picoboot extensions

pub const ONEROM_MAGIC: u32 =
    b'O' as u32 | (b'N' as u32) << 8 | (b'E' as u32) << 16 | (b'R' as u32) << 24;
pub const ONEROM_CMD_SET_LED: u8 = 0x01;

#[repr(u8)]
#[derive(Debug, Clone, Copy)]
pub enum LedSubCmd {
    Off = 0x00,
    On = 0x01,
    Beacon = 0x02,
    Flame = 0x03,
}
