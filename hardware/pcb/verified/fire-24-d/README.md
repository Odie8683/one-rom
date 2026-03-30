# Fire 24 Rev D

**Verified** - both RP2350 and RP2354 versions.

23xx Fire (RP2350 24 pin) combined USB+SWD One ROM PCB.  Includes 4 image select jumpers.  Supports PIO and CPU serving algorithms.

Note that firmware version v0.6.1+ is required to support 4 image select jumpers - v0.6.0 only supports the first 2.

There are two variants of the BOM/POS files - one with the RP2350A and external flash, the second with the RP2354, which has built-in flash, so the external flash is omitted.  The RP2354 version is cheaper.

## Contents

- [Schematic](./fire-24-d-schematic.pdf)
- [Fab Files](fab/)
- [KiCad Design Files](kicad/)
- [Errata](#errata)
- [Notes](#notes)
- [Changelog](#changelog)

## Errata

## Notes

In the 2332 base, CS1 and CS2 are non-contiguous.  This is a side-effect of making the 27xx OE/CE pins contiguous.  There is no way to make them both contiguous, and also make the X pins contiguous with the CS pin in the 2364 case.  This design makes the trade-off to keep the pins contiguous for more ROM types.  The PIO algorithm also supports non-contiguous CS pins, as used for the 2332, so this is not a problem.

## Changelog

Changes from USB rev D
- Connected additional sel GPIOs to SWDIO/SWCLK pins for up to 4 image select pins.
