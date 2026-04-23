# SmartSignal Execution Guardrails

This project supports two SmartSignal execution modes. Only one mode is allowed per run.

## Native Mode (Preferred)

- Active when `SimulationManager::isNativeSmartSignalMode()` is `true`.
- Netlist emits native A-device instances (for example `A_SB1 ... viospice_jit_model_SB1`).
- The ngspice/VioMATRIXC solver executes SmartSignal JIT logic directly inside the solver loop.
- Qt-side legacy feedback (`runUpdate` + `queueFluxSourceUpdate` + `bg_halt/bg_resume`) must stay disabled.

## Legacy Mode (Fallback)

- Active when `SimulationManager::isNativeSmartSignalMode()` is `false`.
- SmartSignal output is driven through explicit voltage source feedback updates.
- Qt side runs `runUpdate(...)` and applies source changes through the synchronized halt/alter/resume flow.

## Invariants (Do Not Break)

1. Never execute native and legacy SmartSignal loops at the same time.
2. In native mode, `FluxScriptTarget::outputVoltageSources` must be empty.
3. In native mode, real-time tick and `cbSendData` must skip legacy SmartSignal update dispatch.
4. Netlist load command order is sensitive:
   - `reset`
   - `set ngbehavior=ltps`
   - `set filetype=binary`
   - `set vicompat=lt` (native) or `set vicompat=all` (fallback)
   - then parse/load netlist

## Failure Signatures

- Legacy accidentally active in native mode:
  - glitches, stale outputs, or extra transport delay
  - possible instability from unexpected halt/resume cycles
- Native accidentally disabled on VioMATRIXC:
  - digital probing may expose internal bridge nodes
  - behavior falls back to mixed-mode bridge path
- Native accidentally enabled on plain ngspice:
  - load errors around missing native logic models/rewrite

## Safe Extension Rules

- When adding SmartSignal features, gate all runtime mode decisions through
  `SimulationManager::isNativeSmartSignalMode()`.
- If adding new target wiring fields, preserve the native-mode invariant that
  legacy output source lists remain empty.
- Do not reorder the netlist-load compatibility commands.
