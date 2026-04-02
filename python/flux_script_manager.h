#ifndef FLUX_SCRIPT_MANAGER_H
#define FLUX_SCRIPT_MANAGER_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QVariantMap>

/**
 * @brief Manages FluxScript execution for the viospice plugin system.
 * Replaces the legacy PythonManager.
 */
class FluxScriptManager : public QObject {
    Q_OBJECT
public:
    static FluxScriptManager& instance();

    void runScript(const QString& scriptName, const QStringList& args = {});

    static QString getFluxExecutable();
    static QString getPythonExecutable();
    static QProcessEnvironment getConfiguredEnvironment();
    static QString getScriptsDir();

Q_SIGNALS:
    void scriptOutput(const QString& output);
    void scriptError(const QString& error);
    void scriptFinished(int exitCode);

private:
    explicit FluxScriptManager(QObject* parent = nullptr);
};

#endif // FLUX_SCRIPT_MANAGER_H
