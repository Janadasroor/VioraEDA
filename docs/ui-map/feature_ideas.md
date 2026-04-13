# VioSpice — Feature Ideas

## 🧠 1. Viora "Smart Probe" — AI-Powered Net Annotation
**What:** When you hover a net during simulation, instead of just showing voltage/current, Viora AI annotates what the signal *means* — e.g. "This looks like the gate drive waveform of Q1. Rise time is ~200ns. Possible issue: shoot-through if Q2 is still conducting."

**Why it's cool:** Bridges raw numbers and engineering insight. Completely unique to VioSpice. 
**Hooks:** `SimulationPanel` hover probing + Viora Gemini API.

---

## ⚡ 2. Live Tuning Sliders — Realtime Parametric Simulation
**What:** A `TuningSlider` is already partially implemented (`tuning_slider_item.cpp`). Extend it: when you drag a slider for R, C, or L on the schematic, ngspice automatically re-runs the sim in the background and the waveform viewer updates in near-realtime (< 200ms latency on small circuits).

**Why it's cool:** Makes circuit design feel as instant and tactile as Desmos/GeoGebra for math.  
**Hooks:** `TuningSliderSymbolItem` → `SimulationManager` → `WaveformViewer`.

---

## 🖥️ 3. Virtual Oscilloscope with Probe Memories
**What:** The oscilloscope exists (`oscilloscope_window.cpp`). Upgrade it to have:
- **Channel memories**: Save/load named waveform snapshots to compare across runs ("before cap filter" vs "after cap filter")
- **Cursor measurements**: Δt, ΔV, frequency, duty cycle measured between cursor pairs shown in a live stats panel
- **Math channels**: `CH1 - CH2`, `FFT(CH1)`, `dV/dt(CH1)` directly in the scope display

**Why it's cool:** Makes it feel like a real $3000 Rigol bench scope, inside the EDA tool.  
**Hooks:** `OscilloscopeWindow`, `simulation_panel_plot.cpp`, existing `FftAnalyzer`.

---

## 📦 4. Component Datasheet Viewer — Inline PDF + AI Pinout Extractor
**What:** Right-click any symbol → "View Datasheet". VioSpice fetches the PDF from a user-configured URL (or local file), shows it in a floating panel. A "Extract Pinout" button sends the first pages to Viora AI, which auto-fills any missing pin names/types on the symbol.

**Why it's cool:** No more alt-tabbing to a browser. Connects to the existing `ui_ux_improvements.md` "AI-Assisted Generating" idea.  
**Hooks:** `SymbolDefinition`, Viora Gemini API, `SpiceSubcircuitImportDialog`.

---

## 🔥 5. Heat Map Overlay — Power Dissipation Visualization
**What:** After running a `.op` or `.tran` simulation, overlay a heat-map color gradient on top of the schematic. Components dissipating more power glow red/orange; low-power ones stay cool blue. Hovering shows exact Watt values.

**Why it's cool:** Instantly shows you which component is going to burn up — visually striking and practically useful.  
**Hooks:** `SimResults::branchCurrents/nodeVoltages` → `SchematicItem::paint()` overlay.

---

## 🎛️ 6. FluxScript Automation Macro Recorder
**What:** VioSpice already has `FluxCodeEditor` and `FluxScriptPanel`. Add a **Macro Recorder**: click "Record", interact with the schematic (place components, add wires, set values, run sim), click "Stop". VioSpice auto-generates a FluxScript that repeats those exact actions. 

**Why it's cool:** Zero-friction automation for repetitive designs (e.g. "generate this filter topology with these values for 10 different cutoffs").  
**Hooks:** `SchematicCommands`, `FluxScriptPanel`, command pattern already in `schematic_commands.cpp`.

---

## 🌐 7. Circuit Sharing — One-Click "Share as URL"
**What:** Serialize the schematic to a compact JSON, compress + base64 encode it, and append to a URL (like Falstad does). Clicking the URL opens VioSpice directly to that schematic. For larger files: upload to a lightweight paste-like backend and share a short URL.

**Why it's cool:** Makes VioSpice viral. Easy collaboration without git.  
**Hooks:** `SchematicEditorFile` serialization, already produces JSON.

---

## 🔬 8. S-Parameter & Impedance Analyzer (RF Mode)
**What:** Add an RF simulation mode that performs AC sweep and computes:
- Input impedance Z(ω) plotted on a **Smith Chart**
- S11 / S21 in dB (transmission line + matching network analysis)
- Group delay

**Why it's cool:** VioSpice becomes useful for RF/antenna designers, a segment completely ignored by free EDA tools.  
**Hooks:** `SimAnalysisType::AC` already exists, add complex Y/Z/S parameter extraction post-processing.

---

## 🧩 9. AI Circuit Intent Recognition — "What Does This Do?"
**What:** "Analyze Circuit" button in Viora panel. Viora inspects the entire netlist + component values, then returns a plain-English explanation: "This is a non-inverting op-amp amplifier. Your gain is 1 + R2/R1 = 11. The bandwidth will be approximately 100kHz given the LM358's GBP of 1MHz."

**Why it's cool:** Killer feature for students and hobbyists. No other EDA tool does this.  
**Hooks:** Viora Gemini AI + full netlist export already available.

---

## 📊 10. Parameter Sweep Animator
**What:** Extend the existing `SimAnalysisType::Optimization` sweep engine to animate. Run a sweep for a parameter (e.g. R1 from 1k → 10k in 20 steps), then play back each waveform frame-by-frame like a GIF, showing how the waveform morphs as the parameter changes. Export as animated SVG or GIF.

**Why it's cool:** Makes VioSpice output *publishable* educational content, not just numbers.  
**Hooks:** Existing parametric sweep engine + `WaveformViewer` + `QTimer` playback loop.

---

## 🕹️ 11. Interactive "What If" Mode (Circuit Sandbox)
**What:** A dedicated "Sandbox" mode where components can be toggled/tweaked without ever stopping the simulation. Internally, VioSpice recomputes only the affected branch of MNA when a single component changes (incremental solver). Wire the `PushButtonItem` and `SwitchItem` directly to sim state changes.

**Why it's cool:** Makes VioSpice feel like a REPL for circuits — instant, live, tactile.  
**Hooks:** `PushButtonItem`, `SwitchItem`, `VoltageControlledSwitchItem` + `SimulationManager` incremental reruns.

---

## 🗂️ 12. Project Templates Gallery
**What:** Extend the existing `templates/circuits/` directory with 20+ clickable circuit templates: common topologies like inverting amplifier, Colpitts oscillator, H-bridge, buck converter, 555 timer, RC ladder filter, Butterworth LP/HP. Show them in a visual card gallery on the welcome screen with thumbnail previews and a "Try It" button.

**Why it's cool:** Instant value for new users. Lowers the barrier from "blank canvas" to working simulation in 1 click.  
**Hooks:** `templates/circuits/metadata.json` already exists, just needs templates + a gallery UI on the welcome screen.
