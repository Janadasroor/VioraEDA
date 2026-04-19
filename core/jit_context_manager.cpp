#include "jit_context_manager.h"
#include "simulation_manager.h"
#include <QDebug>
#include <mutex>
#include <vector>
#include <cstring>
#include <cstdint>
#include <iostream>

#ifdef HAVE_FLUXSCRIPT
#include <flux/compiler/compiler_instance.h>
#include <flux/compiler/lexer.h>
#include <flux/compiler/parser.h>
#endif

namespace Flux {

// Global storage for real-time simulation values for V() and I() lookups.
// These are updated every simulation step by SimulationManager via JITContextManager::runUpdate.
static std::vector<double> g_simValues;
static std::mutex g_simMutex;

JITContextManager& JITContextManager::instance() {
    static JITContextManager manager;
    return manager;
}

JITContextManager::JITContextManager() {
#ifdef HAVE_FLUXSCRIPT
    m_jit = std::make_unique<FluxJIT>();
    
    // Register EDA runtime functions so JIT code can call V() and I()
    m_jit->registerFunction("flux_get_voltage", (void*)&JITContextManager::getVoltage);
    m_jit->registerFunction("flux_get_current", (void*)&JITContextManager::getCurrent);
#endif
}

JITContextManager::~JITContextManager() {}

bool JITContextManager::compileAndLoad(const QString& id, const QString& source, QMap<int, QString>& errors) {
#ifdef HAVE_FLUXSCRIPT
    if (!m_jit) return false;

    // 1. Create compiler components
    std::string code = source.toStdString();
    CompilerOptions options;
    options.moduleName = id.toStdString(); // Use the ID as module name
    options.injectStdlib = true;
    CompilerInstance compiler(options);
    
    std::string errorStr;
    auto artifacts = compiler.compileToIR(code, &errorStr);
    
    if (!artifacts || !artifacts->codegenContext || !artifacts->codegenContext->OwnedModule) {
        errors[0] = QString::fromStdString(errorStr.empty() ? "Compilation failed" : errorStr);
        return false;
    }

    // 4. Add module to JIT
    m_jit->addModule(std::move(artifacts->codegenContext->OwnedModule), std::move(artifacts->codegenContext->OwnedContext));

    // 5. Look up 'update' function for simulation hook
    // If user didn't provide 'update', look for '__anon_expr' (top-level code)
    void* func = m_jit->getPointerToFunction("update");
    if (!func) {
        std::string anonName = id.toStdString() + "_anon_expr";
        func = m_jit->getPointerToFunction(anonName);
    }

    if (func) {
        m_updateFunctions[id] = func;
        qDebug() << "FluxScript: Loaded logic for" << id << "at" << func;
    } else {
        errors[0] = "FluxScript: No executable logic found (define 'update' or write top-level code).";
        return false;
    }

    Q_EMIT compilationFinished(true, "Script compiled and loaded in JIT.");
    return true;
#else
    Q_UNUSED(id);
    Q_UNUSED(source);
    errors[0] = "FluxScript support is disabled in this build.";
    return false;
#endif
}

double JITContextManager::runUpdate(const QString& id, double time, const std::vector<double>& inputs) {
#ifdef HAVE_FLUXSCRIPT
    // Update global sim values for V() and I() lookups
    setSimulationData(inputs);

    void* func = m_updateFunctions.value(id);
    if (func) {
        // signature: double update(double t, const double* inputs, int count)
        typedef double (*UpdateFunc)(double, const double*, int);
        double result = reinterpret_cast<UpdateFunc>(func)(time, inputs.data(), static_cast<int>(inputs.size()));
        std::cout << "[JIT DEBUG] runUpdate id=" << id.toStdString() << " time=" << time << " result=" << result << std::endl;
        return result;
    }
#else
    Q_UNUSED(id);
    Q_UNUSED(time);
    Q_UNUSED(inputs);
#endif
    return 0.0;
}

void JITContextManager::setPinMapping(const QMap<QString, QString>& mapping) {
#ifdef HAVE_FLUXSCRIPT
    m_currentPinMap = mapping;
#else
    Q_UNUSED(mapping);
#endif
}

void JITContextManager::logMessage(const QString& msg) {
    Q_EMIT scriptOutput(msg);
}

void JITContextManager::reset() {
#ifdef HAVE_FLUXSCRIPT
    m_updateFunctions.clear();
    m_currentPinMap.clear();
    m_jit = std::make_unique<FluxJIT>();
    
    // Re-register functions for the new JIT instance
    m_jit->registerFunction("flux_get_voltage", (void*)&JITContextManager::getVoltage);
    m_jit->registerFunction("flux_get_current", (void*)&JITContextManager::getCurrent);
#endif
}

// --- Runtime Helpers ---

double JITContextManager::getVoltage(double namePtr) {
    uint64_t addr;
    std::memcpy(&addr, &namePtr, sizeof(addr));
    const char* nameStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(addr));
    if (!nameStr) return 0.0;

    QString pinOrNet = QString::fromUtf8(nameStr);

    // Resolve pin name to net name if mapping exists (Case-Insensitive)
    QString targetName = pinOrNet;
    auto& self = JITContextManager::instance();
    #ifdef HAVE_FLUXSCRIPT
    std::lock_guard<std::mutex> lock(g_simMutex);

    // Search for pinOrNet in map keys case-insensitively
    bool found = false;
    for (auto it = self.m_currentPinMap.constBegin(); it != self.m_currentPinMap.constEnd(); ++it) {
        if (it.key().compare(pinOrNet, Qt::CaseInsensitive) == 0) {
            targetName = it.value();
            found = true;
            break;
        }
    }
    #endif
    int idx = SimulationManager::instance().getVectorIndex(targetName);

    // Fallback: If "Net" failed, try "V(Net)"
    if (idx < 0 && !targetName.startsWith("V(", Qt::CaseInsensitive) && !targetName.startsWith("I(", Qt::CaseInsensitive)) {
        idx = SimulationManager::instance().getVectorIndex("V(" + targetName + ")");
    }

    if (idx >= 0 && idx < (int)g_simValues.size()) {
        const double value = g_simValues[idx];
        static int debugCount = 0;
        if (debugCount < 20) {
            ++debugCount;
            std::cout << "[JIT VDBG] pinOrNet=" << pinOrNet.toStdString()
                      << " target=" << targetName.toStdString()
                      << " idx=" << idx
                      << " size=" << g_simValues.size()
                      << " value=" << value << std::endl;
        }
        return value;
    }

    static int missCount = 0;
    if (missCount < 20) {
        ++missCount;
        std::cout << "[JIT VDBG] MISS pinOrNet=" << pinOrNet.toStdString()
                  << " target=" << targetName.toStdString()
                  << " idx=" << idx
                  << " size=" << g_simValues.size() << std::endl;
    }

    return 0.0;
}

double JITContextManager::getCurrent(double namePtr) {
    uint64_t addr;
    std::memcpy(&addr, &namePtr, sizeof(addr));
    const char* nameStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(addr));
    if (!nameStr) return 0.0;

    QString branch = QString::fromUtf8(nameStr);

    // Resolve pin name to net name if mapping exists (Case-Insensitive)
    QString targetName = branch;
    auto& self = JITContextManager::instance();
    #ifdef HAVE_FLUXSCRIPT
    std::lock_guard<std::mutex> lock(g_simMutex);

    // Search for branch in map keys case-insensitively
    for (auto it = self.m_currentPinMap.constBegin(); it != self.m_currentPinMap.constEnd(); ++it) {
        if (it.key().compare(branch, Qt::CaseInsensitive) == 0) {
            targetName = it.value();
            break;
        }
    }
    #endif
    // Current vectors in ngspice are typically I(device)
    if (!targetName.startsWith("I(", Qt::CaseInsensitive)) {
        targetName = QString("I(%1)").arg(targetName);
    }

    int idx = SimulationManager::instance().getVectorIndex(targetName);

    // Fallback: If "V1" failed, try "I(V1)"
    if (idx < 0 && !targetName.startsWith("I(", Qt::CaseInsensitive)) {
        idx = SimulationManager::instance().getVectorIndex("I(" + targetName + ")");
    }

    if (idx >= 0 && idx < (int)g_simValues.size()) {
        return g_simValues[idx];
    }

    return 0.0;
}
void JITContextManager::setSimulationData(const std::vector<double>& values) {
    std::lock_guard<std::mutex> lock(g_simMutex);
    g_simValues = values;
}

} // namespace Flux
