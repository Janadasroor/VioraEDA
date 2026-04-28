#ifndef FLUX_SCRIPT_EDITOR_TAB_H
#define FLUX_SCRIPT_EDITOR_TAB_H

#include <QWidget>
#include <QStackedWidget>
#include <QListWidget>
#include <QLineEdit>
#include "flux_code_editor.h"

class QGraphicsScene;
class NetManager;
class QTextEdit;
class QPushButton;
class SchematicAPI;

namespace Flux {

class ScriptEditorTab : public QWidget {
    Q_OBJECT
public:
    explicit ScriptEditorTab(QGraphicsScene* scene, NetManager* netManager, SchematicAPI* api, QWidget* parent = nullptr);
    
    bool openFile(const QString& filePath);
    bool saveFile(const QString& filePath = QString());
    QString currentFilePath() const { return m_filePath; }
    
    bool isModified() const { return m_isModified; }

signals:
    void modificationChanged(bool modified);

private slots:
    void onRunRequested();
    void onContentChanged();
    void onClearConsole();
    void onSearchTemplates(const QString& text);
    void onTemplateSelected(QListWidgetItem* item);
    void onCategorySelected(QListWidgetItem* current, QListWidgetItem* previous);
    void showTemplates();

private:
    void setupUI(QGraphicsScene* scene, NetManager* netManager);
    void setupWelcomeView();
    void updateWelcomeVisibility();
    void refreshTemplatesList();
    void applyFilters();

    QStackedWidget* m_stack;
    QWidget* m_editorContainer;
    QWidget* m_welcomeView;
    CodeEditor* m_editor;
    QListWidget* m_templateList;
    QListWidget* m_categoryList;
    QLineEdit* m_searchEdit;
    QTextEdit* m_console;
    SchematicAPI* m_api;
    QString m_filePath;
    bool m_isModified = false;
};

} // namespace Flux

#endif // FLUX_SCRIPT_EDITOR_TAB_H
