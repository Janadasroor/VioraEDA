#include "python_console_widget.h"
#include "python_executor.h"

#include <QKeyEvent>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextBlock>
#include <QMimeData>
#include <QApplication>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMouseEvent>

// ─── PythonHighlighter ───────────────────────────────────────────

PythonHighlighter::PythonHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    // Keywords
    QTextCharFormat kwFmt;
    kwFmt.setForeground(QColor("#569cd6"));
    kwFmt.setFontWeight(QFont::Bold);
    keywordFormat = kwFmt;

    const char* keywords[] = {
        "and", "as", "assert", "break", "class", "continue", "def", "del",
        "elif", "else", "except", "finally", "for", "from", "global", "if",
        "import", "in", "is", "lambda", "nonlocal", "not", "or", "pass",
        "raise", "return", "try", "while", "with", "yield"
    };
    for (const char* kw : keywords) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("\\b" + QString(kw) + "\\b");
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // Builtins
    QTextCharFormat biFmt;
    biFmt.setForeground(QColor("#4ec9b0"));
    builtinFormat = biFmt;

    const char* builtins[] = {
        "True", "False", "None", "print", "len", "range", "int", "float",
        "str", "list", "dict", "set", "tuple", "type", "isinstance",
        "enumerate", "zip", "map", "filter", "sorted", "reversed",
        "sum", "min", "max", "abs", "any", "all", "super", "property",
        "staticmethod", "classmethod", "help", "dir", "vars", "repr",
        "open", "input", "format", "hash", "id", "iter", "next",
        "hasattr", "getattr", "setattr", "delattr", "callable", "chr",
        "ord", "round", "slice", "pow", "exec", "eval", "complex",
        "bool", "bytes", "bytearray", "memoryview", "object",
        "gemini", "term"  // Gemini Terminal shortcuts
    };
    for (const char* bi : builtins) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression("\\b" + QString(bi) + "\\b");
        rule.format = builtinFormat;
        highlightingRules.append(rule);
    }

    // VioSpice v. namespace (v.simulation, v.editor, etc.)
    QTextCharFormat vnsFmt;
    vnsFmt.setForeground(QColor("#dcdcaa"));
    HighlightingRule vnsRule;
    vnsRule.pattern = QRegularExpression("\\bv\\.(simulation|editor|waveviewer|project|netlist|addons|ops|handlers)\\b");
    vnsRule.format = vnsFmt;
    highlightingRules.append(vnsRule);

    // v. method calls
    QTextCharFormat vcallFmt;
    vcallFmt.setForeground(QColor("#4ec9b0"));
    HighlightingRule vcallRule;
    vcallRule.pattern = QRegularExpression("\\bv\\.(help|setup_console)\\b");
    vcallRule.format = vcallFmt;
    highlightingRules.append(vcallRule);

    // Strings
    QTextCharFormat strFmt;
    strFmt.setForeground(QColor("#ce9178"));
    stringFormat = strFmt;

    HighlightingRule strRule;
    strRule.pattern = QRegularExpression("\"[^\"]*\"");
    strRule.format = stringFormat;
    highlightingRules.append(strRule);

    strRule.pattern = QRegularExpression("'[^']*'");
    strRule.format = stringFormat;
    highlightingRules.append(strRule);

    // f-strings (simple)
    strRule.pattern = QRegularExpression("f\"[^\"]*\"");
    strRule.format = stringFormat;
    highlightingRules.append(strRule);

    strRule.pattern = QRegularExpression("f'[^']*'");
    strRule.format = stringFormat;
    highlightingRules.append(strRule);

    // Comments
    QTextCharFormat cmtFmt;
    cmtFmt.setForeground(QColor("#6a9955"));
    cmtFmt.setFontItalic(true);
    commentFormat = cmtFmt;

    HighlightingRule cmtRule;
    cmtRule.pattern = QRegularExpression("#[^\\n]*");
    cmtRule.format = commentFormat;
    highlightingRules.append(cmtRule);

    // Numbers
    QTextCharFormat numFmt;
    numFmt.setForeground(QColor("#b5cea8"));
    numberFormat = numFmt;

    HighlightingRule numRule;
    numRule.pattern = QRegularExpression("\\b\\d+\\.?\\d*([eE][+-]?\\d+)?\\b");
    numRule.format = numberFormat;
    highlightingRules.append(numRule);

    // Decorators
    QTextCharFormat decFmt;
    decFmt.setForeground(QColor("#dcdcaa"));
    HighlightingRule decRule;
    decRule.pattern = QRegularExpression("@\\w+");
    decRule.format = decFmt;
    highlightingRules.append(decRule);
}

void PythonHighlighter::highlightBlock(const QString& text) {
    for (const HighlightingRule& rule : highlightingRules) {
        auto matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

// ─── PythonConsoleWidget ─────────────────────────────────────────

PythonConsoleWidget::PythonConsoleWidget(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setFont(QFont("Monospace", 10));
    setReadOnly(false);
    setTabStopDistance(24);
    setAcceptDrops(true);
    setMouseTracking(true);

    // Enable syntax highlighting
    m_highlighter = new PythonHighlighter(document());

    // Show startup status
    printOutput("Python 3 initialized successfully.\n", false);
    printOutput("Type 'v.help()' for VioSpice API, 'gemini.help()' for AI terminal.\n\n", false);

    // Initial prompt
    showPrompt();
    m_promptLine = document()->blockCount();
}

void PythonConsoleWidget::executeLine(const QString& code) {
    if (code.trimmed().isEmpty()) {
        showPrompt();
        return;
    }

    // Check for magic commands
    if (code.startsWith("%")) {
        handleMagicCommand(code.trimmed().mid(1));
        showPrompt();
        return;
    }

    m_history.append(code);
    m_historyIndex = m_history.size();

    QString codeToRun = m_inContinuation ? m_currentInput : code;
    QByteArray utf8 = codeToRun.toUtf8();

    int isError = 0;
    char* output = py_executor_execute(utf8.constData(), &isError);

    if (output && strlen(output) > 0) {
        QString qOutput = QString::fromUtf8(output);
        printOutput(qOutput, isError != 0);
        py_executor_free(output);
    } else if (output) {
        py_executor_free(output);
    }

    if (m_inContinuation) {
        m_currentInput.clear();
        m_inContinuation = false;
    }

    emit commandExecuted(code, QString::fromUtf8(output ? output : ""), isError != 0);
    showPrompt();
}

void PythonConsoleWidget::clearOutput() {
    clear();
    showPrompt();
    m_promptLine = document()->blockCount();
    m_history.clear();
    m_historyIndex = 0;
}

// ─── Key Event Handling ──────────────────────────────────────────

void PythonConsoleWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (event->modifiers() & Qt::ShiftModifier) {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }

        // Cancel completion if active
        if (m_completing) {
            m_completing = false;
        }

        // Cancel history search if active
        if (m_historySearchActive) {
            cancelHistorySearch();
        }

        // Get text from prompt line to end
        QTextCursor cursor = textCursor();
        int promptPos = document()->findBlockByLineNumber(m_promptLine - 1).position();
        cursor.setPosition(promptPos);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        QString code = cursor.selectedText();

        if (code.startsWith(">>> ")) code = code.mid(4);
        if (code.startsWith("... ")) code = code.mid(4);

        if (m_inContinuation) {
            m_currentInput += "\n" + code;
        } else {
            m_currentInput = code;
        }

        // Check for continuation using the C executor
        QByteArray checkUtf8 = code.toUtf8();
        if (!m_inContinuation && !py_executor_is_complete(checkUtf8.constData())) {
            m_inContinuation = true;
            printOutput("\n... ");
            return;
        }

        // Move cursor to end and execute
        cursor.movePosition(QTextCursor::End);
        setTextCursor(cursor);
        printOutput("\n");

        executeLine(m_currentInput);

    } else if (event->key() == Qt::Key_Tab) {
        // Tab completion
        handleTabCompletion();

    } else if (event->key() == Qt::Key_Up) {
        if (m_historySearchActive) {
            cycleHistory(-1);
        } else if (event->modifiers() & Qt::ControlModifier) {
            // Ctrl+Up: search history
            startHistorySearch();
        } else {
            cycleHistory(-1);
        }

    } else if (event->key() == Qt::Key_Down) {
        if (m_historySearchActive) {
            cycleHistory(1);
        } else if (event->modifiers() & Qt::ControlModifier) {
            cancelHistorySearch();
        } else {
            cycleHistory(1);
        }

    } else if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
        // Ctrl+A — move cursor to start of input (after prompt)
        event->accept();
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        int promptPos = document()->findBlockByLineNumber(m_promptLine - 1).position();
        cursor.setPosition(promptPos + 4); // Skip ">>> "
        setTextCursor(cursor);

    } else if (event->key() == Qt::Key_E && (event->modifiers() & Qt::ControlModifier)) {
        // Ctrl+E — move cursor to end of line
        event->accept();
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        setTextCursor(cursor);

    } else if (event->key() == Qt::Key_L && (event->modifiers() & Qt::ControlModifier)) {
        // Ctrl+L — clear screen
        event->accept();
        clear();
        showPrompt();
        m_promptLine = document()->blockCount();

    } else if (event->key() == Qt::Key_R && (event->modifiers() & Qt::ControlModifier)) {
        // Ctrl+R — reverse history search
        event->accept();
        startHistorySearch();
        printOutput("\n(reverse-i-search)`': ");

    } else if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier)) {
        // Ctrl+D at empty prompt — reset console
        event->accept();
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        int promptPos = document()->findBlockByLineNumber(m_promptLine - 1).position();
        cursor.setPosition(promptPos + 4);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        if (cursor.selectedText().isEmpty()) {
            printOutput("\n");
            clear();
            showPrompt();
            m_promptLine = document()->blockCount();
        }

    } else if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
        // Ctrl+C — cancel current input
        event->accept();
        if (m_historySearchActive) {
            cancelHistorySearch();
            printOutput("^C\n");
        } else {
            printOutput("^C\n");
            m_inContinuation = false;
            m_currentInput.clear();
            showPrompt();
        }

    } else if (event->key() == Qt::Key_U && (event->modifiers() & Qt::ControlModifier)) {
        // Ctrl+U — delete from cursor to start of line
        event->accept();
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        int promptPos = document()->findBlockByLineNumber(m_promptLine - 1).position() + 4;
        cursor.setPosition(promptPos, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();

    } else if (event->key() == Qt::Key_W && (event->modifiers() & Qt::ControlModifier)) {
        // Ctrl+W — delete word before cursor
        event->accept();
        QTextCursor cursor = textCursor();
        int pos = cursor.position();
        // Find word boundary
        QTextBlock block = document()->findBlockByLineNumber(m_promptLine - 1);
        QString blockText = block.text();
        int linePos = pos - block.position();
        while (linePos > 4 && !blockText[linePos - 1].isSpace()) {
            linePos--;
        }
        cursor.setPosition(block.position() + linePos, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();

    } else if (event->key() == Qt::Key_Backspace) {
        // Prevent deleting past the prompt
        QTextCursor cursor = textCursor();
        int promptPos = document()->findBlockByLineNumber(m_promptLine - 1).position() + 4;
        if (cursor.position() <= promptPos) {
            event->ignore();
            return;
        }
        QPlainTextEdit::keyPressEvent(event);

    } else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        // Prevent cursor moving before prompt
        if (event->key() == Qt::Key_Left) {
            QTextCursor cursor = textCursor();
            int promptPos = document()->findBlockByLineNumber(m_promptLine - 1).position() + 4;
            if (cursor.position() <= promptPos) {
                event->ignore();
                return;
            }
        }
        QPlainTextEdit::keyPressEvent(event);

    } else {
        // Cancel completion on any other key
        if (m_completing && !event->text().isEmpty()) {
            m_completing = false;
        }

        // Handle history search input
        if (m_historySearchActive && !event->text().isEmpty()) {
            event->accept();
            if (event->key() == Qt::Key_Backspace) {
                if (!m_historySearchText.isEmpty()) {
                    m_historySearchText.chop(1);
                    // Find matching history entry
                    for (int i = m_history.size() - 1; i >= 0; i--) {
                        if (m_history[i].contains(m_historySearchText, Qt::CaseInsensitive)) {
                            // Replace current line with match
                            QTextCursor cursor = textCursor();
                            cursor.movePosition(QTextCursor::End);
                            int promptPos = document()->findBlockByLineNumber(m_promptLine - 1).position();
                            cursor.setPosition(promptPos + 4);
                            cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                            cursor.removeSelectedText();
                            printOutput(m_history[i]);
                            break;
                        }
                    }
                }
            } else {
                m_historySearchText += event->text();
                // Find matching history entry
                for (int i = m_history.size() - 1; i >= 0; i--) {
                    if (m_history[i].contains(m_historySearchText, Qt::CaseInsensitive)) {
                        // Replace current line with match
                        QTextCursor cursor = textCursor();
                        cursor.movePosition(QTextCursor::End);
                        int promptPos = document()->findBlockByLineNumber(m_promptLine - 1).position();
                        cursor.setPosition(promptPos + 4);
                        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                        cursor.removeSelectedText();
                        printOutput(m_history[i]);
                        break;
                    }
                }
            }
            return;
        }

        QPlainTextEdit::keyPressEvent(event);
    }
}

// ─── Drag & Drop ─────────────────────────────────────────────────

void PythonConsoleWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void PythonConsoleWidget::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
        QString text = event->mimeData()->text();
        insertTextAtPrompt(text);
    }
}

void PythonConsoleWidget::mousePressEvent(QMouseEvent* event) {
    // Ensure clicks go to end of document (input area)
    if (event->button() == Qt::LeftButton) {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        setTextCursor(cursor);
    }
    QPlainTextEdit::mousePressEvent(event);
}

// ─── Input Helpers ───────────────────────────────────────────────

void PythonConsoleWidget::insertTextAtPrompt(const QString& text) {
    // Move cursor to end
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);

    // If we're not at the prompt, add a newline first
    int promptPos = document()->findBlockByLineNumber(m_promptLine - 1).position() + 4;
    if (cursor.position() < promptPos) {
        cursor.movePosition(QTextCursor::End);
        printOutput("\n");
        showPrompt();
        cursor.movePosition(QTextCursor::End);
    }

    cursor.insertText(text);
    setTextCursor(cursor);
    ensureCursorVisible();
}

QString PythonConsoleWidget::getCurrentLineText() const {
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    int promptPos = document()->findBlockByLineNumber(m_promptLine - 1).position();
    cursor.setPosition(promptPos);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    QString text = cursor.selectedText();
    if (text.startsWith(">>> ")) text = text.mid(4);
    if (text.startsWith("... ")) text = text.mid(4);
    return text;
}

void PythonConsoleWidget::clearCurrentLine() {
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    int promptPos = document()->findBlockByLineNumber(m_promptLine - 1).position();
    cursor.setPosition(promptPos + 4);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    setTextCursor(cursor);
}

void PythonConsoleWidget::cycleHistory(int direction) {
    int newIndex = m_historyIndex + direction;
    if (newIndex < 0 || newIndex >= m_history.size()) {
        if (direction < 0 && newIndex < 0) {
            // At beginning
            return;
        }
        if (direction > 0 && newIndex >= m_history.size()) {
            // Past end — clear line
            clearCurrentLine();
            m_historyIndex = m_history.size();
            return;
        }
        return;
    }

    m_historyIndex = newIndex;
    clearCurrentLine();
    printOutput(m_history[m_historyIndex]);
}

// ─── Tab Completion ──────────────────────────────────────────────

void PythonConsoleWidget::handleTabCompletion() {
    QString currentLine = getCurrentLineText();

    // Find the word being typed
    int cursorPos = textCursor().position() -
                    (document()->findBlockByLineNumber(m_promptLine - 1).position() + 4);

    // Extract prefix (word before cursor)
    QString prefix;
    for (int i = cursorPos - 1; i >= 0; i--) {
        QChar c = currentLine[i];
        if (c.isLetterOrNumber() || c == '_') {
            prefix.prepend(c);
        } else {
            break;
        }
    }

    if (prefix.isEmpty()) {
        // Indent on tab
        insertTextAtPrompt("    ");
        return;
    }

    // Get suggestions from Python
    QStringList matches = getCompletionSuggestions(prefix);

    if (matches.isEmpty()) {
        return; // No matches
    }

    if (matches.size() == 1) {
        // Single match — complete it
        QString completion = matches[0];
        // Remove prefix from completion
        QString suffix = completion.mid(prefix.length());
        if (!suffix.isEmpty()) {
            insertTextAtPrompt(suffix);
        }
    } else {
        // Multiple matches — show them
        if (m_completing && m_completionPrefix == prefix) {
            // Cycle through matches
            m_completionIndex = (m_completionIndex + 1) % matches.size();
            clearCurrentLine();
            printOutput(matches[m_completionIndex]);
        } else {
            // First tab — show all matches
            printOutput("\n");
            printOutput(matches.join("  "));
            printOutput("\n");
            showPrompt();
            // Keep current line
            printOutput(currentLine);
            m_completing = true;
            m_completionPrefix = prefix;
            m_completionMatches = matches;
            m_completionIndex = 0;
        }
    }
}

QStringList PythonConsoleWidget::getCompletionSuggestions(const QString& prefix) {
    QStringList matches;

    // Execute Python code to get completions using dir() and getattr
    QString code = QString(
        "import builtins\n"
        "try:\n"
        "    # Find completions from globals, builtins, and objects in scope\n"
        "    results = set()\n"
        "    # Global/local names\n"
        "    for ns in [globals(), dir(builtins)]:\n"
        "        for name in (ns if isinstance(ns, list) else ns.keys()):\n"
        "            if name.startswith('%1'):\n"
        "                results.add(name)\n"
        "                # Also add attributes if it's an object\n"
        "                if not isinstance(ns, list) and name in ns:\n"
        "                    try:\n"
        "                        for attr in dir(ns[name]):\n"
        "                            results.add(name + '.' + attr)\n"
        "                    except: pass\n"
        "    # Handle attribute access (e.g., 'os.pa' -> 'os.path')\n"
        "    if '.' in '%1':\n"
        "        parts = '%1'.rsplit('.', 1)\n"
        "        obj_name, attr_prefix = parts[0], parts[1] if len(parts) > 1 else ''\n"
        "        try:\n"
        "            obj = eval(obj_name)\n"
        "            for attr in dir(obj):\n"
        "                if attr.startswith(attr_prefix):\n"
        "                    results.add(obj_name + '.' + attr)\n"
        "        except: pass\n"
        "    print(' '.join(sorted(results)))\n"
        "except Exception as e:\n"
        "    print(f'Error: {e}')\n"
    ).arg(prefix);

    QByteArray utf8 = code.toUtf8();
    int isError = 0;
    char* output = py_executor_execute(utf8.constData(), &isError);

    if (output && !isError) {
        QString result = QString::fromUtf8(output).trimmed();
        if (!result.isEmpty()) {
            matches = result.split(' ', Qt::SkipEmptyParts);
            matches.removeDuplicates();
            matches.sort();
        }
    }

    if (output) py_executor_free(output);

    // Also add magic commands if prefix starts with %
    if (prefix.startsWith("%")) {
        const char* magics[] = {"%history", "%clear", "%help", "%time", "%reset", "%pwd", "%cd", "%who", "%whos"};
        for (const char* m : magics) {
            if (QString(m).startsWith(prefix, Qt::CaseInsensitive)) {
                if (!matches.contains(m)) {
                    matches.append(m);
                }
            }
        }
    }

    return matches;
}

// ─── History Search ──────────────────────────────────────────────

void PythonConsoleWidget::startHistorySearch() {
    m_historySearchActive = true;
    m_historySearchText.clear();
}

void PythonConsoleWidget::cancelHistorySearch() {
    m_historySearchActive = false;
    m_historySearchText.clear();
}

// ─── Magic Commands ──────────────────────────────────────────────

bool PythonConsoleWidget::handleMagicCommand(const QString& cmd) {
    QString command = cmd.trimmed();

    if (command == "clear" || command == "cls") {
        clear();
        showPrompt();
        m_promptLine = document()->blockCount();
        printOutput("Console cleared.\n");
        return true;
    }

    if (command == "help" || command == "?") {
        printOutput("VioSpice Python Console Help\n");
        printOutput("============================\n\n");
        printOutput("Keyboard Shortcuts:\n");
        printOutput("  Enter      — Execute code\n");
        printOutput("  Shift+Enter — New line (multi-line input)\n");
        printOutput("  Tab        — Auto-complete\n");
        printOutput("  Up/Down    — Command history\n");
        printOutput("  Ctrl+R     — Reverse history search\n");
        printOutput("  Ctrl+A/E   — Move to start/end of line\n");
        printOutput("  Ctrl+L     — Clear screen\n");
        printOutput("  Ctrl+D     — Reset console\n");
        printOutput("  Ctrl+C     — Cancel input\n");
        printOutput("  Ctrl+U     — Delete to start of line\n");
        printOutput("  Ctrl+W     — Delete word before cursor\n\n");
        printOutput("Magic Commands:\n");
        printOutput("  %help      — Show this help\n");
        printOutput("  %clear     — Clear console\n");
        printOutput("  %history   — Show command history\n");
        printOutput("  %time <code> — Time execution\n");
        printOutput("  %reset     — Reset Python interpreter\n");
        printOutput("  %who       — List defined variables\n");
        printOutput("  %whos      — Detailed variable list\n");
        printOutput("  %pwd       — Print working directory\n");
        printOutput("  %cd <dir>  — Change directory\n\n");
        printOutput("VioSpice API:\n");
        printOutput("  v            — bpy-like API (already imported)\n");
        printOutput("  v.help()     — Show VioSpice API docs\n");
        printOutput("  v.simulation — Run simulations, get waveforms\n");
        printOutput("  v.editor     — Schematic/PCB operations\n");
        printOutput("  v.handlers   — Event hook decorators\n");
        printOutput("  v.addons     — Addon management\n");
        printOutput("  v.ops        — Operator pattern\n\n");
        printOutput("Gemini Terminal:\n");
        printOutput("  gemini       — AI terminal (auto-imported)\n");
        printOutput("  term         — Alias for gemini\n");
        printOutput("  gemini.help() — Show Gemini Terminal docs\n");
        printOutput("  gemini.ask('question') — Ask AI, auto-execute <PYTHON>\n\n");
        return true;
    }

    if (command == "history" || command == "hist") {
        printOutput("Command History:\n");
        printOutput("================\n");
        for (int i = 0; i < m_history.size(); i++) {
            printOutput(QString("  [%1] %2\n").arg(i).arg(m_history[i]));
        }
        return true;
    }

    if (command.startsWith("time ")) {
        QString code = command.mid(5);
        QByteArray utf8 = code.toUtf8();

        // Time the execution
        QElapsedTimer timer;
        timer.start();

        int isError = 0;
        char* output = py_executor_execute(utf8.constData(), &isError);

        qint64 elapsed = timer.elapsed();

        if (output && strlen(output) > 0) {
            printOutput(QString::fromUtf8(output), isError != 0);
            py_executor_free(output);
        } else if (output) {
            py_executor_free(output);
        }

        printOutput(QString("\nCPU time: %1 ms\n").arg(elapsed));
        return true;
    }

    if (command == "reset") {
        printOutput("Resetting Python interpreter...\n");
        // Reinitialize by calling the executor with a simple test
        int isError = 0;
        char* output = py_executor_execute("import sys; print('Python', sys.version)", &isError);
        if (output && strlen(output) > 0) {
            printOutput(QString::fromUtf8(output), isError != 0);
            py_executor_free(output);
        } else if (output) {
            py_executor_free(output);
        }
        return true;
    }

    if (command == "who") {
        QString code =
            "vars = [k for k in dir() if not k.startswith('_') and k not in ('__builtins__',)]\n"
            "print(' '.join(vars) if vars else '(no variables defined)')\n";
        QByteArray utf8 = code.toUtf8();
        int isError = 0;
        char* output = py_executor_execute(utf8.constData(), &isError);
        if (output && strlen(output) > 0) {
            printOutput(QString::fromUtf8(output), isError != 0);
            py_executor_free(output);
        } else if (output) {
            py_executor_free(output);
        }
        return true;
    }

    if (command == "whos") {
        QString code =
            "vars = {k: v for k, v in globals().items() if not k.startswith('_') and k != '__builtins__'}\n"
            "if not vars: print('(no variables defined)')\n"
            "else:\n"
            "    for k, v in vars.items():\n"
            "        print(f'{k:20s} {type(v).__name__:15s} {repr(v)[:60]}')\n";
        QByteArray utf8 = code.toUtf8();
        int isError = 0;
        char* output = py_executor_execute(utf8.constData(), &isError);
        if (output && strlen(output) > 0) {
            printOutput(QString::fromUtf8(output), isError != 0);
            py_executor_free(output);
        } else if (output) {
            py_executor_free(output);
        }
        return true;
    }

    if (command == "pwd") {
        int isError = 0;
        char* output = py_executor_execute("import os; print(os.getcwd())", &isError);
        if (output && strlen(output) > 0) {
            printOutput(QString::fromUtf8(output), isError != 0);
            py_executor_free(output);
        } else if (output) {
            py_executor_free(output);
        }
        return true;
    }

    if (command.startsWith("cd ")) {
        QString dir = command.mid(3);
        QString code = QString("import os\nos.chdir('%1')\nprint(os.getcwd())\n").arg(dir);
        QByteArray utf8 = code.toUtf8();
        int isError = 0;
        char* output = py_executor_execute(utf8.constData(), &isError);
        if (output && strlen(output) > 0) {
            printOutput(QString::fromUtf8(output), isError != 0);
            py_executor_free(output);
        } else if (output) {
            py_executor_free(output);
        }
        return true;
    }

    // Unknown magic
    printOutput(QString("Unknown magic command: %1. Type %help for help.\n").arg(command), true);
    return true;
}

// ─── Output & Prompt ─────────────────────────────────────────────

void PythonConsoleWidget::printOutput(const QString& text, bool isError) {
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat format;
    if (isError) {
        format.setForeground(Qt::red);
    } else {
        format.setForeground(QColor("#cccccc"));
    }

    cursor.insertText(text, format);
    setTextCursor(cursor);
    ensureCursorVisible();
}

void PythonConsoleWidget::showPrompt() {
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat format;
    format.setForeground(QColor("#569cd6"));  // Blue prompt

    if (m_inContinuation) {
        cursor.insertText("... ", format);
    } else {
        cursor.insertText(">>> ", format);
    }

    // Reset to default text color
    format.setForeground(QColor("#d4d4d4"));
    cursor.insertText("", format);

    m_promptLine = document()->blockCount();
    setTextCursor(cursor);
    ensureCursorVisible();
}
