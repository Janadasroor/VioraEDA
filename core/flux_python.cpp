#include "flux_python.h"

#ifdef slots
#undef slots
#endif
#include <flux/jit_engine.h>
#define slots Q_SLOTS

FluxPython& FluxPython::instance() {
    static FluxPython inst;
    return inst;
}

FluxPython::FluxPython(QObject* parent) : QObject(parent) {}

void FluxPython::initialize() { 
    Flux::JITEngine::instance().initialize(); 
}

void FluxPython::finalize() { 
    Flux::JITEngine::instance().finalize(); 
}

bool FluxPython::isInitialized() const { 
    return Flux::JITEngine::instance().isInitialized(); 
}

bool FluxPython::executeString(const QString& code, QString* error) {
    return Flux::JITEngine::instance().executeString(code, error);
}

bool FluxPython::validateScript(const QString& code, QString* error) {
    // Validate by compiling but maybe not executing, or just compile it
    return Flux::JITEngine::instance().compileScript(code, error);
}

pybind11::object FluxPython::safeCall(pybind11::object object, const char* method, pybind11::tuple args, QString* error) {
    // Legacy Python bridge using new JIT engine under the hood
    // For pure JIT, we assume functions take doubles and return double.
    
    std::vector<double> cpp_args;
    for (size_t i = 0; i < args.size(); ++i) {
        try {
            cpp_args.push_back(args[i].cast<double>());
        } catch (...) {
            if (error) *error = "Invalid argument type passed to JIT";
            return pybind11::none();
        }
    }

    double result = Flux::JITEngine::instance().callFunction(method, cpp_args, error);
    
    if (error && !error->isEmpty()) {
        return pybind11::none();
    }
    
    return pybind11::float_(result);
}
