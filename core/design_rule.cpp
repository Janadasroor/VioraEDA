// design_rule.cpp
// Unified design rule data model for VIOSPICE

#include "design_rule.h"
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

// QUuid helper for JSON serialization
namespace {
    QString uuidToString(const QUuid& uuid) {
        return uuid.toString(QUuid::WithoutBraces);
    }
    
    QUuid uuidFromString(const QString& str) {
        return QUuid::fromString(str);
    }
}

namespace Flux {
namespace Core {

// ────────────────────────────────────────────────────────────────────────────
// DesignRule Implementation
// ────────────────────────────────────────────────────────────────────────────

DesignRule::DesignRule(QObject* parent)
    : QObject(parent)
    , m_id(QUuid::createUuid())
    , m_category(RuleCategory::ERC)
    , m_scope(RuleScope::Global)
    , m_defaultSeverity(RuleSeverity::Warning)
    , m_enabled(true)
    , m_configurable(true)
{
}

DesignRule::DesignRule(const QString& name, RuleCategory category, QObject* parent)
    : QObject(parent)
    , m_id(QUuid::createUuid())
    , m_name(name)
    , m_category(category)
    , m_scope(RuleScope::Global)
    , m_defaultSeverity(RuleSeverity::Warning)
    , m_enabled(true)
    , m_configurable(true)
{
}

DesignRule::~DesignRule() {
}

void DesignRule::setId(const QUuid& id) {
    if (m_id != id) {
        m_id = id;
        emit idChanged(id);
    }
}

void DesignRule::setName(const QString& name) {
    if (m_name != name) {
        m_name = name;
        emit nameChanged(name);
    }
}

void DesignRule::setDescription(const QString& description) {
    if (m_description != description) {
        m_description = description;
        emit descriptionChanged(description);
    }
}

void DesignRule::setCategory(RuleCategory category) {
    if (m_category != category) {
        m_category = category;
        emit categoryChanged(category);
    }
}

void DesignRule::setScope(RuleScope scope) {
    if (m_scope != scope) {
        m_scope = scope;
        emit scopeChanged(scope);
    }
}

void DesignRule::setDefaultSeverity(RuleSeverity severity) {
    if (m_defaultSeverity != severity) {
        m_defaultSeverity = severity;
        emit defaultSeverityChanged(severity);
    }
}

void DesignRule::setEnabled(bool enabled) {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit enabledChanged(enabled);
    }
}

void DesignRule::setConfigurable(bool configurable) {
    if (m_configurable != configurable) {
        m_configurable = configurable;
        emit configurableChanged(configurable);
    }
}

void DesignRule::setParameters(const QVariantMap& params) {
    if (m_parameters != params) {
        m_parameters = params;
        emit parametersChanged(params);
    }
}

void DesignRule::setExpression(const QString& expr) {
    if (m_expression != expr) {
        m_expression = expr;
        emit expressionChanged(expr);
    }
}

void DesignRule::setTags(const QStringList& tags) {
    if (m_tags != tags) {
        m_tags = tags;
        emit tagsChanged(tags);
    }
}

QJsonObject DesignRule::toJson() const {
    QJsonObject obj;
    obj["id"] = m_id.toString();
    obj["name"] = m_name;
    obj["description"] = m_description;
    obj["category"] = static_cast<int>(m_category);
    obj["scope"] = static_cast<int>(m_scope);
    obj["defaultSeverity"] = static_cast<int>(m_defaultSeverity);
    obj["enabled"] = m_enabled;
    obj["configurable"] = m_configurable;
    obj["tags"] = QJsonArray::fromStringList(m_tags);

    // Serialize parameters
    QJsonObject paramsObj;
    for (auto it = m_parameters.begin(); it != m_parameters.end(); ++it) {
        paramsObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    obj["parameters"] = paramsObj;

    if (!m_expression.isEmpty()) {
        obj["expression"] = m_expression;
    }

    return obj;
}

void DesignRule::fromJson(const QJsonObject& json) {
    if (json.contains("id")) {
        m_id = uuidFromString(json["id"].toString());
    }
    if (json.contains("name")) {
        m_name = json["name"].toString();
    }
    if (json.contains("description")) {
        m_description = json["description"].toString();
    }
    if (json.contains("category")) {
        m_category = static_cast<RuleCategory>(json["category"].toInt());
    }
    if (json.contains("scope")) {
        m_scope = static_cast<RuleScope>(json["scope"].toInt());
    }
    if (json.contains("defaultSeverity")) {
        m_defaultSeverity = static_cast<RuleSeverity>(json["defaultSeverity"].toInt());
    }
    if (json.contains("enabled")) {
        m_enabled = json["enabled"].toBool();
    }
    if (json.contains("configurable")) {
        m_configurable = json["configurable"].toBool();
    }
    if (json.contains("tags")) {
        QJsonArray tagsArray = json["tags"].toArray();
        m_tags.clear();
        for (const QJsonValue& val : tagsArray) {
            m_tags.append(val.toString());
        }
    }

    // Deserialize parameters
    if (json.contains("parameters")) {
        QJsonObject paramsObj = json["parameters"].toObject();
        for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it) {
            m_parameters[it.key()] = it.value().toVariant();
        }
    }

    if (json.contains("expression")) {
        m_expression = json["expression"].toString();
    }
}

bool DesignRule::isValid() const {
    if (m_name.isEmpty()) {
        return false;
    }
    if (m_category == RuleCategory::Custom && m_expression.isEmpty()) {
        return false;
    }
    return true;
}

QString DesignRule::validationError() const {
    if (m_name.isEmpty()) {
        return "Rule name is required";
    }
    if (m_category == RuleCategory::Custom && m_expression.isEmpty()) {
        return "Custom rules require an expression";
    }
    return QString();
}

// ────────────────────────────────────────────────────────────────────────────
// DesignRuleSet Implementation
// ────────────────────────────────────────────────────────────────────────────

DesignRuleSet::DesignRuleSet(QObject* parent)
    : QObject(parent)
{
}

DesignRuleSet::DesignRuleSet(const QString& name, QObject* parent)
    : QObject(parent)
    , m_name(name)
{
}

DesignRuleSet::~DesignRuleSet() {
    qDeleteAll(m_rules);
}

void DesignRuleSet::setName(const QString& name) {
    m_name = name;
}

void DesignRuleSet::setDescription(const QString& description) {
    m_description = description;
}

void DesignRuleSet::addRule(DesignRule* rule) {
    if (rule && !m_rules.contains(rule)) {
        m_rules.append(rule);
        rule->setParent(this);
        emit ruleAdded(rule);
        emit rulesChanged();
    }
}

void DesignRuleSet::removeRule(const QUuid& ruleId) {
    for (int i = 0; i < m_rules.count(); ++i) {
        if (m_rules[i]->id() == ruleId) {
            DesignRule* rule = m_rules.takeAt(i);
            rule->deleteLater();
            emit ruleRemoved(ruleId);
            emit rulesChanged();
            return;
        }
    }
}

DesignRule* DesignRuleSet::rule(const QUuid& ruleId) const {
    for (DesignRule* rule : m_rules) {
        if (rule->id() == ruleId) {
            return rule;
        }
    }
    return nullptr;
}

QList<DesignRule*> DesignRuleSet::rulesByCategory(RuleCategory category) const {
    QList<DesignRule*> result;
    for (DesignRule* rule : m_rules) {
        if (rule->category() == category) {
            result.append(rule);
        }
    }
    return result;
}

void DesignRuleSet::setAllEnabled(bool enabled) {
    for (DesignRule* rule : m_rules) {
        rule->setEnabled(enabled);
    }
    emit rulesChanged();
}

QJsonObject DesignRuleSet::toJson() const {
    QJsonObject obj;
    obj["name"] = m_name;
    obj["description"] = m_description;

    QJsonArray rulesArray;
    for (const DesignRule* rule : m_rules) {
        rulesArray.append(rule->toJson());
    }
    obj["rules"] = rulesArray;

    return obj;
}

void DesignRuleSet::fromJson(const QJsonObject& json) {
    if (json.contains("name")) {
        m_name = json["name"].toString();
    }
    if (json.contains("description")) {
        m_description = json["description"].toString();
    }

    // Clear existing rules
    qDeleteAll(m_rules);
    m_rules.clear();

    // Load rules from JSON
    if (json.contains("rules")) {
        QJsonArray rulesArray = json["rules"].toArray();
        for (const QJsonValue& value : rulesArray) {
            DesignRule* rule = new DesignRule(this);
            rule->fromJson(value.toObject());
            m_rules.append(rule);
        }
    }

    emit rulesChanged();
}

bool DesignRuleSet::saveToFile(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson());
    file.close();
    return true;
}

DesignRuleSet* DesignRuleSet::loadFromFile(const QString& filePath, QObject* parent) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return nullptr;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isNull()) {
        return nullptr;
    }

    DesignRuleSet* ruleSet = new DesignRuleSet(parent);
    ruleSet->fromJson(doc.object());
    return ruleSet;
}

DesignRuleSet* DesignRuleSet::createDefaultERCSet(QObject* parent) {
    DesignRuleSet* set = new DesignRuleSet("Default ERC Rules", parent);
    set->setDescription("Standard Electrical Rules Check based on KiCad defaults");

    // Output pin conflicts
    auto* outputConflict = new DesignRule("Output Pin Conflicts", RuleCategory::ERC, set);
    outputConflict->setDescription("Two or more output pins connected to same net");
    outputConflict->setDefaultSeverity(RuleSeverity::Error);
    outputConflict->setTags({"connectivity", "output"});
    outputConflict->parameters()["checkOutputs"] = true;
    set->addRule(outputConflict);

    // Unconnected pins
    auto* unconnectedPin = new DesignRule("Unconnected Pins", RuleCategory::ERC, set);
    unconnectedPin->setDescription("Pins that should be connected but are not");
    unconnectedPin->setDefaultSeverity(RuleSeverity::Warning);
    unconnectedPin->setTags({"connectivity", "unconnected"});
    unconnectedPin->parameters()["ignoreTypes"] = QStringList{"NC", "Not Connected"};
    set->addRule(unconnectedPin);

    // Duplicate designators
    auto* duplicateRef = new DesignRule("Duplicate Designators", RuleCategory::ERC, set);
    duplicateRef->setDescription("Multiple components with same reference designator");
    duplicateRef->setDefaultSeverity(RuleSeverity::Error);
    duplicateRef->setTags({"annotation", "reference"});
    set->addRule(duplicateRef);

    // Missing values
    auto* missingValue = new DesignRule("Missing Component Values", RuleCategory::ERC, set);
    missingValue->setDescription("Components without value specification");
    missingValue->setDefaultSeverity(RuleSeverity::Warning);
    missingValue->setTags({"annotation", "value"});
    set->addRule(missingValue);

    // Power conflicts
    auto* powerConflict = new DesignRule("Power Output Conflicts", RuleCategory::ERC, set);
    powerConflict->setDescription("Multiple power outputs connected together");
    powerConflict->setDefaultSeverity(RuleSeverity::Error);
    powerConflict->setTags({"power", "connectivity"});
    set->addRule(powerConflict);

    return set;
}

DesignRuleSet* DesignRuleSet::createDefaultDRCSet(QObject* parent) {
    DesignRuleSet* set = new DesignRuleSet("Default DRC Rules", parent);
    set->setDescription("Standard Design Rules Check");

    // Minimum clearance
    auto* clearance = new DesignRule("Minimum Component Clearance", RuleCategory::DRC, set);
    clearance->setDescription("Components must maintain minimum clearance");
    clearance->setDefaultSeverity(RuleSeverity::Warning);
    clearance->setTags({"clearance", "mechanical"});
    clearance->parameters()["minClearance"] = 0.2; // mm
    clearance->parameters()["unit"] = "mm";
    set->addRule(clearance);

    // Unconnected nets
    auto* unconnectedNet = new DesignRule("Unconnected Nets", RuleCategory::DRC, set);
    unconnectedNet->setDescription("Nets with no connections");
    unconnectedNet->setDefaultSeverity(RuleSeverity::Warning);
    unconnectedNet->setTags({"connectivity", "nets"});
    unconnectedNet->parameters()["ignorePatterns"] = QStringList{"NC", "N/C", "No Connect"};
    set->addRule(unconnectedNet);

    // Missing footprint
    auto* missingFootprint = new DesignRule("Missing Footprint", RuleCategory::DRC, set);
    missingFootprint->setDescription("Components without footprint assignment");
    missingFootprint->setDefaultSeverity(RuleSeverity::Error);
    missingFootprint->setTags({"footprint", "PCB"});
    set->addRule(missingFootprint);

    return set;
}

DesignRuleSet* DesignRuleSet::createKiCadCompatibleSet(QObject* parent) {
    // KiCad-compatible rules (similar to default ERC)
    return createDefaultERCSet(parent);
}

} // namespace Core
} // namespace Flux
