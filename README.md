# VioraEDA 

**VioraEDA** is a high-performance, open-source SPICE-class circuit simulator with a modern, interactive schematic editor. Designed for speed and visual excellence, it bridges the gap between traditional simulation engines and modern user interfaces.


## Key Features

- **High-Performance Simulation**: Native Sparse LU solver and Verilog-A JIT compilation for lightning-fast analysis.
- **Modern Interactive UI**: A stunning, hardware-accelerated schematic editor with smooth animations and professional aesthetics.
- **Real-Time Oscilloscope**: Integrated CRT-style analog oscilloscope for live signal visualization.
- **Automatic Probing**: LTspice-inspired hover-probing system for effortless voltage measurements.
- **Gemini AI Co-Pilot**: Integrated AI assistant to help you debug ERC violations and generate FluxScript snippets.
- **Hierarchical Design**: Full support for `.SUBCKT` expansion and complex hierarchical schematics.
- **Virtual Instruments**: A full suite of virtual tools including Logic Analyzers, Voltmeters, Ammeters, and Wattmeters.
- **ML Dataset API**: Local HTTP API for batch simulation and rich JSONL dataset export for AI training workflows.
- **OTA (Operational Transconductance Amplifier) Support**: Full LTspice OTA compatibility with automatic ngspice translation, including configurable transconductance (gm), output resistance (Rout), output capacitance (Cout), and current limiting.

## Development Status

**VioraEDA is under active development.** This project is currently being developed and tested on **Ubuntu Linux**. While the core simulation engine, schematic editor, and PCB layout tools are functional, additional work is required to achieve feature completeness and production readiness. Contributions and feedback are welcome.

## Project Vision

The long-term goal of VioraEDA is to provide a fully AI-integrated electronic design platform. Planned capabilities include:

- **AI-Assisted Circuit Design**: Natural language input for circuit creation and modification. Users describe the desired functionality, and the system generates complete schematics.
- **AI-Powered Logic Circuit Synthesis**: Automated generation of digital logic circuits from behavioral specifications, truth tables, or high-level descriptions.
- **Intelligent Design Review**: Automated analysis of circuit correctness, performance bottlenecks, and design optimization suggestions.
- **Conversational Debugging**: Interactive AI assistant that identifies simulation errors, suggests fixes, and explains circuit behavior in plain language.
- **Automated Component Selection**: AI-driven recommendation of component values and part numbers based on design requirements and availability.

These features are planned for future releases. The current AI integration (Gemini Co-Pilot) provides a foundation for this roadmap.

## Tech Stack

- **Core**: C++20 / Qt6
- **Graphics**: Qt Graphics View Framework
- **Sim Engine**: Custom Sparse LU Solver / ngspice compatible
- **AI**: Google Gemini API Integration

## Getting Started

### Prerequisites

**Required:**

- **Qt 6.11+** (recommended; 6.5+ minimum) — components required:
  `Widgets PrintSupport Sql OpenGLWidgets Charts Svg Network Multimedia Concurrent Test Qml Quick QuickWidgets`
- **CMake 3.16+**
- **C++20 Compiler** (GCC 10+, Clang 12+, or MSVC 2019+)
- **LLVM 18+** (for FluxScript JIT) — components required:
  `Core ExecutionEngine MCJIT OrcJIT Support native BitWriter BitReader Linker IRReader X86`
- **Python 3.10+** (for ML dataset API, Gemini AI co-pilot, and MCP server)
- **libcurl** (for FluxScript package manager)

**Currently tested on Ubuntu 24.04+.** Other Linux distros with Qt 6.11 and LLVM 18+ should work with package equivalents:

| Distro | Command |
|--------|---------|
| Ubuntu 24.04+ | `sudo apt install build-essential cmake qt6-base-dev qt6-charts-dev qt6-svg-dev qt6-tools-dev qt6-l10n-tools libgl1-mesa-dev llvm-18-dev libclang-18-dev libcurl4-openssl-dev python3 python3-pip` |
| Fedora 40+ | `sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qtcharts-devel qt6-qtsvg-devel llvm-devel clang-devel libcurl-devel python3` |
| Arch Linux | `sudo pacman -S base-devel cmake qt6-base qt6-charts qt6-svg llvm clang curl python3` |

**Windows and macOS are not yet supported.** The simulation engine (VioMATRIXC / ngspice) uses autotools and Linux-specific shared memory APIs. Cross-platform support is planned but requires porting the engine build system.

**Install on Fedora:**

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qtcharts-devel \
  qt6-qtsvg-devel llvm-devel clang-devel libcurl-devel python3
```

### Installation

The build automatically fetches ngspice (VioMATRIXC), FluxScript, Eigen, and other dependencies via CMake `FetchContent`. No manual install needed for these.

1. **Clone the repository**:
   ```bash
   git clone https://github.com/Janadasroor/VioraEDA.git
   cd VioraEDA
   ```

2. **Configure and build**:
   ```bash
   cmake -B build
   cmake --build build -j$(nproc)
   ```

3. **Run the application**:
   ```bash
   ./build/VioraEDA
   ```

## Workflow

1. **Draw**: Build your circuit using the extensive component library.
2. **Probing**: Place probes or simply hover over wires while simulating to see real-time waveforms.
3. **Analyze**: Use the integrated oscilloscope and measurement tools to verify your design.
4. **Automate**: Write FluxScripts to automate complex simulation tasks.


## Extensions

VioSpice supports two kinds of extensions:

- **FluxScript Extensions** — UI panels, calculators, and automation tools written in FluxScript, compiled at runtime via LLVM JIT. No C++ build required.
- **Native Plugins** — C++ shared libraries (`.so`) loaded via `QPluginLoader` for deeper integration.

### Quick Start — FluxScript Extension

Create a directory and two files:

```bash
mkdir -p ~/.config/VioraEDA/extensions/my-tool
```

**`manifest.json`**:
```json
{
  "id": "my-tool",
  "name": "My Tool",
  "description": "A custom panel",
  "menu": [{"path": "My Tool", "action": "open_panel"}]
}
```

**`main.flux`**:
```flux
def open_panel() {
    win = flux_qt_create_window("My Tool")
    btn = flux_qt_create_button("About")
    flux_qt_add_widget(win, btn)
    flux_qt_on_click_by_name(btn, "show_about")
}

def show_about() {
    flux_qt_msg_box("My Tool", "Hello!")
}
```

Launch VioSpice, open a schematic, and the **Extensions** menu will show "My Tool".

### Standalone Testing

For faster iteration, use the standalone runner (no schematic editor needed):

```bash
cmake --build build --target flux_runner
build/flux_runner examples/component_calc.flux
```

### API Reference

Full reference: [docs/EXTENSION_API.md](docs/EXTENSION_API.md)

Includes all 27+ Qt widget functions, workspace/simulation API, math built-ins, and the `manifest.json` schema.

### Examples

```
examples/
├── smart_signal_template.flux     ← Simple SPICE behavioral block
├── adevice_latch.flux             ← Digital device (A-device) example
├── minimal_dashboard.flux         ← Minimal Qt window demo
├── component_calc.flux            ← Interactive filter + divider calculator
└── dashboard_demo.flux            ← Full simulation dashboard
```

### Extension Manager

The **Extensions** dialog (Project Manager → Extensions) lists both native plugins and FluxScript extensions with type badges. It supports enable/disable for native plugins, reload with file watcher auto-reload, and an online plugin catalog.

## ML Dataset API

VioSpice now includes a Python HTTP API for ML-oriented simulation pipelines. It can run single jobs or large concurrent batches and emit dataset records containing netlists, waveforms, stats, measures, and custom labels.

```bash
python3 python/scripts/ml_dataset_api.py --port 8787
```

Documentation: [docs/developer/ML_DATASET_API.md](docs/developer/ML_DATASET_API.md)

ML engineer guide: [docs/developer/ML_ENGINEER_GUIDE.md](docs/developer/ML_ENGINEER_GUIDE.md)
Examples: [examples/ml_api/README.md](examples/ml_api/README.md)

## Licensing Compliance

VioSpice is licensed under the Apache License, Version 2.0. To ensure full compliance and avoid "GPL infection," the underlying simulation engine (**VioMATRIXC**) is built without GPL-licensed components (specifically the XSpice table models). This ensures that the entire VioSpice binary distribution is compatible with permissive open-source licensing.

## License

This project is licensed under the Apache-2.0 license - see the [LICENSE](LICENSE) file for details.

---
Copyright 2026 Janadasroor Team. Licensed under the Apache License, Version 2.0.
