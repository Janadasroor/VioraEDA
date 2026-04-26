#ifndef ERC_RULES_DIALOG_H
#define ERC_RULES_DIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QTabWidget>
#include <QListWidget>
#include "../analysis/schematic_erc_rules.h"
#include "../core/design_rule.h"

using namespace Flux::Core;

class ERCRulesDialog : public QDialog {
    Q_OBJECT

public:
    explicit ERCRulesDialog(const SchematicERCRules& currentRules, DesignRuleSet* customRules = nullptr, QWidget* parent = nullptr);
    SchematicERCRules getRules() const;
    DesignRuleSet* getCustomRules() const { return m_customRulesSet; }

private Q_SLOTS:
    void onCellClicked(int row, int col);
    void onResetDefaults();
    void onAddCustomRule();
    void onEditCustomRule();
    void onRemoveCustomRule();

private:
    void setupTable();
    void setupCustomRulesTab(QWidget* page);
    void updateCell(int row, int col);
    void updateCustomRulesList();
    QString typeToName(int type) const;
    QColor severityToColor(SchematicERCRules::RuleResult res) const;
    QString severityToIcon(SchematicERCRules::RuleResult res) const;

    QTabWidget* m_tabWidget;
    QTableWidget* m_table;
    QListWidget* m_customRulesList;
    SchematicERCRules m_rules;
    DesignRuleSet* m_customRulesSet;
};

#endif // ERC_RULES_DIALOG_H
