# VioSpice Python API — Architecture Guide

## Design Philosophy

The `vspice` Python API follows a strict **separation of concerns**:

```
Python (external orchestration layer)
  ↓  imports vspice._core, vspice.ui
  ↓  calls vspice.run_simulation()
  ↓  vspice.UIProxy connects to GUI WebSocket
  ↓  vspice.register_callback() for C++ → Python callbacks
  ↓
VioSpice C++ Core (headless solver)
  ↓  SimNetlist, SimComponentInstance, SimModel
  ↓  sim_value_parser, sim_net_evaluator, raw_data_parser
  ↓
FluxScript (LLVM JIT — in-simulation engine only)
  ↓  behavioral sources inside the schematic
  ↓  NOT Python — FluxScript is compiled to native code
```

### Key Rule: Behavioral Sources Are FluxScript-Only

`SimComponentInstance` behavioral sources use **FluxScript exclusively**. Python is the **external orchestration layer** — it builds circuits, runs simulations, and receives results, but does NOT execute inside the simulation loop.

| Layer | Purpose | Language |
|---|---|---|
| `vspice._core` (nanobind) | Circuit building, simulation, parsing | C++ ↔ Python |
| `vspice.ui` (WebSocket) | GUI integration, menus, messages | Python → Qt |
| `vspice.run_simulation()` | Runs SPICE via `vio-cmd` subprocess | C++ → ngspice |
| FluxScript (JIT) | Behavioral sources inside simulation | FluxScript → LLVM → native |

## Module Structure

```
python/vspice/
├── __init__.py          # Package init, re-exports _core + ui
├── _core.cpython-...so  # nanobind extension (compiled)
├── ui.py                # WebSocket client for GUI command server
└── docs/
    ├── ARCHITECTURE.md  # This file
    └── API_REFERENCE.md # Detailed API docs
```

## Installation

The package is symlinked into your user site-packages:

```bash
~/.local/lib/python3.12/site-packages/vspice → ~/qt_projects/viospice/python/vspice
```

After rebuilding:
```bash
cmake --build build -j8 --target vspice_core
cp build/python/vspice/_core.cpython-312-x86_64-linux-gnu.so python/vspice/
```
