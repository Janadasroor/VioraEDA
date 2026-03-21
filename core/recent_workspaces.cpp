#include "recent_workspaces.h"
#include <QDir>
#include <QFile>
#include <QSettings>

RecentWorkspaces& RecentWorkspaces::instance() {
    static RecentWorkspaces instance;
    return instance;
}

RecentWorkspaces::RecentWorkspaces(QObject* parent)
    : QObject(parent)
    , m_maxWorkspaces(10) {
    load();
}

RecentWorkspaces::~RecentWorkspaces() {
    save();
}

void RecentWorkspaces::addWorkspace(const QString& workspacePath) {
    m_workspaces.removeAll(workspacePath);
    m_workspaces.prepend(workspacePath);
    trimToMax();
    save();
    emit workspacesChanged();
}

void RecentWorkspaces::removeWorkspace(const QString& workspacePath) {
    if (m_workspaces.removeAll(workspacePath) > 0) {
        save();
        emit workspacesChanged();
    }
}

void RecentWorkspaces::clear() {
    m_workspaces.clear();
    save();
    emit workspacesChanged();
}

QStringList RecentWorkspaces::workspaces() const {
    return m_workspaces;
}

QString RecentWorkspaces::mostRecent() const {
    return m_workspaces.isEmpty() ? QString() : m_workspaces.first();
}

void RecentWorkspaces::load() {
    QSettings settings;
    m_workspaces = settings.value("recentWorkspaces").toStringList();
}

void RecentWorkspaces::save() {
    QSettings settings;
    settings.setValue("recentWorkspaces", m_workspaces);
}

QString RecentWorkspaces::settingsFilePath() const {
    return QString();
}

void RecentWorkspaces::trimToMax() {
    while (m_workspaces.size() > m_maxWorkspaces) {
        m_workspaces.removeLast();
    }
}
