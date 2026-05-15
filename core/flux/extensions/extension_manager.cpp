#include "extension_manager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include <flux/jit_engine.h>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>
#include <QMenu>
#include <QTimer>
#include "../engine/flux_script_engine.h"

bool ExtensionManifest::parse(const QJsonObject& json, QString* error) {
    id = json["id"].toString();
    if (id.isEmpty()) {
        if (error) *error = "Extension manifest missing 'id'";
        return false;
    }
    name = json["name"].toString(id);
    version = json["version"].toString("0.1.0");
    author = json["author"].toString();
    description = json["description"].toString();
    mainFile = json["main"].toString("main.flux");

    QJsonObject hooks = json["hooks"].toObject();
    onActivate = hooks["onActivate"].toString();
    onDeactivate = hooks["onDeactivate"].toString();

    QJsonArray menuArr = json["menu"].toArray();
    for (const auto& m : menuArr) {
        QJsonObject mo = m.toObject();
        MenuEntry e;
        e.path = mo["path"].toString();
        e.action = mo["action"].toString();
        if (!e.path.isEmpty() && !e.action.isEmpty())
            menuEntries.append(e);
    }

    QJsonArray ctxArr = json["contexts"].toArray();
    for (const auto& c : ctxArr) {
        QJsonObject co = c.toObject();
        ContextEntry e;
        e.componentType = co["componentType"].toString();
        e.action = co["action"].toString();
        if (!e.componentType.isEmpty() && !e.action.isEmpty())
            contexts.append(e);
    }

    QJsonArray permArr = json["permissions"].toArray();
    for (const auto& p : permArr)
        permissions.append(p.toString());

    return true;
}

ExtensionManager& ExtensionManager::instance() {
    static ExtensionManager inst;
    return inst;
}

ExtensionManager::ExtensionManager(QObject* parent) : QObject(parent) {
    // Linux
    m_scanPaths.append(QDir::homePath() + "/.config/VioraEDA/extensions");
    // macOS
    m_scanPaths.append(QDir::homePath() + "/Library/Application Support/VioraEDA/extensions");
    // Windows
    m_scanPaths.append(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/extensions");
    // App bundle / portable
    QString appDir = QCoreApplication::applicationDirPath();
    m_scanPaths.append(appDir + "/../extensions");
    m_scanPaths.append(appDir + "/extensions");
}

void ExtensionManager::addScanPath(const QString& path) {
    if (!m_scanPaths.contains(path))
        m_scanPaths.append(path);
}

void ExtensionManager::scanDirectories() {
    m_extensions.clear();

    for (const auto& basePath : m_scanPaths) {
        QDir dir(basePath);
        if (!dir.exists()) continue;

        for (const auto& entryName : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString extDir = dir.filePath(entryName);
            QString manifestPath = extDir + "/manifest.json";

            if (!QFileInfo::exists(manifestPath)) continue;

            QFile f(manifestPath);
            if (!f.open(QIODevice::ReadOnly)) {
                qWarning() << "[ExtensionManager] Failed to read" << manifestPath;
                continue;
            }

            QJsonParseError parseErr;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseErr);
            f.close();

            if (parseErr.error != QJsonParseError::NoError) {
                qWarning() << "[ExtensionManager] JSON error in" << manifestPath << parseErr.errorString();
                continue;
            }

            Extension ext;
            ext.dirPath = extDir;
            QString error;
            if (!ext.manifest.parse(doc.object(), &error)) {
                qWarning() << "[ExtensionManager]" << manifestPath << error;
                continue;
            }

            // Check for duplicate IDs
            bool dup = false;
            for (const auto& existing : m_extensions) {
                if (existing.manifest.id == ext.manifest.id) {
                    dup = true;
                    break;
                }
            }
            if (!dup)
                m_extensions.append(ext);
        }
    }
}

void ExtensionManager::loadAll() {
    for (auto& ext : m_extensions) {
        if (!ext.loaded)
            loadExtension(ext.manifest.id);
    }
}

void ExtensionManager::reloadExtension(const QString& id) {
    unloadExtension(id);
    loadExtension(id);
    Q_EMIT extensionsChanged();
}

void ExtensionManager::reloadAll() {
    // Unload all loaded extensions
    for (auto& ext : m_extensions) {
        if (ext.loaded) {
            if (!ext.manifest.onDeactivate.isEmpty())
                callExtensionFn(ext.manifest.id, ext.manifest.onDeactivate);
            ext.loaded = false;
            Q_EMIT extensionUnloaded(ext.manifest.id);
        }
    }
    // Re-scan directories for new/removed extensions
    scanDirectories();
    // Reload all
    for (auto& ext : m_extensions) {
        if (!ext.loaded)
            loadExtension(ext.manifest.id);
    }
    // Re-watch directories
    if (m_fileWatcher)
        watchDirectories();
    Q_EMIT extensionsChanged();
}

void ExtensionManager::watchDirectories() {
    if (!m_fileWatcher) {
        m_fileWatcher = new QFileSystemWatcher(this);
        connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString& path) {
            if (!m_watcherEnabled) return;
            // Debounce: wait 300ms before reloading to avoid partial writes
            static QTimer* debounce = nullptr;
            if (!debounce) {
                debounce = new QTimer(this);
                debounce->setSingleShot(true);
                connect(debounce, &QTimer::timeout, this, [this]() {
                    // Find which extension owns the changed file and reload it
                    for (const auto& ext : m_extensions) {
                        QString manifestPath = ext.dirPath + "/manifest.json";
                        QString mainPath = ext.dirPath + "/" + ext.manifest.mainFile;
                        if (m_fileWatcher->files().contains(manifestPath) ||
                            m_fileWatcher->files().contains(mainPath)) {
                            reloadExtension(ext.manifest.id);
                            break;
                        }
                    }
                });
            }
            debounce->start(300);
        });
    }

    // Clear existing watches and re-add
    if (!m_fileWatcher->files().isEmpty())
        m_fileWatcher->removePaths(m_fileWatcher->files());

    QStringList filesToWatch;
    for (const auto& ext : m_extensions) {
        QString manifestPath = ext.dirPath + "/manifest.json";
        QString mainPath = ext.dirPath + "/" + ext.manifest.mainFile;
        if (QFileInfo::exists(manifestPath))
            filesToWatch.append(manifestPath);
        if (QFileInfo::exists(mainPath))
            filesToWatch.append(mainPath);
    }
    if (!filesToWatch.isEmpty())
        m_fileWatcher->addPaths(filesToWatch);
}

void ExtensionManager::setFileWatcherEnabled(bool enabled) {
    m_watcherEnabled = enabled;
}

QString ExtensionManager::sanitizeId(const QString& id) const {
    QString safe;
    for (const QChar& c : id) {
        if (c.isLetterOrNumber() || c == '_')
            safe += c;
        else
            safe += '_';
    }
    return safe;
}

QString ExtensionManager::fnName(const QString& extId, const QString& action) const {
    return action;
}

void ExtensionManager::loadExtension(const QString& id) {
    for (auto& ext : m_extensions) {
        if (ext.manifest.id != id) continue;
        if (ext.loaded) return;

        QString mainPath = ext.dirPath + "/" + ext.manifest.mainFile;
        if (!QFileInfo::exists(mainPath)) {
            QString err = "main file not found: " + mainPath;
            Q_EMIT extensionError(id, err);
            return;
        }

        QFile f(mainPath);
        if (!f.open(QIODevice::ReadOnly)) {
            Q_EMIT extensionError(id, "cannot read " + mainPath);
            return;
        }

        QString source = QString::fromUtf8(f.readAll());
        f.close();

        // Each extension is compiled in its own executeString() call,
        // so no name collision between extensions.
        QString prefixedSource = "// Extension: " + ext.manifest.id + "\n" + source;

        QString error;
        if (!FluxScriptEngine::instance().executeString(prefixedSource, &error)) {
            Q_EMIT extensionError(id, "compile error: " + error);
            return;
        }

        // Run top-level code (anonymous expressions at global scope), if any.
        // Most extensions only define functions, so "not found" is harmless.
        {   std::string anonErr;
            Flux::JITEngine::instance().callFunction("__anon_expr", {}, &anonErr);
        }

        ext.loaded = true;
        Q_EMIT extensionLoaded(id);

        // Call onActivate hook
        if (!ext.manifest.onActivate.isEmpty())
            callExtensionFn(id, ext.manifest.onActivate);

        break;
    }
}

void ExtensionManager::unloadExtension(const QString& id) {
    for (auto& ext : m_extensions) {
        if (ext.manifest.id != id) continue;
        if (!ext.loaded) return;

        if (!ext.manifest.onDeactivate.isEmpty())
            callExtensionFn(id, ext.manifest.onDeactivate);

        ext.loaded = false;
        Q_EMIT extensionUnloaded(id);
        break;
    }
}

bool ExtensionManager::callExtensionFn(const QString& extId, const QString& action) {
    QString error;
    FluxScriptEngine::instance().callFunction(action.toUtf8().constData(), {}, &error);
    if (!error.isEmpty()) {
        Q_EMIT extensionError(extId, "error calling " + action + ": " + error);
        return false;
    }
    return true;
}

QVector<QAction*> ExtensionManager::createMenuActions(QWidget* parent) {
    QVector<QAction*> actions;
    for (const auto& ext : m_extensions) {
        if (!ext.loaded) continue;
        for (const auto& entry : ext.manifest.menuEntries) {
            QString text = entry.path;
            // Support nested menus via "/" separator
            QString label = text.contains('/')
                ? text.section('/', -1)
                : text;

            QAction* action = new QAction(label, parent);
            QString extId = ext.manifest.id;
            QString actionName = entry.action;
            connect(action, &QAction::triggered, this, [this, extId, actionName]() {
                callExtensionFn(extId, actionName);
            });
            actions.append(action);
        }
    }
    return actions;
}

void ExtensionManager::dispatchComponentDoubleClick(const QString& componentType, double componentHandle) {
    for (const auto& ext : m_extensions) {
        if (!ext.loaded) continue;
        for (const auto& ctx : ext.manifest.contexts) {
            if (ctx.componentType == componentType) {
                // Pass the component handle as an argument
                QString fname = fnName(ext.manifest.id, ctx.action);
                QString error;
                FluxScriptEngine::instance().callFunction(fname.toUtf8().constData(), {componentHandle}, &error);
                if (!error.isEmpty())
                    Q_EMIT extensionError(ext.manifest.id, "context error: " + error);
            }
        }
    }
}

QVector<ExtensionManager::ExtensionInfo> ExtensionManager::listExtensions() const {
    QVector<ExtensionInfo> info;
    for (const auto& ext : m_extensions) {
        info.append({ext.manifest.id, ext.manifest.name, ext.manifest.version, ext.manifest.author, ext.loaded});
    }
    return info;
}
