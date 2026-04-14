#ifndef PYTHON_CONSOLE_WIDGET_H
#define PYTHON_CONSOLE_WIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QStringList>

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

private:
    void printOutput(const QString& text, bool isError = false);
    void showPrompt();
    bool isCompleteStatement(const QString& code) const;

    QString m_currentInput;      // Multi-line input being accumulated
    QStringList m_history;       // Command history
    int m_historyIndex = -1;     // Current position in history
    int m_promptLine = 0;        // Line number where current prompt starts
    bool m_inContinuation = false; // Whether we're in a multi-line input
};

#endif // PYTHON_CONSOLE_WIDGET_H
