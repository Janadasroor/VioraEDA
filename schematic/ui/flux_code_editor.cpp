#include "flux_code_editor.h"
#include <QGraphicsScene>
#include "flux/core/net_manager.h"
#include <QCompleter>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QKeyEvent>
#include <QPainter>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringListModel>
#include <QToolTip>
#include "../../core/diagnostics/debugger.h"

namespace Flux {

CodeEditor::CodeEditor(QGraphicsScene* scene, NetManager* netManager, QWidget* parent)
    : FluxEditor(parent), m_scene(scene), m_netManager(netManager) {
    
    setMouseTracking(true);
    
    // Initialize default completer
    QStringList keywords = {
        "def", "class", "if", "else", "elif", "for", "while", "return", "import", "from",
        "True", "False", "None", "self", "SmartSignal", "update", "init", "inputs",
        "math.sin", "math.cos", "math.tan", "math.pi", "math.exp", "math.log", "math.sqrt",
        "inputs.get", "abs", "round", "min", "max", "len", "print"
    };
    QCompleter* completer = new QCompleter(keywords, this);
    completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setWrapAround(false);
    setCompleter(completer);
}

void CodeEditor::setScene(QGraphicsScene* scene, NetManager* netManager) {
    m_scene = scene;
    m_netManager = netManager;
}

void CodeEditor::updateCompletionKeywords(const QStringList& additionalKeywords) {
    if (!m_completer) return;
    
    QStringList baseKeywords = {
        "def", "class", "if", "else", "elif", "for", "while", "return", "import", "from",
        "True", "False", "None", "self", "SmartSignal", "update", "init", "inputs",
        "math.sin", "math.cos", "math.tan", "math.pi", "math.exp", "math.log", "math.sqrt",
        "inputs.get", "abs", "round", "min", "max", "len", "print"
    };
    
    QStringList allKeywords = baseKeywords + additionalKeywords;
    allKeywords.removeDuplicates();
    allKeywords.sort();

    auto* model = new QStringListModel(allKeywords, m_completer);
    m_completer->setModel(model);
}

void CodeEditor::setErrorLines(const QMap<int, QString>& errors) {
    m_errorLines = errors;
    highlightCurrentLine();
}

void CodeEditor::setActiveDebugLine(int line) {
    m_activeDebugLine = line;
    highlightCurrentLine();
}

void CodeEditor::onRunRequested() {
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

    FluxEditor::keyPressEvent(e);
}

void CodeEditor::focusInEvent(QFocusEvent* e) {
    if (m_completer)
        m_completer->setWidget(this);
    FluxEditor::focusInEvent(e);
}

bool CodeEditor::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(e);
        QTextCursor cursor = cursorForPosition(helpEvent->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        QString word = cursor.selectedText();
        
        static QMap<QString, QString> helpDb = {
            {"update", "<b>update(self, t, inputs)</b><br>The main simulation loop. Called at every time-step.<br><i>t</i>: current time in seconds.<br><i>inputs</i>: dictionary of input pin voltages."},
            {"init", "<b>init(self)</b><br>Called once when the simulation starts.<br>Use this to initialize parameters or internal state."},
            {"self", "Refers to the current <b>SmartSignal</b> instance."},
            {"params", "<b>self.params</b><br>A dictionary used to define UI-controllable parameters."},
            {"inputs", "A dictionary containing the current voltages of all input pins."},
            {"math", "Python's standard math library."},
            {"np", "NumPy library for high-performance numerical operations."},
            {"sin", "<b>math.sin(x)</b><br>Returns the sine of x (measured in radians)."},
            {"pi", "The mathematical constant π (3.14159...)."}
        };
        
        if (helpDb.contains(word)) {
            QToolTip::showText(helpEvent->globalPos(), helpDb[word], this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return FluxEditor::event(e);
}

void CodeEditor::showFindReplaceDialog() {}
void CodeEditor::findNext(const QString& /* text */, bool /* forward */) {}
void CodeEditor::replaceText(const QString& /* find */, const QString& /* replace */) {}
void CodeEditor::onContentChanged() {}
void CodeEditor::insertCompletion(const QString& /* completion */) {}

} // namespace Flux
