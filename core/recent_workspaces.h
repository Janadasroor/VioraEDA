#ifndef RECENTWORKSPACES_H
#define RECENTWORKSPACES_H

#include <QObject>
#include <QStringList>

class RecentWorkspaces : public QObject {
    Q_OBJECT

public:
    static RecentWorkspaces& instance();

    void addWorkspace(const QString& workspacePath);
    void removeWorkspace(const QString& workspacePath);
    void clear();

    QStringList workspaces() const;
    QString mostRecent() const;

    void load();
    void save();

    int maxWorkspaces() const { return m_maxWorkspaces; }
    void setMaxWorkspaces(int max);

signals:
    void workspacesChanged();

private:
    RecentWorkspaces(QObject* parent = nullptr);
    ~RecentWorkspaces();

    QString settingsFilePath() const;
    void trimToMax();

    QStringList m_workspaces;
    int m_maxWorkspaces;
};

#endif // RECENTWORKSPACES_H
