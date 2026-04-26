#ifndef FLUX_SCRIPT_EDITOR_TAB_H
#define FLUX_SCRIPT_EDITOR_TAB_H

#include <QWidget>
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

private:
    void setupUI(QGraphicsScene* scene, NetManager* netManager);

    CodeEditor* m_editor;
    QTextEdit* m_console;
    SchematicAPI* m_api;
    QString m_filePath;
    bool m_isModified = false;
};

} // namespace Flux

#endif // FLUX_SCRIPT_EDITOR_TAB_H
