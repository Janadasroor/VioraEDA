# VioSpice Simulation Pipeline Architecture

This document describes the high-level architecture and data flow of the VioSpice simulation pipeline. This pipeline connects the visual schematic editor to the underlying simulation engines (like Ngspice and the native VioSolver), handling everything from netlist generation to real-time waveform plotting.

## 1. High-Level Architecture Overview

The simulation pipeline is divided into three primary tiers:

1.  **Frontend / UI (`schematic/ui/`)**: The user interface where simulations are configured, launched, and visualized.
2.  **Core Simulation Management (`core/simulation/`)**: The business logic layer that orchestrates netlist processing, JIT (Just-In-Time) script compilation, and communication with the simulation backend.
3.  **Backend Engines (`simulator/`, Ngspice)**: The mathematical solvers that compute the circuit state.

### Directory Structure

```text
viospice/
├── schematic/
│   ├── ui/
│   │   ├── simulation_panel_ui.cpp     # Initializes the Simulation panel widget layout and controls
│   │   ├── simulation_panel.cpp        # Handles simulation state, `.meas` evaluation, execution flow
│   │   ├── simulation_panel_plot.cpp   # Handles rendering waveforms and spectra via QtCharts
│   │   └── simulation_panel.h          # Central API for the Simulation GUI
│   └── analysis/
│       └── spice_netlist_generator.cpp # Traverses the QGraphicsScene to generate a SPICE netlist
├── core/
│   └── simulation/
│       ├── simulation_manager.cpp      # Coordinates the simulation execution process
│       ├── simulation_manager.h        # Central API for programmatic simulation control
│       ├── simulation_types.h          # Shared data structures (e.g., FluxScriptTarget)
│       ├── spice_backend.cpp           # Low-level FFI/C-API bindings to the shared ngspice library
│       ├── netlist_processor.cpp       # Pre-processes netlists (handles compatibility layers)
│       ├── jit_bridge.cpp              # Bridges real-time updates between the simulator and Flux Scripts
│       └── flux_script_engine.cpp      # Compiles and executes dynamic user-defined behavioral scripts
└── simulator/
    └── bridge/
        └── sim_manager.cpp             # Legacy bridge / native solver facade (transitioning to core)
```

---

## 2. The Execution Pipeline

When a user clicks "Run Simulation", the following pipeline is executed:

### Phase 1: Preparation (Frontend)
1.  **Trigger**: `SimulationPanel::onRunSimulation()` is invoked.
2.  **Configuration**: The panel reads the current analysis settings (Transient, AC, DC, etc.) and constructs the appropriate SPICE directives (e.g., `.tran 1u 1m`).
3.  **Netlist Generation**: `SpiceNetlistGenerator::generate()` traverses the schematic scene, converting `SchematicItem` objects (Resistors, Transistors, Smart Signals) into a raw SPICE netlist.
4.  **Handoff**: The netlist string is passed to `SimulationManager::runSimulation()`.

### Phase 2: Processing (Core)
1.  **Pre-processing**: The `SimulationManager` (with help from `NetlistProcessor`) formats the netlist, handling compatibility translations (e.g., converting LTspice-specific syntax into Ngspice-compatible syntax).
2.  **JIT Compilation**: If the schematic contains "Smart Signals" or SystemVerilog blocks, `JitBridge` and `flux_script_engine` compile this code into executable functions.
3.  **Backend Handoff**: `SimulationManager` creates a temporary `.cir` file (if necessary for predictable file output) and passes the initialization commands to `SpiceBackend`.

### Phase 3: Execution (Backend)
1.  **Launch**: `SpiceBackend` invokes `bg_run` (background run) on the shared Ngspice library thread.
2.  **Streaming**: As the simulator runs, `SpiceBackend` intercepts C-callbacks (`cbSendChar`, `cbSendStat`, `cbBGThreadRunning`).
3.  **Real-Time Bridge**: For time-domain simulations involving JIT scripts, `JitBridge` pauses the engine at specific time steps to execute Python/Flux scripts, feeding the computed analog values back into the simulator as voltage/current sources.

### Phase 4: Data Retrieval & Visualization (Frontend)
1.  **Completion**: When the background thread finishes, `SimulationManager::cbBGThreadRunning` detects the end state and triggers `handleSimulationFinished()`.
2.  **Data Parsing**: `SimulationManager` emits a `simulationFinished` signal. The frontend reads the generated `.raw` binary file using `RawDataParser`.
3.  **Plotting**: `SimulationPanel::plotBuiltinResults()` in `simulation_panel_plot.cpp` takes the parsed `SimResults` and populates the `QChart` (Oscilloscope) and `SmithChartWidget` (RF Analysis).
4.  **Post-Processing**: `SimulationPanel::evaluateMeasStatements()` evaluates any `.meas` directives against the final dataset and populates the Efficiency/Design Explorer tables.

---

## 3. Real-Time Diagnostics & UI Updates

- **Logging**: Ngspice `stdout`/`stderr` is captured by `SimulationManager::cbSendChar`, buffered, and flushed to the `m_logOutput` widget in `SimulationPanel`.
- **Live Updating**: If real-time streaming is enabled, `SimulationManager` periodically emits `rawResultsReady` or `timeSnapshotReady` signals, which `simulation_panel_plot.cpp` uses to draw the waveform while the simulation is actively computing (`updateChartRealTime`).

## 4. Key Architectural Rules

1.  **No UI in Core**: The `core/simulation/` namespace must *never* include `<QWidget>`, `<QChart>`, or any GUI components. It operates strictly via signals/slots and standard data types (`QString`, `SimResults`).
2.  **Strict Threading**: `SpiceBackend` interacts with an external C-library running on a background thread. All callbacks (like `cbSendChar`) must use `Qt::QueuedConnection` or `std::lock_guard` when mutating state or emitting signals that the UI will catch.
3.  **SimulationPanel Split**: `SimulationPanel` is split by concern:
    - Edit `simulation_panel_ui.cpp` for layout/button changes.
    - Edit `simulation_panel_plot.cpp` for waveform rendering/chart logic.
    - Edit `simulation_panel.cpp` for `.meas` math, export functionality, and state management.
