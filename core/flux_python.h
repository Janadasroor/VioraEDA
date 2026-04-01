#ifndef FLUX_PYTHON_BRIDGE_H
#define FLUX_PYTHON_BRIDGE_H

#include <QObject>
#include <QString>

#ifdef slots
#undef slots
#endif
#include <pybind11/pybind11.h>
#define slots Q_SLOTS

/**
 * @brief Legacy bridge to the new standalone FluxScript Interpreter.
 * Maintained for backward compatibility in VioSpice.
 */
class FluxPython : public QObject {
    Q_OBJECT
public:
    static FluxPython& instance();

    void initialize();
    void finalize();
    bool isInitialized() const;

    bool executeString(const QString& code, QString* error = nullptr);
    bool validateScript(const QString& code, QString* error = nullptr);

    pybind11::object safeCall(pybind11::object object, const char* method, pybind11::tuple args, QString* error = nullptr);

private:
    explicit FluxPython(QObject* parent = nullptr);
};

#endif // FLUX_PYTHON_BRIDGE_H
