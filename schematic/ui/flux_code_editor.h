#ifndef FLUX_CODE_EDITOR_H
#define FLUX_CODE_EDITOR_H

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QMap>

class QGraphicsScene;
class NetManager;
class QCompleter;

namespace Flux {

class FluxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit FluxHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat keywordFormat;
    QTextCharFormat typeFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat spiceSuffixFormat;
};

/**
 * @brief VioSpice-specific FluxScript editor.
 */
class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CodeEditor(QGraphicsScene* scene, NetManager* netManager, QWidget* parent = nullptr);
    void setScene(QGraphicsScene* scene, NetManager* netManager);
    
    void updateCompletionKeywords(const QStringList& keywords);
    void setErrorLines(const QMap<int, QString>& errors);
    void setActiveDebugLine(int line);

    void setCompleter(QCompleter* completer);
    QCompleter* completer() const;

public Q_SLOTS:
    virtual void onRunRequested();
    void showFindReplaceDialog();
    void findNext(const QString& text, bool forward = true);
    void replaceText(const QString& find, const QString& replace);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    bool event(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private Q_SLOTS:
    void insertCompletion(const QString& completion);

private:
    QString textUnderCursor() const;

    QGraphicsScene* m_scene;
    NetManager* m_netManager;
    QCompleter* m_completer;
    FluxHighlighter* m_highlighter;
    QMap<int, QString> m_errorLines;
    int m_activeDebugLine = -1;
};

} // namespace Flux

#endif // FLUX_CODE_EDITOR_H
