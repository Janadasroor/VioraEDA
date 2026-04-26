// design_rule_editor.cpp
// UI dialogs for design rule management (DesignRuleEditor only)

#include "design_rule_editor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QStandardPaths>
#include <QDir>

// ────────────────────────────────────────────────────────────────────────────
// DesignRuleEditor Implementation
// ────────────────────────────────────────────────────────────────────────────

DesignRuleEditor::DesignRuleEditor(DesignRule* rule, QWidget* parent)
    : QDialog(parent)
    , m_rule(rule)
    , m_isNewRule(rule == nullptr)
    , m_nameEdit(nullptr)
    , m_descriptionEdit(nullptr)
    , m_categoryCombo(nullptr)
    , m_severityCombo(nullptr)
    , m_scopeCombo(nullptr)
    , m_enabledCheck(nullptr)
    , m_ruleTypeStack(nullptr)
    , m_ercWidget(nullptr)
    , m_drcWidget(nullptr)
    , m_customWidget(nullptr)
    , m_expressionEdit(nullptr)
    , m_expressionHelpLabel(nullptr)
    , m_paramsTable(nullptr)
    , m_addParamBtn(nullptr)
    , m_removeParamBtn(nullptr)
    , m_tagsEdit(nullptr)
    , m_testBtn(nullptr)
    , m_okBtn(nullptr)
    , m_cancelBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_minClearanceEdit(nullptr)
{
    if (m_isNewRule) {
        m_rule = new DesignRule("New Rule", RuleCategory::ERC, this);
    } else {
        m_rule->setParent(this);
    }

    setWindowTitle(m_isNewRule ? "Create Design Rule" : "Edit Design Rule");
    setMinimumSize(700, 600);
    resize(800, 700);

    setupUI();
    loadRuleIntoUI();
}

DesignRuleEditor::~DesignRuleEditor() {
    if (m_isNewRule) {
        delete m_rule;
    }
}

DesignRule* DesignRuleEditor::editRule(DesignRule* rule, QWidget* parent) {
    DesignRuleEditor dialog(rule, parent);
    if (dialog.exec() == QDialog::Accepted) {
        return dialog.getRule();
    }
    return nullptr;
}

DesignRule* DesignRuleEditor::createRule(RuleCategory category, QWidget* parent) {
    DesignRule* rule = new DesignRule("New Rule", category);
    DesignRuleEditor dialog(rule, parent);
    dialog.m_isNewRule = true;
    
    if (dialog.exec() == QDialog::Accepted) {
        rule->setParent(nullptr);
        return rule;
    }
    
    delete rule;
    return nullptr;
}

void DesignRuleEditor::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Basic Information
    QGroupBox* basicGroup = new QGroupBox("Basic Information");
    QFormLayout* basicLayout = new QFormLayout(basicGroup);
    basicLayout->setSpacing(20);
    basicLayout->setContentsMargins(15, 20, 15, 20);

    QString fieldStyle = "QWidget { min-height: 42px; padding: 8px; font-size: 10pt; }";

    m_nameEdit = new QLineEdit();
    m_nameEdit->setStyleSheet(fieldStyle);
    m_nameEdit->setPlaceholderText("e.g., Check Decoupling Capacitors");
    connect(m_nameEdit, &QLineEdit::textChanged, this, &DesignRuleEditor::onNameChanged);
    basicLayout->addRow("Name:", m_nameEdit);

    m_descriptionEdit = new QTextEdit();
    m_descriptionEdit->setMinimumHeight(100);
    m_descriptionEdit->setPlaceholderText("Describe what this rule checks...");
    basicLayout->addRow("Description:", m_descriptionEdit);

    m_categoryCombo = new QComboBox();
    m_categoryCombo->setStyleSheet(fieldStyle);
    m_categoryCombo->addItem("Electrical Rules Check (ERC)", static_cast<int>(RuleCategory::ERC));
    m_categoryCombo->addItem("Design Rules Check (DRC)", static_cast<int>(RuleCategory::DRC));
    m_categoryCombo->addItem("Custom Rule", static_cast<int>(RuleCategory::Custom));
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DesignRuleEditor::onCategoryChanged);
    basicLayout->addRow("Category:", m_categoryCombo);

    m_severityCombo = new QComboBox();
    m_severityCombo->setStyleSheet(fieldStyle);
    m_severityCombo->addItem("Information", static_cast<int>(RuleSeverity::Info));
    m_severityCombo->addItem("Warning", static_cast<int>(RuleSeverity::Warning));
    m_severityCombo->addItem("Error", static_cast<int>(RuleSeverity::Error));
    m_severityCombo->addItem("Critical", static_cast<int>(RuleSeverity::Critical));
    connect(m_severityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DesignRuleEditor::onSeverityChanged);
    basicLayout->addRow("Default Severity:", m_severityCombo);

    m_scopeCombo = new QComboBox();
    m_scopeCombo->setStyleSheet(fieldStyle);
    m_scopeCombo->addItem("Global", static_cast<int>(RuleScope::Global));
    m_scopeCombo->addItem("Project", static_cast<int>(RuleScope::Project));
    m_scopeCombo->addItem("Sheet", static_cast<int>(RuleScope::Sheet));
    basicLayout->addRow("Scope:", m_scopeCombo);

    m_enabledCheck = new QCheckBox("Enable this rule");
    m_enabledCheck->setChecked(true);
    m_enabledCheck->setStyleSheet("QCheckBox { padding-top: 10px; font-weight: bold; }");
    basicLayout->addRow(m_enabledCheck);

    mainLayout->addWidget(basicGroup);

    // Rule Configuration
    QGroupBox* configGroup = new QGroupBox("Rule Configuration");
    configGroup->setContentsMargins(15, 15, 15, 15);
    QVBoxLayout* configLayout = new QVBoxLayout(configGroup);
    configLayout->setSpacing(15);

    m_ruleTypeStack = new QStackedWidget();

    m_ercWidget = new QWidget();
    m_ercWidget->setLayout(new QFormLayout());
    m_ercWidget->layout()->addWidget(new QLabel("ERC rules use the ERC Matrix Editor"));
    m_ruleTypeStack->addWidget(m_ercWidget);

    m_drcWidget = new QWidget();
    QFormLayout* drcLayout = new QFormLayout(m_drcWidget);
    m_minClearanceEdit = new QLineEdit("0.2");
    drcLayout->addRow("Min Clearance (mm):", m_minClearanceEdit);
    m_ruleTypeStack->addWidget(m_drcWidget);

    m_customWidget = new QWidget();
    QVBoxLayout* customLayout = new QVBoxLayout(m_customWidget);
    m_expressionEdit = new Flux::CodeEditor(nullptr, nullptr, this);
    m_expressionEdit->setPlaceholderText("FluxScript design rule logic...");
    m_expressionEdit->updateCompletionKeywords({
        "erc_get_component_count", "erc_get_ref", "erc_get_value", 
        "erc_get_type", "erc_get_pin_count", "erc_get_pin_net", "erc_report"
    });
    customLayout->addWidget(m_expressionEdit);
    
    m_expressionHelpLabel = new QLabel(
        "<b>FluxScript API:</b><br/>"
        "• <i>erc_get_component_count()</i><br/>"
        "• <i>erc_get_ref(idx)</i>, <i>erc_get_value(idx)</i>, <i>erc_get_type(idx)</i><br/>"
        "• <i>erc_report(severity, message, comp_idx)</i> (Severity: 1=Warn, 2=Err)"
    );
    m_expressionHelpLabel->setWordWrap(true);
    m_expressionHelpLabel->setStyleSheet("color: #71717a; font-size: 9pt;");
    customLayout->addWidget(m_expressionHelpLabel);
    m_ruleTypeStack->addWidget(m_customWidget);

    configLayout->addWidget(m_ruleTypeStack);
    mainLayout->addWidget(configGroup);

    // Parameters
    QGroupBox* paramsGroup = new QGroupBox("Parameters");
    QVBoxLayout* paramsLayout = new QVBoxLayout(paramsGroup);

    m_paramsTable = new QTableWidget();
    m_paramsTable->setColumnCount(3);
    m_paramsTable->setHorizontalHeaderLabels({"Name", "Value", "Type"});
    m_paramsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_paramsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_paramsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    paramsLayout->addWidget(m_paramsTable);

    QHBoxLayout* paramBtnLayout = new QHBoxLayout();
    m_addParamBtn = new QPushButton("Add Parameter");
    m_removeParamBtn = new QPushButton("Remove");
    connect(m_addParamBtn, &QPushButton::clicked, this, &DesignRuleEditor::onAddParameter);
    connect(m_removeParamBtn, &QPushButton::clicked, this, &DesignRuleEditor::onRemoveParameter);
    paramBtnLayout->addWidget(m_addParamBtn);
    paramBtnLayout->addWidget(m_removeParamBtn);
    paramBtnLayout->addStretch();
    paramsLayout->addLayout(paramBtnLayout);

    mainLayout->addWidget(paramsGroup);

    // Tags
    QGroupBox* tagsGroup = new QGroupBox("Tags");
    QHBoxLayout* tagsLayout = new QHBoxLayout(tagsGroup);
    m_tagsEdit = new QLineEdit();
    m_tagsEdit->setPlaceholderText("comma,separated,tags");
    tagsLayout->addWidget(m_tagsEdit);
    mainLayout->addWidget(tagsGroup);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_statusLabel = new QLabel();
    buttonLayout->addWidget(m_statusLabel);
    buttonLayout->addStretch();

    m_testBtn = new QPushButton("Test Rule");
    connect(m_testBtn, &QPushButton::clicked, this, &DesignRuleEditor::onTestRule);
    buttonLayout->addWidget(m_testBtn);

    m_cancelBtn = new QPushButton("Cancel");
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelBtn);

    m_okBtn = new QPushButton("OK");
    m_okBtn->setDefault(true);
    connect(m_okBtn, &QPushButton::clicked, this, [this]() {
        saveUIIntoRule();
        if (m_rule->isValid()) {
            accept();
        } else {
            QMessageBox::warning(this, "Validation Error", m_rule->validationError());
        }
    });
    buttonLayout->addWidget(m_okBtn);

    mainLayout->addLayout(buttonLayout);
}

void DesignRuleEditor::loadRuleIntoUI() {
    m_nameEdit->setText(m_rule->name());
    m_descriptionEdit->setText(m_rule->description());
    
    for (int i = 0; i < m_categoryCombo->count(); ++i) {
        if (m_categoryCombo->itemData(i).toInt() == static_cast<int>(m_rule->category())) {
            m_categoryCombo->setCurrentIndex(i);
            break;
        }
    }

    for (int i = 0; i < m_severityCombo->count(); ++i) {
        if (m_severityCombo->itemData(i).toInt() == static_cast<int>(m_rule->defaultSeverity())) {
            m_severityCombo->setCurrentIndex(i);
            break;
        }
    }

    m_enabledCheck->setChecked(m_rule->enabled());
    m_expressionEdit->setPlainText(m_rule->expression());
    m_tagsEdit->setText(m_rule->tags().join(", "));

    m_paramsTable->setRowCount(0);
    for (auto it = m_rule->parameters().begin(); it != m_rule->parameters().end(); ++it) {
        int row = m_paramsTable->rowCount();
        m_paramsTable->insertRow(row);
        m_paramsTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_paramsTable->setItem(row, 1, new QTableWidgetItem(it.value().toString()));
        m_paramsTable->setItem(row, 2, new QTableWidgetItem(it.value().typeName()));
    }

    onCategoryChanged(m_categoryCombo->currentIndex());
}

void DesignRuleEditor::saveUIIntoRule() {
    m_rule->setName(m_nameEdit->text());
    m_rule->setDescription(m_descriptionEdit->toPlainText());
    m_rule->setCategory(static_cast<RuleCategory>(m_categoryCombo->currentData().toInt()));
    m_rule->setDefaultSeverity(static_cast<RuleSeverity>(m_severityCombo->currentData().toInt()));
    m_rule->setEnabled(m_enabledCheck->isChecked());
    m_rule->setExpression(m_expressionEdit->toPlainText());
    m_rule->setTags(m_tagsEdit->text().split(",", Qt::SkipEmptyParts));

    QVariantMap params;
    for (int row = 0; row < m_paramsTable->rowCount(); ++row) {
        params[m_paramsTable->item(row, 0)->text()] = m_paramsTable->item(row, 1)->text();
    }
    m_rule->setParameters(params);
}

void DesignRuleEditor::onCategoryChanged(int index) {
    RuleCategory category = static_cast<RuleCategory>(m_categoryCombo->itemData(index).toInt());
    switch (category) {
        case RuleCategory::DRC: m_ruleTypeStack->setCurrentWidget(m_drcWidget); break;
        case RuleCategory::Custom: m_ruleTypeStack->setCurrentWidget(m_customWidget); break;
        default: m_ruleTypeStack->setCurrentWidget(m_ercWidget);
    }
}

void DesignRuleEditor::onNameChanged(const QString& text) {
    m_statusLabel->setText(text.trimmed().isEmpty() ? "Name required" : "");
    m_okBtn->setEnabled(!text.trimmed().isEmpty());
}

void DesignRuleEditor::onSeverityChanged(int) {}
void DesignRuleEditor::onTestRule() { QMessageBox::information(this, "Test", "Save and run from ERC/DRC panel"); }
void DesignRuleEditor::onAddParameter() {
    int row = m_paramsTable->rowCount();
    m_paramsTable->insertRow(row);
    m_paramsTable->setItem(row, 0, new QTableWidgetItem("param_" + QString::number(row)));
    m_paramsTable->setItem(row, 1, new QTableWidgetItem(""));
    m_paramsTable->setItem(row, 2, new QTableWidgetItem("String"));
}
void DesignRuleEditor::onRemoveParameter() {
    int row = m_paramsTable->currentRow();
    if (row >= 0) m_paramsTable->removeRow(row);
}
void DesignRuleEditor::onParameterChanged() {}
void DesignRuleEditor::onExpressionHelp() {
    QMessageBox::information(this, "FluxScript Rule Example", 
        "for i in range(0, erc_get_component_count()) {\n"
        "    if (erc_get_type(i) == \"Resistor\") {\n"
        "        if (erc_get_value(i) == \"TBD\") {\n"
        "            erc_report(2, \"Value not set for resistor\", i);\n"
        "        }\n"
        "    }\n"
        "}"
    );
}
