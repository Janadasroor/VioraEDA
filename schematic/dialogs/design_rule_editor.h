#ifndef DESIGN_RULE_EDITOR_H
#define DESIGN_RULE_EDITOR_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include "../core/design_rule.h"

using namespace Flux::Core;

class DesignRuleEditor : public QDialog {
    Q_OBJECT

public:
    explicit DesignRuleEditor(DesignRule* rule = nullptr, QWidget* parent = nullptr);
    ~DesignRuleEditor();

    DesignRule* getRule() const { return m_rule; }
    static DesignRule* editRule(DesignRule* rule, QWidget* parent);
    static DesignRule* createRule(RuleCategory category, QWidget* parent);

private slots:
    void onCategoryChanged(int index);
    void onSeverityChanged(int index);
    void onNameChanged(const QString& text);
    void onTestRule();
    void onAddParameter();
    void onRemoveParameter();
    void onParameterChanged();
    void onExpressionHelp();

private:
    void setupUI();
    void loadRuleIntoUI();
    void saveUIIntoRule();

    DesignRule* m_rule;
    bool m_isNewRule;

    QLineEdit* m_nameEdit;
    QTextEdit* m_descriptionEdit;
    QComboBox* m_categoryCombo;
    QComboBox* m_severityCombo;
    QComboBox* m_scopeCombo;
    QCheckBox* m_enabledCheck;
    QStackedWidget* m_ruleTypeStack;
    QWidget* m_ercWidget;
    QWidget* m_drcWidget;
    QWidget* m_customWidget;
    QTextEdit* m_expressionEdit;
    QLabel* m_expressionHelpLabel;
    QTableWidget* m_paramsTable;
    QPushButton* m_addParamBtn;
    QPushButton* m_removeParamBtn;
    QLineEdit* m_tagsEdit;
    QPushButton* m_testBtn;
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
    QLabel* m_statusLabel;
    QLineEdit* m_minClearanceEdit;
};

#endif // DESIGN_RULE_EDITOR_H
