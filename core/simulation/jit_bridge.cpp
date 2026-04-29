#include "jit_bridge.h"
#include "spice_backend.h"
#include "jit_context_manager.h"
#include "simulation_manager.h"
#include <QDebug>

namespace Flux {

JitBridge::JitBridge() {}

JitBridge& JitBridge::instance() {
    static JitBridge instance;
    return instance;
}

void JitBridge::registerTargetsWithEngine(const QMap<QString, FluxScriptTarget>& targets) {
    typedef void (*ngSpice_RegisterJitLogic_t)(const char*, double (*)(double, const double*));
    typedef double (*viospice_jit_func_t)(double, const double*);
    typedef viospice_jit_func_t (*viospice_jit_lookup_t)(const char*);

    static auto registerFunc = reinterpret_cast<ngSpice_RegisterJitLogic_t>(
        SpiceBackend::instance().resolveSymbol("ngSpice_RegisterJitLogic"));
    static auto lookupFunc = reinterpret_cast<viospice_jit_lookup_t>(
        SpiceBackend::instance().resolveSymbol("viospice_jit_lookup"));

    if (!registerFunc) {
        qWarning() << "[JitBridge] Native JIT API not found. Falling back to slow mode.";
        return;
    }

    for (auto it = targets.constBegin(); it != targets.constEnd(); ++it) {
        void* funcPtr = JITContextManager::instance().getFunctionAddress(it.key());
        if (!funcPtr) {
            qWarning() << "[JitBridge] Missing JIT function address for" << it.key();
            continue;
        }

        const QStringList aliases = getRegistrationAliases(it.key());
        for (const QString& alias : aliases) {
            registerFunc(alias.toUtf8().constData(), (double (*)(double, const double*))funcPtr);
        }

        if (lookupFunc) {
            bool matched = false;
            for (const QString& alias : aliases) {
                if (reinterpret_cast<void*>(lookupFunc(alias.toUtf8().constData())) == funcPtr) {
                    matched = true;
                    break;
                }
            }
            if (!matched) qWarning() << "[JitBridge] Native JIT registration lookup mismatch for" << it.key();
        }
    }
}

void JitBridge::executeFeedbackLoop(const std::vector<double>& sampleValues, double timeValue, 
                                   const QMap<QString, FluxScriptTarget>& targets) {
    if (targets.isEmpty()) return;

    JITContextManager::instance().setSimulationData(sampleValues);

    for (auto it = targets.begin(); it != targets.end(); ++it) {
        const QString& scriptId = it.key();
        const FluxScriptTarget& target = it.value();

        if (target.outputVoltageSources.isEmpty()) continue;

        JITContextManager::instance().setPinMapping(target.pinToNetMap);
        double vOut = JITContextManager::instance().runUpdate(scriptId, timeValue, sampleValues);

        if (!std::isfinite(vOut)) vOut = 0.0;

        for (const QString& vSrc : target.outputVoltageSources) {
            SimulationManager::instance().queueFluxSourceUpdate(vSrc, vOut);
        }
        
        if (SimulationManager::instance().isJitUpdateInProgress()) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }
}

QStringList JitBridge::getRegistrationAliases(const QString& id) {
    QStringList aliases;
    const QString trimmed = id.trimmed();
    if (trimmed.isEmpty()) return aliases;

    aliases << trimmed << trimmed.toUpper() << trimmed.toLower();
    aliases << QString("\"%1\"").arg(trimmed);
    aliases << QString("\"%1\"").arg(trimmed.toUpper());
    aliases << QString("\"%1\"").arg(trimmed.toLower());
    aliases.removeDuplicates();
    return aliases;
}

} // namespace Flux
