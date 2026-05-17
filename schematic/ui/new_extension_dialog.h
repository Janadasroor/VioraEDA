#ifndef NEW_EXTENSION_DIALOG_H
#define NEW_EXTENSION_DIALOG_H

#include <QDialog>

class QLineEdit;
class QTextEdit;
class QComboBox;
class QLabel;
class QPushButton;

class NewExtensionDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewExtensionDialog(QWidget* parent = nullptr);

    QString extensionId() const { return m_id; }
    QString extensionPath() const { return m_path; }

private slots:
    void onCreate();
    void onIdChanged(const QString& text);

private:
    void setupUI();
    QString generateId(const QString& name) const;
    QString scaffoldManifest() const;
    QString scaffoldMain(const QString& templateType) const;

    QLineEdit* m_nameEdit;
    QLineEdit* m_idEdit;
    QLineEdit* m_authorEdit;
    QLineEdit* m_versionEdit;
    QTextEdit* m_descEdit;
    QComboBox* m_templateCombo;
    QLabel* m_statusLabel;
    QPushButton* m_createBtn;

    QString m_id;
    QString m_path;
};

#endif // NEW_EXTENSION_DIALOG_H
