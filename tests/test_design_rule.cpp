// test_design_rule.cpp
// Unit tests for Design Rule Management System

#include "test_design_rule.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonDocument>

// ────────────────────────────────────────────────────────────────────────────
// DesignRule Tests
// ────────────────────────────────────────────────────────────────────────────

void TestDesignRule::testRuleCreation() {
    // Test basic rule creation
    DesignRule rule("Test Rule", RuleCategory::ERC);
    
    QVERIFY(!rule.id().isNull());
    QCOMPARE(rule.name(), QString("Test Rule"));
    QCOMPARE(rule.category(), RuleCategory::ERC);
    QCOMPARE(rule.defaultSeverity(), RuleSeverity::Warning);
    QCOMPARE(rule.enabled(), true);
    QCOMPARE(rule.isConfigurable(), true);
    QVERIFY(rule.isValid());
    
    // Test property setters
    rule.setDescription("Test description");
    QCOMPARE(rule.description(), QString("Test description"));
    
    rule.setDefaultSeverity(RuleSeverity::Error);
    QCOMPARE(rule.defaultSeverity(), RuleSeverity::Error);
    
    rule.setEnabled(false);
    QCOMPARE(rule.enabled(), false);
    
    // Test parameters
    rule.parameters()["testParam"] = 42;
    QCOMPARE(rule.parameters()["testParam"].toInt(), 42);
}

void TestDesignRule::testRuleSerialization() {
    DesignRule rule("Serialize Test", RuleCategory::DRC);
    rule.setDescription("Test serialization");
    rule.setDefaultSeverity(RuleSeverity::Critical);
    rule.parameters()["minValue"] = 0.5;
    rule.parameters()["stringValue"] = "test";
    rule.setTags(QStringList() << "tag1" << "tag2");
    
    // Serialize to JSON
    QJsonObject json = rule.toJson();
    
    // Verify JSON structure
    QCOMPARE(json["name"].toString(), QString("Serialize Test"));
    QCOMPARE(json["category"].toInt(), static_cast<int>(RuleCategory::DRC));
    QCOMPARE(json["defaultSeverity"].toInt(), static_cast<int>(RuleSeverity::Critical));
    QVERIFY(json.contains("id"));
    QVERIFY(json.contains("parameters"));
    
    // Deserialize from JSON
    DesignRule rule2;
    rule2.fromJson(json);
    
    // Verify deserialization
    QCOMPARE(rule2.name(), rule.name());
    QCOMPARE(rule2.category(), rule.category());
    QCOMPARE(rule2.defaultSeverity(), rule.defaultSeverity());
    QCOMPARE(rule2.parameters()["minValue"].toDouble(), 0.5);
    QCOMPARE(rule2.tags(), rule.tags());
}

void TestDesignRule::testRuleValidation() {
    // Valid rule
    DesignRule validRule("Valid", RuleCategory::ERC);
    QVERIFY(validRule.isValid());
    QVERIFY(validRule.validationError().isEmpty());
    
    // Invalid: empty name
    DesignRule invalidName("", RuleCategory::ERC);
    QVERIFY(!invalidName.isValid());
    QCOMPARE(invalidName.validationError(), QString("Rule name is required"));
    
    // Invalid: custom rule without expression
    DesignRule invalidCustom("Custom", RuleCategory::Custom);
    QVERIFY(!invalidCustom.isValid());
    QCOMPARE(invalidCustom.validationError(), QString("Custom rules require an expression"));
    
    // Valid custom rule with expression
    DesignRule validCustom("Custom", RuleCategory::Custom);
    validCustom.setExpression("value > 0");
    QVERIFY(validCustom.isValid());
}

void TestDesignRule::testRuleParameters() {
    DesignRule rule("Param Test", RuleCategory::DRC);
    
    // Test various parameter types
    rule.parameters()["int"] = 42;
    rule.parameters()["double"] = 3.14;
    rule.parameters()["string"] = "hello";
    rule.parameters()["bool"] = true;
    rule.parameters()["list"] = QStringList() << "a" << "b" << "c";
    
    QCOMPARE(rule.parameters()["int"].toInt(), 42);
    QCOMPARE(rule.parameters()["double"].toDouble(), 3.14);
    QCOMPARE(rule.parameters()["string"].toString(), QString("hello"));
    QCOMPARE(rule.parameters()["bool"].toBool(), true);
    QCOMPARE(rule.parameters()["list"].toStringList().size(), 3);
}

void TestDesignRule::testRuleSignals() {
    DesignRule rule("Signal Test", RuleCategory::ERC);
    
    QSignalSpy nameSpy(&rule, SIGNAL(nameChanged(QString)));
    QSignalSpy severitySpy(&rule, SIGNAL(defaultSeverityChanged(RuleSeverity)));
    QSignalSpy enabledSpy(&rule, SIGNAL(enabledChanged(bool)));
    
    rule.setName("New Name");
    QCOMPARE(nameSpy.count(), 1);
    
    rule.setDefaultSeverity(RuleSeverity::Error);
    QCOMPARE(severitySpy.count(), 1);
    
    rule.setEnabled(false);
    QCOMPARE(enabledSpy.count(), 1);
}

// ────────────────────────────────────────────────────────────────────────────
// DesignRuleSet Tests
// ────────────────────────────────────────────────────────────────────────────

void TestDesignRule::testRuleSetCreation() {
    DesignRuleSet ruleSet("Test Set");
    
    QCOMPARE(ruleSet.name(), QString("Test Set"));
    QCOMPARE(ruleSet.ruleCount(), 0);
    QVERIFY(ruleSet.rules().isEmpty());
}

void TestDesignRule::testRuleSetAddRemove() {
    DesignRuleSet ruleSet("Test Set");
    
    // Add rules
    auto* rule1 = new DesignRule("Rule 1", RuleCategory::ERC);
    auto* rule2 = new DesignRule("Rule 2", RuleCategory::DRC);
    
    ruleSet.addRule(rule1);
    ruleSet.addRule(rule2);
    
    QCOMPARE(ruleSet.ruleCount(), 2);
    QCOMPARE(ruleSet.rulesByCategory(RuleCategory::ERC).count(), 1);
    QCOMPARE(ruleSet.rulesByCategory(RuleCategory::DRC).count(), 1);
    
    // Remove rule
    ruleSet.removeRule(rule1->id());
    QCOMPARE(ruleSet.ruleCount(), 1);
    QVERIFY(ruleSet.rule(rule1->id()) == nullptr);
    QVERIFY(ruleSet.rule(rule2->id()) != nullptr);
}

void TestDesignRule::testRuleSetSerialization() {
    DesignRuleSet ruleSet("Serialize Set");
    ruleSet.setDescription("Test description");
    
    auto* rule1 = new DesignRule("Rule 1", RuleCategory::ERC);
    rule1->setDefaultSeverity(RuleSeverity::Warning);
    rule1->parameters()["param1"] = 100;
    
    auto* rule2 = new DesignRule("Rule 2", RuleCategory::DRC);
    rule2->setDefaultSeverity(RuleSeverity::Error);
    rule2->setTags(QStringList() << "test");
    
    ruleSet.addRule(rule1);
    ruleSet.addRule(rule2);
    
    // Serialize
    QJsonObject json = ruleSet.toJson();
    
    QCOMPARE(json["name"].toString(), QString("Serialize Set"));
    QJsonArray rulesArray = json["rules"].toArray();
    QCOMPARE(rulesArray.size(), 2);
    
    // Deserialize
    DesignRuleSet ruleSet2("Empty");
    ruleSet2.fromJson(json);
    
    QCOMPARE(ruleSet2.name(), ruleSet.name());
    QCOMPARE(ruleSet2.ruleCount(), 2);
}

void TestDesignRule::testRuleSetFileIO() {
    QTemporaryDir tempDir;
    QString filePath = tempDir.filePath("test_rules.json");
    
    // Create and save rule set
    DesignRuleSet ruleSet("File IO Test");
    ruleSet.addRule(new DesignRule("Rule 1", RuleCategory::ERC));
    ruleSet.addRule(new DesignRule("Rule 2", RuleCategory::DRC));
    
    bool saved = ruleSet.saveToFile(filePath);
    QVERIFY(saved);
    QVERIFY(QFile::exists(filePath));
    
    // Load rule set
    DesignRuleSet* loaded = DesignRuleSet::loadFromFile(filePath);
    QVERIFY(loaded != nullptr);
    QCOMPARE(loaded->name(), QString("File IO Test"));
    QCOMPARE(loaded->ruleCount(), 2);
    
    delete loaded;
}

void TestDesignRule::testBuiltInRuleSets() {
    // Test default ERC set
    DesignRuleSet* ercSet = DesignRuleSet::createDefaultERCSet();
    QVERIFY(ercSet != nullptr);
    QCOMPARE(ercSet->name(), QString("Default ERC Rules"));
    QVERIFY(ercSet->ruleCount() > 0);
    
    // Verify specific rules exist
    bool foundOutputConflict = false;
    bool foundUnconnectedPin = false;
    for (const auto* rule : ercSet->rules()) {
        if (rule->name() == "Output Pin Conflicts")
            foundOutputConflict = true;
        if (rule->name() == "Unconnected Pins")
            foundUnconnectedPin = true;
    }
    QVERIFY(foundOutputConflict);
    QVERIFY(foundUnconnectedPin);
    
    delete ercSet;
    
    // Test default DRC set
    DesignRuleSet* drcSet = DesignRuleSet::createDefaultDRCSet();
    QVERIFY(drcSet != nullptr);
    QCOMPARE(drcSet->name(), QString("Default DRC Rules"));
    QVERIFY(drcSet->ruleCount() > 0);
    
    delete drcSet;
}

// ────────────────────────────────────────────────────────────────────────────
// Violation Tests
// ────────────────────────────────────────────────────────────────────────────

void TestDesignRule::testViolationCreation() {
    DesignRuleViolation violation;
    violation.ruleId = QUuid::createUuid();
    violation.ruleName = "Test Rule";
    violation.severity = RuleSeverity::Warning;
    violation.message = "Test violation message";
    violation.objectRef = "R1";
    violation.netName = "Net1";
    
    QCOMPARE(violation.severity, RuleSeverity::Warning);
    QCOMPARE(violation.message, QString("Test violation message"));
    QCOMPARE(violation.objectRef, QString("R1"));
}

void TestDesignRule::testViolationOrdering() {
    DesignRuleViolation error;
    error.severity = RuleSeverity::Error;
    error.message = "Error message";
    
    DesignRuleViolation warning1;
    warning1.severity = RuleSeverity::Warning;
    warning1.message = "Warning A";
    
    DesignRuleViolation warning2;
    warning2.severity = RuleSeverity::Warning;
    warning2.message = "Warning B";
    
    // Errors should come before warnings (higher severity = higher priority)
    QVERIFY(error < warning1);
    
    // Warnings should be sorted by message
    QVERIFY(warning1 < warning2);
}

QTEST_APPLESS_MAIN(TestDesignRule)
