# PANELISING PCBs

One ROM can be panelised for efficient manufacturing at scale.  At the time of writing. 150 `fire-28-a` (RP2354A variant) One ROMs can be manufactured and assembled just under £3 ($4) per unit, including all shipping and taxes to the UK.

If panelising you are recommended to choose USB-C variants, as these lend themselves better to panelised more easily.  Micro-USB variants (such as fire-24-d) require a cut-out in the panel for the connector to overhang the edge of the PCB, which is complicated to achieve, at least in an automated fashion.

It is possible to build the gerbers for the panel yourself, either manually, or using a tool like kikit.  However, with USB-C variants, JLC's panelising service is the least effort.

To panelise using JLC:
- Upload the gerbers for a single piece (found the in `fab` folder of the design you are using) to JLC as normal
- Choose yor PCB colour
- Under PCB quantity, select the number of **panels** you want (e.g. 5)
- Under panlisation select "Panel by JLCPCB" or similar.  Then select
    - The number of columns (e.g. 5)
    - The number of rows (e.g. 4)
    - Edge rails (recommend left and right)
    As part of this check that the lines between the PCBs are marked "V-CUT".
- This example would give 100 One ROMs (5 columns x 4 rows x 5 panels = 100)
- Select assembly as normal, making sure you select top _and_ bottom for boards that require it
- When asked whether your BOM is for a single PCB or all PCBs, select "single piece pls help me duplicate" or similar.  JLC then multiply the part quantities in your BOM by the number of One ROMs.
- Ensure all parts are selected as usual.
- Ensure the parts placement and rotation is correct for the single PCB - JLC will then duplicate the placement for all PCBs in the panel.
- Place your order.

When the panels arrive, separate the panels into individual PCBs by snapping them apart along the V-CUT lines, program, test and enjoy!
