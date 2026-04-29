# VioraEDA Simulation Engine (Simulator)

The VioSpice simulation engine is a high-performance, mixed-signal solver that combines traditional SPICE algorithms with modern JIT-compiled behavioral modeling.

## Overview

This module is responsible for:
- **Netlist Parsing and Flattening**: Converting hierarchical schematic designs into flat SPICE netlists.
- **Sparse MNA Solver**: A custom-optimized Sparse Linear Algebra solver for DC, AC, and Transient analysis.
- **VioMATRIXC Integration**: A high-speed bridge to a customized `ngspice` core for industry-standard model compatibility.
- **FluxScript JIT Bridge**: Real-time execution of behavioral models compiled directly into the simulation loop.

## Module Structure

- `core/`: The pure mathematical engine, including the Sparse LU solver and analysis algorithms.
- `bridge/`: Qt-dependent integration layer that connects the simulation engine to the Schematic Graphics View.
- `mixedmode/`: Synchronization logic for analog/digital co-simulation.
- `synthesis/`: Tools for generating behavioral models from waveform data or specifications.

## Key Documentation

For deep technical details, please refer to:
- [ARCHITECTURE.md](ARCHITECTURE.md): Class-level design and data flow.
- [component-model-mapping.md](component-model-mapping.md): How schematic symbols map to simulation primitives.
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md): Common simulation errors and convergence tuning.
- [BETA_READINESS_CHECKLIST.md](BETA_READINESS_CHECKLIST.md): Current status and stability verification.

## Integration

The simulator is typically driven by the `SimulationManager` class in `core/simulation_manager.h`. It can be run in **Process Mode** (standard batch simulation) or **Shared Library Mode** for real-time interactive tuning.

---
Copyright 2026 Janadasroor Team. Licensed under the Apache License, Version 2.0.
