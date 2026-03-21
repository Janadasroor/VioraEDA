#ifndef TERMINAL_PANEL_H
#define TERMINAL_PANEL_H

#include <QWidget>
#include <QProcess>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>

class TerminalPanel : public QWidget {
    Q_OBJECT

public:
    explicit TerminalPanel(QWidget* parent = nullptr);
    ~TerminalPanel() override;

    void setWorkingDirectory(const QString& dir);

public slots:
    void openTerminal();
    void closeTerminal();
    void clearTerminal();

signals:
    void terminalClosed();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    void setupUi();
    void applyTheme();
    void writePrompt();
    void scrollToBottom();
    QString getPrompt() const;
    void executeCommand(const QString& cmd);

    QProcess* m_process = nullptr;
    QPlainTextEdit* m_output = nullptr;
    QWidget* m_toolbar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QComboBox* m_shellCombo = nullptr;
    QPushButton* m_newBtn = nullptr;
    QPushButton* m_killBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;

    QString m_workingDir;
    QString m_inputBuffer;
    int m_promptPosition = 0;
    bool m_processRunning = false;
};

#endif // TERMINAL_PANEL_H
