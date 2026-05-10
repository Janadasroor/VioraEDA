# FluxScript Language Reference

FluxScript is the high-performance, domain-specific automation and behavioral modeling language for the **Viora & VioSpice** ecosystem. It is used for both real-time simulation logic (Smart Signal Blocks) and Automated Electronic Design (AEDA) tasks.

## 1. Behavioral Modeling (Smart Signal Blocks)

When used inside a schematic block, FluxScript acts as a transfer function between input and output pins.

### Implicit Simulation State
- `t`: The current simulation time (float).
- `inputs`: An array of values representing input pins (e.g., `inputs[0]`, `inputs[1]`).

### Basic Syntax
- **Assignment**: `var = expression`
- **Arithmetic**: `+`, `-`, `*`, `/`, `^` (power).
- **Control Flow**: `if <cond> then <val> else <val>` (can be nested).
- **Functions**: `sin(x)`, `cos(x)`, `tan(x)`, `exp(x)`, `log(x)`, `sqrt(x)`, `abs(x)`, `floor(x)`, `ceil(x)`.

### Example: PWM Generator
```flux
# Inputs: duty (0..1)
# Outputs: 5V pulse train at 1kHz
freq = 1000.0
period = 1.0 / freq
local_t = t - (floor(t / period) * period)
duty = inputs[0]

if local_t < (period * duty) then 5.0 else 0.0
```

---

## 2. AEDA Automation (Schematic/PCB Scripting)

When run via `viora flux run` or in the Script Editor tab, FluxScript provides full access to the Viora design database.

### Logic Syntax
- **Loops**: `while <cond> do { <body> }`
- **Strings**: `"Hello " + "World"`
- **Printing**: `print("Message")`

### Built-in API Functions

#### Project & Workspace
- `flux_get_project_name()`: Returns current project name.
- `flux_get_schematic_file()`: Returns current file path.
- `flux_get_open_schematics()`: Returns comma-separated list of open tabs.
- `flux_select_schematic(name)`: Switches the active editor tab.

#### Design Data (AEDA)
- `erc_get_component_count()`: Returns total components in schematic.
- `erc_get_type(index)`: Returns component type (e.g., "Resistor").
- `erc_get_ref(index)`: Returns component designator (e.g., "R1").
- `erc_set_block_pins(index, inputs, outputs)`: Configures Smart Signal Block pins.
- `flux_set_prop(ref, prop, value)`: Sets a numerical property (e.g., `flux_set_prop("R1", "resistance", 1000)`).
- `flux_set_prop_str(ref, prop, str)`: Sets a string property.

#### Simulation Control
- `flux_run_sim(type, stop, step)`: Triggers a simulation (e.g., `flux_run_sim("tran", 1e-3, 1e-6)`).
- `flux_sim_get_vector_size(name)`: Returns size of a result vector (e.g., "V(OUT)").
- `flux_sim_get_vector_val(name, index)`: Returns Y value at index.
- `flux_sim_get_vector_x(name, index)`: Returns X (time/freq) value at index.

#### Output & Visualization
- `print(msg)`: Output to the console.
- `flux_plot_point(name, x, y)`: Appends a point to the live waveform viewer.

### Example: Automated Sensitivity Analysis
```flux
# Sweep R1 and record output voltage
r_val = 100.0
while r_val <= 1000.0 do {
    flux_set_prop("R1", "resistance", r_val)
    flux_run_sim("tran", 1e-3, 1e-6)
    
    vout = flux_sim_get_vector_val("V(OUT)", 0)
    print("R= " + r_val + ", Vout= " + vout)
    
    r_val = r_val + 100.0
}
```

---

## 3. CLI Usage

AI agents can execute FluxScript using the following MCP tool or CLI command:

```bash
viora flux run my_automation.flux
```

For AI agents: Use `viora_flux_run` to execute automation scripts or test behavioral blocks.
