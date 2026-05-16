# RF Analysis & Smith Chart

VioraEDA supports S-Parameter analysis for RF circuit design with an interactive Smith Chart display.

## Setting Up RF Analysis

1. Place a **Voltage Source** — this is your port 1 stimulus. Set an **AC amplitude** (e.g., 1 V).
2. Add **RF ports** using `SPICE Directive` items on the schematic:
   ```
   .sp DEC 100 10 10G
   ```
   Or select **RF S-Parameter** from the analysis mode dropdown and set parameters in the dashboard.

3. When using .sp directives, ngspice automatically detects S-parameter output variables (S11, S21, S12, S22).

## Smith Chart

After an S-Parameter simulation, the **RF Analysis** tab appears in the results panel with the Smith Chart.

### Reading the Chart
- **Unity circle** — boundary of passive reflection (|&#915;| = 1)
- **Constant resistance circles** — r = 0.2, 0.5, 1.0, 2.0, 5.0
- **Constant reactance arcs** — x = 0.2, 0.5, 1.0, 2.0, 5.0
- **Real axis** (center line) — purely resistive impedance

### Trace Colors
| Trace | Color | Meaning |
|-------|-------|---------|
| S11 | Blue | Input reflection coefficient |
| S21 | Yellow | Forward gain (dB shown in tooltip) |
| S12 | Purple | Reverse isolation |
| S22 | Red | Output reflection coefficient |

### Interaction
- **Hover** over a trace to see a tooltip with &#915; (reflection coefficient), Z (impedance in &#937;), and VSWR.
- **Click/drag** the **timeline slider** to highlight a specific frequency point.
- The highlighted point shows a red circle with the complex value.

### Interpreting S11 = 0
S11 = 0 + j0 means the input is perfectly matched to the reference impedance (usually 50 &#937;). The Smith Chart shows the dot at the center (&#915; = 0). Impedance Z = 50 + j0 &#937;.

## Frequency Sweep

The timeline slider below the plot controls the frequency cursor. The current frequency is labeled in the timeline display. S-Parameter points are listed in the **sParameterResults** array internally.

## Reference Impedance

Default Z0 = 50 &#937;. Change via the **Z0** parameter in the simulation dashboard when RF S-Parameter is selected.
