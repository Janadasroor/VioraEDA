#include "terminal_panel.h"
#include "theme_manager.h"
#include <QKeyEvent>
#include <QScrollBar>
#include <QFontMetrics>
#include <QDir>
#include <QProcessEnvironment>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QFileInfo>

TerminalPanel::TerminalPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    applyTheme();
}

TerminalPanel::~TerminalPanel() {
    closeTerminal();
}

void TerminalPanel::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // === Toolbar ===
    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName("TermToolbar");
    m_toolbar->setFixedHeight(30);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(m_toolbar);
    toolbarLayout->setContentsMargins(8, 2, 8, 2);
    toolbarLayout->setSpacing(4);

    m_shellCombo = new QComboBox(m_toolbar);
    m_shellCombo->addItem("bash");
    m_shellCombo->addItem("sh");
    m_shellCombo->addItem("zsh");
    m_shellCombo->setFixedWidth(80);
    m_shellCombo->setFixedHeight(22);
    toolbarLayout->addWidget(m_shellCombo);

    m_newBtn = new QPushButton("+", m_toolbar);
    m_newBtn->setFixedSize(22, 22);
    m_newBtn->setToolTip("New Terminal");
    toolbarLayout->addWidget(m_newBtn);

    m_killBtn = new QPushButton("\u00d7", m_toolbar);
    m_killBtn->setFixedSize(22, 22);
    m_killBtn->setToolTip("Kill Terminal");
    toolbarLayout->addWidget(m_killBtn);

    m_clearBtn = new QPushButton("Clear", m_toolbar);
    m_clearBtn->setFixedHeight(22);
    m_clearBtn->setToolTip("Clear Terminal");
    toolbarLayout->addWidget(m_clearBtn);

    toolbarLayout->addStretch();

    m_statusLabel = new QLabel(m_toolbar);
    m_statusLabel->setStyleSheet("font-size: 10px; color: #888;");
    toolbarLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(m_toolbar);

    // === Terminal output ===
    m_output = new QPlainTextEdit(this);
    m_output->setObjectName("TermOutput");
    m_output->setReadOnly(false);
    m_output->setWordWrapMode(QTextOption::WrapAnywhere);
    m_output->setUndoRedoEnabled(false);
    m_output->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_output->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_output->setTabStopDistance(QFontMetrics(m_output->font()).horizontalAdvance(' ') * 4);
    m_output->setFrameShape(QFrame::NoFrame);
    mainLayout->addWidget(m_output, 1);

    // Connect toolbar buttons
    connect(m_newBtn, &QPushButton::clicked, this, [this]() {
        closeTerminal();
        openTerminal();
    });
    connect(m_killBtn, &QPushButton::clicked, this, &TerminalPanel::closeTerminal);
    connect(m_clearBtn, &QPushButton::clicked, this, &TerminalPanel::clearTerminal);
    connect(m_shellCombo, &QComboBox::currentTextChanged, this, [this]() {
        closeTerminal();
        openTerminal();
    });
}

void TerminalPanel::applyTheme() {
    bool isLight = ThemeManager::theme() && ThemeManager::theme()->type() == PCBTheme::Light;

    if (isLight) {
        m_output->setStyleSheet(
            "QPlainTextEdit#TermOutput {"
            "  background-color: #1e1e1e;"
            "  color: #d4d4d4;"
            "  border: none;"
            "  font-family: 'Cascadia Code', 'Fira Code', 'JetBrains Mono', 'Consolas', 'Courier New', monospace;"
            "  font-size: 12px;"
            "  padding: 4px;"
            "  selection-background-color: #264f78;"
            "}"
        );
        if (m_toolbar) {
            m_toolbar->setStyleSheet(
                "QWidget#TermToolbar {"
                "  background-color: #f8fafc;"
                "  border-bottom: 1px solid #e2e8f0;"
                "}"
                "QPushButton {"
                "  background-color: #ffffff; color: #334155;"
                "  border: 1px solid #cbd5e1; border-radius: 4px;"
                "  font-size: 12px; font-weight: bold;"
                "}"
                "QPushButton:hover { background-color: #f1f5f9; border-color: #94a3b8; }"
                "QPushButton:pressed { background-color: #e2e8f0; }"
                "QComboBox {"
                "  background-color: #ffffff; color: #334155;"
                "  border: 1px solid #cbd5e1; border-radius: 4px;"
                "  padding: 1px 4px; font-size: 11px;"
                "}"
                "QComboBox:hover { border-color: #94a3b8; }"
            );
        }
    } else {
        m_output->setStyleSheet(
            "QPlainTextEdit#TermOutput {"
            "  background-color: #1e1e1e;"
            "  color: #d4d4d4;"
            "  border: none;"
            "  font-family: 'Cascadia Code', 'Fira Code', 'JetBrains Mono', 'Consolas', 'Courier New', monospace;"
            "  font-size: 12px;"
            "  padding: 4px;"
            "  selection-background-color: #264f78;"
            "}"
        );
        if (m_toolbar) {
            m_toolbar->setStyleSheet(
                "QWidget#TermToolbar {"
                "  background-color: #252526;"
                "  border-bottom: 1px solid #3c3c3c;"
                "}"
                "QPushButton {"
                "  background-color: #2d2d30; color: #cccccc;"
                "  border: 1px solid #3c3c3c; border-radius: 4px;"
                "  font-size: 12px; font-weight: bold;"
                "}"
                "QPushButton:hover { background-color: #3c3c3c; border-color: #555; }"
                "QPushButton:pressed { background-color: #094771; }"
                "QComboBox {"
                "  background-color: #3c3c3c; color: #cccccc;"
                "  border: 1px solid #555; border-radius: 4px;"
                "  padding: 1px 4px; font-size: 11px;"
                "}"
                "QComboBox:hover { border-color: #777; }"
            );
        }
    }
}

void TerminalPanel::setWorkingDirectory(const QString& dir) {
    m_workingDir = dir;
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->write(("cd " + dir + "\n").toUtf8());
    } else {
        openTerminal();
    }
}

QString TerminalPanel::getPrompt() const {
    QString dir = m_workingDir.isEmpty() ? "~" : QFileInfo(m_workingDir).fileName();
    if (dir.isEmpty()) dir = "/";
    return dir + " $ ";
}

void TerminalPanel::writePrompt() {
    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_output->setTextCursor(cursor);
    m_output->insertPlainText(getPrompt());
    m_promptPosition = m_output->textCursor().position();
    m_inputBuffer.clear();
    scrollToBottom();
}

void TerminalPanel::scrollToBottom() {
    QScrollBar* sb = m_output->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void TerminalPanel::openTerminal() {
    if (m_process && m_process->state() == QProcess::Running) return;

    m_output->clear();
    m_inputBuffer.clear();
    m_promptPosition = 0;

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TERM", "dumb");
    env.insert("PROMPT_COMMAND", "");
    env.insert("PS1", "");
    env.insert("PS2", "");
    m_process->setProcessEnvironment(env);

    if (!m_workingDir.isEmpty()) {
        m_process->setWorkingDirectory(m_workingDir);
    }

    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalPanel::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalPanel::onReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TerminalPanel::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &TerminalPanel::onProcessError);

    QString shell = m_shellCombo->currentText();
    if (shell.isEmpty()) shell = "bash";

    m_process->start(shell, {"--norc", "--noprofile", "-i"});
    m_processRunning = true;
    m_statusLabel->setText("Terminal: " + shell);

    // Wait a moment for shell to start, then show prompt
    QTimer::singleShot(300, this, [this]() {
        if (m_process && m_process->state() == QProcess::Running) {
            writePrompt();
        }
    });

    m_output->setFocus();
}

void TerminalPanel::closeTerminal() {
    if (m_process) {
        if (m_process->state() == QProcess::Running) {
            m_process->write("exit\n");
            if (!m_process->waitForFinished(1000)) {
                m_process->kill();
                m_process->waitForFinished(500);
            }
        }
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_processRunning = false;
    m_statusLabel->setText("Terminal stopped");
    m_inputBuffer.clear();
    m_promptPosition = 0;
}

void TerminalPanel::clearTerminal() {
    m_output->clear();
    m_inputBuffer.clear();
    m_promptPosition = 0;
    if (m_processRunning) {
        writePrompt();
    }
}

void TerminalPanel::onReadyRead() {
    if (!m_process) return;

    QByteArray data = m_process->readAllStandardOutput();
    if (data.isEmpty()) data = m_process->readAllStandardError();
    if (data.isEmpty()) return;

    QString text = QString::fromUtf8(data);

    // Filter ANSI escape sequences
    static QRegularExpression ansiRegex(R"(\x1b\[[0-9;]*[a-zA-Z]|\x1b\][^\x07]*\x07|\r)");
    text.remove(ansiRegex);

    if (text.isEmpty()) return;

    // Remove shell prompt echoes (PS1 that the shell might output)
    static QRegularExpression promptEcho(R"(^(bash|sh|zsh)[-.]?\d?[#$>]\s*)");
    QStringList lines = text.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        if (i == 0 || i == lines.size() - 1) {
            lines[i].remove(promptEcho);
        }
    }
    text = lines.join('\n');

    if (text.trimmed().isEmpty()) return;

    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_output->setTextCursor(cursor);
    m_output->insertPlainText(text);
    scrollToBottom();
}

void TerminalPanel::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status)
    m_processRunning = false;
    m_statusLabel->setText("Exited (code: " + QString::number(exitCode) + ")");

    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_output->setTextCursor(cursor);
    m_output->insertPlainText("\n[Process exited with code " + QString::number(exitCode) + "]\n");
    m_promptPosition = 0;
    m_inputBuffer.clear();
}

void TerminalPanel::onProcessError(QProcess::ProcessError error) {
    QString msg;
    switch (error) {
        case QProcess::FailedToStart: msg = "Failed to start process"; break;
        case QProcess::Crashed: msg = "Process crashed"; break;
        case QProcess::Timedout: msg = "Process timed out"; break;
        default: msg = "Process error"; break;
    }
    m_statusLabel->setText(msg);
    m_processRunning = false;

    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_output->setTextCursor(cursor);
    m_output->insertPlainText("\n[" + msg + "]\n");
    m_promptPosition = 0;
}

void TerminalPanel::executeCommand(const QString& cmd) {
    if (!m_process || m_process->state() != QProcess::Running) {
        openTerminal();
        if (!m_process || m_process->state() != QProcess::Running) return;
    }

    // Echo the command to the output
    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_output->setTextCursor(cursor);
    m_output->insertPlainText(cmd + "\n");
    scrollToBottom();

    m_process->write((cmd + "\n").toUtf8());
    m_inputBuffer.clear();

    // Schedule prompt write after command output
    QTimer::singleShot(200, this, [this]() {
        if (m_process && m_process->state() == QProcess::Running) {
            writePrompt();
        }
    });
}

void TerminalPanel::keyPressEvent(QKeyEvent* event) {
    if (!m_process || m_process->state() != QProcess::Running) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            openTerminal();
            return;
        }
        QWidget::keyPressEvent(event);
        return;
    }

    QTextCursor cursor = m_output->textCursor();

    switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            QString cmd = m_inputBuffer.trimmed();
            cursor.movePosition(QTextCursor::End);
            m_output->setTextCursor(cursor);
            m_output->insertPlainText("\n");
            scrollToBottom();

            if (cmd.isEmpty()) {
                writePrompt();
            } else {
                m_process->write((cmd + "\n").toUtf8());
                m_inputBuffer.clear();

                // Schedule prompt write after command completes
                QTimer::singleShot(200, this, [this]() {
                    if (m_process && m_process->state() == QProcess::Running) {
                        writePrompt();
                    }
                });
            }
            return;
        }

        case Qt::Key_Backspace: {
            if (!m_inputBuffer.isEmpty()) {
                m_inputBuffer.chop(1);
                cursor.movePosition(QTextCursor::End);
                cursor.deletePreviousChar();
                m_output->setTextCursor(cursor);
            }
            return;
        }

        case Qt::Key_Delete: {
            return;
        }

        case Qt::Key_Left: {
            cursor.movePosition(QTextCursor::Left);
            m_output->setTextCursor(cursor);
            return;
        }

        case Qt::Key_Right: {
            cursor.movePosition(QTextCursor::Right);
            m_output->setTextCursor(cursor);
            return;
        }

        case Qt::Key_Up: {
            m_process->write("\x1b[A");
            return;
        }

        case Qt::Key_Down: {
            m_process->write("\x1b[B");
            return;
        }

        case Qt::Key_Home: {
            cursor.movePosition(QTextCursor::StartOfLine);
            m_output->setTextCursor(cursor);
            return;
        }

        case Qt::Key_End: {
            cursor.movePosition(QTextCursor::EndOfLine);
            m_output->setTextCursor(cursor);
            return;
        }

        case Qt::Key_Tab: {
            // Tab completion
            m_process->write("\t");
            return;
        }

        case Qt::Key_C: {
            if (event->modifiers() & Qt::ControlModifier) {
                m_process->write("\x03");
                m_output->insertPlainText("^C\n");
                m_inputBuffer.clear();
                QTimer::singleShot(100, this, [this]() {
                    if (m_process && m_process->state() == QProcess::Running)
                        writePrompt();
                });
                return;
            }
            break;
        }

        case Qt::Key_D: {
            if (event->modifiers() & Qt::ControlModifier) {
                m_process->write("\x04");
                return;
            }
            break;
        }

        case Qt::Key_L: {
            if (event->modifiers() & Qt::ControlModifier) {
                clearTerminal();
                return;
            }
            break;
        }

        case Qt::Key_V: {
            if (event->modifiers() & Qt::ControlModifier) {
                QString clipText = QApplication::clipboard()->text();
                if (!clipText.isEmpty()) {
                    cursor.movePosition(QTextCursor::End);
                    m_output->setTextCursor(cursor);
                    m_output->insertPlainText(clipText);
                    m_inputBuffer += clipText;
                    scrollToBottom();
                }
                return;
            }
            break;
        }

        default:
            break;
    }

    // Normal printable character input
    QString text = event->text();
    if (!text.isEmpty() && text.at(0).isPrint() && !(event->modifiers() & Qt::ControlModifier)) {
        cursor.movePosition(QTextCursor::End);
        m_output->setTextCursor(cursor);
        m_output->insertPlainText(text);
        m_inputBuffer += text;
        scrollToBottom();
        return;
    }

    QWidget::keyPressEvent(event);
}

bool TerminalPanel::focusNextPrevChild(bool next) {
    Q_UNUSED(next)
    return false;
}
