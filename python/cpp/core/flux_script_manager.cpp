#include "flux_script_engine.h"
#include "flux_script_manager.h"
#include "config_manager.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFile>

FluxScriptManager& FluxScriptManager::instance() {
    static FluxScriptManager inst;
    return inst;
}

FluxScriptManager::FluxScriptManager(QObject* parent) : QObject(parent) {
}

void FluxScriptManager::runScript(const QString& scriptName, const QStringList& args) {
    QString fullPath = QDir(getScriptsDir()).absoluteFilePath(scriptName);
    
    // Support both .flux and .py (for backward compatibility if needed) 
    // but prioritize .flux
    if (!fullPath.endsWith(".flux") && !fullPath.endsWith(".py")) {
        if (QFile::exists(fullPath + ".flux")) fullPath += ".flux";
        else if (QFile::exists(fullPath + ".py")) fullPath += ".py";
    }

    if (!QFile::exists(fullPath)) {
        Q_EMIT scriptError(tr("FluxScript not found: %1").arg(fullPath));
        return;
    }

    QProcess* process = new QProcess(this);
    process->setProcessEnvironment(getConfiguredEnvironment());
    
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        QByteArray data = process->readAllStandardOutput();
        if (!data.isEmpty()) {
            Q_EMIT scriptOutput(QString::fromUtf8(data));
        }
    });

    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        QByteArray data = process->readAllStandardError();
        if (!data.isEmpty()) {
            Q_EMIT scriptError(QString::fromUtf8(data));
        }
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, [this, process](int exitCode) {
        Q_EMIT scriptFinished(exitCode);
        process->deleteLater();
    });

    QString fluxExec = getFluxExecutable();
    QString pythonExec = getPythonExecutable();
    
    QStringList allArgs;
    QString cmdToRun;
    
    if (fullPath.endsWith(".py")) {
        cmdToRun = pythonExec;
        allArgs << fullPath << args;
    } else {
        cmdToRun = fluxExec;
        allArgs << fullPath << args;
    }
    
    qDebug() << "[FluxScriptManager] Running:" << cmdToRun << "script:" << scriptName << "with" << args.size() << "args";
    process->start(cmdToRun, allArgs);
    if (!process->waitForStarted(1000)) {
        qDebug() << "[FluxScriptManager] Process failed to start!" << process->errorString();
    } else {
        qDebug() << "[FluxScriptManager] Process started successfully. PID:" << process->processId();
    }
}

QString FluxScriptManager::getScriptsDir() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath("../python/scripts"),
        QDir(appDir).absoluteFilePath("python/scripts"),
        QDir::current().absoluteFilePath("python/scripts"),
        QDir(appDir).absoluteFilePath("../fluxscript/scripts")
    };

    for (const QString& candidate : candidates) {
        if (QFile::exists(QDir(candidate).absoluteFilePath("gemini_query.py"))) {
            return candidate;
        }
    }

    return QDir::current().absoluteFilePath("python/scripts");
}

QString FluxScriptManager::getFluxExecutable() {
    // Prioritize the newly built flux binary in the sibling directory
    QString buildFlux = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../fluxscript_build/bin/flux");
    if (QFile::exists(buildFlux)) {
        return buildFlux;
    }
    
    // Fallback to system flux
    return "flux";
}

QString FluxScriptManager::getPythonExecutable() {
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../python/qml/venv/bin/python"),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("python/qml/venv/bin/python"),
        QStringLiteral("python3"),
        QStringLiteral("python")
    };

    for (const QString& candidate : candidates) {
        if (candidate == QLatin1String("python3") || candidate == QLatin1String("python")) {
            return candidate;
        }
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    return QStringLiteral("python3");
}

QString FluxScriptManager::getPythonRoot() {
    return QDir(getScriptsDir()).absoluteFilePath("..");
}

QProcessEnvironment FluxScriptManager::getConfiguredEnvironment() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    
    // Set PYTHONPATH so scripts can find ai_pipeline and vspice modules
    QString pyRoot = getPythonRoot();
    QString currentPath = env.value("PYTHONPATH");
    if (currentPath.isEmpty()) {
        env.insert("PYTHONPATH", pyRoot);
    } else {
        env.insert("PYTHONPATH", pyRoot + ":" + currentPath);
    }

    QString geminiKey = ConfigManager::instance().geminiApiKey();
    if (!geminiKey.isEmpty()) {
        env.insert("GEMINI_API_KEY", geminiKey);
        env.insert("GOOGLE_API_KEY", geminiKey);
    }
    return env;
}
