#ifndef PYTHON_CONSOLE_WIDGET_H
#define PYTHON_CONSOLE_WIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QVector>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QElapsedTimer>

/**
 * @brief Syntax highlighter for the Python console.
 *
 * Highlights keywords, strings, comments, numbers, and builtins
 * in a Blender/VS Code-inspired color scheme.
 */
class PythonHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit PythonHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightingRule> highlightingRules;
    QTextCharFormat keywordFormat;
    QTextCharFormat builtinFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat numberFormat;
};

/**
 * @brief Interactive Python REPL console embedded in the VioSpice GUI.
 *
 * Provides a Blender-style Python console where users can type Python code
 * and see output in real-time. Uses the embedded Python runtime initialized
 * at startup.
 */
class PythonConsoleWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit PythonConsoleWidget(QWidget* parent = nullptr);

    /** Execute a single line of Python code and capture output. */
    void executeLine(const QString& code);

    /** Clear the console output. */
    void clearOutput();

Q_SIGNALS:
    /** Emitted when a complete command has been executed. */
    void commandExecuted(const QString& code, const QString& output, bool error);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;

private:
    void printOutput(const QString& text, bool isError = false);
    void showPrompt();
    bool isCompleteStatement(const QString& code) const;

    // Input handling helpers
    void insertTextAtPrompt(const QString& text);
    QString getCurrentLineText() const;
    void clearCurrentLine();
    void cycleHistory(int direction);

    // Tab completion
    void handleTabCompletion();
    QStringList getCompletionSuggestions(const QString& prefix);

    // History search
    void startHistorySearch();
    void cancelHistorySearch();

    // Magic commands
    bool handleMagicCommand(const QString& cmd);

    // Formatting
    void applyPromptFormat();

    QString m_currentInput;      // Multi-line input being accumulated
    QStringList m_history;       // Command history
    int m_historyIndex = 0;      // Current position in history
    int m_promptLine = 0;        // Line number where current prompt starts
    bool m_inContinuation = false; // Whether we're in a multi-line input

    // Tab completion state
    QString m_completionPrefix;
    QStringList m_completionMatches;
    int m_completionIndex = -1;
    bool m_completing = false;

    // History search state
    bool m_historySearchActive = false;
    QString m_historySearchText;

    // Syntax highlighter
    PythonHighlighter* m_highlighter = nullptr;
};

#endif // PYTHON_CONSOLE_WIDGET_H
