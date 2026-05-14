#ifndef EXTENSION_MANAGER_H
#define EXTENSION_MANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QVector>
#include <QAction>
#include <QDir>
#include <QFileSystemWatcher>

struct ExtensionManifest {
    QString id;
    QString name;
    QString version;
    QString author;
    QString description;

    QString mainFile;
    QString onActivate;
    QString onDeactivate;

    struct MenuEntry {
        QString path;
        QString action;
    };
    QVector<MenuEntry> menuEntries;

    struct ContextEntry {
        QString componentType;
        QString action;
    };
    QVector<ContextEntry> contexts;

    QStringList permissions;

    bool parse(const QJsonObject& json, QString* error = nullptr);
};

class ExtensionManager : public QObject {
    Q_OBJECT
public:
    static ExtensionManager& instance();

    void addScanPath(const QString& path);
    void scanDirectories();
    void loadAll();
    void loadExtension(const QString& id);
    void unloadExtension(const QString& id);
    void reloadExtension(const QString& id);
    void reloadAll();

    void watchDirectories();
    void setFileWatcherEnabled(bool enabled);

    QVector<QAction*> createMenuActions(QWidget* parent);
    void dispatchComponentDoubleClick(const QString& componentType, double componentHandle);

    struct ExtensionInfo {
        QString id;
        QString name;
        QString version;
        QString author;
        bool loaded;
    };
    QVector<ExtensionInfo> listExtensions() const;

signals:
    void extensionLoaded(const QString& id);
    void extensionUnloaded(const QString& id);
    void extensionError(const QString& id, const QString& error);
    void extensionsChanged();

private:
    explicit ExtensionManager(QObject* parent = nullptr);

    struct Extension {
        ExtensionManifest manifest;
        QString dirPath;
        bool loaded = false;
    };

    QString sanitizeId(const QString& id) const;
    QString fnName(const QString& extId, const QString& action) const;
    bool callExtensionFn(const QString& extId, const QString& action);

    QVector<QString> m_scanPaths;
    QVector<Extension> m_extensions;
    QFileSystemWatcher* m_fileWatcher = nullptr;
    bool m_watcherEnabled = true;
};

#endif
