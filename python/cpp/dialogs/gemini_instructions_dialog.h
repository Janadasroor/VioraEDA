#ifndef GEMINI_INSTRUCTIONS_DIALOG_H
#define GEMINI_INSTRUCTIONS_DIALOG_H

#include <QDialog>
#include <QString>
#include <QListWidget>

class QPlainTextEdit;
class QComboBox;
class QTabWidget;
class QListWidget;

class GeminiInstructionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit GeminiInstructionsDialog(const QString& projectPath, QWidget* parent = nullptr);

private Q_SLOTS:
    void onScopeChanged(int index);
    void onSaveClicked();
    
    // Skill Management
    void onAddSkillClicked();
    void onImportSkillClicked();
    void onRemoveSkillClicked();
    void onRenameSkillClicked();
    void onSkillSelectionChanged();
    void onProjectSelectionChanged(int index);

private:
    void loadInstructions();
    void saveInstructions();
    void refreshSkillList();
    void updateProjectCombo();
    
    QString getGlobalSkillsPath() const;
    QString getSelectedProjectPath() const;
    QString getSkillsPathForProject(const QString& projectPath) const;

    QString m_projectPath;
    QTabWidget* m_tabWidget;
    
    // Instructions Tab
    QPlainTextEdit* m_editor;
    QComboBox* m_scopeCombo;
    
    // Skills Tab
    QListWidget* m_skillList;
    QComboBox* m_projectCombo;
    QPushButton* m_removeSkillBtn;
    QPushButton* m_renameSkillBtn;
    
    QString m_globalCache;
    QString m_projectCache;
    bool m_loading = false;
};

#endif // GEMINI_INSTRUCTIONS_DIALOG_H
