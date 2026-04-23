# Smart Signal Guide

Smart Signal blocks let you run custom Flux logic inside your schematic simulation.

## What It Does
* Reads one or more input pins from your schematic (for example `In1`).
* Computes output values in Flux `update(t, inputs)`.
* Drives output pins (for example `Out1`) during transient simulation.

## Basic Setup
1. Place a **Smart Signal Block** on the schematic.
2. Connect at least one input and one output pin.
3. Add a load path on the output (for example a resistor to ground) so the node is physically defined.
4. Open the block code and select **Flux** engine.
5. Run **Transient (.tran)** analysis.

## Minimal Flux Example
Use this to pass input voltage directly to output:

```flux
def update(t, inputs) {
    return V("In1");
}
```

Use this to generate PWM from `In1`:

```flux
def update(t, inputs) {
    let freq = 1000.0;
    let duty = V("In1") / 5.0;

    if (duty < 0.0) duty = 0.0;
    if (duty > 1.0) duty = 1.0;

    let ramp = (t * freq) - floor(t * freq);
    if (ramp < duty) return 5.0;
    return 0.0;
}
```

## Probing Smart Signal Output
* Probe the actual connected net at the Smart Signal output pin.
* If needed, add a **Net Label** (for example `OUT`) and probe `V(OUT)`.
* In current versions, probing external output nets is the recommended view for Smart Signal behavior.

## Troubleshooting
* Output stuck at `0V`:
  * Check Flux compile errors in logs.
  * Confirm input pin names in code exactly match block pin names.
  * Confirm output node has a load/reference path (for example resistor to ground).
* Old/incorrect waveform:
  * Re-run simulation after netlist changes.
  * Ensure you are probing the current schematic net, not an old internal signal.
* Native engine mismatch errors:
  * Verify VioSpice is using the VioMATRIXC ngspice build.

## Best Practices
* Keep Smart Signal logic deterministic in `update(t, inputs)`.
* Clamp outputs and computed duty/ratios to valid ranges.
* Start with a simple pass-through script before adding advanced logic.
