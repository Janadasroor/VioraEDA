#include "flux_script_manager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QDebug>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>

FluxScriptManager::FluxScriptManager(QObject *parent) : QObject(parent) {}

FluxScriptManager& FluxScriptManager::instance() {
    static FluxScriptManager inst;
    return inst;
}

QString FluxScriptManager::getScriptsDir() {
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        QDir(appDir).absoluteFilePath("../python/scripts"),
        QDir(appDir).absoluteFilePath("../../python/scripts"),
        QDir::current().absoluteFilePath("python/scripts")
    };

    for (const QString& cand : candidates) {
        if (QDir(cand).exists()) {
            return QDir(cand).absolutePath();
        }
    }
    return QDir(appDir).absoluteFilePath("python/scripts");
}

QString FluxScriptManager::getFluxExecutable() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString bundledFlux = QDir(appDir).absoluteFilePath("flux");
    if (QFile::exists(bundledFlux)) return bundledFlux;

    QString buildFlux = QDir(appDir).absoluteFilePath("../../fluxscript_build/bin/flux");
    if (QFile::exists(buildFlux)) return buildFlux;
    
    return "flux";
}

QString FluxScriptManager::getPythonExecutable() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath("../python/qml/venv/bin/python"),
        QDir(appDir).absoluteFilePath("python/qml/venv/bin/python"),
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
    QString pyRoot = getPythonRoot();
    QString currentPath = env.value("PYTHONPATH");
    if (currentPath.isEmpty()) {
        env.insert("PYTHONPATH", pyRoot);
    } else {
        env.insert("PYTHONPATH", pyRoot + QDir::listSeparator() + currentPath);
    }
    
    QString sitePkgs = QDir(pyRoot).absoluteFilePath("site-packages");
    if (QDir(sitePkgs).exists()) {
        QString path = env.value("PYTHONPATH");
        env.insert("PYTHONPATH", sitePkgs + QDir::listSeparator() + path);
    }
    return env;
}

void FluxScriptManager::runScript(const QString &scriptName, const QVariantMap &args) {
    QString scriptPath = QDir(getScriptsDir()).absoluteFilePath(scriptName);
    if (!QFile::exists(scriptPath)) {
        emit errorOccurred(QString("Script not found: %1").arg(scriptPath));
        emit scriptError(QString("Script not found: %1").arg(scriptPath));
        return;
    }

    QStringList processArgs;
    processArgs << scriptPath;
    
    if (!args.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromVariant(args);
        processArgs << QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    }

    QProcess *process = new QProcess(this);
    process->setProcessEnvironment(getConfiguredEnvironment());
    
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        QString out = QString::fromUtf8(process->readAllStandardOutput());
        emit outputReceived(out);
        emit scriptOutput(out);
    });

    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        QString err = QString::fromUtf8(process->readAllStandardError());
        emit errorOccurred(err);
        emit scriptError(err);
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::CrashExit) {
            emit errorOccurred("Process crashed");
            emit scriptError("Process crashed");
        }
        emit finished(exitCode);
        emit scriptFinished(exitCode);
        process->deleteLater();
    });

    process->start(getPythonExecutable(), processArgs);
}

void FluxScriptManager::runScript(const QString &scriptName, const QStringList &args) {
    QString scriptPath = QDir(getScriptsDir()).absoluteFilePath(scriptName);
    if (!QFile::exists(scriptPath)) {
        emit errorOccurred(QString("Script not found: %1").arg(scriptPath));
        emit scriptError(QString("Script not found: %1").arg(scriptPath));
        return;
    }

    QStringList processArgs;
    processArgs << scriptPath << args;

    QProcess *process = new QProcess(this);
    process->setProcessEnvironment(getConfiguredEnvironment());
    
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        QString out = QString::fromUtf8(process->readAllStandardOutput());
        emit outputReceived(out);
        emit scriptOutput(out);
    });

    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        QString err = QString::fromUtf8(process->readAllStandardError());
        emit errorOccurred(err);
        emit scriptError(err);
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::CrashExit) {
            emit errorOccurred("Process crashed");
            emit scriptError("Process crashed");
        }
        emit finished(exitCode);
        emit scriptFinished(exitCode);
        process->deleteLater();
    });

    process->start(getPythonExecutable(), processArgs);
}
