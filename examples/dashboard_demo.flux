# test_fluxqt_dashboard.flux
# A demonstration of FluxQt integration with VioSpice simulation.
# Corrected for FluxScript syntax and handle scoping.

# 1. Setup UI
dash = flux_qt_create_window("VioSpice Dashboard")
flux_qt_add_widget(dash, flux_qt_create_label("Interactive Simulation Control"))

# Create and store the slider handle
flux_qt_add_widget(dash, flux_qt_create_label("Adjust R1 Value (Ohms):"))
r_slider = flux_qt_create_slider(0.0) # Horizontal
r_slider.minimum = 100
r_slider.maximum = 10000
r_slider.value = 1000
flux_qt_add_widget(dash, r_slider)

# Create and store the LCD handle
flux_qt_add_widget(dash, flux_qt_create_label("Measured Voltage (V):"))
v_lcd = flux_qt_create_lcd()
flux_qt_add_widget(dash, v_lcd)

# Share handles globally so top-level functions can access them
flux_set_var("R_SLIDER", r_slider)
flux_set_var("V_LCD", v_lcd)

# 2. Logic: Define interaction handlers using 'def'
def update_resistor() {
    # Retrieve our handles from the global bridge
    slider = flux_get_var("R_SLIDER")
    val = slider.value
    
    # Update the schematic property
    flux_set_prop("R1", "value", val)
    viora_flux_print("Updated R1 to " + val + " ohms")
}

def run_dashboard_sim() {
    viora_flux_print("Starting simulation...")
    
    # Run a transient analysis (1ms stop, 1us step)
    flux_run_sim("tran", 0.001, 0.000001)
    
    # Update the LCD with results
    size = flux_sim_get_vector_size("v(out)")
    if (size > 0) {
        last_v = flux_sim_get_vector_val("v(out)", size - 1)
        lcd = flux_get_var("V_LCD")
        lcd.display = last_v
        viora_flux_print("Simulation finished. Final voltage: " + last_v + "V")
    } else {
        viora_flux_print("No simulation results found for v(out)")
    }
}

# 3. Bind events
flux_qt_on_value_changed(r_slider, update_resistor)

run_btn = flux_qt_create_button("Run Simulation")
flux_qt_on_click(run_btn, run_dashboard_sim)
flux_qt_add_widget(dash, run_btn)

viora_flux_print("FluxQt Dashboard Initialized.")
