# Project Manager

The Project Manager is the launcher window shown when VioraEDA starts. It manages your workspace, recent projects, and provides access to all editors.

## Layout

- **Recent Projects** — List of recently opened schematics and projects. Click to open.
- **Workspace Folders** — Configure source directories for your design files.
- **New Project** — Create a blank schematic in a new project directory.
- **Open File** — Browse for `.vsch` (schematic), `.vpcb` (board), or `.sym` (symbol) files.

## Preferences

Access via **Preferences > Settings** or the settings button. See [Settings Reference] for details.

Settings categories:
- **General** — Theme (Dark/Engineering/Light), auto-save, snap-to-grid, wire color.
- **Simulator** — Solver, integration method, tolerances, auto-show simulation tab, net table overlay.
- **PCB** — Enable PCB and footprint editors.
- **Libraries** — Symbol library paths, SPICE model paths, KiCad import settings.
- **AI Assistant** — Gemini API key, model selection, feature toggles (chat, smart probe, ERC).
- **Connectivity** — UI command server for remote control.

## Multi-Document Interface

Open multiple schematics in tabs. Each tab has its own simulation panel, waveform viewer, and undo history. The status bar shows the active file path.
