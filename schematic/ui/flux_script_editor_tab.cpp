#include "flux_script_editor_tab.h"
#include "flux_completer.h"
#include "flux_workspace_bridge.h"
#include "../../schematic/editor/schematic_api.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include "jit_context_manager.h"

namespace Flux {

ScriptEditorTab::ScriptEditorTab(QGraphicsScene* scene, NetManager* netManager, SchematicAPI* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    setupUI(scene, netManager);
}

void ScriptEditorTab::setupUI(QGraphicsScene* scene, NetManager* netManager) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Toolbar
    QWidget* toolbar = new QWidget();
    toolbar->setStyleSheet("background: #2d2d30; border-bottom: 1px solid #3e3e42;");
    QHBoxLayout* toolLayout = new QHBoxLayout(toolbar);
    toolLayout->setContentsMargins(5, 5, 5, 5);

    QPushButton* runBtn = new QPushButton("Run Script");
    runBtn->setStyleSheet(
        "QPushButton { background: #10b981; color: white; border: none; padding: 6px 16px; border-radius: 4px; font-weight: bold; } "
        "QPushButton:hover { background: #059669; }"
    );
    connect(runBtn, &QPushButton::clicked, this, &ScriptEditorTab::onRunRequested);
    toolLayout->addWidget(runBtn);

    QPushButton* saveBtn = new QPushButton("Save");
    saveBtn->setStyleSheet("background: #3e3e42; color: #ccc; border: 1px solid #555; padding: 5px 12px; border-radius: 4px;");
    connect(saveBtn, &QPushButton::clicked, [this]() { saveFile(); });
    toolLayout->addWidget(saveBtn);

    toolLayout->addStretch();

    QPushButton* clearBtn = new QPushButton("Clear Console");
    clearBtn->setStyleSheet("background: transparent; color: #888; border: none; padding: 4px;");
    connect(clearBtn, &QPushButton::clicked, this, &ScriptEditorTab::onClearConsole);
    toolLayout->addWidget(clearBtn);

    mainLayout->addWidget(toolbar);

    // Editor
    m_editor = new CodeEditor(scene, netManager, this);
    FluxCompleter* completer = new FluxCompleter(this);
    completer->updateCompletions();
    m_editor->setCompleter(completer);
    
    connect(m_editor, &CodeEditor::textChanged, this, &ScriptEditorTab::onContentChanged);
    
    mainLayout->addWidget(m_editor, 2);

    // Console
    m_console = new QTextEdit();
    m_console->setReadOnly(true);
    m_console->setStyleSheet(
        "QTextEdit { background: #1a1a1a; color: #e0e0e0; border-top: 2px solid #333; font-family: 'Consolas', monospace; font-size: 10pt; }"
    );
    m_console->setPlaceholderText("Execution output will appear here...");
    m_console->setMinimumHeight(150);
    mainLayout->addWidget(m_console, 1);

    // Connect JIT signals
    connect(&JITContextManager::instance(), &JITContextManager::scriptOutput, [this](const QString& msg) {
        qDebug() << "[ScriptEditorTab] Received scriptOutput:" << msg;
        m_console->insertPlainText(msg + "\n");
        m_console->ensureCursorVisible();
    });
    
    connect(&JITContextManager::instance(), &JITContextManager::compilationFinished, [this](bool success, QString message) {
        qDebug() << "[ScriptEditorTab] Received compilationFinished:" << success << message;
        QString color = success ? "#4ade80" : "#f87171";
        m_console->append(QString("<span style='color:%1; font-weight:bold;'>[JIT] %2</span>").arg(color, message));
    });
}

bool ScriptEditorTab::openFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    m_editor->setPlainText(in.readAll());
    m_filePath = filePath;
    m_isModified = false;
    emit modificationChanged(false);
    return true;
}

bool ScriptEditorTab::saveFile(const QString& filePath) {
    QString target = filePath.isEmpty() ? m_filePath : filePath;
    if (target.isEmpty()) {
        target = QFileDialog::getSaveFileName(this, "Save FluxScript", "", "FluxScript Files (*.flux)");
        if (target.isEmpty()) return false;
    }

    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Save Error", "Could not open file for writing.");
        return false;
    }

    QTextStream out(&file);
    out << m_editor->toPlainText();
    m_filePath = target;
    m_isModified = false;
    emit modificationChanged(false);
    return true;
}

void ScriptEditorTab::onRunRequested() {
    m_console->append("<i style='color:#71717a;'>Running FluxScript...</i>");
    
    // Explicitly set the target for this execution run
    if (m_api) {
        Flux::Core::set_active_schematic_api(m_api);
        m_console->append(QString("<i style='color:#3b82f6;'>Targeting: %1 (%2)</i>")
            .arg(m_api->projectName(), QFileInfo(m_api->filePath()).fileName()));
    }
    
    m_editor->onRunRequested();
}

void ScriptEditorTab::onContentChanged() {
    if (!m_isModified) {
        m_isModified = true;
        emit modificationChanged(true);
    }
}

void ScriptEditorTab::onClearConsole() {
    m_console->clear();
}

} // namespace Flux
