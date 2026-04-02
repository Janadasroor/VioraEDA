#include "flux_script_engine.h"
#include <flux/jit_engine.h>
#include <QDebug>

FluxScriptEngine& FluxScriptEngine::instance() {
    static FluxScriptEngine inst;
    return inst;
}

FluxScriptEngine::FluxScriptEngine(QObject* parent) : QObject(parent) {}

void FluxScriptEngine::initialize() { 
    Flux::JITEngine::instance().initialize(); 
}

void FluxScriptEngine::finalize() { 
    Flux::JITEngine::instance().finalize(); 
}

bool FluxScriptEngine::isInitialized() const { 
    return Flux::JITEngine::instance().isInitialized(); 
}

bool FluxScriptEngine::executeString(const QString& code, QString* error) {
    return Flux::JITEngine::instance().executeString(code, error);
}

bool FluxScriptEngine::validateScript(const QString& code, QString* error) {
    return Flux::JITEngine::instance().compileScript(code, error);
}

FluxScriptEngine::FluxValue FluxScriptEngine::callFunction(const char* method, const std::vector<double>& args, QString* error) {
    // Pure JIT call without Python overhead
    auto result = Flux::JITEngine::instance().callFunction(method, args, error);

    if (error && !error->isEmpty()) {
        qDebug() << "[FluxScriptEngine] Error calling function:" << method << ":" << *error;
        return 0.0;
    }

    // Convert Flux::JITEngine::Result (assumed to be similar std::variant) to FluxScriptEngine::FluxValue
    // For now, assume it's directly compatible or handle conversion
    if (std::holds_alternative<double>(result)) {
        return std::get<double>(result);
    } else if (std::holds_alternative<int>(result)) {
        return std::get<int>(result);
    } else if (std::holds_alternative<std::complex<double>>(result)) {
        return std::get<std::complex<double>>(result);
    }
    
    return 0.0;
}
