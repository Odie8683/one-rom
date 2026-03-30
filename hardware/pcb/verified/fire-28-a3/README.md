# One ROM Fire 28 pin

**Verified** - both RP2350 and RP2354 versions.

A3 revision of the 28 pin Fire ROM board.  Combined USB/Pro version with USB-C and SWD header pins.

This design only differs from A2 in correcting the SWD CLK/DIO silkscreen labels.

Very minor difference from revision A2:

- CLK/DIO silkscreen labels swapped to correct positions.

There are two versions of the BOM/POS files:

- The original version with an RP2350A and external flash
- A lower cost version with an RP2354A and no external flash

## Contents

- [Schematic](./fire-28-a3-schematic.pdf)
- [Fab Files](fab/)
- [KiCad Design Files](kicad/)
- [Errata](#errata)
- [Notes](#notes)
- [Changelog](#changelog)
- [BOM](#bom)

## Errata

#121 - The front silkscreen has 28 A3 printed by pin 14.  28 A3 signifies One ROM 28, rev A3.  However, it could be misconstrued as meaning pin 28 pin board, which might lead someone to insert the board incorrectly.  This marking will be moved or changed in a future revision.

## Notes

## Changelog

## BOM

See fab files.
