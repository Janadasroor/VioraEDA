#ifndef TEST_DESIGN_RULE_H
#define TEST_DESIGN_RULE_H

#include <QObject>
#include <QTest>
#include "../core/design_rule.h"

using namespace Flux::Core;

class TestDesignRule : public QObject {
    Q_OBJECT

private slots:
    // DesignRule tests
    void testRuleCreation();
    void testRuleSerialization();
    void testRuleValidation();
    void testRuleParameters();
    void testRuleSignals();

    // DesignRuleSet tests
    void testRuleSetCreation();
    void testRuleSetAddRemove();
    void testRuleSetSerialization();
    void testRuleSetFileIO();
    void testBuiltInRuleSets();

    // Violation tests
    void testViolationCreation();
    void testViolationOrdering();
};

#endif // TEST_DESIGN_RULE_H
