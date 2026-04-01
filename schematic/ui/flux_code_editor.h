#ifndef FLUX_CODE_EDITOR_H
#define FLUX_CODE_EDITOR_H

#include <flux/flux_editor.h>
#include <flux/flux_highlighter.h>

class QGraphicsScene;
class NetManager;

namespace Flux {

/**
 * @brief VioSpice-specific FluxScript editor.
 * Extends the base FluxEditor with schematic and netlist context.
 */
class CodeEditor : public FluxEditor {
    Q_OBJECT
public:
    explicit CodeEditor(QGraphicsScene* scene, NetManager* netManager, QWidget* parent = nullptr);
    void setScene(QGraphicsScene* scene, NetManager* netManager);
    
    // Explicitly override to match library virtuals
    void updateCompletionKeywords(const QStringList& keywords) override;
    void setErrorLines(const QMap<int, QString>& errors) override;
    void setActiveDebugLine(int line) override;

public slots:
    void onRunRequested() override;
    void showFindReplaceDialog();
    void findNext(const QString& text, bool forward = true);
    void replaceText(const QString& find, const QString& replace);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    bool event(QEvent* e) override;

private slots:
    void onContentChanged();
    void insertCompletion(const QString& completion);

private:
    QGraphicsScene* m_scene;
    NetManager* m_netManager;
};

} // namespace Flux

#endif // FLUX_CODE_EDITOR_H
