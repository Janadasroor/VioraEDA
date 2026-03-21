#ifndef BRANCH_DIALOG_H
#define BRANCH_DIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include "git_backend.h"

class BranchDialog : public QDialog {
    Q_OBJECT

public:
    explicit BranchDialog(QWidget* parent = nullptr);
    void setBranches(const QVector<GitBranch>& branches, const QString& currentBranch);
    void setRemotes(const QStringList& remotes);

signals:
    void switchRequested(const QString& branch);
    void createRequested(const QString& name);
    void deleteRequested(const QString& branch);
    void mergeRequested(const QString& branch);
    void addRemoteRequested(const QString& name, const QString& url);

private slots:
    void onSwitchClicked();
    void onCreateClicked();
    void onDeleteClicked();
    void onMergeClicked();
    void onAddRemoteClicked();

private:
    void setupUi();
    void applyTheme();
    void refreshList();
    QString selectedBranch() const;

    QVector<GitBranch> m_branches;
    QStringList m_remotes;
    QString m_currentBranch;
    QListWidget* m_branchList;
    QLineEdit* m_newBranchEdit;
    QLineEdit* m_remoteNameEdit;
    QLineEdit* m_remoteUrlEdit;
};

#endif // BRANCH_DIALOG_H
