#include "flux_qt_bridge.h"
#include "../engine/flux_script_engine.h"
#include <flux/jit_engine.h>
#include <QMetaProperty>
#include <QDebug>
#include <cstring>

// Helper to cast between void* and double handles
template <typename To, typename From>
inline To bit_cast(const From& src) noexcept {
    static_assert(sizeof(To) == sizeof(From), "bit_cast sizes must match");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

FluxQtBridge& FluxQtBridge::instance() {
    static FluxQtBridge inst;
    return inst;
}

FluxQtBridge::FluxQtBridge(QObject* parent) : QObject(parent) {}

double FluxQtBridge::registerObject(QObject* obj) {
    if (!obj) return 0.0;
    std::lock_guard<std::mutex> lock(m_mutex);
    void* ptr = static_cast<void*>(obj);
    m_registry[ptr] = obj;
    return bit_cast<double>(ptr);
}

QObject* FluxQtBridge::resolveHandle(double handle) const {
    void* ptr = bit_cast<void*>(handle);
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_registry.find(ptr);
    if (it != m_registry.end()) {
        if (it.value()) return it.value().data();
        // Object was deleted but not unregistered
        const_cast<FluxQtBridge*>(this)->m_registry.erase(it);
    }
    return nullptr;
}

double FluxQtBridge::getProperty(double handle, const char* name) {
    QObject* obj = instance().resolveHandle(handle);
    if (!obj) return 0.0;

    QVariant val = obj->property(name);
    if (val.userType() == QMetaType::Double || val.userType() == QMetaType::Int) {
        return val.toDouble();
    } else if (val.userType() == QMetaType::QString) {
        // FluxScript strings are also double-handles to const char*
        QString s = val.toString();
        // In a full implementation, we would pool this string. 
        // For the prototype, we assume it's a static or managed buffer.
        return bit_cast<double>(strdup(s.toUtf8().constData())); 
    }
    return 0.0;
}

void FluxQtBridge::setProperty(double handle, const char* name, double value) {
    QObject* obj = instance().resolveHandle(handle);
    if (!obj) return;

    // Check if the property expects a string handle (bitcast back)
    int propIdx = obj->metaObject()->indexOfProperty(name);
    if (propIdx >= 0) {
        QMetaProperty prop = obj->metaObject()->property(propIdx);
        if (prop.userType() == QMetaType::QString) {
            const char* s = static_cast<const char*>(bit_cast<void*>(value));
            obj->setProperty(name, QString::fromUtf8(s));
            return;
        }
    }

    obj->setProperty(name, value);
}

void FluxQtBridge::connectSignal(double handle, const char* signal, double functionHandle) {
    QObject* obj = resolveHandle(handle);
    if (!obj) return;

    // Use QObject's dynamic property system to store the handle
    this->QObject::setProperty("last_func_handle", functionHandle);

    // Standard string-based connect to a real slot
    QObject::connect(obj, signal, this, SLOT(onBridgeEvent()));
}

void FluxQtBridge::onBridgeEvent() {
    QString error;
    // Generic call for the prototype
    FluxScriptEngine::instance().callFunction("", {}, &error);
}

// C-API hooks for FluxScript Runtime
extern "C" {
    void flux_register_property_bridge(double (*getter)(double, const char*), 
                                      void (*setter)(double, const char*, double));
}

void initialize_flux_qt_runtime() {
    flux_register_property_bridge(FluxQtBridge::getProperty, FluxQtBridge::setProperty);
}

// Forward declarations for C-API bridge functions defined in flux_qt_widgets.cpp
extern "C" {
    double flux_qt_create_window(const char*);
    double flux_qt_create_button(const char*);
    double flux_qt_create_slider(double);
    double flux_qt_create_lcd();
    double flux_qt_create_label(const char*);
    double flux_qt_create_combobox();
    double flux_qt_create_checkbox(const char*);
    double flux_qt_create_spinbox();
    double flux_qt_create_progressbar();
    double flux_qt_create_tableview(double, double);
    void flux_qt_add_widget(double, double);
    void flux_qt_msg_box(const char*, const char*);
    void flux_qt_on_click(double, double);
    void flux_qt_on_value_changed(double, double);
    void flux_qt_on_current_index_changed(double, double);
    void flux_qt_on_toggled(double, double);
    void flux_qt_combo_add_item(double, const char*);
    void flux_qt_combo_clear(double);
    void flux_qt_combo_set_current_index(double, double);
    void flux_qt_table_set_item(double, double, double, const char*);
    void flux_qt_table_set_header(double, double, const char*);
    double flux_qt_table_row_count(double);
    double flux_qt_table_col_count(double);
    // Workspace bridge
    void viora_flux_print(const char*);
    double flux_get_var(const char*);
    void flux_set_prop(const char*, const char*, double);
}

void register_flux_qt_jit_symbols() {
    auto& jit = Flux::JITEngine::instance();
    if (!jit.isInitialized()) return;

    jit.registerFunction("flux_qt_create_window", (void*)&flux_qt_create_window);
    jit.registerFunction("flux_qt_create_button", (void*)&flux_qt_create_button);
    jit.registerFunction("flux_qt_create_slider", (void*)&flux_qt_create_slider);
    jit.registerFunction("flux_qt_create_lcd", (void*)&flux_qt_create_lcd);
    jit.registerFunction("flux_qt_create_label", (void*)&flux_qt_create_label);
    jit.registerFunction("flux_qt_create_combobox", (void*)&flux_qt_create_combobox);
    jit.registerFunction("flux_qt_create_checkbox", (void*)&flux_qt_create_checkbox);
    jit.registerFunction("flux_qt_create_spinbox", (void*)&flux_qt_create_spinbox);
    jit.registerFunction("flux_qt_create_progressbar", (void*)&flux_qt_create_progressbar);
    jit.registerFunction("flux_qt_create_tableview", (void*)&flux_qt_create_tableview);
    jit.registerFunction("flux_qt_add_widget", (void*)&flux_qt_add_widget);
    jit.registerFunction("flux_qt_msg_box", (void*)&flux_qt_msg_box);
    jit.registerFunction("flux_qt_on_click", (void*)&flux_qt_on_click);
    jit.registerFunction("flux_qt_on_value_changed", (void*)&flux_qt_on_value_changed);
    jit.registerFunction("flux_qt_on_current_index_changed", (void*)&flux_qt_on_current_index_changed);
    jit.registerFunction("flux_qt_on_toggled", (void*)&flux_qt_on_toggled);
    jit.registerFunction("flux_qt_combo_add_item", (void*)&flux_qt_combo_add_item);
    jit.registerFunction("flux_qt_combo_clear", (void*)&flux_qt_combo_clear);
    jit.registerFunction("flux_qt_combo_set_current_index", (void*)&flux_qt_combo_set_current_index);
    jit.registerFunction("flux_qt_table_set_item", (void*)&flux_qt_table_set_item);
    jit.registerFunction("flux_qt_table_set_header", (void*)&flux_qt_table_set_header);
    jit.registerFunction("flux_qt_table_row_count", (void*)&flux_qt_table_row_count);
    jit.registerFunction("flux_qt_table_col_count", (void*)&flux_qt_table_col_count);
    jit.registerFunction("viora_flux_print", (void*)&viora_flux_print);
    jit.registerFunction("flux_get_var", (void*)&flux_get_var);
    jit.registerFunction("flux_set_prop", (void*)&flux_set_prop);
}
