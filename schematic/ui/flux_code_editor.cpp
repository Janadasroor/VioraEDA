#include "flux_code_editor.h"
#include <QGraphicsScene>
#include "net_manager.h"
#include <QCompleter>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QKeyEvent>
#include <QPainter>
#include <QToolTip>
#include <QStringListModel>
#include <QDebug>
#include "jit_context_manager.h"

namespace Flux {

FluxHighlighter::FluxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
    HighlightingRule rule;

    keywordFormat.setForeground(Qt::darkBlue);
    keywordFormat.setFontWeight(QFont::Bold);
    QStringList keywordPatterns = {
        "\\bdef\\b", "\\bextern\\b", "\\breturn\\b", "\\bvar\\b",
        "\\blet\\b", "\\bfn\\b", "\\bif\\b", "\\belse\\b", "\\bfor\\b",
        "\\bin\\b", "\\bdo\\b", "\\bwhile\\b", "\\bimport\\b", "\\bcase\\b",
        "\\bswitch\\b", "\\bdefault\\b", "\\bbreak\\b", "\\bcontinue\\b",
        "\\bstruct\\b", "\\bclass\\b", "\\bnamespace\\b"
    };
    for (const QString& pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    typeFormat.setForeground(Qt::darkMagenta);
    QStringList typePatterns = {
        "\\bfloat\\b", "\\bdouble\\b", "\\bint\\b", "\\bvoid\\b",
        "\\bcomplex\\b", "\\bstring\\b", "\\bvector\\b", "\\bmatrix\\b"
    };
    for (const QString& pattern : typePatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = typeFormat;
        highlightingRules.append(rule);
    }

    singleLineCommentFormat.setForeground(Qt::darkGreen);
    rule.pattern = QRegularExpression("//[^\n]*");
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);
    
    rule.pattern = QRegularExpression("#[^\n]*");
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    quotationFormat.setForeground(Qt::darkRed);
    rule.pattern = QRegularExpression("\".*\"");
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    functionFormat.setFontItalic(true);
    functionFormat.setForeground(Qt::blue);
    rule.pattern = QRegularExpression("\\b[A-Za-z0-9_]+(?=\\()");
    rule.format = functionFormat;
    highlightingRules.append(rule);

    spiceSuffixFormat.setForeground(Qt::darkCyan);
    rule.pattern = QRegularExpression("\\b[0-9.]+[kMegunpfaGT]\\b");
    rule.format = spiceSuffixFormat;
    highlightingRules.append(rule);
}

void FluxHighlighter::highlightBlock(const QString& text) {
    for (const HighlightingRule& rule : highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

CodeEditor::CodeEditor(QGraphicsScene* scene, NetManager* netManager, QWidget* parent)
    : QPlainTextEdit(parent), m_scene(scene), m_netManager(netManager), m_completer(nullptr) {
    
    m_highlighter = new FluxHighlighter(document());
    
    setMouseTracking(true);
    
    QFont font;
    font.setFamily("Monospace");
    font.setFixedPitch(true);
    font.setPointSize(10);
    setFont(font);

    // Dark theme default
    setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; selection-background-color: #264f78;");
}

void CodeEditor::setScene(QGraphicsScene* scene, NetManager* netManager) {
    m_scene = scene;
    m_netManager = netManager;
}

void CodeEditor::setCompleter(QCompleter* completer) {
    if (m_completer)
        m_completer->disconnect(this);

    m_completer = completer;

    if (!m_completer)
        return;

    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    QObject::connect(m_completer, QOverload<const QString&>::of(&QCompleter::activated),
                     this, &CodeEditor::insertCompletion);
}

QCompleter* CodeEditor::completer() const {
    return m_completer;
}

void CodeEditor::insertCompletion(const QString& completion) {
    if (m_completer->widget() != this)
        return;
    QTextCursor tc = textCursor();
    int extra = completion.length() - m_completer->completionPrefix().length();
    tc.movePosition(QTextCursor::Left);
    tc.movePosition(QTextCursor::EndOfWord);
    tc.insertText(completion.right(extra));
    setTextCursor(tc);
}

QString CodeEditor::textUnderCursor() const {
    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}

void CodeEditor::updateCompletionKeywords(const QStringList& keywords) {
    if (!m_completer) return;
    auto* model = new QStringListModel(keywords, m_completer);
    m_completer->setModel(model);
}

void CodeEditor::setErrorLines(const QMap<int, QString>& errors) {
    m_errorLines = errors;
    viewport()->update();
}

void CodeEditor::setActiveDebugLine(int line) {
    m_activeDebugLine = line;
    viewport()->update();
}

void CodeEditor::onRunRequested() {
#ifdef HAVE_FLUXSCRIPT
    QString source = toPlainText();
    QMap<int, QString> errors;
    setErrorLines({});
    
    if (JITContextManager::instance().compileAndLoad("standalone_editor", source, errors)) {
        qDebug() << "FluxScript: Run successful.";
    } else {
        setErrorLines(errors);
        qDebug() << "FluxScript: Run failed with errors.";
    }
#endif
}

void CodeEditor::keyPressEvent(QKeyEvent* e) {
    if (m_completer && m_completer->popup()->isVisible()) {
        switch (e->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore();
            return;
        default:
            break;
        }
    }

    const bool isShortcut = (e->modifiers().testFlag(Qt::ControlModifier) && e->key() == Qt::Key_E);
    if (!m_completer || !isShortcut)
        QPlainTextEdit::keyPressEvent(e);

    const bool ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    if (!m_completer || (ctrlOrShift && e->text().isEmpty()))
        return;

    static QString eow("~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-=");
    bool hasModifier = (e->modifiers() != Qt::NoModifier) && !ctrlOrShift;
    QString completionPrefix = textUnderCursor();

    if (!isShortcut && (hasModifier || e->text().isEmpty() || completionPrefix.length() < 2
                      || eow.contains(e->text().right(1)))) {
        m_completer->popup()->hide();
        return;
    }

    if (completionPrefix != m_completer->completionPrefix()) {
        m_completer->setCompletionPrefix(completionPrefix);
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }
    QRect cr = cursorRect();
    cr.setWidth(m_completer->popup()->sizeHintForColumn(0)
                + m_completer->popup()->verticalScrollBar()->sizeHint().width());
    m_completer->complete(cr);
}

void CodeEditor::focusInEvent(QFocusEvent* e) {
    if (m_completer)
        m_completer->setWidget(this);
    QPlainTextEdit::focusInEvent(e);
}

bool CodeEditor::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(e);
        QTextCursor cursor = cursorForPosition(helpEvent->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        QString word = cursor.selectedText();
        
        static QMap<QString, QString> helpDb = {
            {"V", "<b>V(node)</b><br>Returns voltage at node."},
            {"I", "<b>I(branch)</b><br>Returns current through branch."},
            {"math", "FluxScript math library."}
        };

        if (helpDb.contains(word)) {
            QToolTip::showText(helpEvent->globalPos(), helpDb[word], this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QPlainTextEdit::event(e);
}

void CodeEditor::paintEvent(QPaintEvent* e) {
    QPlainTextEdit::paintEvent(e);
    
    QPainter painter(viewport());
    for (auto it = m_errorLines.begin(); it != m_errorLines.end(); ++it) {
        int line = it.key();
        QTextBlock block = document()->findBlockByLineNumber(line);
        if (block.isValid()) {
            QRectF r = blockBoundingRect(block).translated(contentOffset());
            painter.fillRect(r.toRect(), QColor(255, 0, 0, 40));
        }
    }
    
    if (m_activeDebugLine >= 0) {
        QTextBlock block = document()->findBlockByLineNumber(m_activeDebugLine);
        if (block.isValid()) {
            QRectF r = blockBoundingRect(block).translated(contentOffset());
            painter.fillRect(r.toRect(), QColor(255, 255, 0, 40));
        }
    }
}

void CodeEditor::showFindReplaceDialog() {}
void CodeEditor::findNext(const QString&, bool) {}
void CodeEditor::replaceText(const QString&, const QString&) {}

} // namespace Flux
