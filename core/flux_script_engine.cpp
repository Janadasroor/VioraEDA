#include "flux_script_engine.h"
#include <flux/jit_engine.h>
#include <QDebug>
#include <string>

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
    std::string stdError;
    bool ok = Flux::JITEngine::instance().executeString(code.toStdString(), error ? &stdError : nullptr);
    if (error) *error = QString::fromStdString(stdError);
    return ok;
}

bool FluxScriptEngine::validateScript(const QString& code, QString* error) {
    std::string stdError;
    bool ok = Flux::JITEngine::instance().compileScript(code.toStdString(), error ? &stdError : nullptr);
    if (error) *error = QString::fromStdString(stdError);
    return ok;
}

FluxScriptEngine::FluxValue FluxScriptEngine::callFunction(const char* method, const std::vector<double>& args, QString* error) {
    std::string stdError;
    auto result = Flux::JITEngine::instance().callFunction(std::string(method), args, error ? &stdError : nullptr);

    if (error) {
        *error = QString::fromStdString(stdError);
        if (!error->isEmpty()) {
            qDebug() << "[FluxScriptEngine] Error calling function:" << method << ":" << *error;
            return 0.0;
        }
    }

    if (std::holds_alternative<double>(result)) {
        return std::get<double>(result);
    } else if (std::holds_alternative<int>(result)) {
        return std::get<int>(result);
    } else if (std::holds_alternative<std::complex<double>>(result)) {
        return std::get<std::complex<double>>(result);
    }
    
    return 0.0;
}
