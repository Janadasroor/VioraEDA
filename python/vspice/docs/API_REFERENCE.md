# VioSpice Python API — Reference

## Quick Start

```python
import vspice

# Build a circuit
nl = vspice.SimNetlist()
nl.add_node('IN')
nl.add_node('OUT')

r1 = vspice.SimComponentInstance('R1', vspice.SimComponentType.Resistor, [1, 2])
r1.params['resistance'] = 1e3
nl.add_component(r1)

# Simulate it
r = vspice.run_simulation(
    '* Voltage Divider\n'
    'V1 in 0 DC 5\n'
    'R1 in out 1k\n'
    'R2 out 0 2k\n'
    '.op\n'
    '.end',
    'op',
    vio_cmd_path='build/vio-cmd'
)
print(r['node_voltages'])  # {'OUT': 3.333}
```

---

## `vspice._core` — Nanobind Extension

### SPICE Number Utilities

#### `parse_spice_number(text: str) -> (value: float, ok: bool)`

Parse SPICE engineering-suffix notation.

```python
v, ok = vspice.parse_spice_number("4.7u")   # (4.7e-6, True)
v, ok = vspice.parse_spice_number("1k")     # (1000.0, True)
v, ok = vspice.parse_spice_number("2meg")   # (2000000.0, True)
v, ok = vspice.parse_spice_number("33n")    # (3.3e-8, True)
v, ok = vspice.parse_spice_number("invalid") # (0.0, False)
```

Supported suffixes: `t`, `g`, `meg`, `k`, `m`, `u`, `n`, `p`, `f`, `mil`.
Units (`V`, `A`, `Hz`, `F`, `H`, `ohm`) are silently accepted and ignored.
Unicode micro (`µ`, `μ`) and omega (`Ω`, `℧`) signs are normalized.

#### `format_spice_number(value: float, precision: int = 6) -> str`

```python
vspice.format_spice_number(0.0047)  # "4.7m"
vspice.format_spice_number(1000)    # "1k"
vspice.format_spice_number(4.7e-9)  # "4.7n"
```

---

### Circuit Building

#### `SimNetlist`

The main circuit container.

```python
nl = vspice.SimNetlist()

# Nodes
n_in = nl.add_node("IN")    # returns node ID (1, 2, 3, ...)
n_gnd = nl.ground_node()    # always 0

# Components
r1 = vspice.SimComponentInstance("R1", vspice.SimComponentType.Resistor, [n_in, n_gnd])
r1.params["resistance"] = 1e3
nl.add_component(r1)

# Models
model = vspice.SimModel("2N3904", vspice.SimComponentType.BJT_NPN)
model.params["IS"] = 6.734e-15
model.params["BF"] = 416.4
nl.add_model(model)

# Parameters
nl.set_parameter("TEMP", 27.0)

# Access
nl.node_count()          # int
nl.find_node("IN")       # int or -1
nl.node_name(1)          # str
nl.find_model("2N3904")  # SimModel or None
nl.to_dict()             # summary dict
```

#### `SimComponentInstance`

```python
r1 = vspice.SimComponentInstance("R1", vspice.SimComponentType.Resistor, [1, 2])
r1.params["resistance"] = 1000.0
r1.param_expressions["resistance"] = "{RVAL*1.2}"  # symbolic
r1.tolerances["resistance"] = vspice.SimTolerance(0.01, vspice.ToleranceDistribution.Uniform)
r1.model_name = "RES_1K"
r1.flux_script = "def update(t) { return V(\"In1\"); }"  # FluxScript behavioral source
r1.to_spice()  # "R1 1 2 resistance=1000"
```

#### `SimModel`

```python
model = vspice.SimModel("2N3904", vspice.SimComponentType.BJT_NPN)
model.params["IS"] = 6.734e-15
```

#### `SimSubcircuit`

```python
sub = vspice.SimSubcircuit("VOLTAGE_DIVIDER", ["IN", "OUT", "GND"])
sub.components = [r1, r2]
sub.models = {"RES": model}
sub.parameters = {"RATIO": 0.5}
```

#### `SimAnalysisConfig`

```python
cfg = vspice.SimAnalysisConfig()
cfg.type = vspice.SimAnalysisType.Transient
cfg.t_stop = 0.01
cfg.t_step = 1e-6
cfg.f_start = 1.0
cfg.f_stop = 1e6
cfg.f_points = 100
cfg.rf_z0 = 50.0
cfg.rel_tol = 1e-3
cfg.abs_tol = 1e-6
```

---

### Simulation

#### `run_simulation(netlist_text, analysis, stop_time, step_time, vio_cmd_path, timeout_seconds) -> dict`

Run a SPICE netlist simulation via `vio-cmd` subprocess, parsing the raw binary output.

```python
r = vspice.run_simulation(
    '* Voltage Divider\n'
    'V1 in 0 DC 5\n'
    'R1 in out 1k\n'
    'R2 out 0 2k\n'
    '.op\n'
    '.end',
    analysis='op',
    stop_time='',
    step_time='',
    vio_cmd_path='build/vio-cmd',
    timeout_seconds=60
)

if r['ok']:
    print(r['node_voltages'])     # {'OUT': 3.333, 'IN': 5.0}
    print(r['branch_currents'])   # {'V1': -0.00167}
    for wf in r.get('waveforms', []):
        print(f"{wf['name']}: {len(wf['x'])} points")
else:
    print(r['error'])
```

Return format:
```python
{
    'ok': True,
    'analysis': 'op',
    'waveforms': [{'name': 'V(OUT)', 'x': [...], 'y': [...]}],
    'node_voltages': {'OUT': 3.333},
    'branch_currents': {'V1': -0.00167},
}
```

---

### Model Parser

#### `SimModelParser.parse_model_line(line, line_number, source_name) -> (ok, models, diagnostics)`

```python
ok, models, diags = vspice.SimModelParser.parse_model_line(
    '.model 2N3904 NPN (IS=6.734f XTI=3 EG=1.11 VAF=74.03 BF=416.4)'
)
# ok = True
# models = {'2N3904': SimModel object}
```

#### `SimModelParser.parse_library(netlist, content, options) -> (ok, diagnostics)`

Parse a full SPICE library string containing `.model`, `.subckt`, and other definitions.

```python
lib = '''
.model 2N3904 NPN (IS=6.734f BF=416.4)
.model 2N3906 PNP (IS=1.41f BF=180.7)
'''
ok, diags = vspice.SimModelParser.parse_library(nl, lib)
```

---

### Measurement Evaluator

#### `meas_parse(line, line_number, source_name) -> (statement, ok)`

Parse a `.meas` statement.

```python
stmt, ok = vspice.meas_parse('.meas tran VOUT_MAX MAX V(out)')
# stmt.name = 'VOUT_MAX', stmt.function = MeasFunction.MAX
```

#### `meas_evaluate(statements, results, analysis_type) -> list[MeasResult]`

Evaluate measurements against simulation results.

```python
wf = vspice.SimWaveform('V(out)', [0, 0.001, 0.002], [0, 1.5, 3.0])
results = vspice.SimResults()
results.waveforms = [wf]
results.x_axis_name = 'time_s'

stmt, _ = vspice.meas_parse('.meas tran VOUT_MAX MAX V(out)')
[r] = vspice.meas_evaluate([stmt], results, 'tran')
# r.value = 3.0, r.valid = True
```

---

### Callback Bridge

Register Python functions that C++ code can invoke (e.g., when a GUI menu item is clicked).

#### `register_callback(name: str, callback: callable)`

```python
def my_handler(msg):
    print(f"C++ called me with: {msg}")

vspice.register_callback("on_sim_done", my_handler)
```

#### `invoke_callback(name: str, args: tuple, kwargs: dict) -> object`

```python
result = vspice.invoke_callback("on_sim_done", ("hello",))
```

#### `CallbackHandle`

A lightweight reference to a registered callback.

```python
handle = vspice.get_callback_handle("on_sim_done")
if handle and handle.exists():
    handle(("direct call",))
```

#### `unregister_callback(name: str)`

```python
vspice.unregister_callback("on_sim_done")
```

#### `list_callbacks() -> list[str]`

```python
print(vspice.list_callbacks())  # ['on_sim_done']
```

---

### Enums

| Enum | Values |
|---|---|
| `SimAnalysisType` | `OP`, `Transient`, `AC`, `DC`, `MonteCarlo`, `Sensitivity`, `ParametricSweep`, `Noise`, `Distortion`, `Optimization`, `FFT`, `RealTime`, `SParameter` |
| `SimComponentType` | `Resistor`, `Capacitor`, `Inductor`, `VoltageSource`, `CurrentSource`, `Diode`, `BJT_NPN`, `BJT_PNP`, `MOSFET_NMOS`, `MOSFET_PMOS`, `JFET_NJF`, `JFET_PJF`, `OpAmpMacro`, `Switch`, `TransmissionLine`, `SubcircuitInstance`, `VCVS`, `VCCS`, `CCVS`, `CCCS`, `B_VoltageSource`, `B_CurrentSource` |
| `SimIntegrationMethod` | `BackwardEuler`, `Trapezoidal`, `Gear2` |
| `SimTransientStorageMode` | `Full`, `Strided`, `AutoDecimate` |
| `ToleranceDistribution` | `Uniform`, `Gaussian`, `WorstCase` |
| `MeasFunction` | `FIND`, `DERIV`, `PARAM`, `MAX`, `MIN`, `PP`, `AVG`, `RMS`, `N`, `INTEG`, `MIN_AT`, `MAX_AT`, `FIRST`, `LAST`, `DUTY`, `SLEWRATE`, `SLEWRATE_FALL`, `SLEWRATE_RISE`, `FREQ`, `PERIOD`, `TRIG_TARG` |
| `SimParseDiagnosticSeverity` | `Info`, `Warning`, `Error` |

---

## `vspice.ui` — GUI Integration

Connect to the running VioSpice GUI via WebSocket (port 18790).

```python
from vspice.ui import UIProxy

ui = UIProxy()
ui.connect()

ui.show_message("Hello", "Message from Python!")
ui.add_menu_item("Tools", "Run Script", code="print('clicked!')")

@ui.on_menu_item_triggered
def handler(event):
    print(f"Menu '{event['label']}' clicked!")

ui.run_forever()  # keep alive for events
```

### Commands

| Command | Description |
|---|---|
| `show_message(title, text, type)` | Show a message dialog in the GUI |
| `add_menu_item(menu, label, code)` | Add a menu bar item |
| `remove_menu_item(menu_id)` | Remove a menu item |
| `run_python_code(code)` | Execute Python code in GUI context |
| `get_schematic_context()` | Get current schematic info |
| `ping()` | Check connectivity |
