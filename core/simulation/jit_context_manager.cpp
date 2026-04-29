/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0
 http://www.apache.org/licenses/LICENSE-2.0
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
     http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License. */

#include "jit_context_manager.h"
#include "simulation_manager.h"
#include "../../simulator/core/sim_results.h"
#include <flux/compiler/compiler_instance.h>
#include <flux/runtime/flux_runtime.h>
#include <QDebug>
#include <QRegularExpression>

#include "flux_design_rule_bridge.h"
#include "flux_workspace_bridge.h"

namespace Flux {

static QString cleanId(const QString& id) {
    QString s = id.trimmed();
    bool changed = true;
    while(changed) {
        changed = false;
        if (s.startsWith('"') && s.endsWith('"')) { s = s.mid(1, s.length() - 2).trimmed(); changed = true; }
        else if (s.startsWith('\'') && s.endsWith('\'')) { s = s.mid(1, s.length() - 2).trimmed(); changed = true; }
    }
    return s;
}

static std::vector<double> g_simValues;
static std::mutex g_simMutex;

JITContextManager& JITContextManager::instance() {
    static JITContextManager inst;
    return inst;
}

JITContextManager::JITContextManager() {
#ifdef HAVE_FLUXSCRIPT
    m_jit = std::make_unique<FluxJIT>();
    // Register runtime helpers for FluxScript
    Flux::registerRuntimeFunctions(*m_jit);
    m_jit->registerFunction("flux_get_voltage", (void*)&JITContextManager::getVoltage);
    m_jit->registerFunction("flux_get_current", (void*)&JITContextManager::getCurrent);
    
    // Register Design Rule API
    m_jit->registerFunction("erc_get_component_count", (void*)&flux_erc_get_component_count);
    m_jit->registerFunction("erc_get_ref", (void*)&flux_erc_get_ref);
    m_jit->registerFunction("erc_get_value", (void*)&flux_erc_get_value);
    m_jit->registerFunction("erc_get_type", (void*)&flux_erc_get_type);
    m_jit->registerFunction("erc_get_pin_count", (void*)&flux_erc_get_pin_count);
    m_jit->registerFunction("erc_get_pin_net", (void*)&flux_erc_get_pin_net);
    m_jit->registerFunction("erc_report", (void*)&flux_erc_report);
    m_jit->registerFunction("erc_set_block_pins", (void*)&flux_erc_set_block_pins);
    m_jit->registerFunction("print", (void*)&viora_flux_print);
    m_jit->registerFunction("printf", (void*)&viora_flux_print);
    m_jit->registerFunction("flux_sim_get_vector_size", (void*)&flux_sim_get_vector_size);
    m_jit->registerFunction("flux_sim_get_vector_val", (void*)&flux_sim_get_vector_val);
    m_jit->registerFunction("flux_sim_get_vector_x", (void*)&flux_sim_get_vector_x);
    
    // Register Workspace Bridge
    m_jit->registerFunction("get_var", (void*)&flux_get_var);
    m_jit->registerFunction("schematic_set_prop", (void*)&flux_set_prop);
    m_jit->registerFunction("schematic_set_prop_str", (void*)&flux_set_prop_str);
    m_jit->registerFunction("run_sim", (void*)&flux_run_sim);
    m_jit->registerFunction("get_project_name", (void*)&flux_get_project_name);
    m_jit->registerFunction("get_schematic_file", (void*)&flux_get_schematic_file);
    m_jit->registerFunction("get_open_schematics", (void*)&flux_get_open_schematics);
    m_jit->registerFunction("select_schematic", (void*)&flux_select_schematic);
    m_jit->registerFunction("plot_point", (void*)&flux_plot_point);
    m_jit->registerFunction("flux_register_analysis", (void*)&flux_register_analysis);
    m_jit->registerFunction("flux_register_measure", (void*)&flux_register_measure);
    m_jit->registerFunction("flux_register_probe", (void*)&flux_register_probe);
    m_jit->registerFunction("flux_register_save", (void*)&flux_register_save);
#endif
}

JITContextManager::~JITContextManager() {
}

bool JITContextManager::compileAndLoad(const QString& id, const QString& source, QMap<int, QString>& errors) {
#ifdef HAVE_FLUXSCRIPT
    if (!m_jit) {
        errors[0] = "JIT not initialized.";
        return false;
    }

    // 1. Transform V("PinName") to inputs[i] if it matches a known pin
    QString transformedSource = source;
    {
        std::lock_guard<std::mutex> lock(m_funcMutex);
        QString cid = cleanId(id);
        if (m_blockInputs.contains(cid)) {
            const QStringList& pins = m_blockInputs[cid];
            for (int i = 0; i < pins.size(); ++i) {
                // Match V("PinName") or V('PinName') case-insensitively
                // We use \b to ensure we don't match substrings.
                // We escape the pin name for regex safety.
                QRegularExpression re(QString(R"(\bV\s*\(\s*["']%1["']\s*\))").arg(QRegularExpression::escape(pins[i])), QRegularExpression::CaseInsensitiveOption);
                if (transformedSource.contains(re)) {
                    transformedSource.replace(re, QString("inputs[%1]").arg(i));
                }
            }
        }
    }

    // Wrap the source in the expected format if needed
    // Use a unique function name to avoid JIT symbol collisions
    QString safeId = cleanId(id);
    safeId.replace("-", "_").replace(".", "_").replace(" ", "_");
    
    static int salt = 0;
    QString uniqueFuncName = QString("update_%1_%2").arg(safeId).arg(salt++);

    QString wrapped = transformedSource;
    bool isStandalone = id.startsWith("standalone_");

    if (isStandalone) {
        // Wrap standalone scripts in a callable function if they don't look like they have one
        if (!transformedSource.contains("def main")) {
            wrapped = QString("def %1() {\n%2\n}").arg(uniqueFuncName, transformedSource);
        } else {
             // If it has main, we'll try to call main directly
             wrapped = transformedSource;
             uniqueFuncName = "main";
        }
    } else {
        static const QRegularExpression updateDefRe(R"(^\s*(def\s+)?update\s*[\({])");
        if (!updateDefRe.match(transformedSource).hasMatch()) {
            wrapped = QString("def %1(t, inputs) {\n%2\n}").arg(uniqueFuncName, transformedSource);
        } else {
            // Rename 'update' to uniqueFuncName
            wrapped.replace(updateDefRe, QString("def %1(").arg(uniqueFuncName));
        }
    }

    CompilerOptions options;
    options.inputName = id.toStdString();
    options.optimizationLevel = OptimizationLevel::O3;

    CompilerInstance compiler(options);
    std::string errStr;
    auto artifacts = compiler.compileToIR(wrapped.toStdString(), &errStr);

    if (!artifacts) {
        errors[0] = QString::fromStdString(errStr);
        return false;
    }

    // Add module and context to JIT
    m_jit->addModule(std::move(artifacts->codegenContext->OwnedModule), 
                     std::move(artifacts->codegenContext->OwnedContext));

    void* func = m_jit->getPointerToFunction(uniqueFuncName.toStdString());
    if (func) {
        std::lock_guard<std::mutex> lock(m_funcMutex);
        m_updateFunctions[id] = func;
        qDebug() << "FluxScript: Loaded logic for" << id << "at" << func << "(symbol:" << uniqueFuncName << ")";
    } else {
        errors[0] = QString("FluxScript: Failed to find generated function %1").arg(uniqueFuncName);
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

void JITContextManager::reset() {
#ifdef HAVE_FLUXSCRIPT
    std::lock_guard<std::mutex> lock(m_funcMutex);
    m_updateFunctions.clear();
    m_blockInputs.clear();
    m_currentPinMap.clear();
    m_jit = std::make_unique<FluxJIT>();
    
    // Re-register functions for the new JIT instance
    Flux::registerRuntimeFunctions(*m_jit);
    m_jit->registerFunction("flux_get_voltage", (void*)&JITContextManager::getVoltage);
    m_jit->registerFunction("flux_get_current", (void*)&JITContextManager::getCurrent);
    
    // Register Design Rule API
    m_jit->registerFunction("erc_get_component_count", (void*)&flux_erc_get_component_count);
    m_jit->registerFunction("erc_get_ref", (void*)&flux_erc_get_ref);
    m_jit->registerFunction("erc_get_value", (void*)&flux_erc_get_value);
    m_jit->registerFunction("erc_get_type", (void*)&flux_erc_get_type);
    m_jit->registerFunction("erc_get_pin_count", (void*)&flux_erc_get_pin_count);
    m_jit->registerFunction("erc_get_pin_net", (void*)&flux_erc_get_pin_net);
    m_jit->registerFunction("erc_report", (void*)&flux_erc_report);
    m_jit->registerFunction("erc_set_block_pins", (void*)&flux_erc_set_block_pins);
    m_jit->registerFunction("print", (void*)&viora_flux_print);
    m_jit->registerFunction("printf", (void*)&viora_flux_print);
    m_jit->registerFunction("flux_sim_get_vector_size", (void*)&flux_sim_get_vector_size);
    m_jit->registerFunction("flux_sim_get_vector_val", (void*)&flux_sim_get_vector_val);
    m_jit->registerFunction("flux_sim_get_vector_x", (void*)&flux_sim_get_vector_x);
    
    // Register Workspace Bridge
    m_jit->registerFunction("get_var", (void*)&flux_get_var);
    m_jit->registerFunction("schematic_set_prop", (void*)&flux_set_prop);
    m_jit->registerFunction("schematic_set_prop_str", (void*)&flux_set_prop_str);
    m_jit->registerFunction("run_sim", (void*)&flux_run_sim);
    m_jit->registerFunction("get_project_name", (void*)&flux_get_project_name);
    m_jit->registerFunction("get_schematic_file", (void*)&flux_get_schematic_file);
    m_jit->registerFunction("get_open_schematics", (void*)&flux_get_open_schematics);
    m_jit->registerFunction("select_schematic", (void*)&flux_select_schematic);
    m_jit->registerFunction("plot_point", (void*)&flux_plot_point);
    m_jit->registerFunction("flux_register_analysis", (void*)&flux_register_analysis);
    m_jit->registerFunction("flux_register_measure", (void*)&flux_register_measure);
    m_jit->registerFunction("flux_register_probe", (void*)&flux_register_probe);
    m_jit->registerFunction("flux_register_save", (void*)&flux_register_save);
#endif
}

void JITContextManager::setPinMapping(const QMap<QString, QString>& mapping) {
#ifdef HAVE_FLUXSCRIPT
    std::lock_guard<std::mutex> lock(m_funcMutex);
    m_currentPinMap = mapping;
#else
    Q_UNUSED(mapping);
#endif
}

void JITContextManager::setInputPinMapping(const QString& id, const QStringList& pins) {
#ifdef HAVE_FLUXSCRIPT
    std::lock_guard<std::mutex> lock(m_funcMutex);
    QStringList cleaned;
    for (const auto& p : pins) cleaned << cleanId(p);
    m_blockInputs[cleanId(id)] = cleaned;
#else
    Q_UNUSED(id);
    Q_UNUSED(pins);
#endif
}

void JITContextManager::setSimulationResults(const SimResults* results) {
#ifdef HAVE_FLUXSCRIPT
    std::lock_guard<std::mutex> lock(m_funcMutex);
    m_lastResults = results;
#else
    Q_UNUSED(results);
#endif
}

const SimResults* JITContextManager::getSimulationResults() const {
#ifdef HAVE_FLUXSCRIPT
    return m_lastResults;
#else
    return nullptr;
#endif
}

void JITContextManager::logMessage(const QString& msg) {
    qDebug() << "[JIT Context] logMessage:" << msg;
    Q_EMIT scriptOutput(msg);
}

void* JITContextManager::getFunctionAddress(const QString& id) {
#ifdef HAVE_FLUXSCRIPT
    std::lock_guard<std::mutex> lock(m_funcMutex);
    return m_updateFunctions.value(id);
#else
    Q_UNUSED(id);
    return nullptr;
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
    QString targetNet = pinOrNet;
    {
        std::lock_guard<std::mutex> lock(instance().m_funcMutex);
        for (auto it = instance().m_currentPinMap.begin(); it != instance().m_currentPinMap.end(); ++it) {
            if (it.key().compare(pinOrNet, Qt::CaseInsensitive) == 0) {
                targetNet = it.value();
                break;
            }
        }
    }

    // Lookup voltage from simulation data
    // (This part is still used for global net lookups if not optimized to direct input access)
    std::lock_guard<std::mutex> lock(g_simMutex);
    // In a real implementation, we would use targetNet to find the correct index
    // For now, return 0.0 or a dummy value if not in the optimized path
    return 0.0;
}

double JITContextManager::getCurrent(double namePtr) {
    uint64_t addr;
    std::memcpy(&addr, &namePtr, sizeof(addr));
    const char* nameStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(addr));
    if (!nameStr) return 0.0;

    // Resolve current (e.g., branch current of a component)
    return 0.0;
}

double JITContextManager::runUpdate(const QString& id, double time, const std::vector<double>& inputs) {
#ifdef HAVE_FLUXSCRIPT
    void* func = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_funcMutex);
        func = m_updateFunctions.value(id);
    }

    if (func) {
        typedef double (*UpdateFunc)(double, const double*);
        return reinterpret_cast<UpdateFunc>(func)(time, inputs.data());
    }
#endif
    return 0.0;
}

void JITContextManager::setSimulationData(const std::vector<double>& values) {
    std::lock_guard<std::mutex> lock(g_simMutex);
    g_simValues = values;
}

} // namespace Flux
