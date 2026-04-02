#include "jit_context_manager.h"
#include <QDebug>

#ifdef HAVE_FLUXSCRIPT
#include <flux/compiler/compiler_instance.h>
#include <flux/compiler/lexer.h>
#include <flux/compiler/parser.h>
#endif

namespace Flux {

JITContextManager& JITContextManager::instance() {
    static JITContextManager manager;
    return manager;
}

JITContextManager::JITContextManager() {
#ifdef HAVE_FLUXSCRIPT
    m_jit = std::make_unique<FluxJIT>();
#endif
}

JITContextManager::~JITContextManager() {}

bool JITContextManager::compileAndLoad(const QString& source, QMap<int, QString>& errors) {
#ifdef HAVE_FLUXSCRIPT
    if (!m_jit) return false;

    // Reset old functions
    m_updateFunc = nullptr;

    // 1. Create compiler components
    std::string code = source.toStdString();
    CompilerOptions options;
    options.moduleName = "viospice_jit_module";
    CompilerInstance compiler(options);
    
    std::string errorStr;
    auto artifacts = compiler.compileToIR(code, &errorStr);
    
    if (!artifacts || !artifacts->codegenContext || !artifacts->codegenContext->TheModule) {
        errors[0] = QString::fromStdString(errorStr.empty() ? "Compilation failed" : errorStr);
        return false;
    }

    // 4. Add module to JIT
    m_jit->addModule(std::move(artifacts->codegenContext->TheModule), std::move(artifacts->codegenContext->OwnedContext));

    // 5. Look up 'update' function for simulation hook
    m_updateFunc = m_jit->getPointerToFunction("update");
    if (m_updateFunc) {
        qDebug() << "FluxScript: Found 'update' function at" << m_updateFunc;
    }

    Q_EMIT compilationFinished(true, "Script compiled and loaded in JIT.");
    return true;
#else
    errors[0] = "FluxScript support is disabled in this build.";
    return false;
#endif
}

void JITContextManager::runUpdate(double time) {
#ifdef HAVE_FLUXSCRIPT
    if (m_updateFunc) {
        typedef void (*UpdateFunc)(double);
        reinterpret_cast<UpdateFunc>(m_updateFunc)(time);
    }
#else
    Q_UNUSED(time);
#endif
}

void JITContextManager::logMessage(const QString& msg) {
    Q_EMIT scriptOutput(msg);
}

void JITContextManager::reset() {

#ifdef HAVE_FLUXSCRIPT
    m_jit = std::make_unique<FluxJIT>();
    m_updateFunc = nullptr;
#endif
}

} // namespace Flux
