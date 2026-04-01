# Design Rule Management System - Testing Guide

## Overview
This guide explains how to test the Design Rule Management System implementation.

---

## 1. Automated Unit Tests

### Build and Run Tests

```bash
cd /home/jnd/qt_projects/viospice/build

# Configure with testing enabled
cmake .. -DCMAKE_PREFIX_PATH=/home/jnd/Qt/6.10.1/gcc_64 -DBUILD_TESTING=ON

# Build the test executable
make test_design_rule

# Run the tests
ctest -R design_rule -V
# OR run directly
./test_design_rule
```

### Expected Output
```
********* Start testing of TestDesignRule *********
Config: Using QtTest library 6.10.1, Qt 6.10.1 (x86_64-little_endian-lp64 shared (dynamic) release build - by GCC 13.3.0)
PASS   : TestDesignRule::testRuleCreation()
PASS   : TestDesignRule::testRuleSerialization()
PASS   : TestDesignRule::testRuleValidation()
PASS   : TestDesignRule::testRuleParameters()
PASS   : TestDesignRule::testRuleSignals()
PASS   : TestDesignRule::testRuleSetCreation()
PASS   : TestDesignRule::testRuleSetAddRemove()
PASS   : TestDesignRule::testRuleSetSerialization()
PASS   : TestDesignRule::testRuleSetFileIO()
PASS   : TestDesignRule::testBuiltInRuleSets()
PASS   : TestDesignRule::testViolationCreation()
PASS   : TestDesignRule::testViolationOrdering()
Totals: 12 passed, 0 failed, 0 skipped
********* Finished testing of TestDesignRule *********
```

---

## 2. Manual Testing - API Usage

### Test 1: Create and Configure Rules

Create a test program `test_rules.cpp`:

```cpp
#include <QCoreApplication>
#include <QDebug>
#include "../core/design_rule.h"

using namespace Flux::Core;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Test 1: Create a rule set
    qDebug() << "=== Test 1: Create Rule Set ===";
    DesignRuleSet* ruleSet = new DesignRuleSet("My Custom Rules");
    ruleSet->setDescription("Testing custom rule creation");
    
    // Test 2: Add built-in ERC rules
    qDebug() << "\n=== Test 2: Add Built-in Rules ===";
    DesignRuleSet* ercSet = DesignRuleSet::createDefaultERCSet();
    qDebug() << "Created ERC set with" << ercSet->ruleCount() << "rules";
    
    for (const auto* rule : ercSet->rules()) {
        qDebug() << "  -" << rule->name() 
                 << "(Severity:" << rule->defaultSeverity() << ")";
    }
    
    // Test 3: Create custom rule
    qDebug() << "\n=== Test 3: Create Custom Rule ===";
    auto* customRule = new DesignRule("Check Decoupling Capacitors", RuleCategory::Custom);
    customRule->setDescription("All ICs must have VCC decoupling capacitor");
    customRule->setDefaultSeverity(RuleSeverity::Warning);
    customRule->setTags(QStringList() << "power" << "decoupling" << "ic");
    customRule->parameters()["minCapacitance"] = 0.1e-6; // 100nF
    customRule->parameters()["unit"] = "F";
    
    qDebug() << "Custom rule valid:" << customRule->isValid();
    qDebug() << "Custom rule JSON:" << QJsonDocument(customRule->toJson()).toJson(QJsonDocument::Compact);
    
    // Test 4: Save and load rule set
    qDebug() << "\n=== Test 4: Save/Load Rule Set ===";
    QString testFile = "/tmp/test_rules.json";
    
    DesignRuleSet* saveSet = new DesignRuleSet("Save Test");
    saveSet->addRule(new DesignRule("Rule 1", RuleCategory::ERC));
    saveSet->addRule(new DesignRule("Rule 2", RuleCategory::DRC));
    
    bool saved = saveSet->saveToFile(testFile);
    qDebug() << "Saved:" << saved;
    
    DesignRuleSet* loaded = DesignRuleSet::loadFromFile(testFile);
    if (loaded) {
        qDebug() << "Loaded set name:" << loaded->name();
        qDebug() << "Loaded rule count:" << loaded->ruleCount();
        delete loaded;
    }
    
    // Cleanup
    delete ruleSet;
    delete ercSet;
    delete saveSet;
    
    qDebug() << "\n=== All Tests Complete ===";
    return 0;
}
```

Compile and run:
```bash
cd /home/jnd/qt_projects/viospice/build
g++ -std=c++17 -I../core -I/home/jnd/Qt/6.10.1/gcc_64/include \
    -L. -lFluxCore -lQt6Core -o test_rules ../test_rules.cpp
./test_rules
```

---

## 3. Integration Testing with Schematic Editor

### Test Scenario 1: Load Default ERC Rules in Editor

Add this test code to `schematic_editor.cpp` temporarily:

```cpp
#include "../core/design_rule.h"
using namespace Flux::Core;

// In constructor after setup:
DesignRuleSet* ercRules = DesignRuleSet::createDefaultERCSet(this);
qDebug() << "Loaded" << ercRules->ruleCount() << "ERC rules";

// Print rule summary
for (const auto* rule : ercRules->rules()) {
    qDebug() << "ERC Rule:" << rule->name() 
             << "- Enabled:" << rule->enabled()
             << "- Severity:" << rule->defaultSeverity();
}
```

### Test Scenario 2: Test Rule Violation Creation

```cpp
// Create a test violation
DesignRuleViolation violation;
violation.ruleId = QUuid::createUuid();
violation.ruleName = "Output Pin Conflicts";
violation.severity = RuleSeverity::Error;
violation.message = "R1 and U1 both drive net VCC";
violation.objectRef = "R1";
violation.netName = "VCC";
violation.involvedRefs = QStringList() << "R1" << "U1";

// Test violation comparison (for sorting)
DesignRuleViolation warning;
warning.severity = RuleSeverity::Warning;
warning.message = "Unconnected pin";

// Error should have higher priority (sort first)
Q_ASSERT(violation < warning);
qDebug() << "Violation ordering test passed";
```

---

## 4. Performance Testing

### Test Rule Set Loading Performance

```cpp
#include <QElapsedTimer>

QElapsedTimer timer;
timer.start();

// Load rule set 100 times
for (int i = 0; i < 100; ++i) {
    DesignRuleSet* set = DesignRuleSet::createDefaultERCSet();
    delete set;
}

qint64 elapsed = timer.elapsed();
qDebug() << "Loaded ERC rule set 100 times in" << elapsed << "ms";
qDebug() << "Average:" << (double)elapsed / 100 << "ms per load";
```

**Expected:** < 1ms per load

### Test JSON Serialization Performance

```cpp
DesignRuleSet* set = DesignRuleSet::createDefaultERCSet();

QElapsedTimer timer;
timer.start();

// Serialize/deserialize 1000 times
for (int i = 0; i < 1000; ++i) {
    QJsonObject json = set->toJson();
    DesignRuleSet set2("temp");
    set2.fromJson(json);
}

qint64 elapsed = timer.elapsed();
qDebug() << "1000 serialize/deserialize cycles:" << elapsed << "ms";
```

**Expected:** < 10ms for 1000 cycles

---

## 5. Edge Case Testing

### Test 1: Empty Rule Set
```cpp
DesignRuleSet emptySet("Empty");
Q_ASSERT(emptySet.ruleCount() == 0);
Q_ASSERT(emptySet.rules().isEmpty());

// Save empty set
bool saved = emptySet.saveToFile("/tmp/empty.json");
Q_ASSERT(saved == true);

// Load empty set
DesignRuleSet* loaded = DesignRuleSet::loadFromFile("/tmp/empty.json");
Q_ASSERT(loaded != nullptr);
Q_ASSERT(loaded->ruleCount() == 0);
delete loaded;
```

### Test 2: Invalid Rule
```cpp
DesignRule invalidRule(""); // Empty name
Q_ASSERT(!invalidRule.isValid());
Q_ASSERT(invalidRule.validationError() == "Rule name is required");

DesignRule invalidCustom("Custom", RuleCategory::Custom);
Q_ASSERT(!invalidCustom.isValid());
Q_ASSERT(invalidCustom.validationError() == "Custom rules require an expression");
```

### Test 3: Corrupt JSON File
```cpp
// Create corrupt JSON file
QFile file("/tmp/corrupt.json");
file.open(QIODevice::WriteOnly);
file.write("{ invalid json }}}");
file.close();

DesignRuleSet* loaded = DesignRuleSet::loadFromFile("/tmp/corrupt.json");
Q_ASSERT(loaded == nullptr); // Should return nullptr for invalid JSON
```

### Test 4: Missing File
```cpp
DesignRuleSet* loaded = DesignRuleSet::loadFromFile("/nonexistent/path.json");
Q_ASSERT(loaded == nullptr);
```

---

## 6. Test Checklist

### Core Functionality
- [ ] Rule creation with all categories
- [ ] Rule severity levels (Info, Warning, Error, Critical)
- [ ] Rule parameters (int, double, string, bool, list)
- [ ] Rule validation
- [ ] Rule serialization to JSON
- [ ] Rule deserialization from JSON
- [ ] Rule set creation
- [ ] Add/remove rules from set
- [ ] Filter rules by category
- [ ] Save rule set to file
- [ ] Load rule set from file
- [ ] Built-in ERC rule set creation
- [ ] Built-in DRC rule set creation
- [ ] Violation creation
- [ ] Violation ordering (by severity)

### Qt Integration
- [ ] Signal emission on property changes
- [ ] Parent-child object relationships
- [ ] Meta-object system integration
- [ ] Property system access

### Performance
- [ ] Rule set loading < 1ms
- [ ] JSON serialization < 0.01ms per rule
- [ ] No memory leaks (test with Valgrind)

### Edge Cases
- [ ] Empty rule sets
- [ ] Invalid rule names
- [ ] Corrupt JSON files
- [ ] Missing files
- [ ] Large rule sets (1000+ rules)

---

## 7. Debugging Tips

### Enable Debug Output
```cpp
// In your test code
QLoggingCategory::setFilterRules("qt.debug=true");
```

### Memory Leak Detection
```bash
# Run with Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./test_design_rule
```

### Qt Test Logging
```bash
# Verbose output
ctest -R design_rule -V

# Output to file
ctest -R design_rule --output-on-failure
```

---

## 8. Next Steps After Testing

Once all tests pass:

1. **Phase 1.3**: Implement DesignRuleEngine with multi-threading
2. **Phase 2**: Add new ERC check types and DRC engine
3. **Phase 3**: Create UI dialogs for rule editing
4. **Phase 4**: Integrate with ERCDiagnosticsPanel
5. **Phase 5**: Add rule templates and storage
6. **Phase 6**: Add Python scripting API

---

## 9. Reporting Issues

If you find bugs, create an issue with:
- Test case that reproduces the issue
- Expected vs actual behavior
- Qt version and platform
- Build configuration (Debug/Release)

---

## Contact

For questions about testing, refer to:
- Qt Test Framework documentation: https://doc.qt.io/qt-6/qtest.html
- Project README.md
- Core team members
