# One ROM Ice USB I 24 Pin 

Similar to ice-24-usb-h, but adds SWD programming pins and reduces image select pins to 2 (from 5).  Future firmware versions may extend from 2 to 4 image select pins without the need for additional hardware revisions (but utilising the SWD pins).

There is a single set of gerbers, and two sets of POS/BOM files:
- One for the STM32F411RET6 MCU.
- One for the STM32F446RCT6 MCU.

To create the BOM for an alternate MCU, replace the microcontroller part number.  For the STM32F405RET6, you must also replace RC4/RC5/C9 - see the schematic for details.

## Contents

- [Schematic](ice-24-i-schematic.pdf)
- [Fab Files and BOM](fab/)
- [KiCad Design Files](kicad/)
- [Errata](#errata)
- [Notes](#notes)
- [Changelog](#changelog)

## Errata

## Notes

## Changelog

- Move to 1N5819 diodes from MSK4005 (better availability and JLC basic part)
- Moved programming header to top left, replacing Sel pins C-E
- Reduced rom pin hole and ring size
- Reduced silkscreen markings on the top side
