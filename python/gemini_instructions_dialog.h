#ifndef GEMINI_INSTRUCTIONS_DIALOG_H
#define GEMINI_INSTRUCTIONS_DIALOG_H

#include <QDialog>
#include <QString>

class QPlainTextEdit;
class QComboBox;

class GeminiInstructionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit GeminiInstructionsDialog(const QString& projectPath, QWidget* parent = nullptr);

private slots:
    void onScopeChanged(int index);
    void onSaveClicked();

private:
    void loadInstructions();
    void saveInstructions();

    QString m_projectPath;
    QPlainTextEdit* m_editor;
    QComboBox* m_scopeCombo;
    
    QString m_globalCache;
    QString m_projectCache;
    bool m_loading = false;
};

#endif // GEMINI_INSTRUCTIONS_DIALOG_H
