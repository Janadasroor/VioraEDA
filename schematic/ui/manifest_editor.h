#ifndef MANIFEST_EDITOR_H
#define MANIFEST_EDITOR_H

#include <QDialog>
#include <QJsonObject>

class QLineEdit;
class QTextEdit;
class QListWidget;
class QLabel;
class QPushButton;

class ManifestEditor : public QDialog {
    Q_OBJECT
public:
    explicit ManifestEditor(const QString& manifestPath, QWidget* parent = nullptr);

    bool save();

private slots:
    void onAddMenuEntry();
    void onRemoveMenuEntry();
    void onAddContext();
    void onRemoveContext();
    void onTextChanged();

private:
    void setupUI();
    void loadManifest();
    void parseMenuEntries(const QJsonObject& obj);
    void parseContexts(const QJsonObject& obj);

    QString m_manifestPath;
    bool m_modified = false;

    QLineEdit* m_idEdit;
    QLineEdit* m_nameEdit;
    QLineEdit* m_versionEdit;
    QLineEdit* m_authorEdit;
    QLineEdit* m_mainFileEdit;
    QTextEdit* m_descEdit;

    // Menu entries
    QListWidget* m_menuList;
    QLineEdit* m_menuPathEdit;
    QLineEdit* m_menuActionEdit;

    // Contexts
    QListWidget* m_contextList;
    QLineEdit* m_contextTypeEdit;
    QLineEdit* m_contextActionEdit;

    QLabel* m_statusLabel;
    QPushButton* m_saveBtn;
};

#endif // MANIFEST_EDITOR_H
