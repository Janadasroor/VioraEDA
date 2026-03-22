# Simulator TODO – LTspice Root Built-ins (46)

This list tracks LTspice root symbols that are mapped in the bridge but lack full simulator behavior or proper models.

## Missing Core Device Implementations
- `e`, `e2` (VCVS)
- `g`, `g2` (VCCS)
- `f` (CCCS)
- `h` (CCVS)
- Logic gates (`A` devices): `and`, `or`, `xor`, `nand`, `nor`, `inv`, `buf`, `dflop`, etc.
- `csw` (current-controlled switch, Prefix `W`)
- JFET/MESFET: `njf`, `pjf`, `mesfet`

## Needs Proper Models (Currently Generic)
- `LED` (diode model)
- `zener` (diode model)
- `schottky` (diode model)
- `varactor` (voltage-dependent capacitance model)
- `TVSdiode` (clamping model)
- `FerriteBead`, `FerriteBead2` (frequency-dependent L+R model)

## Notes
- These symbols are hardcoded in the LTspice root built-ins list (bridge mapping).
- Until implemented, they map to generic devices or approximations and may be inaccurate.
