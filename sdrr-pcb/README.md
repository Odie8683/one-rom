# sdrr-pcb

Contains SDRR PCB design files.  These are organised into two directories:

- [`verified/`](verified/README.md) - Verified designs that have been tested and work.
- [`unverified/`](unverified/README.md) - Unverified designs that have not been tested.

## Recommended Revisions

Last Updated 3rd February 2026.

I recommend Fire over Ice boards for most users.  See [MCU-SELECTION.md](../docs/MCU-SELECTION.md) for a comparison between Fire and Ice capabilities.

The following table lists the recommended verified designs.  Each design has been manufactured and assembled using JLCPCB's PCB assembly service, and verified to work correctly.

| Model | MCU | Pins | USB | SWD | Image Select Pins | X Pins | PCB |
|-------|-----|------|-----|-----|-------------------|--------|-------|
| Fire  | RP2350 | 24 | Micro-B | ✓ | 4 | 2 | [fire-24-d](./verified/fire-24-d/README.md) |
| Fire  | RP2354 | 24 | C | ✓ | 4 | 2 | [fire-24-e](./verified/fire-24-e/README.md) |
| Ice   | STM3F4 | 24 | Micro-B | ✓ | 4 | 2 | [ice-24-j](./verified/ice-24-j/README.md) |
| Fire  | RP2350 or RP2354 | 28 | C | ✓ | 2 | n/a | [fire-28-a3](./verified/fire-28-a3/README.md) |

## Notes on Fabrication

All recommended designs have been manufactured and assembled using JLCPCB's PCB assembly service.  The gerbers and BOM/POS files in each revision's `fab/` directory are compatible with JLCPCB's requirements.

Some variants offer multiple sets of BOM/POS files or different combinations of components.  Read the relevant revision's README for details.

You need to exercise care when submitting the order to ensure the correct options are selected.  In particular:

- Select economic assembly, not standard assembly, for cost reasons.
- Use standard 1.6mm PCB thickness.  You probably want HASL with lead finish for cost reasons.
- You have to select the desired PCB colour as part of the PCB ordering process.  Red is recommended for Fire and blue for Ice.
- As of late 2025 you no longer need to remove JLC's order ID from the PCBs, but it is worth checking this is still the case when you place your order.
- After uploading the BOM and position files for assembly, you need to ensure all parts are available and checked.
- You must ensure JLC is showing the correct orientation for all components.  There are silkscreen markings for pin 1 on all ICs and polarized components - ensure this matches the pink dot of the component in the viewer.  Also ensure the USB connector is the correct orientation.

Occasionally, some chosen parts are out of stock on JLCPCB.  One ROM has been designed with extremely common parts where possible, which JLC tend to hold large stocks of.  However, if a part is unavailable you may need to select an alternative part.  Ensure the alternative part has the same footprint and electrical characteristics (e.g. capacitance, voltage rating, etc) as the original part.  If in doubt, query within github discussions for advice. 

JLC may raise an issue with you after submitting your order.  Common issues include:
- JLC may ask to add side rails with stamp holes, to help with the PCB assembly process.  While One ROM has been successfully fabbed without these, if JLC recommends them it is best to accept this change, as otherwise they may not warrant the assembled boards.  The side rails can be snapped off using the "mouse bites" (stamp holes) after assembly.  JLC may or may not accept replacing stamp holes with v-score - as v-score is not available on their economic service.

If in doubt, query within github discussions for advice.

Assembly errors:
- Ice boards have rarely, if ever, experienced assembly errors.
- The first few batches of Fire boards experienced some assembly issues, particularly with respect to the RP2350 MCU solder pad fillets.  More recent boards and orders have been without issue.  It is unclear whether differences in the later PCBs or improvements in JLC's assembly process are responsible for this improvement.  If you do experience assembly issues, raise with JLC support.  It would also be interesting to hear on github discussions what issues you experienced, so others can be aware.
