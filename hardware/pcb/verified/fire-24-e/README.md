# Fire 24 Rev E

**Verified** - The standard version of this design has been verified.

`*-alt-ap2112k` provides an alternative AP2112K part, if the standard AP2112K-3.3 is not available.  This alternative part has not been verified.

23xx Fire (RP2350 24 pin) combined USB+SWD One ROM PCB.  Includes 4 image select jumpers.  Supports PIO and CPU serving algorithms.  USB-C.

This board is intended for production and panelisation:
- The move to USB-C makes panlisation simpler, with micro-B USB requiring cutouts in the PCB for each board in the panel.
- The board is now rectangular, making it easier to panelise.
- Pin assigment is slightly changed from D.
- Many passives moved to 0201 to make USB-C viable.  This is requires JLC standard assembly, but that is also required for panelisation.
- Requires RP2354A only (in-built flash), as no footprint for external flash is provided.  Again, this makes USB-C viable.

## Contents

- [Schematic](./fire-24-e-schematic.pdf)
- [Fab Files](fab/)
- [KiCad Design Files](kicad/)
- [Errata](#errata)
- [Notes](#notes)
- [Changelog](#changelog)
- [Panelisation](#panelisation)

## Errata

## Notes

In the 2332 base, CS1 and CS2 are non-contiguous.  This is a side-effect of making the 27xx OE/CE pins contiguous.  There is no way to make them both contiguous, and also make the X pins contiguous with the CS pin in the 2364 case.  This design makes the trade-off to keep the pins contiguous for more ROM types.  The PIO algorithm also supports non-contiguous CS pins, as used for the 2332, so this is not a problem.

## Changelog

Changes from USB rev E
- USB-C connector instead of micro-B.
- Rectangular board shape for easier panelisation.
- Some passives changed to 0201.
- Only supports RP2354A (in-built flash), as no footprint for external flash is provided.
- Pin assignment changes.

## Panelisation

This revision is intended for panelisation.
- With JLC, when selecting the number of PCBs, choose the number panels.
- Under panlisation, choose "by JLC" and choose the number of boards per panel (e.g. 5 columns x 4 rows = 20 boards per panel), plus edge rails (left and right).
- JLC will automatically add V-grooves.
- Under assembly, when selecting BOM/POS file, select "single piece help me repeat", and JLC will include X times the components for X boards in the panel x the number of panels (PCBs).