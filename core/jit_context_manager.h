#ifndef JIT_CONTEXT_MANAGER_H
#define JIT_CONTEXT_MANAGER_H

#ifdef HAVE_FLUXSCRIPT
#pragma push_macro("emit")
#undef emit
#include <flux/jit/flux_jit.h>
#include <flux/compiler/compiler_instance.h>
#include <flux/compiler/lexer.h>
#include <flux/compiler/parser.h>
#pragma pop_macro("emit")
#endif

#include <QObject>
#include <QString>
#include <QMap>
#include <memory>

namespace Flux {

/**
 * @brief Manages the FluxScript JIT environment for the current VioSpice session.
 * This class isolates the LLVM state and handles compilation requests from the UI.
 */
class JITContextManager : public QObject {
    Q_OBJECT
public:
    static JITContextManager& instance();

    /**
     * @brief Compiles and loads a script into the JIT engine.
     * @return true if compilation succeeded, false otherwise.
     */
    bool compileAndLoad(const QString& source, QMap<int, QString>& errors);

    /**
     * @brief Executes the 'update' function in the JIT if it exists.
     * Guaranteed to be thread-safe for calls from the simulation thread.
     */
    void runUpdate(double time);

    /**
     * @brief Resets the JIT state (clears all loaded modules).
     */
    void reset();

    /**
     * @brief Logs a message from the JIT to the console.
     */
    void logMessage(const QString& msg);

Q_SIGNALS:
    void compilationFinished(bool success, QString message);
    void scriptOutput(const QString& message);

private:
    JITContextManager();
    ~JITContextManager();

#ifdef HAVE_FLUXSCRIPT
    std::unique_ptr<FluxJIT> m_jit;
    void* m_updateFunc = nullptr;
#endif
};

} // namespace Flux

#endif // JIT_CONTEXT_MANAGER_H
