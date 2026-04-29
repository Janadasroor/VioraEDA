#ifndef FLUX_SCRIPT_ENGINE_H
#define FLUX_SCRIPT_ENGINE_H

#include <QObject>
#include <QString>

// CRITICAL: Undefine Qt's emit macro to prevent conflict with LLVM's emit() method
#ifdef emit
#undef emit
#endif

#include <variant>
#include <vector>
#include <complex>

/**
 * @brief Bridge to the standalone FluxScript Interpreter.
 * Replaces the legacy Python/pybind11 engine.
 */
class FluxScriptEngine : public QObject {
    Q_OBJECT
public:
    static FluxScriptEngine& instance();

    void initialize();
    void finalize();
    bool isInitialized() const;

    bool executeString(const QString& code, QString* error = nullptr);
    bool validateScript(const QString& code, QString* error = nullptr);

    // Generic call interface for simulation components
    using FluxValue = std::variant<double, int, std::complex<double>>;
    FluxValue callFunction(const char* method, const std::vector<double>& args, QString* error = nullptr);

private:
    explicit FluxScriptEngine(QObject* parent = nullptr);
};

#endif // FLUX_SCRIPT_ENGINE_H
