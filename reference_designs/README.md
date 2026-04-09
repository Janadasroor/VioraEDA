# VioSpice Reference Designs

Production-ready SPICE circuit templates organized by category.

## Usage

Copy a design file and adapt values to your requirements:
```bash
cp reference_designs/power/boost_converter_5v_to_15v.cir my_circuit.cir
```

## Power Converters
- `boost_converter_5v_to_15v.cir` — 5V → 15V, 100kHz, 50% duty, ~11V output
- `buck_converter_12v_to_5v.cir` — 12V → 5V, 200kHz, synchronous
- `flyback_converter.cir` — Isolated 12V → 5V, 50kHz

## Amplifiers
- `common_emitter_amp.cir` — Single-stage BJT amplifier, gain ~10
- `differential_amp.cir` — Differential pair with current mirror
- `opamp_inverting.cir` — Op-amp inverting amplifier, gain = -Rf/Rin
- `class_ab_output.cir` — Class AB push-pull output stage

## Filters
- `rc_lowpass.cir` — First-order RC low-pass, fc = 1/(2πRC)
- `rlc_bandpass.cir` — Second-order RLC bandpass, Q = R√(C/L)
- `active_lowpass_sallen_key.cir` — Sallen-Key 2nd-order active LPF
- `pi_filter.cir` — EMI Pi filter for power supply

## Oscillators
- `rc_phase_shift_oscillator.cir` — 3-stage RC phase shift oscillator
- `colpitts_oscillator.cir` — BJT Colpitts oscillator, ~1MHz
- `wein_bridge_oscillator.cir` — Wein bridge sine wave generator

## Digital
- `ring_oscillator.cir` — CMOS ring oscillator (5-stage inverter chain)
- `schmitt_trigger.cir` — Hysteresis comparator with BJT
