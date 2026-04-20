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
#include <flux/compiler/compiler_instance.h>
#include <QDebug>
#include <QRegularExpression>
#include <iostream>

namespace Flux {

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
    m_jit->registerFunction("flux_get_voltage", (void*)&JITContextManager::getVoltage);
    m_jit->registerFunction("flux_get_current", (void*)&JITContextManager::getCurrent);
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

    // Wrap the source in the expected format if needed
    QString wrapped = source;
    static const QRegularExpression updateDefRe(R"(^\s*(def\s+)?update\s*[\({])");
    if (!updateDefRe.match(source).hasMatch()) {
        wrapped = "def update(t, inputs) {\n" + source + "\n}";
    }


    CompilerOptions options;
    options.inputName = id.toStdString();
    options.debugInfo = true;
    
    CompilerInstance compiler(options);
    std::string err;
    auto artifacts = compiler.compileToIR(wrapped.toStdString(), &err);

    if (!artifacts || !artifacts->codegenContext) {
        errors[0] = QString::fromStdString(err);
        return false;
    }

    // Transfer module and context to JIT
    m_jit->addModule(std::move(artifacts->codegenContext->OwnedModule), 
                     std::move(artifacts->codegenContext->OwnedContext));

    void* func = m_jit->getPointerToFunction("update");
    if (func) {
        std::lock_guard<std::mutex> lock(m_funcMutex);
        m_updateFunctions[id] = func;
        qDebug() << "FluxScript: Loaded logic for" << id << "at" << func;
    } else {
        errors[0] = "FluxScript: No executable logic found (define 'update').";
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
    void* func = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_funcMutex);
        func = m_updateFunctions.value(id);
    }

    if (func) {
        // signature: double update(double t, const double* inputs, int count)
        typedef double (*UpdateFunc)(double, const double*, int);
        double result = reinterpret_cast<UpdateFunc>(func)(time, inputs.data(), static_cast<int>(inputs.size()));
        // std::cout << "[JIT RESULT] id=" << id.toStdString() << " t=" << time << " res=" << result << std::endl;
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
    std::lock_guard<std::mutex> lock(m_funcMutex);
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
    std::lock_guard<std::mutex> lock(m_funcMutex);
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
    {
        std::lock_guard<std::mutex> lock(self.m_funcMutex);
        // Search for pinOrNet in map keys case-insensitively
        for (auto it = self.m_currentPinMap.constBegin(); it != self.m_currentPinMap.constEnd(); ++it) {
            if (it.key().compare(pinOrNet, Qt::CaseInsensitive) == 0) {
                targetName = it.value();
                break;
            }
        }
    }

    std::lock_guard<std::mutex> lock(g_simMutex);
    int idx = SimulationManager::instance().getVectorIndex(targetName);

    // Fallback: If "Net" failed, try "V(Net)"
    if (idx < 0 && !targetName.startsWith("V(", Qt::CaseInsensitive) && !targetName.startsWith("I(", Qt::CaseInsensitive)) {
        idx = SimulationManager::instance().getVectorIndex("V(" + targetName + ")");
    }

    if (idx >= 0 && idx < (int)g_simValues.size()) {
        double val = g_simValues[idx];
        // std::cout << "[JIT READ] V(" << targetName.toStdString() << ") = " << val << std::endl;
        return val;
    }
    #endif
    return 0.0;
}

double JITContextManager::getCurrent(double namePtr) {
    uint64_t addr;
    std::memcpy(&addr, &namePtr, sizeof(addr));
    const char* nameStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(addr));
    if (!nameStr) return 0.0;

    QString deviceName = QString::fromUtf8(nameStr);
    auto& self = JITContextManager::instance();

    #ifdef HAVE_FLUXSCRIPT
    std::lock_guard<std::mutex> lock(g_simMutex);
    int idx = SimulationManager::instance().getVectorIndex(deviceName);
    
    // Fallback: If "R1" failed, try "I(R1)"
    if (idx < 0 && !deviceName.startsWith("I(", Qt::CaseInsensitive)) {
        idx = SimulationManager::instance().getVectorIndex("I(" + deviceName + ")");
    }

    if (idx >= 0 && idx < (int)g_simValues.size()) {
        double val = g_simValues[idx];
        std::cout << "[JIT READ] I(" << deviceName.toStdString() << ") = " << val << std::endl;
        return val;
    }
    #endif
    return 0.0;
}

void JITContextManager::setSimulationData(const std::vector<double>& values) {
    std::lock_guard<std::mutex> lock(g_simMutex);
    g_simValues = values;
}

} // namespace Flux
