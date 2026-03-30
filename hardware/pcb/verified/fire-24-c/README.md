# Fire 24 Rev C

**Verified**

23xx Fire (RP2350 24 pin) combined USB+SWD One ROM PCB.  Includes 2 image select jumpers.  Supports PIO and CPU serving algorithms.

## Contents

- [Schematic](./fire-24-c-schematic.pdf)
- [Fab Files](fab/)
- [KiCad Design Files](kicad/)
- [Errata](#errata)
- [Notes](#notes)
- [Changelog](#changelog)

## Errata

## Notes

In the 2332 base, CS1 and CS2 are non-contiguous.  This is a side-effect of making the 27xx OE/CE pins contiguous.  There is no way to make them both contiguous, and also make the X pins contiguous with the CS pin in the 2364 case.  This design makes the trade-off to keep the pins contiguous for more ROM types.  The PIO algorithm also supports non-contiguous CS pins, as used for the 2332, so this is not a problem.

## Changelog

Changes from USB rev B
- Moved SWD pads from underside to dedicated programming pins at the top.
- Reduced image select pins from 4 to 2, to make room for SWD pins.
- Other minor changes.