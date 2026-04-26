#include "erc_rules_dialog.h"
#include "design_rule_editor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QToolButton>
#include <QMessageBox>

ERCRulesDialog::ERCRulesDialog(const SchematicERCRules& currentRules, DesignRuleSet* customRules, QWidget* parent)
    : QDialog(parent), m_rules(currentRules), m_customRulesSet(customRules) {
    
    setWindowTitle("Electrical Rules Configuration");
    resize(900, 700);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    m_tabWidget = new QTabWidget(this);

    // --- Tab 1: Pin Matrix ---
    QWidget* matrixPage = new QWidget();
    QVBoxLayout* matrixLayout = new QVBoxLayout(matrixPage);

    QLabel* info = new QLabel("Click on a cell to cycle through severities: OK → Warning → Error → Critical");
    info->setStyleSheet("font-style: italic; color: #888;");
    matrixLayout->addWidget(info);

    m_table = new QTableWidget(12, 12, this);
    setupTable();
    matrixLayout->addWidget(m_table);

    // Legend
    QHBoxLayout* legend = new QHBoxLayout();
    auto addLegend = [&](const QString& name, SchematicERCRules::RuleResult res) {
        QLabel* color = new QLabel();
        color->setFixedSize(16, 16);
        color->setStyleSheet(QString("background-color: %1; border: 1px solid #444;").arg(severityToColor(res).name()));
        legend->addWidget(color);
        legend->addWidget(new QLabel(name));
        legend->addSpacing(10);
    };
    addLegend("OK", SchematicERCRules::OK);
    addLegend("Warning", SchematicERCRules::Warning);
    addLegend("Error", SchematicERCRules::Error);
    addLegend("Critical", SchematicERCRules::Critical);
    legend->addStretch();
    matrixLayout->addLayout(legend);

    QPushButton* resetBtn = new QPushButton("Reset to Defaults");
    connect(resetBtn, &QPushButton::clicked, this, &ERCRulesDialog::onResetDefaults);
    matrixLayout->addWidget(resetBtn, 0, Qt::AlignLeft);

    m_tabWidget->addTab(matrixPage, "Connectivity Matrix");

    // --- Tab 2: Custom Rules ---
    QWidget* customPage = new QWidget();
    setupCustomRulesTab(customPage);
    m_tabWidget->addTab(customPage, "Custom Rules (FluxScript)");

    mainLayout->addWidget(m_tabWidget);

    // --- Global Buttons ---
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addStretch();

    QPushButton* cancelBtn = new QPushButton("Cancel");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancelBtn);

    QPushButton* okBtn = new QPushButton("Apply Changes");
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(okBtn);

    mainLayout->addLayout(buttons);

    connect(m_table, &QTableWidget::cellClicked, this, &ERCRulesDialog::onCellClicked);
}

void ERCRulesDialog::setupCustomRulesTab(QWidget* page) {
    QVBoxLayout* layout = new QVBoxLayout(page);
    
    QLabel* desc = new QLabel("Extend the ERC system with programmable FluxScript rules.");
    desc->setStyleSheet("font-weight: bold; color: #a1a1aa;");
    layout->addWidget(desc);

    m_customRulesList = new QListWidget();
    m_customRulesList->setAlternatingRowColors(true);
    m_customRulesList->setStyleSheet("QListWidget::item { padding: 8px; }");
    layout->addWidget(m_customRulesList);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton("Add New Rule...");
    QPushButton* editBtn = new QPushButton("Edit Selected...");
    QPushButton* removeBtn = new QPushButton("Remove");
    
    connect(addBtn, &QPushButton::clicked, this, &ERCRulesDialog::onAddCustomRule);
    connect(editBtn, &QPushButton::clicked, this, &ERCRulesDialog::onEditCustomRule);
    connect(removeBtn, &QPushButton::clicked, this, &ERCRulesDialog::onRemoveCustomRule);
    connect(m_customRulesList, &QListWidget::itemDoubleClicked, this, &ERCRulesDialog::onEditCustomRule);

    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(editBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    updateCustomRulesList();
}

void ERCRulesDialog::updateCustomRulesList() {
    m_customRulesList->clear();
    if (!m_customRulesSet) return;

    for (auto* rule : m_customRulesSet->rules()) {
        QListWidgetItem* item = new QListWidgetItem(m_customRulesList);
        item->setText(rule->name());
        item->setToolTip(rule->description());
        if (!rule->enabled()) item->setForeground(Qt::gray);
        item->setData(Qt::UserRole, rule->id().toString());
    }
}

void ERCRulesDialog::onAddCustomRule() {
    if (!m_customRulesSet) m_customRulesSet = new DesignRuleSet("Custom Rules", this);
    
    DesignRule* rule = DesignRuleEditor::createRule(RuleCategory::Custom, this);
    if (rule) {
        m_customRulesSet->addRule(rule);
        updateCustomRulesList();
    }
}

void ERCRulesDialog::onEditCustomRule() {
    auto* item = m_customRulesList->currentItem();
    if (!item || !m_customRulesSet) return;

    QUuid id(item->data(Qt::UserRole).toString());
    DesignRule* rule = m_customRulesSet->rule(id);
    if (rule) {
        if (DesignRuleEditor::editRule(rule, this)) {
            updateCustomRulesList();
        }
    }
}

void ERCRulesDialog::onRemoveCustomRule() {
    auto* item = m_customRulesList->currentItem();
    if (!item || !m_customRulesSet) return;

    if (QMessageBox::question(this, "Remove Rule", "Are you sure you want to remove this custom rule?") == QMessageBox::Yes) {
        QUuid id(item->data(Qt::UserRole).toString());
        m_customRulesSet->removeRule(id);
        updateCustomRulesList();
    }
}

void ERCRulesDialog::setupTable() {
    QStringList headers;
    for (int i = 0; i < 12; ++i) {
        headers << typeToName(i);
    }
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setVerticalHeaderLabels(headers);
    
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    
    for (int r = 0; r < 12; ++r) {
        for (int c = 0; c < 12; ++c) {
            updateCell(r, c);
        }
    }
}

void ERCRulesDialog::updateCell(int row, int col) {
    SchematicERCRules::RuleResult res = m_rules.getRule(static_cast<SchematicItem::PinElectricalType>(row), 
                                                       static_cast<SchematicItem::PinElectricalType>(col));
    
    QTableWidgetItem* item = m_table->item(row, col);
    if (!item) {
        item = new QTableWidgetItem();
        m_table->setItem(row, col, item);
    }
    
    item->setBackground(severityToColor(res));
    item->setText(severityToIcon(res));
    item->setTextAlignment(Qt::AlignCenter);
    
    QString tooltip = QString("%1 connected to %2: %3")
                        .arg(typeToName(row), typeToName(col), 
                             (res == SchematicERCRules::OK ? "OK" : 
                              (res == SchematicERCRules::Warning ? "Warning" : 
                               (res == SchematicERCRules::Error ? "Error" : "Critical"))));
    item->setToolTip(tooltip);
}

void ERCRulesDialog::onCellClicked(int row, int col) {
    SchematicItem::PinElectricalType t1 = static_cast<SchematicItem::PinElectricalType>(row);
    SchematicItem::PinElectricalType t2 = static_cast<SchematicItem::PinElectricalType>(col);
    
    int current = static_cast<int>(m_rules.getRule(t1, t2));
    int next = (current + 1) % 4;
    
    m_rules.setRule(t1, t2, static_cast<SchematicERCRules::RuleResult>(next));
    
    updateCell(row, col);
    if (row != col) updateCell(col, row); // Mirror
}

void ERCRulesDialog::onResetDefaults() {
    m_rules = SchematicERCRules::defaultRules();
    for (int r = 0; r < 12; ++r) {
        for (int c = 0; c < 12; ++c) {
            updateCell(r, c);
        }
    }
}

SchematicERCRules ERCRulesDialog::getRules() const {
    return m_rules;
}

QString ERCRulesDialog::typeToName(int type) const {
    switch (type) {
        case 0: return "Passive";
        case 1: return "Input";
        case 2: return "Output";
        case 3: return "BiDi";
        case 4: return "TriState";
        case 5: return "Free";
        case 6: return "Unspec";
        case 7: return "Pwr In";
        case 8: return "Pwr Out";
        case 9: return "OC";
        case 10: return "OE";
        case 11: return "NC";
        default: return "?";
    }
}

QColor ERCRulesDialog::severityToColor(SchematicERCRules::RuleResult res) const {
    switch (res) {
        case SchematicERCRules::OK: return QColor(40, 40, 40);
        case SchematicERCRules::Warning: return QColor(180, 150, 0, 180);
        case SchematicERCRules::Error: return QColor(200, 50, 50, 180);
        case SchematicERCRules::Critical: return QColor(255, 0, 0, 220);
        default: return Qt::transparent;
    }
}

QString ERCRulesDialog::severityToIcon(SchematicERCRules::RuleResult res) const {
    switch (res) {
        case SchematicERCRules::OK: return "•";
        case SchematicERCRules::Warning: return "!";
        case SchematicERCRules::Error: return "X";
        case SchematicERCRules::Critical: return "!!!";
        default: return "";
    }
}
