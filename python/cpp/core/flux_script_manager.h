#ifndef FLUX_SCRIPT_MANAGER_H
#define FLUX_SCRIPT_MANAGER_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QVariantMap>

/**
 * @brief Manages FluxScript execution for the viospice plugin system.
 */
class FluxScriptManager : public QObject {
    Q_OBJECT
public:
    explicit FluxScriptManager(QObject* parent = nullptr);
    static FluxScriptManager& instance();

    // New JSON-based signature
    Q_INVOKABLE void runScript(const QString& scriptName, const QVariantMap& args);
    
    // Legacy/Convenience signature
    Q_INVOKABLE void runScript(const QString& scriptName, const QStringList& args = {});

    static QString getFluxExecutable();
    static QString getPythonExecutable();
    static QString getPythonRoot();
    static QProcessEnvironment getConfiguredEnvironment();
    static QString getScriptsDir();

Q_SIGNALS:
    // New Signal Names
    void outputReceived(const QString& output);
    void errorOccurred(const QString& error);
    void finished(int exitCode);

    // Legacy Aliases for compatibility
    void scriptOutput(const QString& output);
    void scriptError(const QString& error);
    void scriptFinished(int exitCode);
};

#endif // FLUX_SCRIPT_MANAGER_H
