#ifndef DESIGN_RULE_H
#define DESIGN_RULE_H

#include <QObject>
#include <QUuid>
#include <QString>
#include <QJsonObject>
#include <QVariantMap>
#include <QMetaType>
#include <QPointF>

namespace Flux {
namespace Core {

/**
 * @brief Severity levels for design rule violations
 */
enum class RuleSeverity {
    Info = 0,       ///< Informational message
    Warning = 1,    ///< Potential issue, design may still work
    Error = 2,      ///< Definite issue that should be fixed
    Critical = 3    ///< Severe issue that will cause failure
};

/**
 * @brief Categories of design rules
 */
enum class RuleCategory {
    ERC = 0,        ///< Electrical Rules Check
    DRC = 1,        ///< Design Rules Check
    Custom = 2,     ///< User-defined custom rules
    Manufacturing = 3, ///< Manufacturing/DFM rules
    Assembly = 4    ///< Assembly/DFA rules
};

/**
 * @brief Scope where the rule applies
 */
enum class RuleScope {
    Global = 0,     ///< System-wide default
    Project = 1,    ///< Project-specific
    Sheet = 2,      ///< Single schematic sheet
    Component = 3   ///< Specific component type
};

/**
 * @brief Represents a single design rule violation
 */
struct DesignRuleViolation {
    QUuid ruleId;           ///< ID of the rule that was violated
    QString ruleName;       ///< Name of the rule
    RuleSeverity severity;  ///< Severity level
    QString message;        ///< Human-readable description
    QString detailedMessage; ///< Extended details (optional)
    QString objectRef;      ///< Reference designator or object ID
    QPointF position;       ///< Position in schematic (if applicable)
    QVariantMap context;    ///< Additional context data
    QString suggestedFix;   ///< Suggested fix (optional)

    // For ERC-specific violations
    QString netName;
    QList<QString> involvedRefs; ///< All components/nets involved

    bool operator<(const DesignRuleViolation& other) const {
        if (severity != other.severity)
            return static_cast<int>(severity) > static_cast<int>(other.severity);
        return message < other.message;
    }
};

/**
 * @brief Represents a design rule definition
 * 
 * Design rules can be:
 * - Built-in ERC rules (pin connection matrix)
 * - Built-in DRC rules (clearance, connectivity)
 * - Custom user-defined rules (Python expressions)
 */
class DesignRule : public QObject {
    Q_OBJECT

    Q_PROPERTY(QUuid id READ id WRITE setId NOTIFY idChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(RuleCategory category READ category WRITE setCategory NOTIFY categoryChanged)
    Q_PROPERTY(RuleSeverity defaultSeverity READ defaultSeverity WRITE setDefaultSeverity NOTIFY defaultSeverityChanged)
    Q_PROPERTY(RuleScope scope READ scope WRITE setScope NOTIFY scopeChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool configurable READ isConfigurable WRITE setConfigurable NOTIFY configurableChanged)
    Q_PROPERTY(QVariantMap parameters READ parameters WRITE setParameters NOTIFY parametersChanged)
    Q_PROPERTY(QString expression READ expression WRITE setExpression NOTIFY expressionChanged)
    Q_PROPERTY(QStringList tags READ tags WRITE setTags NOTIFY tagsChanged)

public:
    explicit DesignRule(QObject* parent = nullptr);
    DesignRule(const QString& name, RuleCategory category, QObject* parent = nullptr);
    ~DesignRule();

    // Identity
    QUuid id() const { return m_id; }
    void setId(const QUuid& id);

    QString name() const { return m_name; }
    void setName(const QString& name);

    QString description() const { return m_description; }
    void setDescription(const QString& description);

    // Classification
    RuleCategory category() const { return m_category; }
    void setCategory(RuleCategory category);

    RuleScope scope() const { return m_scope; }
    void setScope(RuleScope scope);

    // Configuration
    RuleSeverity defaultSeverity() const { return m_defaultSeverity; }
    void setDefaultSeverity(RuleSeverity severity);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    bool isConfigurable() const { return m_configurable; }
    void setConfigurable(bool configurable);

    QVariantMap& parameters() { return m_parameters; }
    const QVariantMap& parameters() const { return m_parameters; }
    void setParameters(const QVariantMap& params);

    // For custom rules
    QString expression() const { return m_expression; }
    void setExpression(const QString& expr);

    // Metadata
    QStringList tags() const { return m_tags; }
    void setTags(const QStringList& tags);

    // Serialization
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

    // Validation
    bool isValid() const;
    QString validationError() const;

Q_SIGNALS:
    void idChanged(const QUuid& id);
    void nameChanged(const QString& name);
    void descriptionChanged(const QString& desc);
    void categoryChanged(RuleCategory category);
    void defaultSeverityChanged(RuleSeverity severity);
    void scopeChanged(RuleScope scope);
    void enabledChanged(bool enabled);
    void configurableChanged(bool configurable);
    void parametersChanged(const QVariantMap& params);
    void expressionChanged(const QString& expr);
    void tagsChanged(const QStringList& tags);

private:
    QUuid m_id;
    QString m_name;
    QString m_description;
    RuleCategory m_category;
    RuleScope m_scope;
    RuleSeverity m_defaultSeverity;
    bool m_enabled;
    bool m_configurable;
    QVariantMap m_parameters;
    QString m_expression;  // For custom rules (Python/QJS)
    QStringList m_tags;
};

/**
 * @brief Container for a set of design rules
 */
class DesignRuleSet : public QObject {
    Q_OBJECT

public:
    explicit DesignRuleSet(QObject* parent = nullptr);
    DesignRuleSet(const QString& name, QObject* parent = nullptr);
    ~DesignRuleSet();

    QString name() const { return m_name; }
    void setName(const QString& name);

    QString description() const { return m_description; }
    void setDescription(const QString& description);

    // Rule management
    void addRule(DesignRule* rule);
    void removeRule(const QUuid& ruleId);
    DesignRule* rule(const QUuid& ruleId) const;
    QList<DesignRule*> rules() const { return m_rules; }
    QList<DesignRule*> rulesByCategory(RuleCategory category) const;
    int ruleCount() const { return m_rules.count(); }

    // Enable/disable all
    void setAllEnabled(bool enabled);

    // Serialization
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

    // Load/save from file
    bool saveToFile(const QString& filePath) const;
    static DesignRuleSet* loadFromFile(const QString& filePath, QObject* parent = nullptr);

    // Built-in rule sets
    static DesignRuleSet* createDefaultERCSet(QObject* parent = nullptr);
    static DesignRuleSet* createDefaultDRCSet(QObject* parent = nullptr);
    static DesignRuleSet* createKiCadCompatibleSet(QObject* parent = nullptr);

Q_SIGNALS:
    void ruleAdded(DesignRule* rule);
    void ruleRemoved(const QUuid& ruleId);
    void rulesChanged();

private:
    QString m_name;
    QString m_description;
    QList<DesignRule*> m_rules;
};

} // namespace Core
} // namespace Flux

Q_DECLARE_METATYPE(Flux::Core::RuleSeverity)
Q_DECLARE_METATYPE(Flux::Core::RuleCategory)
Q_DECLARE_METATYPE(Flux::Core::RuleScope)
Q_DECLARE_METATYPE(Flux::Core::DesignRuleViolation)

#endif // DESIGN_RULE_H
