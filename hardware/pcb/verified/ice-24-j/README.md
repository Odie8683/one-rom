# One ROM Ice USB J 24 Pin 

**Verified**

Adds 2 more image select jumpers to Ice USB I, for a total of 4 image select jumpers.

Note that firmware version v0.6.1 is required to support 4 image select jumpers - v0.6.0 only supports the first 2.

There is a single set of gerbers, and two sets of POS/BOM files:
- One for the STM32F411RET6 MCU.
- One for the STM32F446RCT6 MCU.

To create the BOM for an alternate MCU, replace the microcontroller part number.  For the STM32F405RET6, you must also replace RC4/RC5/C9 - see the schematic for details.

## Contents

- [Schematic](ice-24-j-schematic.pdf)
- [Fab Files and BOM](fab/)
- [KiCad Design Files](kicad/)
- [Errata](#errata)
- [Notes](#notes)
- [Changelog](#changelog)

## Errata

## Notes

## Changelog

- Two more image select GPIOs added, and existing ones modified