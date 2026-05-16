# Net Voltage Summary Table

After a transient (or compatible) simulation, a summary table appears on the schematic canvas showing key statistics for each net's voltage waveform.

## What It Shows

Each row represents one net with:

| Column | Meaning |
|--------|---------|
| Clr | Color swatch — wire is colored to match on the schematic |
| Net | Net name (sorted: AutoNets last) |
| Vavg | Average voltage over the simulation interval |
| Vrms | RMS voltage |
| Vmin | Minimum voltage |
| Vmax | Maximum voltage |

## Mode-Specific Columns

| Analysis Mode | Columns |
|---------------|---------|
| Transient / Real-time | Vavg, Vrms, Vmin, Vmax |
| AC Sweep | Vmag (final), Vphase (final), Vmin, Vmax |
| DC Sweep | Vfinal, ---, Vmin, Vmax |

## Interaction

- **Drag** the table to reposition it on the canvas.
- **Click X** (top-right corner) to close the table.
- **Right-click > Copy Table** copies to clipboard as tab-separated values (paste into spreadsheet).
- **Right-click > Delete Table** removes the overlay.

## Enabling/Disabling

Toggle in **Preferences > Simulator > Show net voltage summary table on schematic after transient simulation**.

## Wire Coloring

Each net's wires are tinted to match the table row color, making it easy to visually trace nets on the schematic. Close the table to restore original wire colors.
