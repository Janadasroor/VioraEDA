#include "gemini_instructions_dialog.h"
#include "../core/config_manager.h"
#include "theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QTabWidget>
#include <QListWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>

GeminiInstructionsDialog::GeminiInstructionsDialog(const QString& projectPath, QWidget* parent)
    : QDialog(parent), m_projectPath(projectPath) {
    
    setWindowTitle("Viora AI Configuration");
    resize(700, 550);

    PCBTheme* theme = ThemeManager::theme();
    QString bg = theme ? theme->panelBackground().name() : "#ffffff";
    QString fg = theme ? theme->textColor().name() : "#000000";
    QString border = theme ? theme->panelBorder().name() : "#cccccc";
    QString secondaryFg = theme ? theme->textSecondary().name() : "#888";

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet(QString(
        "QTabWidget::pane { border: 1px solid %1; background: %2; }"
        "QTabBar::tab { background: %3; color: %4; padding: 10px 20px; border: 1px solid %1; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background: %2; border-bottom: 2px solid #3b82f6; font-weight: bold; }"
    ).arg(border, bg, "#1e293b", secondaryFg));
    
    // --- Tab 1: Custom Instructions ---
    QWidget* instructionsTab = new QWidget(this);
    QVBoxLayout* instLayout = new QVBoxLayout(instructionsTab);
    instLayout->setContentsMargins(16, 16, 16, 16);
    instLayout->setSpacing(12);

    QLabel* instTitle = new QLabel("Global AI Personality & Rules", this);
    instTitle->setStyleSheet(QString("font-weight: bold; font-size: 14px; color: %1;").arg(fg));
    instLayout->addWidget(instTitle);

    QLabel* instDesc = new QLabel("Add custom instructions that will be appended to every AI request. Use this to set formatting rules, tone, or project-specific knowledge.", this);
    instDesc->setWordWrap(true);
    instDesc->setStyleSheet(QString("color: %1; font-size: 11px;").arg(secondaryFg));
    instLayout->addWidget(instDesc);

    QHBoxLayout* scopeLayout = new QHBoxLayout();
    QLabel* scopeLabel = new QLabel("Instruction Scope:", this);
    scopeLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(fg));
    m_scopeCombo = new QComboBox(this);
    m_scopeCombo->addItem("Global (All Projects)");
    m_scopeCombo->addItem("Project Specific");
    m_scopeCombo->setFixedHeight(30);
    m_scopeCombo->setStyleSheet(QString("QComboBox { background: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 0 8px; }")
        .arg(bg, fg, border));
    
    scopeLayout->addWidget(scopeLabel);
    scopeLayout->addWidget(m_scopeCombo, 1);
    instLayout->addLayout(scopeLayout);

    m_editor = new QPlainTextEdit(this);
    m_editor->setPlaceholderText("Enter custom instructions here...\nExample: 'Always explain the physics of the circuit.' or 'Use technical jargon.'");
    m_editor->setStyleSheet(QString("QPlainTextEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 8px; font-family: 'JetBrains Mono', 'Consolas', monospace; font-size: 12px; }")
        .arg(bg, fg, border));
    instLayout->addWidget(m_editor, 1);

    m_tabWidget->addTab(instructionsTab, "Custom Instructions");

    // --- Tab 2: Expert Skills ---
    QWidget* skillsTab = new QWidget(this);
    QVBoxLayout* skillsLayout = new QVBoxLayout(skillsTab);
    skillsLayout->setContentsMargins(16, 16, 16, 16);
    skillsLayout->setSpacing(12);

    QLabel* skillsTitle = new QLabel("Agent Skills (Modular Expertise)", this);
    skillsTitle->setStyleSheet(QString("font-weight: bold; font-size: 14px; color: %1;").arg(fg));
    skillsLayout->addWidget(skillsTitle);

    QHBoxLayout* projectSelectLayout = new QHBoxLayout();
    QLabel* projLabel = new QLabel("Project Workspace:", this);
    projLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(fg));
    m_projectCombo = new QComboBox(this);
    m_projectCombo->setFixedHeight(30);
    m_projectCombo->setStyleSheet(QString("QComboBox { background: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 0 8px; }").arg(bg, fg, border));
    updateProjectCombo();
    connect(m_projectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GeminiInstructionsDialog::onProjectSelectionChanged);

    projectSelectLayout->addWidget(projLabel);
    projectSelectLayout->addWidget(m_projectCombo, 1);
    skillsLayout->addLayout(projectSelectLayout);

    QHBoxLayout* listActionLayout = new QHBoxLayout();
    
    m_skillList = new QListWidget(this);
// ...
    connect(m_skillList, &QListWidget::itemSelectionChanged, this, &GeminiInstructionsDialog::onSkillSelectionChanged);
    listActionLayout->addWidget(m_skillList, 1);

    QVBoxLayout* skillBtns = new QVBoxLayout();
    skillBtns->setSpacing(8);
    
    QPushButton* addSkillBtn = new QPushButton("New Skill", this);
    addSkillBtn->setFixedHeight(32);
    addSkillBtn->setStyleSheet("QPushButton { background: #3b82f6; color: white; border: none; border-radius: 4px; font-weight: bold; } QPushButton:hover { background: #2563eb; }");
    connect(addSkillBtn, &QPushButton::clicked, this, &GeminiInstructionsDialog::onAddSkillClicked);
    
    QPushButton* importSkillBtn = new QPushButton("Import MD", this);
    importSkillBtn->setFixedHeight(32);
    importSkillBtn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 4px; } QPushButton:hover { background: %2; }").arg(fg, border));
    connect(importSkillBtn, &QPushButton::clicked, this, &GeminiInstructionsDialog::onImportSkillClicked);

    m_renameSkillBtn = new QPushButton("Rename", this);
    m_renameSkillBtn->setFixedHeight(32);
    m_renameSkillBtn->setEnabled(false);
    m_renameSkillBtn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 4px; } QPushButton:hover { background: %2; }").arg(fg, border));
    connect(m_renameSkillBtn, &QPushButton::clicked, this, &GeminiInstructionsDialog::onRenameSkillClicked);
    
    m_removeSkillBtn = new QPushButton("Remove", this);
    m_removeSkillBtn->setFixedHeight(32);
    m_removeSkillBtn->setEnabled(false);
    m_removeSkillBtn->setStyleSheet("QPushButton { background: transparent; color: #ef4444; border: 1px solid #ef4444; border-radius: 4px; } QPushButton:hover { background: rgba(239, 68, 68, 0.1); } QPushButton:disabled { color: #555; border-color: #333; }");
    connect(m_removeSkillBtn, &QPushButton::clicked, this, &GeminiInstructionsDialog::onRemoveSkillClicked);
    
    skillBtns->addWidget(addSkillBtn);
    skillBtns->addWidget(importSkillBtn);
    skillBtns->addWidget(m_renameSkillBtn);
    skillBtns->addStretch();
    skillBtns->addWidget(m_removeSkillBtn);
    
    listActionLayout->addLayout(skillBtns);
    skillsLayout->addLayout(listActionLayout);

    m_tabWidget->addTab(skillsTab, "Expert Skills");

    mainLayout->addWidget(m_tabWidget, 1);

    // --- Bottom Buttons ---
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(16, 8, 16, 16);
    bottomLayout->addStretch();
    
    QPushButton* cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setFixedHeight(32);
    cancelBtn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 4px; padding: 0 20px; } QPushButton:hover { background: %2; }")
        .arg(fg, border));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    QPushButton* saveBtn = new QPushButton("Save All Changes", this);
    saveBtn->setFixedHeight(32);
    saveBtn->setStyleSheet("QPushButton { background: #238636; color: white; border: none; border-radius: 4px; padding: 0 24px; font-weight: bold; } QPushButton:hover { background: #2ea043; }");
    connect(saveBtn, &QPushButton::clicked, this, &GeminiInstructionsDialog::onSaveClicked);

    bottomLayout->addWidget(cancelBtn);
    bottomLayout->addWidget(saveBtn);
    mainLayout->addLayout(bottomLayout);

    // Initial Load
    m_globalCache = ConfigManager::instance().geminiGlobalInstructions();
    m_projectCache = "";
    if (!m_projectPath.isEmpty()) {
        QFileInfo info(m_projectPath);
        QString pFile = info.absolutePath() + "/.gemini/custom_instructions.txt";
        QFile file(pFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_projectCache = QString::fromUtf8(file.readAll());
        }
    }

    if (m_projectPath.isEmpty()) {
        m_scopeCombo->removeItem(1);
    } else {
        m_scopeCombo->setCurrentIndex(1); // Default to project if available
    }

    loadInstructions();
    refreshSkillList();
    
    connect(m_scopeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GeminiInstructionsDialog::onScopeChanged);
}

void GeminiInstructionsDialog::loadInstructions() {
    m_loading = true;
    if (m_scopeCombo->currentIndex() == 0) {
        m_editor->setPlainText(m_globalCache);
    } else {
        m_editor->setPlainText(m_projectCache);
    }
    m_loading = false;
}

void GeminiInstructionsDialog::onScopeChanged(int) {
    if (m_loading) return;
    loadInstructions();
}

void GeminiInstructionsDialog::onSaveClicked() {
    if (m_scopeCombo->currentIndex() == 0) {
        m_globalCache = m_editor->toPlainText().trimmed();
        ConfigManager::instance().setGeminiGlobalInstructions(m_globalCache);
    } else {
        m_projectCache = m_editor->toPlainText().trimmed();
        if (!m_projectPath.isEmpty()) {
            QFileInfo info(m_projectPath);
            QString pDir = info.absolutePath() + "/.gemini";
            QDir().mkpath(pDir);
            QFile file(pDir + "/custom_instructions.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << m_projectCache;
            }
        }
    }
    accept();
}

QString GeminiInstructionsDialog::getGlobalSkillsPath() const {
    QString path = QDir::homePath() + "/.viora/skills";
    QDir().mkpath(path);
    return path;
}

QString GeminiInstructionsDialog::getSkillsPathForProject(const QString& projectPath) const {
    if (projectPath.isEmpty()) return QString();
    QString path = projectPath + "/.viora/skills";
    QDir().mkpath(path);
    return path;
}

QString GeminiInstructionsDialog::getSelectedProjectPath() const {
    if (m_projectCombo->currentIndex() <= 0) return QString(); // Global
    return m_projectCombo->currentData().toString();
}

void GeminiInstructionsDialog::updateProjectCombo() {
    m_projectCombo->clear();
    m_projectCombo->addItem("Global User Skills", QString());
    
    QStringList workspaces = ConfigManager::instance().workspaceFolders();
    for (const QString& ws : workspaces) {
        m_projectCombo->addItem(QFileInfo(ws).fileName() + " (Project)", ws);
    }
}

void GeminiInstructionsDialog::onProjectSelectionChanged(int) {
    refreshSkillList();
}

void GeminiInstructionsDialog::refreshSkillList() {
    m_skillList->clear();
    
    QString path;
    if (m_projectCombo->currentIndex() == 0) {
        path = getGlobalSkillsPath();
    } else {
        path = getSkillsPathForProject(getSelectedProjectPath());
    }

    if (!path.isEmpty() && QDir(path).exists()) {
        QDir dir(path);
        QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& entry : entries) {
            QListWidgetItem* item = new QListWidgetItem(entry, m_skillList);
            item->setData(Qt::UserRole, dir.absoluteFilePath(entry));
            m_skillList->addItem(item);
        }
    }
}

void GeminiInstructionsDialog::onSkillSelectionChanged() {
    bool hasSelection = m_skillList->currentItem() != nullptr;
    m_removeSkillBtn->setEnabled(hasSelection);
    m_renameSkillBtn->setEnabled(hasSelection);
}

void GeminiInstructionsDialog::onAddSkillClicked() {
    QString name = QInputDialog::getText(this, "New Skill", "Enter skill name (e.g. SMPSExpert):");
    if (name.isEmpty()) return;
    
    name = name.trimmed().replace(" ", "-"); 
    
    QString path;
    if (m_projectCombo->currentIndex() == 0) {
        path = getGlobalSkillsPath();
    } else {
        QString projectPath = getSelectedProjectPath();
        if (projectPath.isEmpty()) {
            QMessageBox::warning(this, "Error", "Selected project path is invalid.");
            return;
        }
        path = getSkillsPathForProject(projectPath);
    }

    if (path.isEmpty()) return;
    
    QDir skillDir(path + "/" + name);
    qDebug() << "[GeminiInstructionsDialog] Creating skill at:" << skillDir.absolutePath();

    if (skillDir.exists()) {
        QMessageBox::warning(this, "Error", "Skill already exists.");
        return;
    }
    
    if (QDir().mkpath(skillDir.absolutePath())) {
        QFile file(skillDir.absoluteFilePath("SKILL.md"));
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "# Skill: " << name << "\n\n";
            out << "## Description\nExpertise in " << name << ".\n\n";
            out << "## System Prompt\nYou are an expert in...\n";
            file.close();
            qDebug() << "[GeminiInstructionsDialog] SKILL.md created successfully.";
        } else {
            qDebug() << "[GeminiInstructionsDialog] Failed to create SKILL.md:" << file.errorString();
        }
        refreshSkillList();
    } else {
        qDebug() << "[GeminiInstructionsDialog] Failed to create directory path:" << skillDir.absolutePath();
        QMessageBox::critical(this, "Error", "Failed to create skill directory structure.");
    }
}

void GeminiInstructionsDialog::onImportSkillClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "Import Skill Markdown", QString(), "Markdown Files (*.md)");
    if (filePath.isEmpty()) return;
    
    QFileInfo info(filePath);
    QString name = info.baseName().trimmed().replace(" ", "-");
    
    QString targetRoot;
    if (m_projectCombo->currentIndex() == 0) {
        targetRoot = getGlobalSkillsPath();
    } else {
        targetRoot = getSkillsPathForProject(getSelectedProjectPath());
    }

    if (targetRoot.isEmpty()) return;
    
    QDir skillDir(targetRoot + "/" + name);
    if (skillDir.exists()) {
        QMessageBox::warning(this, "Error", "Skill folder already exists.");
        return;
    }
    
    if (QDir().mkpath(skillDir.absolutePath())) {
        QFile::copy(filePath, skillDir.absoluteFilePath("SKILL.md"));
        refreshSkillList();
    }
}

void GeminiInstructionsDialog::onRemoveSkillClicked() {
    QListWidgetItem* item = m_skillList->currentItem();
    if (!item) return;
    
    QString path = item->data(Qt::UserRole).toString();
    if (QMessageBox::question(this, "Remove Skill", 
        QString("Are you sure you want to delete the skill folder: %1?\nThis action cannot be undone.").arg(item->text())) == QMessageBox::Yes) {
        QDir dir(path);
        dir.removeRecursively();
        refreshSkillList();
    }
}

void GeminiInstructionsDialog::onRenameSkillClicked() {
    QListWidgetItem* item = m_skillList->currentItem();
    if (!item) return;

    QString oldName = item->text();
    QString newName = QInputDialog::getText(this, "Rename Skill", "Enter new name:", QLineEdit::Normal, oldName);
    if (newName.isEmpty() || newName == oldName) return;

    newName = newName.trimmed().replace(" ", "-");
    
    QString oldPath = item->data(Qt::UserRole).toString();
    QDir oldDir(oldPath);
    QString newPath = oldDir.absolutePath().section('/', 0, -2) + "/" + newName;

    if (QDir(newPath).exists()) {
        QMessageBox::warning(this, "Error", "A skill with that name already exists.");
        return;
    }

    if (oldDir.rename(oldPath, newPath)) {
        refreshSkillList();
    } else {
        QMessageBox::critical(this, "Error", "Failed to rename skill folder.");
    }
}
