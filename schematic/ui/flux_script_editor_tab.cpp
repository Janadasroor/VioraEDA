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
#include <QStackedWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QDir>
#include <QCoreApplication>
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

    QPushButton* templateBtn = new QPushButton("Templates");
    templateBtn->setStyleSheet("background: #3e3e42; color: #ccc; border: 1px solid #555; padding: 5px 12px; border-radius: 4px;");
    connect(templateBtn, &QPushButton::clicked, this, &ScriptEditorTab::showTemplates);
    toolLayout->addWidget(templateBtn);

    toolLayout->addStretch();

    QPushButton* clearBtn = new QPushButton("Clear Console");
    clearBtn->setStyleSheet("background: transparent; color: #888; border: none; padding: 4px;");
    connect(clearBtn, &QPushButton::clicked, this, &ScriptEditorTab::onClearConsole);
    toolLayout->addWidget(clearBtn);

    mainLayout->addWidget(toolbar);

    m_stack = new QStackedWidget(this);
    
    // Editor Container
    m_editorContainer = new QWidget();
    QVBoxLayout* editorLayout = new QVBoxLayout(m_editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    m_editor = new CodeEditor(scene, netManager, this);
    FluxCompleter* completer = new FluxCompleter(this);
    completer->updateCompletions();
    m_editor->setCompleter(completer);
    
    connect(m_editor, &CodeEditor::textChanged, this, &ScriptEditorTab::onContentChanged);
    connect(m_editor, &CodeEditor::textChanged, this, &ScriptEditorTab::updateWelcomeVisibility);
    
    m_console = new QTextEdit();
    m_console->setReadOnly(true);
    m_console->setStyleSheet(
        "QTextEdit { background: #1a1a1a; color: #e0e0e0; border-top: 2px solid #333; font-family: 'Consolas', monospace; font-size: 10pt; }"
    );
    m_console->setPlaceholderText("Execution output will appear here...");
    m_console->setMinimumHeight(150);

    editorLayout->addWidget(m_editor, 2);
    editorLayout->addWidget(m_console, 1);
    
    setupWelcomeView();

    m_stack->addWidget(m_editorContainer);
    m_stack->addWidget(m_welcomeView);
    
    mainLayout->addWidget(m_stack);

    // Connect JIT signals
    connect(&JITContextManager::instance(), &JITContextManager::scriptOutput, [this](const QString& msg) {
        m_console->insertPlainText(msg + "\n");
        m_console->ensureCursorVisible();
    });
    
    connect(&JITContextManager::instance(), &JITContextManager::compilationFinished, [this](bool success, QString message) {
        QString color = success ? "#4ade80" : "#f87171";
        m_console->append(QString("<span style='color:%1; font-weight:bold;'>[JIT] %2</span>").arg(color, message));
    });
}

void ScriptEditorTab::setupWelcomeView() {
    m_welcomeView = new QWidget();
    m_welcomeView->setStyleSheet("background: #121212;");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(m_welcomeView);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header Area
    QWidget* header = new QWidget();
    header->setFixedHeight(120);
    header->setStyleSheet("background: #1e1e1e; border-bottom: 1px solid #2d2d2d;");
    QVBoxLayout* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(40, 20, 40, 20);

    QLabel* title = new QLabel("FluxScript Explorer");
    title->setStyleSheet("font-size: 20pt; font-weight: bold; color: #f8fafc;");
    headerLayout->addWidget(title);

    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Search code snippets, automation scripts, and simulation templates...");
    m_searchEdit->setStyleSheet(
        "QLineEdit { background: #2a2d2e; color: #e2e8f0; border: 1px solid #3f3f46; border-radius: 6px; padding: 10px 15px; font-size: 10.5pt; }"
        "QLineEdit:focus { border: 1px solid #3b82f6; background: #1e293b; }"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ScriptEditorTab::onSearchTemplates);
    headerLayout->addWidget(m_searchEdit);
    mainLayout->addWidget(header);

    // Content Area (Sidebar + List)
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(20);

    // Sidebar: Categories
    m_categoryList = new QListWidget();
    m_categoryList->setFixedWidth(180);
    m_categoryList->setStyleSheet(
        "QListWidget { background: transparent; border: none; font-size: 10pt; color: #94a3b8; }"
        "QListWidget::item { padding: 10px 15px; border-radius: 6px; margin-bottom: 2px; }"
        "QListWidget::item:hover { background: #1e1e1e; color: #f8fafc; }"
        "QListWidget::item:selected { background: #2563eb; color: white; font-weight: bold; }"
    );
    
    QStringList categories = {"ALL", "SIMULATION", "AUTOMATION", "EXAMPLES"};
    for (const auto& cat : categories) {
        auto* item = new QListWidgetItem(cat, m_categoryList);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    m_categoryList->setCurrentRow(0);
    connect(m_categoryList, &QListWidget::currentItemChanged, this, &ScriptEditorTab::onCategorySelected);
    contentLayout->addWidget(m_categoryList);

    // Right Side: Template List
    m_templateList = new QListWidget();
    m_templateList->setAlternatingRowColors(false);
    m_templateList->setStyleSheet(
        "QListWidget { background: #1e1e1e; color: #cbd5e1; border: 1px solid #2d2d2d; border-radius: 8px; padding: 5px; font-size: 10pt; }"
        "QListWidget::item { padding: 15px; border-bottom: 1px solid #2d2d2d; }"
        "QListWidget::item:hover { background: #262626; color: #f8fafc; }"
        "QListWidget::item:selected { background: #1e293b; color: #3b82f6; border-left: 3px solid #3b82f6; }"
    );
    connect(m_templateList, &QListWidget::itemDoubleClicked, this, &ScriptEditorTab::onTemplateSelected);
    contentLayout->addWidget(m_templateList, 1);

    mainLayout->addLayout(contentLayout);

    refreshTemplatesList();
}

void ScriptEditorTab::refreshTemplatesList() {
    m_templateList->clear();
    
    QString appPath = QCoreApplication::applicationDirPath();
    struct Source { QString path; QString category; };
    QList<Source> sources = {
        { QDir(appPath).absoluteFilePath("../python/templates"), "SIMULATION" },
        { QDir(appPath).absoluteFilePath("../python/automation"), "AUTOMATION" },
        { "/home/jnd/qt_projects/fluxscript/examples", "EXAMPLES" }
    };

    for (const auto& src : sources) {
        QDir dir(src.path);
        if (!dir.exists()) continue;
        
        QStringList files = dir.entryList({"*.flux"}, QDir::Files | QDir::NoDotAndDotDot);
        for (const QString& file : files) {
            QListWidgetItem* item = new QListWidgetItem(m_templateList);
            item->setText(file.section('.', 0, 0).replace('_', ' ').toUpper());
            item->setData(Qt::UserRole, dir.absoluteFilePath(file));
            item->setData(Qt::UserRole + 1, src.category);
            
            QString subText = src.category.toLower();
            subText[0] = subText[0].toUpper();
            item->setToolTip(QString("[%1] %2").arg(subText, dir.absoluteFilePath(file)));
            
            if (src.category == "SIMULATION") item->setIcon(QIcon(":/icons/tool_run.svg"));
            else if (src.category == "AUTOMATION") item->setIcon(QIcon(":/icons/tool_gear.svg"));
            else item->setIcon(QIcon(":/icons/tool_search.svg"));
        }
    }
}

void ScriptEditorTab::updateWelcomeVisibility() {
    if (m_editor->toPlainText().trimmed().isEmpty()) {
        m_stack->setCurrentWidget(m_welcomeView);
    } else {
        m_stack->setCurrentWidget(m_editorContainer);
    }
}

void ScriptEditorTab::showTemplates() {
    m_stack->setCurrentWidget(m_welcomeView);
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
    updateWelcomeVisibility();
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

void ScriptEditorTab::onSearchTemplates(const QString&) {
    applyFilters();
}

void ScriptEditorTab::onCategorySelected(QListWidgetItem*, QListWidgetItem*) {
    applyFilters();
}

void ScriptEditorTab::applyFilters() {
    if (!m_templateList || !m_categoryList || !m_searchEdit) return;

    QString searchText = m_searchEdit->text().trimmed();
    QString selectedCategory = "ALL";
    if (auto* current = m_categoryList->currentItem()) {
        selectedCategory = current->text();
    }

    for (int i = 0; i < m_templateList->count(); ++i) {
        auto* item = m_templateList->item(i);
        QString itemCategory = item->data(Qt::UserRole + 1).toString();
        
        bool matchesSearch = item->text().contains(searchText, Qt::CaseInsensitive);
        bool matchesCategory = (selectedCategory == "ALL" || itemCategory == selectedCategory);
        
        m_templateList->setRowHidden(i, !(matchesSearch && matchesCategory));
    }
}

void ScriptEditorTab::onTemplateSelected(QListWidgetItem* item) {
    if (!item) return;
    
    QString path = item->data(Qt::UserRole).toString();
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_editor->setPlainText(QTextStream(&file).readAll());
        m_stack->setCurrentWidget(m_editorContainer);
    }
}

} // namespace Flux
