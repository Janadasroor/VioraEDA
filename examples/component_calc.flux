# Component Value Calculator
# Adjust parameters with sliders/spinboxes, see results update live.
# Uses string-based signal binding (_by_name functions).

def show_about() {
    flux_qt_msg_box("Component Calculator", "Interactive filter and divider calculator\nv1.0")
}

def update_c() {
    r = flux_get_var("R_SPIN")
    c = 1.0 / (2.0 * 3.14159 * 1000.0 * r) * 1000000.0
    c_val = flux_get_var("C_VAL")
    c_val.display = c
}

def update_divider() {
    r1 = flux_get_var("R1")
    r2 = flux_get_var("R2")
    vin = flux_get_var("VIN")
    vout = vin * r2 / (r1 + r2)
    vout_lcd = flux_get_var("VOUT")
    vout_lcd.display = vout
}

def on_band_selected() {
    band = flux_get_var("BAND")
    idx = band.currentIndex
    f_val = flux_get_var("F_VAL")
    r = flux_get_var("R_SPIN")
    if (idx == 0.0) {
        f_val.display = 1000.0
        c = 1.0 / (2.0 * 3.14159 * 1000.0 * r) * 1000000.0
        c_val = flux_get_var("C_VAL")
        c_val.display = c
    }
    if (idx == 1.0) {
        f_val.display = 50.0
        c = 1.0 / (2.0 * 3.14159 * 50.0 * r) * 1000000.0
        c_val = flux_get_var("C_VAL")
        c_val.display = c
    }
    if (idx == 2.0) {
        f_val.display = 1000000.0
        c = 1.0 / (2.0 * 3.14159 * 1000000.0 * r) * 1000000.0
        c_val = flux_get_var("C_VAL")
        c_val.display = c
    }
    if (idx == 3.0) {
        f_val.display = 98000000.0
        c = 1.0 / (2.0 * 3.14159 * 98000000.0 * r) * 1000000.0
        c_val = flux_get_var("C_VAL")
        c_val.display = c
    }
}

def build_ui() {
    win = flux_qt_create_window("Component Calculator")

    # RC Low-Pass Filter
    flux_qt_add_widget(win, flux_qt_create_label("RC Low-Pass Filter"))
    f_val = flux_qt_create_lcd()
    f_val.digitCount = 6
    f_val.display = 1000
    flux_qt_add_widget(win, f_val)

    r_spin = flux_qt_create_spinbox()
    r_spin.minimum = 10
    r_spin.maximum = 1000000
    r_spin.value = 1000
    r_spin.singleStep = 100
    flux_qt_add_widget(win, r_spin)

    c_val = flux_qt_create_lcd()
    c_val.digitCount = 6
    c_val.display = 0.159
    flux_qt_add_widget(win, c_val)

    # Voltage Divider
    flux_qt_add_widget(win, flux_qt_create_label("Voltage Divider"))
    vin_spin = flux_qt_create_spinbox()
    vin_spin.minimum = 1
    vin_spin.maximum = 50
    vin_spin.value = 5
    flux_qt_add_widget(win, vin_spin)

    r1_slider = flux_qt_create_slider(0.0)
    r1_slider.minimum = 1
    r1_slider.maximum = 100
    r1_slider.value = 10
    flux_qt_add_widget(win, r1_slider)

    r2_slider = flux_qt_create_slider(0.0)
    r2_slider.minimum = 1
    r2_slider.maximum = 100
    r2_slider.value = 10
    flux_qt_add_widget(win, r2_slider)

    vout_lcd = flux_qt_create_lcd()
    vout_lcd.digitCount = 5
    vout_lcd.display = 2.5
    flux_qt_add_widget(win, vout_lcd)

    # Frequency band selector
    band_combo = flux_qt_create_combobox()
    flux_qt_combo_add_item(band_combo, "Audio")
    flux_qt_combo_add_item(band_combo, "Power")
    flux_qt_combo_add_item(band_combo, "AM Radio")
    flux_qt_combo_add_item(band_combo, "FM Radio")
    flux_qt_add_widget(win, band_combo)

    about = flux_qt_create_button("About")
    flux_qt_add_widget(win, about)

    # Wire signals using string-based binding
    flux_qt_on_click_by_name(about, "show_about")
    flux_qt_on_value_changed_by_name(r_spin, "update_c")
    flux_qt_on_value_changed_by_name(r1_slider, "update_divider")
    flux_qt_on_value_changed_by_name(r2_slider, "update_divider")
    flux_qt_on_value_changed_by_name(vin_spin, "update_divider")
    flux_qt_on_current_index_changed_by_name(band_combo, "on_band_selected")

    # Share handles
    flux_set_var("R_SPIN", r_spin)
    flux_set_var("C_VAL", c_val)
    flux_set_var("F_VAL", f_val)
    flux_set_var("VIN", vin_spin)
    flux_set_var("R1", r1_slider)
    flux_set_var("R2", r2_slider)
    flux_set_var("VOUT", vout_lcd)
    flux_set_var("BAND", band_combo)

    viora_flux_print("Calculator ready")
}

build_ui()
