# Simulation Basics

VioraEDA supports multiple SPICE analysis types. Select the mode from the **SIMULATION DASHBOARD** in the left sidebar.

## Analysis Types

### Transient (.tran)
Time-domain simulation. Observes circuit behavior over a time interval.

**Typical use:** Switching circuits, amplifiers, signal integrity.

**Parameters:** Stop time, time step, steady-state detection.

### DC Operating Point (.op)
Calculates steady-state bias voltages and currents. No time sweep — single snapshot.

**Typical use:** Verify biasing before transient.

### DC Sweep (.dc)
Sweeps a source voltage and records the circuit's DC response at each step.

**Typical use:** Transfer characteristics, logic thresholds, IV curves.

**Parameters:** Source name, start/stop voltage, step size.

### AC Sweep (.ac)
Frequency-domain analysis. Computes gain/phase vs. frequency for small-signal behavior.

**Typical use:** Filter response, amplifier bandwidth, stability margins.

**Parameters:** Sweep type (Decade/Octave/Linear), frequency range, points per decade.

### RF S-Parameter (.sp)
Network analysis for RF circuits. Produces S11, S21, S12, S22 vs. frequency, displayed on a **Smith Chart**.

**Typical use:** Impedance matching, antenna design, RF amplifier design.

**Parameters:** Frequency range, reference impedance (default 50&#937;), port mapping.

### Monte Carlo
Repeats simulation with randomly varied component tolerances.

### Parametric Sweep
Repeats simulation while stepping a component parameter.

### Sensitivity
Computes circuit response sensitivity to component variations.

### Real-time Mode
Streams transient data in real time. Place an **Oscilloscope Instrument** on the schematic to view live traces.

## Results Viewing

After simulation finishes, results appear in the **Analog Oscilloscope** dock at the bottom:

- **Waveform Viewer** — Plot signals. Right-click for cursors, FFT, math equations.
- **RF Analysis tab** — Smith Chart for S-Parameter data (auto-activated).
- **Trace Monitor** — Check/uncheck signals to toggle visibility.
- **Timeline slider** — Drag to scrub through time/frequency.
- **Measurements** — .meas results and computed statistics (Avg, RMS, Freq, Delta).

## Net Voltage Summary

After transient simulation, a **Net Voltage Summary** table appears on the schematic canvas showing Vavg, Vrms, Vmin, Vmax for each net, with color-coded wires. Toggle off in **Preferences > Simulator**.

## Probing

Place **Voltage Probes** or **Current Probes** from the toolbar onto nets/components. Probed signals appear as traces in the waveform viewer. The **Smart Probe** overlay shows net info on hover (when enabled).
