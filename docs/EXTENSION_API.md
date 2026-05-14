# Extension API Reference

Functions available to FluxScript extensions via the JIT bridge.

## Qt Widgets — `flux_qt_*`

### Window & Layout

| Function | Returns | Description |
|---|---|---|
| `flux_qt_create_window(title)` | handle | New dialog window with vertical layout |
| `flux_qt_add_widget(container, widget)` | void | Add a widget to a container's layout |

### Widgets

| Function | Returns | Description |
|---|---|---|
| `flux_qt_create_label(text)` | handle | Static text label |
| `flux_qt_create_button(text)` | handle | Clickable push button |
| `flux_qt_create_slider(orientation)` | handle | Slider (0=horizontal, 1=vertical) |
| `flux_qt_create_lcd()` | handle | 8-digit LCD number display |
| `flux_qt_create_combobox()` | handle | Drop-down combo box |
| `flux_qt_create_checkbox(text)` | handle | Checkable box with label |
| `flux_qt_create_spinbox()` | handle | Integer spin box (0–1000000) |
| `flux_qt_create_progressbar()` | handle | Progress bar (0–100) |
| `flux_qt_create_tableview(rows, cols)` | handle | Table grid |

### ComboBox Helpers

| Function | Description |
|---|---|
| `flux_qt_combo_add_item(combo, text)` | Add item to combo box |
| `flux_qt_combo_clear(combo)` | Remove all items |
| `flux_qt_combo_set_current_index(combo, index)` | Select item by index |

### TableView Helpers

| Function | Description |
|---|---|
| `flux_qt_table_set_item(table, row, col, text)` | Set cell text |
| `flux_qt_table_set_header(table, col, text)` | Set column header |
| `flux_qt_table_row_count(table)` | Returns row count |
| `flux_qt_table_col_count(table)` | Returns column count |

### Dialogs

| Function | Description |
|---|---|
| `flux_qt_msg_box(title, text)` | Show information message box |

### Signals

| Function | Connects to |
|---|---|
| `flux_qt_on_click(handle, callbackFn)` | `clicked()` on buttons |
| `flux_qt_on_value_changed(handle, callbackFn)` | `valueChanged(int)` on sliders, spinboxes |
| `flux_qt_on_current_index_changed(handle, callbackFn)` | `currentIndexChanged(int)` on combo boxes |
| `flux_qt_on_toggled(handle, callbackFn)` | `toggled(bool)` on checkboxes |

### Widget Properties

All widget handles support property read/write through `.` syntax:
```flux
slider.value = 500           # write
val = slider.value            # read
lcd.display = 3.14
combo.currentIndex = 2
cb.checked = 1
spin.singleStep = 10
bar.value = 50
table.rowCount               # read-only
table.colCount               # read-only
```

---

## Workspace — `flux_*`, `viora_*`

### Print / Debug

| Function | Description |
|---|---|
| `viora_flux_print(msg)` | Print to stdout `[STDOUT]` |

### Schematic Component Properties

| Function | Description |
|---|---|
| `flux_get_var(name)` | Read a global variable (bridge handle store) |
| `flux_set_prop(ref, prop, value)` | Set a numeric property on component `ref` |
| `flux_set_prop_str(ref, prop, text)` | Set a string property on component `ref` |

### Simulation Control

| Function | Description |
|---|---|
| `flux_run_sim(type, tStop, tStep)` | Run analysis (type: `"tran"`, `"ac"`, `"dc"`, `"op"`) |

### Simulation Results

| Function | Description |
|---|---|
| `flux_sim_get_vector_size(name)` | Number of data points (e.g. `"v(out)"`, `"i(R1)"`) |
| `flux_sim_get_vector_val(name, index)` | Y-value at data point index |
| `flux_sim_get_vector_x(name, index)` | X-value (time/frequency) at data point index |

### Project Info

| Function | Returns |
|---|---|
| `flux_get_project_name()` | Current project name |
| `flux_get_schematic_file()` | Current schematic file path |
| `flux_get_open_schematics()` | Comma-separated open schematic file list |
| `flux_select_schematic(fileName)` | Switch active schematic tab |

### Plotting

| Function | Description |
|---|---|
| `flux_plot_point(seriesName, x, y)` | Add point to waveform viewer series |

---

## Math Built-ins (available without prefix)

| Function | Description |
|---|---|
| `sin(x)`, `cos(x)`, `tan(x)` | Trig functions |
| `asin(x)`, `acos(x)`, `atan(x)` | Inverse trig |
| `sqrt(x)`, `pow(x, y)` | Power/root |
| `exp(x)`, `log(x)`, `log10(x)` | Exponential/log |
| `abs(x)`, `floor(x)`, `ceil(x)` | Rounding |
| `min(x, y)`, `max(x, y)` | Comparison |
| `pi` | π constant |

---

## Writing an Extension

### Directory structure
```
~/.config/viospice/extensions/<id>/
├── manifest.json
└── main.flux
```

### manifest.json schema
```json
{
  "id": "my-ext",              // unique identifier
  "name": "My Extension",      // display name
  "version": "1.0.0",
  "author": "You",
  "description": "What it does",
  "main": "main.flux",         // entry point (optional, default)
  "hooks": {
    "onActivate": "init",      // called on load
    "onDeactivate": "cleanup"  // called on unload
  },
  "menu": [
    {"path": "My Extension", "action": "open_panel"}
  ],
  "contexts": [
    {"componentType": "R", "action": "on_resistor_click"}
  ]
}
```

### Lifecycle
1. **`onActivate`** — called after compilation. Good place to initialize state.
2. **Menu click** — calls the function named in `menu[].action`.
3. **Component double-click** — calls the function named in `contexts[].action`, passing the component handle.
4. **Auto-reload** — saving `main.flux` triggers `unload → reload`.
